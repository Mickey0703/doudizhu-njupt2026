#include "popup_widget.h"
#include <QPainter>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QTimer>

PopupWidget::PopupWidget(const QPixmap &pixmap, QWidget *parent, int durationMs)
    : QLabel(parent), m_pixmap(pixmap), m_durationMs(durationMs)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    int sz = qMax(pixmap.width(), pixmap.height());
    setFixedSize(sz + 20, sz + 20);
}

void PopupWidget::showAt(const QPoint &pos, int offsetIndex)
{
    QPoint offset(offsetIndex * 30, offsetIndex * 20);
    move(pos + offset - QPoint(width()/2, height()/2));
    show();
    raise();

    auto *effect = new QGraphicsOpacityEffect(this);
    effect->setOpacity(1.0);
    setGraphicsEffect(effect);

    auto *anim = new QPropertyAnimation(effect, "opacity", this);
    anim->setDuration(300);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);

    QTimer::singleShot(m_durationMs - 300, this, [anim]() { anim->start(); });
    connect(anim, &QPropertyAnimation::finished, this, &QWidget::close);
}

void PopupWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    int x = (width() - m_pixmap.width()) / 2;
    int y = (height() - m_pixmap.height()) / 2;
    p.drawPixmap(x, y, m_pixmap);
}
