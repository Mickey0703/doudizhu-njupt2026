#include "gamepanel.h"
#include "./ui_gamepanel.h"
#include "playhand.h"

#include "popup_widget.h"
#include "avatar_picker.h"
#include "relay_game_mapping.h"

#include <QPainter>
#include <QMessageBox>
#include <QApplication>
#include <QInputDialog>
#include <QDebug>
#include <QFile>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QGraphicsOpacityEffect>
#include <QEasingCurve>
#include <QGraphicsDropShadowEffect>
#include <QSizePolicy>
#include <QColor>
#include <QComboBox>
#include <QDir>
#include <QRandomGenerator>
#include <QIcon>
#include <QFontMetrics>
#include <QMovie>

// ===================================================================
// Window control styles (card geometry belongs to CardTableView).
// ===================================================================

/// 主菜单按钮样式：半透明黑底 + 金色边框 + 悬停高亮
static const char *S_MENU_BTN =
    "QPushButton{background:rgba(0,0,0,140);color:#FFD700;border:2px solid #FFD700;"
    "border-radius:12px;font-size:14px;font-weight:bold;padding:6px 16px;}"
    "QPushButton:hover{background:rgba(255,215,0,40);border-color:#FFF;color:#FFF;}";

/// 开始/再来按钮：橙色背景
static const char *S_START =
    "QPushButton{background:#FF9800;color:white;border-radius:10px;"
    "font-size:16px;font-weight:bold;padding:8px 16px;}"
    "QPushButton:hover{background:#FFB74D;}";

/// 返回菜单按钮：半透明浅色
static const char *S_BACK =
    "QPushButton{background:rgba(0,0,0,120);color:#ccc;border:1px solid #888;"
    "border-radius:6px;font-size:12px;padding:4px 10px;}"
    "QPushButton:hover{background:rgba(255,255,255,30);color:white;}";

/// 出牌按钮：绿色
static const char *S_PLAY =
    "QPushButton{background:#4CAF50;color:white;border-radius:6px;"
    "font-size:13px;font-weight:bold;padding:6px 12px;}"
    "QPushButton:hover{background:#66BB6A;}";

/// 不出按钮：红色
static const char *S_PASS =
    "QPushButton{background:#f44336;color:white;border-radius:6px;"
    "font-size:13px;font-weight:bold;padding:6px 12px;}"
    "QPushButton:hover{background:#EF5350;}";

/// 提示按钮：蓝色
static const char *S_HINT =
    "QPushButton{background:#2196F3;color:white;border-radius:6px;"
    "font-size:13px;font-weight:bold;padding:6px 12px;}"
    "QPushButton:hover{background:#42A5F5;}";

/// 叫分按钮：橙色
static const char *S_CALL =
    "QPushButton{background:#FF9800;color:white;border-radius:8px;"
    "font-size:14px;font-weight:bold;padding:6px 10px;}"
    "QPushButton:hover{background:#FFB74D;}";

/// 不叫按钮：灰色
static const char *S_NOCALL =
    "QPushButton{background:#9E9E9E;color:white;border-radius:8px;"
    "font-size:14px;font-weight:bold;padding:6px 10px;}"
    "QPushButton:hover{background:#BDBDBD;}";

// ===================================================================
// 构造 / 析构
// ===================================================================

GamePanel::GamePanel(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::GamePanel)
    , m_gameControl(nullptr)
{
    ui->setupUi(this);

    // 全窗口自绘游戏，不需要 QMainWindow 默认的菜单栏/状态栏。留着它们会在窗口
    // 边缘渲染出细横条 + 状态栏右下角的 size-grip 斜网格残影。显式移除以免惰性重建。
    setMenuBar(nullptr);
    setStatusBar(nullptr);

    setWindowTitle(QStringLiteral("斗地主 — DouDiZhu"));

    // 默认窗口大小（5 按钮菜单需要足够高度）
    resize(1024, 720);
    setMinimumSize(900, 680);

    // 初始化顺序很重要：背景 → UI控件 → 游戏逻辑 → 信号连接 → 进入菜单
    initBackground();
    initUI();
    initGame();
    initConnections();
    enterMenuMode();
}

GamePanel::~GamePanel()
{
    delete ui;
}

// ===================================================================
// 背景（独立 QLabel 铺满全窗，支持缩放裁剪）
// ===================================================================

// 解析菜单 logo/图标文件：从磁盘 images/logo/ 目录多路径查找（丢文件即生效，无需重编）。
// 与 initBackground 同款搜索顺序：构建目录 → 源目录。返回首个存在路径，否则返回 ""，
// 调用方据此回退到原有 emoji/文字。
static QString resolveLogo(const QString &file)
{
    const QString base = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        base + "/images/logo/" + file,
        base + "/resources/images/logo/" + file,
        base + "/../images/logo/" + file,
        base + "/../resources/images/logo/" + file,
    };
    for (const QString &p : candidates)
        if (QFile::exists(p))
            return p;
    return QString();
}

