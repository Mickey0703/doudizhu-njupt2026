#ifndef STICKER_WIDGET_H
#define STICKER_WIDGET_H

#include <QWidget>
#include <QGridLayout>

class StickerStore;

class StickerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit StickerWidget(StickerStore *store, QWidget *parent = nullptr);
    void refresh();
signals:
    void stickerSelected(int index);
protected:
    void focusOutEvent(QFocusEvent *event) override;
private:
    void buildGrid();
    StickerStore *m_store;
    QGridLayout *m_grid;
};

#endif
