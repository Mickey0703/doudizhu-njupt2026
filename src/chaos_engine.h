#ifndef CHAOS_ENGINE_H
#define CHAOS_ENGINE_H

#include <QObject>
#include <QPixmap>
#include "card.h"
#include "cards.h"

// Presentation-only chaos (耄耋) helper. Chaos *decisions and execution* (card
// disappearance, out-of-turn transform) live authoritatively in LocalSession;
// this class only fires the banners / stickers / screen-shake the UI shows.
class ChaosEngine : public QObject
{
    Q_OBJECT
public:
    explicit ChaosEngine(QObject *parent = nullptr);
    void setActive(bool active);
    bool isActive() const;
    void setStickers(const QVector<QPixmap> &stickers);

    bool checkSpecialTrigger(const Cards &cards, int handType);
    bool checkCallLordTrigger(int bet);
    bool checkPassStreak(int streak);

signals:
    void chaosTriggered(const QString &msg);
    void popupSticker(const QPixmap &sticker, QPoint screenPos, int durationMs);
    void shakeScreen(int intensity);
    void cardDisappeared(const Card &card);
    void cardTransformed(const Card &from, const Card &to);

private:
    bool m_active = false;
    QVector<QPixmap> m_stickers;
};

#endif // CHAOS_ENGINE_H
