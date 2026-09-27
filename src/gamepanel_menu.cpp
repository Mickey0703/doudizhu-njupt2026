// Menu, theme and avatar presentation. Independent card/room components own their own lifecycle.

#include "gamepanel.h"
#include "avatar_picker.h"

#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QDir>
#include <QRandomGenerator>
#include <QPainter>
#include <QPen>
#include <QFontMetrics>
#include <QMovie>
#include <QCoreApplication>

// ===================================================================
// 模式切换
// ===================================================================

void GamePanel::enterMenuMode()
{
    // 显示标题和菜单按钮，隐藏游戏元素
    m_titleLabel->show();
    if (m_hasTitleLogo) {
        m_titleLogo->show();
        if (m_titleMovie) {   // 回到菜单时恢复动图（首次 NotRunning→start，之后 Paused→resume）
            if (m_titleMovie->state() == QMovie::Paused)
                m_titleMovie->setPaused(false);
            else
                m_titleMovie->start();
        }
    }
    setMenuButtonsVisible(true);
    m_btnChaosMode->show();
    m_btnAvatar->show();
    m_btnStart->hide();
    m_btnRestart->hide();
    m_btnBackMenu->hide();
    setPlayButtonsVisible(false);
    setCallButtonsVisible(false);
    m_chatPanel->hide();
    m_timerLabel->hide();
    m_btnScoreboard->hide();
    m_statusLabel->hide();

    // 清空牌区
    clearPlayArea();
    clearLordCards();

    // Hide the renderer without exposing its owned widgets.
    m_tableView->hideHands();
    m_userLordIcon->hide();
    m_leftLordIcon->hide();
    m_rightLordIcon->hide();
    m_themeCombo->hide();
    m_btnConfirmTheme->hide();
    m_chaosBanner->hide();

    // 隐藏头像和玩家信息面板（仅游戏中显示）
    m_leftAvatar->hide();
    m_leftNameLabel->hide();
    m_rightAvatar->hide();
    m_rightNameLabel->hide();
    m_userAvatar->hide();
    m_userNameLabel->hide();

    m_musicPlayer->playBGM(MusicPlayer::BGM_MainMenu);
    arrangeMenuButtons();
}

void GamePanel::enterGameMode()
{
    // 进入游戏模式：隐藏标题菜单，显示开始按钮和状态
    m_titleLabel->hide();
    if (m_titleLogo) m_titleLogo->hide();
    if (m_titleMovie) m_titleMovie->setPaused(true);   // 游戏中暂停动图省 CPU
    setMenuButtonsVisible(false);
    m_btnChaosMode->hide();
    m_btnAvatar->hide();
    if (!m_online->isOnline() || m_online->isHost())
        m_btnStart->show();
    else
        m_btnStart->hide();
    m_btnBackMenu->show();
    m_statusLabel->show();
    m_btnScoreboard->show();
    m_statusLabel->setText(QStringLiteral("准备开始"));

    // 显示头像和玩家信息面板（进入游戏后才可见）
    m_leftAvatar->show();
    m_leftNameLabel->show();
    m_rightAvatar->show();
    m_rightNameLabel->show();
    m_userAvatar->show();
    m_userNameLabel->show();
}

