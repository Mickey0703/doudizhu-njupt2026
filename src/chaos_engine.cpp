#include "chaos_engine.h"
#include "playhand.h"

ChaosEngine::ChaosEngine(QObject *parent) : QObject(parent) {}

void ChaosEngine::setActive(bool active) { m_active = active; }
bool ChaosEngine::isActive() const { return m_active; }
void ChaosEngine::setStickers(const QVector<QPixmap> &stickers) { m_stickers = stickers; }

bool ChaosEngine::checkSpecialTrigger(const Cards &cards, int handType)
{
    if (!m_active) return false;
    bool triggered = false;
    int intensity = 0;
    QString message;

    if (handType == PlayHand::Hand_Bomb) {
        intensity = 3;
        if (!m_stickers.isEmpty())
            emit popupSticker(m_stickers[0], QPoint(0,0), 1500);
        message = QStringLiteral("耄耋规则：炸弹触发震屏");
        triggered = true;
    } else if (handType == PlayHand::Hand_Bomb_Jokers) {
        intensity = 6;
        if (m_stickers.size() > 1)
            emit popupSticker(m_stickers[1], QPoint(50,30), 1800);
        message = QStringLiteral("耄耋规则：火箭触发强震屏");
        triggered = true;
    } else if (cards.cardCount() >= 5 &&
               (handType == PlayHand::Hand_Plane ||
                handType == PlayHand::Hand_Plane_Two_Single ||
                handType == PlayHand::Hand_Plane_Two_Pair ||
                handType == PlayHand::Hand_Seq_Single ||
                handType == PlayHand::Hand_Seq_Pair)) {
        if (m_stickers.size() > 2)
            emit popupSticker(m_stickers[2], QPoint(-30,-20), 1500);
        message = QStringLiteral("耄耋规则：长牌型触发贴图");
        triggered = true;
    }

    if (triggered)
        emit chaosTriggered(message);
    if (intensity > 0) emit shakeScreen(intensity);
    return triggered;
}

bool ChaosEngine::checkCallLordTrigger(int bet)
{
    if (!m_active) return false;
    if (bet == 3) {
        if (!m_stickers.isEmpty()) {
            int idx = qMin(4, m_stickers.size() - 1);
            emit popupSticker(m_stickers[idx], QPoint(0,-60), 1800);
        }
        emit chaosTriggered(QStringLiteral("耄耋规则：3分叫地主触发贴图"));
        return true;
    }
    return false;
}

bool ChaosEngine::checkPassStreak(int streak)
{
    if (!m_active) return false;
    if (streak >= 2) {
        if (!m_stickers.isEmpty()) {
            int idx = qMin(3, m_stickers.size() - 1);
            emit popupSticker(m_stickers[idx], QPoint(40,20), 1500);
        }
        emit chaosTriggered(QStringLiteral("耄耋规则：连续不出触发贴图"));
        return true;
    }
    return false;
}