void GamePanel::initBackground()
{
    // 多路径搜索背景图片：构建目录 → 源目录 → QRC
    QStringList bgCandidates = {
        QCoreApplication::applicationDirPath() + "/images/Common_BG3_2x.png",
        QCoreApplication::applicationDirPath() + "/resources/images/Common_BG3_2x.png",
        QCoreApplication::applicationDirPath() + "/../images/Common_BG3_2x.png",
        QCoreApplication::applicationDirPath() + "/../resources/images/Common_BG3_2x.png",
    };
    for (const auto &img : bgCandidates)
    {
        if (QFile::exists(img))
        {
            m_bgSource = QPixmap(img);
            break;
        }
    }
    if (m_bgSource.isNull())
    {
        // 最终回退到 Qt 资源文件中的备用背景
        m_bgSource = QPixmap(":/images/ui/table_bg.png");
    }

    // 创建全屏背景标签
    m_bgLabel = new QLabel(this);
    m_bgLabel->setScaledContents(false);               // 我们手动控制缩放
    m_bgLabel->setGeometry(0, 0, width(), height());
    m_bgLabel->setPixmap(m_bgSource);
    m_bgLabel->lower();                                 // 始终最底层
    m_bgLabel->setAttribute(Qt::WA_TransparentForMouseEvents); // 不拦截鼠标事件

    // centralWidget 必须透明才能看到背景
    if (centralWidget())
    {
        centralWidget()->setStyleSheet("background: transparent;");
        centralWidget()->setAutoFillBackground(false);
    }

    // 立即按初始尺寸铺满一次。构造函数里 resize() 在本函数之前执行，那次 resizeEvent
    // 触发时 m_bgLabel 还没创建、缩放被跳过；不在这里补一次的话，初始画面会露出未铺满的
    // 原图，直到用户手动改变窗口大小才铺满。
    applyBackgroundScaled();
}

// 按当前窗口尺寸等比放大铺满背景并裁剪（KeepAspectRatioByExpanding + 居中/顶部裁剪），
// 保证任何窗口尺寸/比例下背景都填满、无黑边。initBackground 与 resizeEvent 共用。
void GamePanel::applyBackgroundScaled()
{
    if (!m_bgLabel || m_bgSource.isNull())
        return;
    const QSize target = size();
    if (target.isEmpty())
        return;
    const QPixmap scaled = m_bgSource.scaled(target, Qt::KeepAspectRatioByExpanding,
                                             Qt::SmoothTransformation);
    const int cropX = (scaled.width() - target.width()) / 2;   // 左右居中裁剪
    int cropY = 0;                                             // 顶部对齐（避免黑边）
    if (cropY + target.height() > scaled.height())
        cropY = qMax(0, scaled.height() - target.height());
    const QPixmap cropped = scaled.copy(cropX, cropY, target.width(), target.height());
    m_bgLabel->setGeometry(0, 0, target.width(), target.height());
    m_bgLabel->setPixmap(cropped);
}

// ===================================================================
// 窗口大小改变时的响应式布局
// ===================================================================