void GamePanel::arrangeMenuButtons()
{
    // 五个主菜单按钮垂直居中排列（标题在上，按钮在下）
    int cx = width() / 2, cy = height() / 2;

    int bw = qBound(180, width() * 28 / 100, 420);     // 按钮宽度
    int bh = qBound(42, height() * 6 / 100, 60);        // 按钮高度（5按钮时缩紧）
    int gap = qMax(10, bh / 4);                          // 间距

    // 标题区域
    int titleW = qBound(240, width() * 40 / 100, 560);
    int titleY = cy - bh - gap * 3 - 30;
    int titleH = bh + 30;
    m_titleLabel->setGeometry(cx - titleW / 2, titleY, titleW, titleH);

    // 标题左侧 logo：把 [logo][间隙][文字] 作为一组水平居中。文字在 label 内居中，
    // 实际文字起点 = 标题中心 - 文字宽/2；logo 紧贴其左侧并与标题垂直居中。
    if (m_hasTitleLogo)
    {
        const int logoSz = 48, gapLT = 10;
        const int textW = QFontMetrics(m_titleLabel->font())
                              .horizontalAdvance(m_titleLabel->text());
        // 整组宽度 = logo + 间隙 + 文字，使其在 cx 居中
        const int groupLeft = cx - (logoSz + gapLT + textW) / 2;
        const int logoX = groupLeft;
        const int logoY = titleY + (titleH - logoSz) / 2;
        m_titleLogo->setGeometry(logoX, logoY, logoSz, logoSz);
        // 文字左对齐到 logo 右侧，避免 label 居中把文字推回中间与 logo 脱开
        m_titleLabel->setGeometry(groupLeft + logoSz + gapLT, titleY, textW + 8, titleH);
        m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }

    // 三个按钮垂直排列
    m_btnVsAI->setFixedSize(bw, bh);
    m_btnCreateRoom->setFixedSize(bw, bh);
    m_btnJoinRoom->setFixedSize(bw, bh);

    m_btnVsAI->move(cx - bw / 2, cy - bh / 2 - gap);
    m_btnCreateRoom->move(cx - bw / 2, cy + bh / 2 + gap);
    m_btnJoinRoom->move(cx - bw / 2, cy + bh / 2 + gap * 2 + bh);

    // 新增：耄耋和头像按钮（在主菜单按钮下方）
    int gap2 = gap + bh;
    m_btnChaosMode->setFixedSize(bw, bh);
    m_btnAvatar->setFixedSize(bw, bh);
    m_btnChaosMode->move(cx - bw / 2, cy + bh / 2 + gap2 * 2 + bh * 2);
    m_btnAvatar->move(cx - bw / 2, cy + bh / 2 + gap2 * 3 + bh * 3);
}

// ===================================================================
// 主菜单操作
// ===================================================================

void GamePanel::onVsAI()
{
    enterGameMode();
    m_btnStart->hide();   // 立刻隐藏开始按钮，避免与主题确认按钮冲突
    m_online->reset();
    m_records->beginSession({m_gameControl->state().players[0].score, m_gameControl->state().players[1].score, m_gameControl->state().players[2].score});
    configureOnlinePlayers();
    m_chatPanel->hide();
    m_leftNameLabel->setText(QStringLiteral("机器人A"));
    m_rightNameLabel->setText(QStringLiteral("机器人B"));
    m_userNameLabel->setText(QStringLiteral("玩家"));

    m_chaosEngine->setActive(false);
    m_chaosBanner->hide();

    // 加载可用主题列表
    QString themesDir = QCoreApplication::applicationDirPath() + "/../themes/";
    m_themeNames = ThemeManager::availableThemes(themesDir);
    if (m_themeNames.isEmpty())
    {
        themesDir = QCoreApplication::applicationDirPath() + "/themes/";
        m_themeNames = ThemeManager::availableThemes(themesDir);
    }
    m_themeNames.removeAll("maodie");

    if (m_themeNames.isEmpty())
    {
        onStartGame();
        return;
    }

    // 显示主题下拉框
    m_themeCombo->clear();
    for (const auto &t : m_themeNames)
    {
        ThemeManager tmp;
        QString tp = themesDir + "/" + t + "/";
        if (tmp.loadTheme(tp))
            m_themeCombo->addItem(tmp.config().displayName, t);
        else
            m_themeCombo->addItem(t, t);
    }
    m_themeCombo->show();
    m_themeCombo->raise();
    m_themeCombo->move(width() / 2 - 110, height() / 2 - 60);

    m_btnConfirmTheme->show();
    m_btnConfirmTheme->raise();
    m_btnConfirmTheme->move(width() / 2 - 110, height() / 2);

    m_btnStart->hide();
    m_statusLabel->setText(QStringLiteral("选择主题"));

    // 显示返回按钮，允许取消主题选择
    m_btnBackMenu->show();
    m_btnBackMenu->raise();
}

