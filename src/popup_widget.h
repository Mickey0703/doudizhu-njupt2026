#ifndef POPUP_WIDGET_H
#define POPUP_WIDGET_H

#include <QLabel>

class PopupWidget : public QLabel
{
    Q_OBJECT
public:
    explicit PopupWidget(const QPixmap &pixmap, QWidget *parent, int durationMs = 1500);
    void showAt(const QPoint &pos, int offsetIndex = 0);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    QPixmap m_pixmap;
    int m_durationMs;
};

#endif