void GamePanel::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (!m_cardLayer || !m_btnVsAI) return;

    // 调整卡片层大小，确保覆盖整个窗口
    if (m_cardLayer)
        m_cardLayer->setGeometry(0, 0, width(), height());

    // ---- 背景缩放 ---- 等比放大铺满 + 裁剪（与初始化共用同一逻辑）
    applyBackgroundScaled();

    int w = width(), h = height();

    // ---- 响应式重排各区域 ----
    arrangeMenuButtons();

    // 返回按钮（左上角固定）
    m_btnBackMenu->move(10, 10);

    // 开始/再来按钮 —— 宽度随窗口自适应
    int startW = qBound(120, width() * 15 / 100, 260);
    int startH = qBound(36, height() * 8 / 100, 64);
    m_btnStart->setFixedSize(startW, startH);
    m_btnRestart->setFixedSize(startW, startH);
    m_btnStart->move(w - startW - 20, h / 2 - startH / 2);
    m_btnRestart->move(w - startW - 20, h / 2 - startH / 2);

    // 机器人信息面板
    if (m_gameControl)
    {
        // 左座信息右移到返回按钮（x=10~110）之后，避免"返回菜单"压住左家昵称。
        m_leftAvatar->move(120, 8);
        m_leftNameLabel->move(180, 8);
        m_leftLordIcon->move(255, 14);
        arrangeRobotCards(m_gameControl->getLeftRobot());

        int rightX = w - 215;
        m_rightAvatar->move(rightX, 8);
        m_rightNameLabel->move(rightX + 60, 8);
        m_rightLordIcon->move(rightX + 140, 14);
        arrangeRobotCards(m_gameControl->getRightRobot());
    }

    // 用户信息面板（底部）
    m_userAvatar->move(15, h - 68);
    m_userNameLabel->move(75, h - 68);
    m_userLordIcon->move(155, h - 66);
    arrangeUserCards();

    // 出牌区 & 底牌区
    arrangePlayArea();
    arrangeLordArea();

    // 状态标签（居中）
    m_statusLabel->setGeometry(w / 2 - 250, h / 2 - 50, 500, 60);


    // 倒计时标签订位
    m_timerLabel->move(w / 2 - 27, 45);

    // ---- 底部控制按钮（出牌/不出/提示）----
    int playH = m_btnPlay->minimumHeight();
    int playW = qBound(64, width() * 8 / 100, 140);
    int spacing = qBound(8, playW / 8, 24);
    int totalW = playW * 3 + spacing * 2;
    int btnX = w - totalW - 20;
    int btnY = h - playH - 18;
    m_btnPlay->setFixedSize(playW, playH);
    m_btnPass->setFixedSize(playW, playH);
    m_btnHint->setFixedSize(playW, playH);
    m_btnPlay->move(btnX, btnY);
    m_btnPass->move(btnX + playW + spacing, btnY);
    m_btnHint->move(btnX + (playW + spacing) * 2, btnY);
    m_btnPlay->raise();
    m_btnPass->raise();
    m_btnHint->raise();

    // ---- 叫分按钮（出牌按钮上方一行）----
    int callW = qBound(48, width() * 6 / 100, 96);
    int callH = m_btnCall1->minimumHeight();
    int callSpacing = qBound(8, callW / 6, 16);
    int callX = w - (callW * 4 + callSpacing * 3) - 20;
    int callY = h - callH - 68;
    m_btnCall1->setFixedSize(callW, callH);
    m_btnCall2->setFixedSize(callW, callH);
    m_btnCall3->setFixedSize(callW, callH);
    m_btnNoCall->setFixedSize(callW, callH);
    m_btnCall1->move(callX, callY);
    m_btnCall2->move(callX + (callW + callSpacing) * 1, callY);
    m_btnCall3->move(callX + (callW + callSpacing) * 2, callY);
    m_btnNoCall->move(callX + (callW + callSpacing) * 3, callY);
    m_btnCall1->raise();
    m_btnCall2->raise();
    m_btnCall3->raise();
    m_btnNoCall->raise();

    // 聊天面板（右侧）
    m_chatPanel->move(w - 275, 80);

    // 混沌横幅（顶部居中）
    if (m_chaosBanner)
    {
        int bannerW = qMin(w * 4 / 5, 500);
        m_chaosBanner->setFixedSize(bannerW, 36);
        m_chaosBanner->move((w - bannerW) / 2, 10);
    }

    if (m_btnScoreboard) m_btnScoreboard->move(220, h - 55);
    // 确保所有交互按钮在最上层（不被卡牌遮挡）
    raiseGameButtons();
}

// ===================================================================
// UI 初始化：创建所有控件并设置样式
// ===================================================================