void GamePanel::resetThemeForOnline()
{
    // 主题与混沌都是单机本地装饰，各端不通过网络同步。进入联机房间时彻底卸载，
    // 避免上一局单机残留的主题牌背/牌面/BGM/音效或混沌效果漏进联机、导致各端不一致。
    m_themeManager->clear();
    m_chaosEngine->setActive(false);
    m_chaosBanner->hide();
    if (m_gameControl)
        m_gameControl->setChaosEngine(nullptr);
    m_themeNames.clear();
    m_themeCombo->hide();
    m_btnConfirmTheme->hide();

    // 彻底清除上一局（尤其单机）残留的卡牌控件与本地手牌数据，否则 resize 时
    // 这些遗留的正面牌控件会被重新摆放/显示，在联机等待室里露出"彩色细线"般的残影。
    m_tableView->clearHands();
    clearPlayArea();
    clearLordCards();
    if (m_gameControl)
    {
        m_gameControl->getUserPlayer()->clearCards();
        m_gameControl->getLeftRobot()->clearCards();
        m_gameControl->getRightRobot()->clearCards();
    }

    // 联机聊天表情：主题不联网同步，但表情包要各端一致。固定加载 maodie 的表情集
    // 作为共享表情源（每个客户端都自带），这样选择器可用、且两端表情序号一致，
    // 收到 [[STK:N]] 标记时能渲染出同一张图。loadFromDisk 用临时 ThemeManager，
    // 不影响已清空的主题（牌背/牌面仍走默认）。
    QString stickerThemeDir = QCoreApplication::applicationDirPath() + "/../themes/maodie/";
    if (!QDir(stickerThemeDir).exists())
        stickerThemeDir = QCoreApplication::applicationDirPath() + "/themes/maodie/";
    m_stickerStore->loadFromDisk(stickerThemeDir);
    m_chatPanel->setStickerStore(m_stickerStore);
}

void GamePanel::onCreateRoom()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("创建房间"),
        QStringLiteral("输入你的昵称:"), QLineEdit::Normal, QStringLiteral("玩家"), &ok);
    if (!ok || name.isEmpty()) return;
    m_gameControl->disable();
    m_userNameLabel->setText(name);
    m_gameControl->getUserPlayer()->setName(name);
    m_online->beginHost(name, m_playerAvatarId);
    m_records->beginSession();
    configureOnlinePlayers();
    enterGameMode();
    resetThemeForOnline();
    m_chatPanel->show();
    m_leftNameLabel->setText(QStringLiteral("等待中..."));
    m_rightNameLabel->setText(QStringLiteral("等待中..."));
    updateRoomStartVisibility();
    m_statusLabel->setText(QStringLiteral("连接中..."));
}

void GamePanel::onJoinRoom()
{
    bool ok = false;
    const int room = QInputDialog::getInt(this, QStringLiteral("加入房间"),
        QStringLiteral("输入6位房间号:"), 100000, 100000, 999999, 1, &ok);
    if (!ok) return;
    const QString name = QInputDialog::getText(this, QStringLiteral("昵称"),
        QStringLiteral("输入你的昵称:"), QLineEdit::Normal, QStringLiteral("玩家"), &ok);
    if (!ok || name.isEmpty()) return;
    m_gameControl->disable();
    m_userNameLabel->setText(name);
    m_gameControl->getUserPlayer()->setName(name);
    m_online->beginJoin(static_cast<quint32>(room), name, m_playerAvatarId);
    m_records->beginSession();
    configureOnlinePlayers();
    enterGameMode();
    resetThemeForOnline();
    m_chatPanel->show();
    updateRoomStartVisibility();
    m_statusLabel->setText(QStringLiteral("连接中..."));
}

void GamePanel::onBackToMenu()
{
    resetOnlineState();
    m_gameControl->disable();
    m_chaosEngine->setActive(false);
    m_gameControl->setChaosEngine(nullptr);
    m_scoreboard->hide();
    enterMenuMode();
}

