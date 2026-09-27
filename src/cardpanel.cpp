#include "cardpanel.h"
#include <QPainter>
#include <QMouseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QFile>

// ============ 构造 ============

CardPanel::CardPanel(QWidget *parent)
    : QWidget(parent)
    , m_isfront(true)       // 默认正面朝上
    , m_isselected(false)   // 默认未选中
    , m_owner(nullptr)
{
    // 固定控件尺寸：105×150（原图 420×600 的 25%）
    setFixedSize(105, 150);
}

// ============ 图片管理 ============

void CardPanel::setFrontPixmap(const QPixmap &front)
{
    m_front = front.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    update();
}

void CardPanel::setBackPixmap(const QPixmap &back)
{
    m_back = back.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    update();
}

void CardPanel::setThemeFace(const QPixmap &face)
{
    m_themeFace = face;
    if (!face.isNull())
        m_front = face.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    update();
}

void CardPanel::setThemeBack(const QPixmap &back)
{
    m_themeBack = back;
    if (!back.isNull())
        m_back = back.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    update();
}

QPixmap CardPanel::getFrontPixmap() const
{
    return m_front;
}

// ============ 状态控制 ============

void CardPanel::setFrontSide(bool flag)
{
    m_isfront = flag;
    update();
}

bool CardPanel::isFrontSide() const
{
    return m_isfront;
}

void CardPanel::setSelected(bool flag)
{
    m_isselected = flag;
    update();
}

bool CardPanel::isSelected() const
{
    return m_isselected;
}

// ============ 数据绑定 ============

void CardPanel::setCard(const Card &card)
{
    m_card = card;
}

Card CardPanel::getCard() const
{
    return m_card;
}

void CardPanel::setOwner(Player *player)
{
    m_owner = player;
}

Player *CardPanel::getOwner() const
{
    return m_owner;
}

// ============ 绘制 ============

void CardPanel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform); // 高质量的缩放

    // 根据正反面选择图片
    const QPixmap &img = m_isfront ? m_front : m_back;
    if (!img.isNull())
    {
        p.drawPixmap(rect(), img);
    }
    else
    {
        // fallback：纯色背景 + 边框（当图片加载失败时）
        p.fillRect(rect(), m_isfront ? Qt::white : QColor(25, 60, 140));
        p.setPen(Qt::gray);
        p.drawRect(rect().adjusted(0, 0, -1, -1));
    }

    // 选中高亮：金色边框 + 半透明金色覆盖
    if (m_isselected)
    {
        p.setPen(QPen(QColor(255, 215, 0), 3));
        p.setBrush(QColor(255, 215, 0, 50));
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4);
    }
}

// ============ 鼠标交互 ============

void CardPanel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        emit clicked(); // 发射点击信号
    }
    QWidget::mousePressEvent(event);
}

// ===================================================================
// 静态方法：加载卡牌图片
// ===================================================================
//
// 图片文件命名规则：
//   扑克牌：斗地主&德州扑克_NN.png
//
// 实际图片布局（经逐张核对）——「点数优先，同点数按 ♠♥♣♦」，且文件 _13.png 被跳过：
//   _01=3♠ _02=3♥ _03=3♣ _04=3♦  _05=4♠ … _12=5♦  (_13 缺失)
//   _14=6♠ …（每个点数占 4 张，花色序 ♠♥♣♦）… _53=2♦
//   _54=小王 _55=大王
// 即位置 pos = (点数-3)*4 + 花色序 + 1，花色序 ♠=0/♥=1/♣=2/♦=3；pos≥13 时文件号 +1
// （跳过 _13.png）。旧代码假设「花色优先、连续 13 张一花色、无跳号」，与实际完全不符，
// 导致牌面图与真实牌值错位（如 Q♦ 曾误取 _49=A♦、10♦ 误取 _47=A♥，两张都显示成 A，
// 玩家看到「假对子」，出牌被引擎按真实点数 Q+10 判为无效）。
//
// CardSuit 枚举值：Spade=4, Heart=3, Club=2, Diamond=1 → 精灵花色序 spriteSuit = Spade - suit。

/// 查找卡牌图片所在目录 —— 多路径搜索
static QString cardsDir()
{
    // 候选路径列表（优先级从高到低）
    QStringList candidates = {
        QCoreApplication::applicationDirPath() + "/images/cards/",
        QCoreApplication::applicationDirPath() + "/resources/images/cards/",
        QCoreApplication::applicationDirPath() + "/../images/cards/",
        QCoreApplication::applicationDirPath() + "/../resources/images/cards/",
    };
    for (const auto &path : candidates)
    {
        QDir d(path);
        if (d.exists())
            return d.absolutePath() + "/";
    }
    // 回退到构建目录（即使不存在）
    return QCoreApplication::applicationDirPath() + "/images/cards/";
}