void GamePanel::initUI()
{
    // ---- 层级容器 ----
    // m_cardLayer: 所有游戏内控件（卡牌、按钮）均挂载在此层，位于背景之上
    m_cardLayer = new QWidget(this);
    m_cardLayer->setObjectName("cardLayer");
    m_cardLayer->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    m_cardLayer->setGeometry(0, 0, width(), height());
    m_cardLayer->setStyleSheet("background:transparent;");

    m_cardLayer->raise();

    // ---- 字体预设 ----
    QFont nf("Microsoft YaHei", 11, QFont::Bold);   // 名字用

    // ============ 标题 ============
    m_titleLabel = new QLabel(QStringLiteral("🃏 斗 地 主"), m_cardLayer);
    m_titleLabel->setFont(QFont("Microsoft YaHei", 36, QFont::Bold));
    m_titleLabel->setStyleSheet("color:#FFD700;background:transparent;");
    m_titleLabel->setAlignment(Qt::AlignCenter);

    // 标题左侧 logo：优先 images/logo/title.gif（动图，QMovie 播放），否则 title.png（静态）。
    // 两者都没有则保持纯文字标题。固定 48×48 方形显示区。
    m_titleLogo = new QLabel(m_cardLayer);
    m_titleLogo->setStyleSheet("background:transparent;");
    m_titleLogo->setAlignment(Qt::AlignCenter);
    m_titleLogo->setScaledContents(true);   // 让动图/静态图自适应到 48×48
    m_titleLogo->hide();
    {
        const QString gifPath = resolveLogo(QStringLiteral("title.gif"));
        const QString pngPath = resolveLogo(QStringLiteral("title.png"));
        if (!gifPath.isEmpty()) {
            m_titleMovie = new QMovie(gifPath, QByteArray(), this);
            if (m_titleMovie->isValid()) {
                m_titleMovie->setScaledSize(QSize(48, 48));
                m_titleLogo->setFixedSize(48, 48);
                m_titleLogo->setMovie(m_titleMovie);
                m_titleMovie->start();
                m_hasTitleLogo = true;
            } else {
                delete m_titleMovie;
                m_titleMovie = nullptr;
            }
        }
        if (!m_hasTitleLogo && !pngPath.isEmpty()) {
            const QPixmap lp(pngPath);
            if (!lp.isNull()) {
                m_titleLogo->setPixmap(lp.scaled(48, 48, Qt::KeepAspectRatio,
                                                 Qt::SmoothTransformation));
                m_titleLogo->setFixedSize(48, 48);
                m_hasTitleLogo = true;
            }
        }
        // 有 logo 时去掉标题的 🃏 前缀，避免与 logo 图标视觉重复。
        if (m_hasTitleLogo)
            m_titleLabel->setText(QStringLiteral("斗 地 主"));
    }

    // ============ 左侧机器人信息面板 ============
    m_leftAvatar = new QLabel(QStringLiteral("🤖"), this);
    m_leftAvatar->setFixedSize(52, 52);
    m_leftAvatar->setStyleSheet(
        "background:#555;border-radius:26px;border:2px solid gold;color:white;font-size:22px;");
    m_leftAvatar->setAlignment(Qt::AlignCenter);

    m_leftNameLabel = new QLabel(QStringLiteral("机器人A"), this);
    m_leftNameLabel->setFont(nf);
    m_leftNameLabel->setStyleSheet("color:white;background:transparent;");

    m_leftLordIcon = new QLabel(this);
    m_leftLordIcon->setFixedSize(70, 28);
    m_leftLordIcon->setStyleSheet(
        "color:#FFD700;font-size:13px;font-weight:bold;background:rgba(255,0,0,180);border-radius:6px;");
    m_leftLordIcon->setAlignment(Qt::AlignCenter);
    m_leftLordIcon->hide();

    // ============ 右侧机器人信息面板 ============
    m_rightAvatar = new QLabel(QStringLiteral("🤖"), this);
    m_rightAvatar->setFixedSize(52, 52);
    m_rightAvatar->setStyleSheet(
        "background:#555;border-radius:26px;border:2px solid gold;color:white;font-size:22px;");
    m_rightAvatar->setAlignment(Qt::AlignCenter);

    m_rightNameLabel = new QLabel(QStringLiteral("机器人B"), this);
    m_rightNameLabel->setFont(nf);
    m_rightNameLabel->setStyleSheet("color:white;background:transparent;");

    m_rightLordIcon = new QLabel(this);
    m_rightLordIcon->setFixedSize(70, 28);
    m_rightLordIcon->setStyleSheet(
        "color:#FFD700;font-size:13px;font-weight:bold;background:rgba(255,0,0,180);border-radius:6px;");
    m_rightLordIcon->setAlignment(Qt::AlignCenter);
    m_rightLordIcon->hide();

    // ============ 用户信息面板 ============
    m_userAvatar = new QLabel(QStringLiteral("😎"), this);
    m_userAvatar->setFixedSize(52, 52);
    m_userAvatar->setStyleSheet(
        "background:#444;border-radius:26px;border:2px solid cyan;color:white;font-size:22px;");
    m_userAvatar->setAlignment(Qt::AlignCenter);

    m_userNameLabel = new QLabel(QStringLiteral("玩家"), this);
    m_userNameLabel->setFont(nf);
    m_userNameLabel->setStyleSheet("color:white;background:transparent;");

    m_userLordIcon = new QLabel(this);
    m_userLordIcon->setFixedSize(70, 28);
    m_userLordIcon->setStyleSheet(
        "color:#FFD700;font-size:13px;font-weight:bold;background:rgba(255,0,0,180);border-radius:6px;");
    m_userLordIcon->setAlignment(Qt::AlignCenter);
    m_userLordIcon->hide();

    // ============ 状态标签 ============
    m_statusLabel = new QLabel(m_cardLayer);
    m_statusLabel->setFont(QFont("Microsoft YaHei", 16, QFont::Bold));
    m_statusLabel->setStyleSheet("color:yellow;background:transparent;");
    m_statusLabel->setAlignment(Qt::AlignCenter);

    // ============ 倒计时 ============
    m_timerLabel = new QLabel(m_cardLayer);
    m_timerLabel->setFont(QFont("Arial", 26, QFont::Bold));
    m_timerLabel->setStyleSheet(
        "color:orange;background:rgba(0,0,0,150);border-radius:18px;");
    m_timerLabel->setAlignment(Qt::AlignCenter);
    m_timerLabel->setFixedSize(54, 54);
    m_timerLabel->hide();


    // ============ 按钮工厂 Lambda ============
    // 统一创建按钮：设置样式、最小高度、阴影，添加到卡片层
    auto btn = [this](const QString &t, const QString &s, int /*w*/, int h)
    {
        auto *b = new QPushButton(t, m_cardLayer);
        b->setStyleSheet(s);
        b->setMinimumHeight(h);
        b->setMaximumHeight(h);
        b->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        QFont f = b->font();
        f.setBold(true);
        f.setPointSize(qMax(10, h / 3));
        b->setFont(f);

        // 添加投影效果
        QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(b);
        shadow->setBlurRadius(14);
        shadow->setOffset(0, 4);
        shadow->setColor(QColor(0, 0, 0, 120));
        b->setGraphicsEffect(shadow);

        return b;
    };

    // ---- 创建所有按钮 ----
    m_btnVsAI       = btn(QStringLiteral("🖥  人机对战"),  S_MENU_BTN, 220, 56);
    m_btnCreateRoom = btn(QStringLiteral("🏠  创建房间"),  S_MENU_BTN, 220, 56);
    m_btnJoinRoom   = btn(QStringLiteral("🔗  加入房间"),  S_MENU_BTN, 220, 56);
    m_btnStart      = btn(QStringLiteral("🎮 开始游戏"),   S_START,    130, 42);
    m_btnRestart    = btn(QStringLiteral("🔄 再来一局"),   S_START,    130, 42);
    m_btnBackMenu   = btn(QStringLiteral("← 返回菜单"),   S_BACK,     100, 30);
    m_btnPlay       = btn(QStringLiteral("出牌"),          S_PLAY,      80, 34);
    m_btnPass       = btn(QStringLiteral("不出"),          S_PASS,      80, 34);
    m_btnHint       = btn(QStringLiteral("提示"),          S_HINT,      80, 34);
    m_btnCall1      = btn(QStringLiteral("1分"),           S_CALL,      60, 32);
    m_btnCall2      = btn(QStringLiteral("2分"),           S_CALL,      60, 32);
    m_btnCall3      = btn(QStringLiteral("3分"),           S_CALL,      60, 32);
    m_btnNoCall     = btn(QStringLiteral("不叫"),          S_NOCALL,    60, 32);

    // 初始隐藏游戏内按钮
    m_btnRestart->hide();
    m_btnPlay->hide();  m_btnPass->hide();  m_btnHint->hide();
    m_btnCall1->hide(); m_btnCall2->hide(); m_btnCall3->hide(); m_btnNoCall->hide();

    // ============ 信号连接 ============
    // 菜单按钮
    connect(m_btnVsAI,       &QPushButton::clicked, this, &GamePanel::onVsAI);
    connect(m_btnCreateRoom, &QPushButton::clicked, this, &GamePanel::onCreateRoom);
    connect(m_btnJoinRoom,   &QPushButton::clicked, this, &GamePanel::onJoinRoom);
    connect(m_btnStart,      &QPushButton::clicked, this, &GamePanel::onStartGame);
    connect(m_btnRestart,    &QPushButton::clicked, this, &GamePanel::onRestartGame);
    connect(m_btnBackMenu,   &QPushButton::clicked, this, &GamePanel::onBackToMenu);

    // 游戏内操作按钮
    connect(m_btnPlay,  &QPushButton::clicked, this, &GamePanel::onBtnPlay);
    connect(m_btnPass,  &QPushButton::clicked, this, &GamePanel::onBtnPass);
    connect(m_btnHint,  &QPushButton::clicked, this, &GamePanel::onBtnHint);

    // 叫分按钮（用 Lambda 传递不同的 bet 值）
    connect(m_btnCall1, &QPushButton::clicked, this, [this] { onBtnCallLord(1); });
    connect(m_btnCall2, &QPushButton::clicked, this, [this] { onBtnCallLord(2); });
    connect(m_btnCall3, &QPushButton::clicked, this, [this] { onBtnCallLord(3); });
    connect(m_btnNoCall, &QPushButton::clicked, this, [this] { onBtnCallLord(0); });

    // 聊天面板
    m_chatPanel = new ChatPanel(this);
    m_chatPanel->hide();
    connect(m_chatPanel, &ChatPanel::sendMessage, this, &GamePanel::onChatSend);

    // ============ 新增：主题选择 UI ============
    m_themeCombo = new QComboBox(m_cardLayer);
    m_themeCombo->setStyleSheet(
        "QComboBox{background:rgba(0,0,0,160);color:#FFD700;border:2px solid #FFD700;"
        "border-radius:8px;font-size:14px;padding:6px 12px;}"
        "QComboBox:hover{background:rgba(255,215,0,30);}"
        "QComboBox QAbstractItemView{background:#222;color:white;selection-background-color:#FFD700;}");
    m_themeCombo->setMinimumHeight(36);
    m_themeCombo->hide();

    m_btnConfirmTheme = new QPushButton(QStringLiteral("✅ 确认"), m_cardLayer);
    m_btnConfirmTheme->setObjectName(QStringLiteral("themeConfirmButton"));
    m_btnConfirmTheme->setStyleSheet(S_MENU_BTN);
    m_btnConfirmTheme->setMinimumHeight(36);
    m_btnConfirmTheme->hide();
    connect(m_btnConfirmTheme, &QPushButton::clicked, this, [this]() {
        m_themeCombo->hide();
        m_btnConfirmTheme->hide();
        m_btnBackMenu->hide();
        onStartGame();
    });

    // 耄耋模式按钮
    m_btnChaosMode = btn(QStringLiteral("🌀 耄耋模式"), S_MENU_BTN, 220, 56);
    m_btnChaosMode->hide();
    connect(m_btnChaosMode, &QPushButton::clicked, this, &GamePanel::onChaosMode);

    // 头像选择按钮
    m_btnAvatar = btn(QStringLiteral("🖼️ 选择头像"), S_MENU_BTN, 220, 56);
    m_btnAvatar->hide();
    connect(m_btnAvatar, &QPushButton::clicked, this, &GamePanel::onAvatarSelect);

    // ---- 菜单按钮 logo 图标（images/logo/<name>.{png,jpg,jpeg,gif} 存在则用图标替换
    //      emoji 前缀，文字保留纯中文；缺图则保持原有 emoji 文字，零回归。
    //      按扩展名依次尝试，用户丢任意常见格式即可）----
    auto applyBtnLogo = [](QPushButton *b, const QString &name, const QString &cleanText,
                           int iconPx) {
        QString p;
        for (const char *ext : {".png", ".jpg", ".jpeg", ".gif"}) {
            p = resolveLogo(name + QLatin1String(ext));
            if (!p.isEmpty())
                break;
        }
        if (p.isEmpty())
            return;
        const QIcon ic(p);
        if (ic.isNull())
            return;
        b->setIcon(ic);
        b->setIconSize(QSize(iconPx, iconPx));
        b->setText(cleanText);
    };
    applyBtnLogo(m_btnVsAI,       QStringLiteral("vsai"),   QStringLiteral("人机对战"), 28);
    applyBtnLogo(m_btnCreateRoom, QStringLiteral("create"), QStringLiteral("创建房间"), 28);
    applyBtnLogo(m_btnJoinRoom,   QStringLiteral("join"),   QStringLiteral("加入房间"), 28);
    applyBtnLogo(m_btnChaosMode,  QStringLiteral("chaos"),  QStringLiteral("耄耋模式"), 28);
    applyBtnLogo(m_btnAvatar,     QStringLiteral("avatar"), QStringLiteral("选择头像"), 28);
    applyBtnLogo(m_btnStart,      QStringLiteral("start"),  QStringLiteral("开始游戏"), 24);

    // 混沌横幅
    m_chaosBanner = new QLabel(QStringLiteral("🌀 混沌领域 🌀"), m_cardLayer);
    m_chaosBanner->setFont(QFont("Microsoft YaHei", 20, QFont::Bold));
    m_chaosBanner->setStyleSheet(
        "color:#00FF00;background:rgba(0,0,0,160);border-radius:8px;padding:4px 16px;");
    m_chaosBanner->setAlignment(Qt::AlignCenter);
    m_chaosBanner->hide();

    // 确保最终层级：聊天面板在上层
    if (m_cardLayer) m_cardLayer->raise();
    m_chatPanel->raise();

    m_tableView = new CardTableView(m_cardLayer);
    connect(m_tableView, &CardTableView::cardClicked, this, &GamePanel::onCardSelected);
    connect(m_tableView, &CardTableView::layersChanged, this, &GamePanel::raiseGameButtons);
    m_btnScoreboard = btn(QStringLiteral("计分 / 记录"), S_BACK, 120, 34);
    m_btnScoreboard->setObjectName(QStringLiteral("scoreboardButton"));
    connect(m_btnScoreboard, &QPushButton::clicked, this, &GamePanel::onShowScoreboard);
    m_btnScoreboard->hide();

    // ============ 房间号显示 ============
    m_roomIdLabel = new QLabel(this);
    m_roomIdLabel->setFont(QFont("Microsoft YaHei", 28, QFont::Bold));
    m_roomIdLabel->setStyleSheet(
        "color:#FFD700;background:rgba(0,0,0,180);border:2px solid gold;"
        "border-radius:12px;padding:10px 24px;");
    m_roomIdLabel->setAlignment(Qt::AlignCenter);
    m_roomIdLabel->hide();

    // ============ 房间内玩家列表 ============
    m_playerListLabel = new QLabel(this);
    m_playerListLabel->setFont(QFont("Microsoft YaHei", 11));
    m_playerListLabel->setStyleSheet(
        "color:white;background:rgba(0,0,0,140);border-radius:6px;padding:6px 12px;");
    m_playerListLabel->setAlignment(Qt::AlignCenter);
    m_playerListLabel->hide();
}

