#include "game_record_model.h"
#include "playhand.h"

void GameRecordModel::beginSession(const std::array<int, 3> &scores)
{
    m_scores = scores;
    m_entries.clear();
    m_round = 0;
    m_multiplier = 1;
    emit changed();
}
void GameRecordModel::observe(const GameEvent &event)
{
    const QString who = isValidSeat(event.seat) ? QStringLiteral("座位 %1").arg(event.seat + 1) : QString();
    QString text;
    switch (event.type) {
    case GameEventType::RoundStarted:
        m_round = event.value;
        m_multiplier = 1;
        text = QStringLiteral("第 %1 局开始").arg(m_round);
        break;
    case GameEventType::LordSelected:
        m_multiplier = event.value;
        text = QStringLiteral("%1 成为地主，倍数 %2").arg(who).arg(m_multiplier);
        break;
    case GameEventType::BidAccepted:
        text = event.value ? QStringLiteral("%1 叫 %2 分").arg(who).arg(event.value)
                           : QStringLiteral("%1 不叫").arg(who);
        break;
    case GameEventType::CardsPlayed:
        text = QStringLiteral("%1 出 %2（%3 张）").arg(who,
            PlayHand::handTypeName(PlayHand(event.cards).getHandType())).arg(event.cards.cardCount());
        break;
    case GameEventType::PlayerPassed: text = who + QStringLiteral(" 不出"); break;
    case GameEventType::TrickReset: text = QStringLiteral("牌墩结束，%1 重新领出").arg(who); break;
    case GameEventType::MultiplierChanged:
        m_multiplier = event.value;
        text = QStringLiteral("倍数变为 %1").arg(m_multiplier);
        break;
    case GameEventType::ScoreChanged:
        if (isValidSeat(event.seat)) m_scores[event.seat] = event.value;
        text = QStringLiteral("%1 累计分 %2").arg(who).arg(event.value);
        break;
    case GameEventType::RoundFinished: text = who + QStringLiteral(" 获胜，本局结束"); break;
    case GameEventType::RoundVoided: text = QStringLiteral("无人叫地主，本局作废"); break;
    case GameEventType::CardVanished:
        text = QStringLiteral("%1 消失 %2 张牌").arg(who).arg(event.cards.cardCount());
        break;
    case GameEventType::PrivateHandDealt:
    case GameEventType::TurnChanged:
        break; // Do not record private card faces or noisy turn notifications.
    }
    if (!text.isEmpty()) {
        m_entries.append(text);
        while (m_entries.size() > 300) m_entries.removeFirst();
    }
    emit changed();
}