void GamePanel::onChaosMode()
{
    enterGameMode();
    m_online->reset();
    m_records->beginSession({m_gameControl->state().players[0].score, m_gameControl->state().players[1].score, m_gameControl->state().players[2].score});
    configureOnlinePlayers();
    m_chatPanel->hide();

    // 加载耄耋主题
    QString themesDir = QCoreApplication::applicationDirPath() + "/../themes/";
    QString maodieDir = themesDir + "/maodie/";
    if (!QDir(maodieDir).exists())
        maodieDir = QCoreApplication::applicationDirPath() + "/themes/maodie/";

    if (m_themeManager->loadTheme(maodieDir))
    {
        const ChaosConfig chaos = m_themeManager->chaosConfig();
        m_chaosEngine->setActive(true);
        m_chaosEngine->setStickers(m_themeManager->stickers());
        m_gameControl->setChaosEngine(m_chaosEngine);
        // 下发改牌参数给引擎：决策(掷骰)+执行都在会话/引擎，权威 GameState 唯一真相；
        // ChaosEngine 仅负责表现(贴图/震屏/横幅)。玩家与 AI 共用同一份变形/消牌逻辑。
        m_gameControl->setChaosParams(
            true, chaos.disappearChance, chaos.transformChance,
            chaos.disappearTiming.contains(QStringLiteral("onDraw")),
            chaos.transformTiming.contains(QStringLiteral("onPlay")),
            chaos.disappearTiming.contains(QStringLiteral("onIdle")));

        m_stickerStore->loadFromTheme(m_themeManager);
        m_chatPanel->setStickerStore(m_stickerStore);

        ThemeConfig cfg = m_themeManager->config();
        m_leftNameLabel->setText(cfg.robotNames.value(0, "耄耋左护法"));
        m_rightNameLabel->setText(cfg.robotNames.value(1, "耄耋右护法"));
        m_chaosBanner->show();

        // 随机机器人头像
        m_leftBotAvatarId = m_avatarStore->list().isEmpty() ? ""
            : m_avatarStore->list()[QRandomGenerator::global()->bounded(m_avatarStore->list().size())].id;
        m_rightBotAvatarId = m_avatarStore->list().isEmpty() ? ""
            : m_avatarStore->list()[QRandomGenerator::global()->bounded(m_avatarStore->list().size())].id;

        // 弹窗介绍规则。概率直接读实际配置(chaos.json)，避免文案与配置脱钩；
        // 并说明混沌对所有座位(含 AI)生效、且仅单机可用。
        const int pDraw = qRound(chaos.disappearChance * 100);
        const int pPlay = qRound(chaos.transformChance * 100);
        QMessageBox box(this);
        box.setWindowTitle(QStringLiteral("🌀 耄耋混沌模式"));
        box.setText(QStringLiteral(
            "<h3>🌀 欢迎来到混沌领域</h3>"
            "<p><b>牌会消失</b> — 发牌时约 %1% 概率一张牌溜走</p>"
            "<p><b>牌会变形</b> — 出牌时约 %2% 概率一张牌变成手牌中另一张</p>"
            "<p><b>倒计时危机</b> — 最后5秒可能随机丢失一张手牌</p>"
            "<p><b>特殊牌型弹窗</b> — 打出炸弹/火箭/长连牌时触发抽象表情包</p>"
            "<p><i>混沌对所有座位(含耄耋左/右护法)一视同仁；仅单机模式生效。</i></p>"
            "<br><p>🎰 祝你好运，在混沌中称霸！</p>"
        ).arg(pDraw).arg(pPlay));
        box.setIcon(QMessageBox::Information);
        box.setStandardButtons(QMessageBox::Ok);
        box.exec();
    }

    // 关掉规则窗后停在准备状态，由玩家点“开始游戏”按钮开局（不再自动开局，
    // 否则开始按钮形同虚设）。enterGameMode 在函数开头调用时 m_online->isOnline() 可能还是
    // 上一局的旧值导致按钮没显示，这里在已确定单机后显式保证它可见。
    m_btnStart->show();
    m_btnStart->raise();
    m_statusLabel->setText(QStringLiteral("点击“开始游戏”进入混沌领域"));
}

void GamePanel::renderAvatarTo(QLabel *label, const QString &avatarId, const QColor &ring)
{
    // 把头像图裁成 52×52 圆形 + 彩色描边贴到座位标签。avatarId 为空或加载失败时不改动
    // （保留原 emoji 占位），避免把对手头像清空成空白。
    if (!label || avatarId.isEmpty() || !m_avatarStore || !m_avatarStore->isLoaded())
        return;
    QPixmap avatar = m_avatarStore->getAvatar(avatarId);
    if (avatar.isNull())
        return;

    QPixmap circle(52, 52);
    circle.fill(Qt::transparent);
    QPainter p(&circle);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(avatar.scaled(52, 52, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    p.setPen(QPen(ring, 2));
    p.drawEllipse(0, 0, 52, 52);
    p.end();
    label->setPixmap(circle);
}

void GamePanel::onAvatarSelect()
{
    if (!m_avatarStore->isLoaded()) return;
    auto *picker = new AvatarPicker(m_avatarStore, this);
    connect(picker, &AvatarPicker::avatarSelected, this, [this](const QString &id) {
        m_playerAvatarId = id;
        renderAvatarTo(m_userAvatar, id, QColor(0, 255, 255));
    });
    picker->exec();
    picker->deleteLater();
}