void GamePanel::initConnections()
{
    // 连接 GameControl 的核心信号到 GamePanel 的槽函数
    auto *gc = m_gameControl;
    connect(gc, &GameControlAdapter::gameStatusChanged,   this, &GamePanel::onGameStatusChanged);
    connect(gc, &GameControlAdapter::playerStatusChanged,  this, &GamePanel::onPlayerStatusChanged);
    connect(gc, &GameControlAdapter::notifyGrabLordBet,    this, &GamePanel::onNotifyCallLord);
    connect(gc, &GameControlAdapter::notifyPlayHand,       this, &GamePanel::onNotifyPlayHand);
    // 一轮结束（两家连续过牌）时适配器发 pendingInfo(nullptr, {})。此前该信号未接，
    // 桌面一直停留在上一手牌，直到下一手覆盖——玩家会误以为「小牌压了大牌」，实则
    // 是新一轮领出。收到清空信号时清桌，让新一轮领出在视觉上与压牌区分开。
    connect(gc, &GameControlAdapter::pendingInfo, this,
            [this](Player *player, const Cards &cards) {
                if (player == nullptr && cards.isEmpty())
                    clearPlayArea();
            });

    // 混沌消牌（发牌/发呆）：权威状态已更新，这里刷新手牌显示并驱动横幅/贴图。
    connect(gc, &GameControlAdapter::chaosCardDisappeared, this,
            [this](Player *player, const Card &card) {
                Q_UNUSED(player);
                updateAllPlayerCards();
                if (m_chaosEngine->isActive())
                    emit m_chaosEngine->cardDisappeared(card);
            });
    // 混沌出牌变形（AI 座位）：横幅提示（玩家变形在 onBtnPlay 本地已提示）。
    connect(gc, &GameControlAdapter::chaosCardsTransformed, this,
            [this](Player *player, const Card &from, const Card &to) {
                Q_UNUSED(player); Q_UNUSED(from); Q_UNUSED(to);
                if (m_chaosEngine->isActive())
                    emit m_chaosEngine->chaosTriggered(
                        QStringLiteral("耄耋规则：出牌前发生变形"));
            });

    connect(gc, &GameControlAdapter::gameEventObserved, m_records, &GameRecordModel::observe);
    connect(m_online, &OnlineRoomController::eventApplied, m_records, &GameRecordModel::observe);
    connect(m_actions, &GameActionController::secondsChanged, this, &GamePanel::onCountdownChanged);
    connect(m_actions, &GameActionController::idleWarning, this, &GamePanel::onIdleWarning);
    connect(m_actions, &GameActionController::timedOut, this, &GamePanel::showActionResult);
    connect(m_online, &OnlineRoomController::connected, this, &GamePanel::onRelayConnected);
    connect(m_online, &OnlineRoomController::disconnected, this, &GamePanel::onRelayDisconnected);
    connect(m_online, &OnlineRoomController::roomCreated, this, &GamePanel::onRoomCreated);
    connect(m_online, &OnlineRoomController::roomJoined, this, &GamePanel::onRoomJoined);
    connect(m_online, &OnlineRoomController::roomJoinFailed, this, &GamePanel::onRoomJoinFailed);
    connect(m_online, &OnlineRoomController::playerJoined, this, &GamePanel::onRemotePlayerJoined);
    connect(m_online, &OnlineRoomController::playerLeft, this, &GamePanel::onRemotePlayerLeft);
    connect(m_online, &OnlineRoomController::roomDissolved, this, &GamePanel::onRoomDissolved);
    connect(m_online, &OnlineRoomController::gameStarted, this, &GamePanel::onRelayGameStarted);
    connect(m_online, &OnlineRoomController::errorOccurred, this, &GamePanel::onRelayError);
    connect(m_online, &OnlineRoomController::eventApplied, this, &GamePanel::onRemoteGameEvent);
    connect(m_online, &OnlineRoomController::commandRejected, this, &GamePanel::onRemoteCommandRejected);
    connect(m_online, &OnlineRoomController::chatReceived, this, &GamePanel::onRemoteChat);
    connect(m_online, &OnlineRoomController::roomStateChanged, this, &GamePanel::updateRoomStartVisibility);

    // 聊天贴图
    connect(m_chatPanel, &ChatPanel::sendSticker, this, [this](int stickerId) {
        QPixmap pix = m_stickerStore->get(stickerId);
        if (!pix.isNull())
            m_chatPanel->addStickerMessage(
                m_gameControl->getUserPlayer()->getName(), pix);
        // 联机：贴图搭聊天通道广播，发一条 [[STK:N]] 标记文本，对端解析后渲染成
        // 同一张表情（两端都固定加载 maodie 表情集，序号一致）。不需改动云端 relay。
        if (m_online->isOnline() && m_online && m_online->isConnected())
            m_online->sendChat(QStringLiteral("[[STK:%1]]").arg(stickerId));
    });
}

