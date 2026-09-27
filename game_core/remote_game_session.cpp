#include "remote_game_session.h"
#include "game_serialization.h"
#include "relay_client.h"
#include "networkdata.h"

#include <QByteArray>
#include <QDataStream>
#include <QIODevice>

RemoteGameSession::RemoteGameSession(RelayClient *client, QObject *parent)
    : QObject(parent)
    , m_client(client)
{
}

void RemoteGameSession::setLocalSeat(int seat)
{
    m_localSeat = seat;
    m_view.currentSeat = kInvalidSeat;
}

void RemoteGameSession::sendCommand(const GameCommand &command)
{
    if (!m_client)
        return;
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    GameSerialization::writeCommand(out, command);
    // sendGameMessage 会自动包上 subType 前缀
    m_client->sendGameMessage(NetMsg::GameSub::SvrCommand, payload);
}

void RemoteGameSession::callLord(int bid)
{
    sendCommand(GameCommand::callLord(m_localSeat, bid));
}

void RemoteGameSession::playCards(const Cards &cards)
{
    sendCommand(GameCommand::playCards(m_localSeat, cards));
}

void RemoteGameSession::pass()
{
    sendCommand(GameCommand::pass(m_localSeat));
}

void RemoteGameSession::handleServerMessage(quint8 subType, const QByteArray &payload)
{
    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_0);

    if (subType == NetMsg::GameSub::SvrEvent)
    {
        const GameEvent event = GameSerialization::readEvent(in);
        applyEvent(event);
        emit eventApplied(event);
        if (event.type == GameEventType::RoundFinished)
            emit roundFinished(event.seat);
    }
    else if (subType == NetMsg::GameSub::SvrRejected)
    {
        quint8 error = 0;
        in >> error;
        emit commandRejected(static_cast<GameError>(error));
    }
}

void RemoteGameSession::applyEvent(const GameEvent &event)
{
    // 用服务器权威事件更新本地只读视图。
    // 客户端不运行规则，只把事件反映到 m_view 供 UI 查询。
    switch (event.type)
    {
    case GameEventType::RoundStarted:
        m_view = GameState();                 // 清空重来
        m_view.phase = GamePhase::CallingLord;
        m_view.currentSeat = event.seat;      // firstSeat
        m_view.roundNumber = event.value;
        // 每人 17 张起手（地主选定后 +3 底牌）
        for (int i = 0; i < kPlayerCount; ++i)
            m_view.players[i].handCount = 17;
        break;

    case GameEventType::PrivateHandDealt:
        // 仅本座位会收到自己的手牌。isValidSeat 守卫防止 m_localSeat 未设(-1)
        // 或恶意/损坏事件 seat=-1 时 -1==-1 通过后 players[-1] 越界写。
        if (isValidSeat(m_localSeat) && event.seat == m_localSeat)
            m_view.players[m_localSeat].hand = event.cards;
        break;

    case GameEventType::TurnChanged:
        m_view.currentSeat = event.seat;
        break;

    case GameEventType::BidAccepted:
        ++m_view.bidCount;
        if (event.value > m_view.highestBid) {
            m_view.highestBid = event.value;
            m_view.highestBidder = event.seat;
        }
        break;

    case GameEventType::LordSelected:
        if (isValidSeat(event.seat)) {
            m_view.players[event.seat].role = PlayerRole::Lord;
            for (int i = 0; i < kPlayerCount; ++i)
                if (i != event.seat)
                    m_view.players[i].role = PlayerRole::Farmer;
        }
        m_view.phase = GamePhase::Playing;
        m_view.multiplier = event.value > 0 ? event.value : m_view.multiplier;
        // event.cards = 明牌底牌（公开）
        m_view.revealedBottomCards = event.cards;
        // 地主拿到 3 张底牌 → 手数 20
        if (isValidSeat(event.seat))
            m_view.players[event.seat].handCount += event.cards.cardCount();
        // 地主加底牌（仅本座位手牌需要更新具体牌面）
        if (isValidSeat(m_localSeat) && event.seat == m_localSeat)
            m_view.players[m_localSeat].hand.add(event.cards);
        break;

    case GameEventType::CardsPlayed:
        m_view.pendingCards = event.cards;
        m_view.pendingSeat = event.seat;
        m_view.playedCards.add(event.cards);
        // 维护对手剩余牌数（本地座位的牌数由完整手牌得出）
        if (isValidSeat(event.seat))
            m_view.players[event.seat].handCount -= event.cards.cardCount();
        // 若是本座位出牌，从本地手牌移除
        if (isValidSeat(m_localSeat) && event.seat == m_localSeat)
            m_view.players[m_localSeat].hand.remove(event.cards);
        break;

    case GameEventType::PlayerPassed:
        // 无状态变化需反映（当前座位由后续 TurnChanged 更新）
        break;

    case GameEventType::TrickReset:
        // 双 pass 结束一墩：清空 pending，领出权回到 event.seat。
        // 否则视图残留上一墩的牌，领出判断（pendingSeat==currentSeat）出错。
        m_view.pendingCards.clear();
        m_view.pendingSeat = kInvalidSeat;
        break;

    case GameEventType::MultiplierChanged:
        m_view.multiplier = event.value;
        break;

    case GameEventType::ScoreChanged:
        if (isValidSeat(event.seat))
            m_view.players[event.seat].score = event.value;
        break;

    case GameEventType::RoundFinished:
        m_view.phase = GamePhase::RoundFinished;
        m_view.winnerSeat = event.seat;
        m_view.currentSeat = kInvalidSeat;
        break;

    case GameEventType::RoundVoided:
        m_view.phase = GamePhase::Waiting;
        m_view.currentSeat = kInvalidSeat;
        break;

    case GameEventType::CardVanished:
        // Chaos mode is single-player only (disabled online), so this event is
        // not expected here. Handle it defensively anyway: shrink the affected
        // seat's view so a stray event can't desync the count.
        if (isValidSeat(event.seat)) {
            m_view.players[event.seat].handCount -= event.cards.cardCount();
            if (event.seat == m_localSeat)
                m_view.players[m_localSeat].hand.remove(event.cards);
        }
        break;
    }
}
