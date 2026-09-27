#include "chatpanel.h"
#include "sticker_store.h"
#include "sticker_widget.h"
#include <QDateTime>
#include <QScrollBar>
#include <QTimer>
#include <QApplication>

ChatPanel::ChatPanel(QWidget *parent)
    : QWidget(parent)
{
    // 宽高都必须固定：本控件由 GamePanel 用 move() 手动定位、不在任何父布局里，
    // 若只定宽不定高，控件尺寸不可靠会塌缩，只露出标题栏+输入行边缘的"细横条"残影。
    // 高度 = 标题(~20) + 滚动区(≤200) + 输入行(32) + 边距/间距 ≈ 290，取 300。
    setFixedSize(260, 300);
    setStyleSheet("background:rgba(0,0,0,160);border-radius:8px;");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);

    // ---- 标题 ----
    m_titleLabel = new QLabel(QStringLiteral("💬 聊天"), this);
    m_titleLabel->setStyleSheet("color:gold;font-size:13px;font-weight:bold;");
    root->addWidget(m_titleLabel);

    // ---- 消息滚动区 ----
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setMaximumHeight(200);
    m_scrollArea->setStyleSheet(
        "QScrollArea{background:rgba(0,0,0,80);border:1px solid #555;border-radius:4px;}"
        "QScrollBar:vertical{background:#333;width:8px;}"
        "QScrollBar::handle:vertical{background:#666;border-radius:4px;}");

    m_msgContainer = new QWidget;
    m_msgContainer->setStyleSheet("background:transparent;");
    m_msgLayout = new QVBoxLayout(m_msgContainer);
    m_msgLayout->setContentsMargins(4, 4, 4, 4);
    m_msgLayout->setSpacing(4);
    m_msgLayout->addStretch();

    m_scrollArea->setWidget(m_msgContainer);
    root->addWidget(m_scrollArea);

    // ---- 输入行 ----
    auto *inputRow = new QHBoxLayout;
    inputRow->setSpacing(4);

    m_stickerBtn = new QPushButton(QStringLiteral("😀"), this);
    m_stickerBtn->setFixedSize(32, 32);
    m_stickerBtn->setFlat(true);
    m_stickerBtn->setStyleSheet(
        "QPushButton{font-size:18px;border:1px solid #555;border-radius:4px;background:rgba(255,255,255,20);}"
        "QPushButton:hover{background:rgba(255,255,255,40);}");
    connect(m_stickerBtn, &QPushButton::clicked, this, &ChatPanel::onStickerBtnClicked);
    inputRow->addWidget(m_stickerBtn);

    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(QStringLiteral("输入消息..."));
    m_input->setStyleSheet(
        "QLineEdit{background:rgba(255,255,255,200);color:black;"
        "border:1px solid #888;border-radius:4px;padding:4px;font-size:12px;}");
    inputRow->addWidget(m_input);

    m_sendBtn = new QPushButton(QStringLiteral("发送"), this);
    m_sendBtn->setFixedWidth(50);
    m_sendBtn->setStyleSheet(
        "QPushButton{background:#4CAF50;color:white;border-radius:4px;"
        "padding:4px 8px;font-size:12px;font-weight:bold;}"
        "QPushButton:hover{background:#45a049;}");
    connect(m_sendBtn, &QPushButton::clicked, this, &ChatPanel::onSendClicked);
    connect(m_input, &QLineEdit::returnPressed, this, &ChatPanel::onSendClicked);
    inputRow->addWidget(m_sendBtn);

    root->addLayout(inputRow);
}

void ChatPanel::setStickerStore(StickerStore *store) { m_stickerStore = store; }

void ChatPanel::onSendClicked()
{
    QString msg = m_input->text().trimmed();
    if (msg.isEmpty()) return;
    m_input->clear();
    emit sendMessage(msg);
}

void ChatPanel::onStickerBtnClicked()
{
    if (!m_stickerStore || !m_stickerStore->isLoaded()) return;
    if (!m_stickerWidget) {
        m_stickerWidget = new StickerWidget(m_stickerStore, this);
        connect(m_stickerWidget, &StickerWidget::stickerSelected,
                this, &ChatPanel::onStickerSelected);
    } else {
        m_stickerWidget->refresh();
    }
    QPoint pos = m_stickerBtn->mapToGlobal(QPoint(0, -m_stickerWidget->height()));
    m_stickerWidget->move(pos);
    m_stickerWidget->show();
}

void ChatPanel::onStickerSelected(int index) { emit sendSticker(index); }

void ChatPanel::appendMessageWidget(QWidget *w)
{
    QLayoutItem *spring = m_msgLayout->takeAt(m_msgLayout->count() - 1);
    m_msgLayout->addWidget(w);
    m_msgLayout->addItem(spring);
    QTimer::singleShot(50, this, [this]() {
        QScrollBar *bar = m_scrollArea->verticalScrollBar();
        bar->setValue(bar->maximum());
    });
}

void ChatPanel::addMessage(const QString &sender, const QString &text)
{
    QString time = QDateTime::currentDateTime().toString("HH:mm");
    auto *w = new QWidget;
    w->setStyleSheet("background:transparent;");
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 2);
    auto *header = new QLabel(
        QStringLiteral("<span style='color:gold;font-weight:bold;'>%1</span> "
                       "<span style='color:gray;font-size:10px;'>%2</span>")
            .arg(sender.toHtmlEscaped(), time));
    header->setStyleSheet("background:transparent;");
    lay->addWidget(header);
    auto *body = new QLabel(text.toHtmlEscaped());
    body->setWordWrap(true);
    body->setStyleSheet("color:white;background:transparent;font-size:12px;");
    lay->addWidget(body);
    appendMessageWidget(w);
}

void ChatPanel::addSystemMessage(const QString &msg)
{
    QString time = QDateTime::currentDateTime().toString("HH:mm");
    auto *w = new QLabel(
        QStringLiteral("<p style='color:#aaa;font-size:11px;text-align:center;'>%1 %2</p>")
            .arg(time, msg.toHtmlEscaped()));
    w->setStyleSheet("background:transparent;");
    w->setAlignment(Qt::AlignCenter);
    appendMessageWidget(w);
}

void ChatPanel::addStickerMessage(const QString &sender, const QPixmap &sticker)
{
    QString time = QDateTime::currentDateTime().toString("HH:mm");
    auto *w = new QWidget;
    w->setStyleSheet("background:transparent;");
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 2);
    auto *header = new QLabel(
        QStringLiteral("<span style='color:gold;font-weight:bold;'>%1</span> "
                       "<span style='color:gray;font-size:10px;'>%2</span>")
            .arg(sender.toHtmlEscaped(), time));
    header->setStyleSheet("background:transparent;");
    lay->addWidget(header);
    auto *img = new QLabel;
    img->setPixmap(sticker.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    img->setStyleSheet("background:transparent;");
    lay->addWidget(img);
    appendMessageWidget(w);
}

void ChatPanel::clear()
{
    while (m_msgLayout->count() > 1) {
        QLayoutItem *item = m_msgLayout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
}