void GamePanel::initGame()
{
    m_gameControl = new GameControlAdapter(this);
    m_gameControl->playerInit();

    m_musicPlayer = new MusicPlayer(this);
    m_musicPlayer->setVolume(50);
    m_musicPlayer->setSFXVolume(70);
    m_musicPlayer->setGender(true);

    m_online = new OnlineRoomController(this);
    m_actions = new GameActionController(m_gameControl, m_online, this);
    m_records = new GameRecordModel(this);
    m_scoreboard = new ScoreboardDialog(m_records, this);

    // ---- 新增：主题/混沌/头像系统 ----
    m_themeManager = new ThemeManager(this);
    m_chaosEngine = new ChaosEngine(this);
    m_stickerStore = new StickerStore(this);
    m_avatarStore = new AvatarStore(this);

    // 加载头像
    QString avatarDir = QCoreApplication::applicationDirPath() + "/../avatars/";
    if (!m_avatarStore->load(avatarDir))
    {
        avatarDir = QCoreApplication::applicationDirPath() + "/avatars/";
        m_avatarStore->load(avatarDir);
    }
    m_playerAvatarId = m_avatarStore->list().isEmpty() ? "" : m_avatarStore->list().first().id;

    // 混沌信号连接
    connect(m_chaosEngine, &ChaosEngine::chaosTriggered, this,
            [this](const QString &msg) {
        m_statusLabel->setText(msg);
        if (m_chatPanel)
            m_chatPanel->addSystemMessage(msg);
    });
    connect(m_chaosEngine, &ChaosEngine::popupSticker, this, &GamePanel::onChaosStickerPopup);
    connect(m_chaosEngine, &ChaosEngine::shakeScreen, this, &GamePanel::onChaosShake);

    // 加载牌背面
    m_tableView->setCardBack(CardPanel::loadCardBack());
}

