#ifndef CHATPANEL_H
#define CHATPANEL_H

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QPixmap>

class StickerStore;
class StickerWidget;

class ChatPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ChatPanel(QWidget *parent = nullptr);

    void addMessage(const QString &sender, const QString &text);
    void addSystemMessage(const QString &msg);
    void addStickerMessage(const QString &sender, const QPixmap &sticker);
    void clear();

    void setStickerStore(StickerStore *store);

signals:
    void sendMessage(const QString &message);
    void sendSticker(int stickerId);

private slots:
    void onSendClicked();
    void onStickerBtnClicked();
    void onStickerSelected(int index);

private:
    void appendMessageWidget(QWidget *w);

    QScrollArea *m_scrollArea;
    QWidget *m_msgContainer;
    QVBoxLayout *m_msgLayout;
    QLineEdit *m_input;
    QPushButton *m_sendBtn;
    QPushButton *m_stickerBtn;
    QLabel *m_titleLabel;

    StickerStore *m_stickerStore = nullptr;
    StickerWidget *m_stickerWidget = nullptr;
};

#endif // CHATPANEL_H