/// 将 Card 映射为精灵图中的索引号（1-54）
static int cardToImageIndex(const Card &card)
{
    int pt = card.getpoint();
    int suit = card.getsuit();

    // 大小王 → _54, _55
    if (pt == Card::Card_SJ)
        return 54;
    if (pt == Card::Card_BJ)
        return 55;

    // 防御性边界检查：花色/点数越界（例如损坏的网络包解码出的非法枚举）返回 0
    // （渲染为占位牌），绝不越界。
    if (suit < Card::Diamond || suit > Card::Spade
        || pt < Card::Card_3 || pt > Card::Card_2)
        return 0;

    // 点数优先、同点数按 ♠♥♣♦ 排列。spriteSuit：♠=0 ♥=1 ♣=2 ♦=3。
    const int spriteSuit = Card::Spade - suit;      // Spade(4)→0, Heart(3)→1, Club(2)→2, Diamond(1)→3
    const int rankIndex  = pt - Card::Card_3;        // Card_3→0 … Card_2→12
    int pos = rankIndex * 4 + spriteSuit + 1;        // 1..52（逻辑序号，未计跳号）

    // 文件序列跳过了 _13.png：逻辑序号 ≥13 的实际文件号要 +1。
    if (pos >= 13)
        pos += 1;
    return pos;
}

QPixmap CardPanel::loadCardPixmap(const Card &card)
{
    int idx = cardToImageIndex(card);

    // 构建文件名：扑克牌：斗地主&德州扑克_01.png ~ _54.png
    QString path = cardsDir() + QString::fromUtf8("扑克牌：斗地主&德州扑克_%1.png")
                                    .arg(idx, 2, 10, QChar('0')); // 补零到2位

    QPixmap pix(path);
    if (pix.isNull())
    {
        // ---- 程序化 fallback 绘制（当图片文件缺失时） ----
        pix = QPixmap(420, 600);
        pix.fill(Qt::white);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor(200, 200, 200), 2));
        p.drawRoundedRect(2, 2, 416, 596, 12, 12);

        int pt = card.getpoint();
        int suit = card.getsuit();

        // 红色：方片、红桃、大小王；黑色：梅花、黑桃
        bool isRed = (suit == Card::Diamond || suit == Card::Heart ||
                      pt == Card::Card_SJ || pt == Card::Card_BJ);
        p.setPen(isRed ? QColor(200, 0, 0) : Qt::black);

        // 点数文字
        QString ptStr;
        switch (pt)
        {
        case Card::Card_3: ptStr = "3"; break;   case Card::Card_4: ptStr = "4"; break;
        case Card::Card_5: ptStr = "5"; break;   case Card::Card_6: ptStr = "6"; break;
        case Card::Card_7: ptStr = "7"; break;   case Card::Card_8: ptStr = "8"; break;
        case Card::Card_9: ptStr = "9"; break;   case Card::Card_10: ptStr = "10"; break;
        case Card::Card_J: ptStr = "J"; break;   case Card::Card_Q: ptStr = "Q"; break;
        case Card::Card_K: ptStr = "K"; break;   case Card::Card_A: ptStr = "A"; break;
        case Card::Card_2: ptStr = "2"; break;
        case Card::Card_SJ: ptStr = "JOKER"; break;
        case Card::Card_BJ: ptStr = "JOKER"; break;
        default: ptStr = "?"; break;
        }

        // 花色符号
        QString suitStr;
        switch (suit)
        {
        case Card::Diamond: suitStr = "♦"; break;
        case Card::Club:    suitStr = "♣"; break;
        case Card::Heart:   suitStr = "♥"; break;
        case Card::Spade:   suitStr = "♠"; break;
        default: suitStr = ""; break;
        }

        // 左上角：点数 + 花色
        QFont big("Arial", 60, QFont::Bold);
        p.setFont(big);
        p.drawText(QRect(20, 20, 160, 100), Qt::AlignLeft, ptStr);
        p.drawText(QRect(20, 100, 160, 100), Qt::AlignLeft, suitStr);

        // 中央大字
        if (pt == Card::Card_SJ)
        {
            p.drawText(QRect(0, 200, 420, 200), Qt::AlignCenter, "🃏\n小王");
        }
        else if (pt == Card::Card_BJ)
        {
            p.drawText(QRect(0, 200, 420, 200), Qt::AlignCenter, "🃏\n大王");
        }
        else
        {
            p.setFont(QFont("Arial", 120));
            p.drawText(QRect(0, 200, 420, 300), Qt::AlignCenter, suitStr);
        }
        p.end();
    }
    return pix;
}

QPixmap CardPanel::loadCardBack()
{
    // 尝试多个路径：文件系统 → qrc 资源 → 程序化绘制
    QPixmap pix(cardsDir() + "card_back.png");
    if (pix.isNull())
    {
        // 尝试 UI 图片目录
        QStringList uiCandidates = {
            QCoreApplication::applicationDirPath() + "/images/ui/card_back.png",
            QCoreApplication::applicationDirPath() + "/resources/images/ui/card_back.png",
            QCoreApplication::applicationDirPath() + "/../images/ui/card_back.png",
            QCoreApplication::applicationDirPath() + "/../resources/images/ui/card_back.png",
        };
        for (const auto &alt : uiCandidates)
        {
            if (QFile::exists(alt))
            {
                pix = QPixmap(alt);
                break;
            }
        }
    }
    if (pix.isNull())
    {
        pix = QPixmap(":/images/ui/card_back.png");
    }
    if (pix.isNull())
    {
        // 程序化绘制牌背面
        pix = QPixmap(420, 600);
        pix.fill(QColor(25, 60, 140)); // 深蓝色底
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor(200, 180, 50), 4)); // 金色双线边框
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(8, 8, 404, 584, 12, 12);
        p.drawRoundedRect(18, 18, 384, 564, 8, 8);
        p.setFont(QFont("Arial", 80));
        p.drawText(QRect(0, 0, 420, 600), Qt::AlignCenter, "🂠"); // 牌背面 Emoji
        p.end();
    }
    return pix;
}