Player *GamePanel::findPlayerPtr(Player *p)
{
    // 匹配 gameControl 中的实际玩家指针
    if (p == m_gameControl->getUserPlayer())
        return m_gameControl->getUserPlayer();
    if (p == m_gameControl->getLeftRobot())
        return m_gameControl->getLeftRobot();
    if (p == m_gameControl->getRightRobot())
        return m_gameControl->getRightRobot();
    return nullptr;
}


void GamePanel::onChaosStickerPopup(const QPixmap &sticker, QPoint pos, int durationMs)
{
    static int offset = 0;
    auto *pw = new PopupWidget(sticker, m_cardLayer, durationMs);
    QPoint center(width() / 2, height() / 2);
    pw->showAt(center + pos, offset);
    offset = (offset + 1) % 5;
}

void GamePanel::onChaosShake(int intensity)
{
    QPoint orig = pos();
    int amplitude = intensity * 3;
    auto *timer = new QTimer(this);
    int *count = new int(0);
    connect(timer, &QTimer::timeout, this, [this, timer, count, orig, amplitude]() {
        (*count)++;
        if (*count > 6)
        {
            timer->stop();
            move(orig);
            timer->deleteLater();
            delete count;
            return;
        }
        int dx = ((*count) % 2 == 0) ? amplitude : -amplitude;
        int dy = ((*count + 1) % 2 == 0) ? amplitude / 2 : -amplitude / 2;
        move(orig.x() + dx, orig.y() + dy);
    });
    timer->start(40);
}
