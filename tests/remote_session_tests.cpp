#include "game_core/remote_game_session.h"
#include "game_core/game_serialization.h"
#include "game_core/game_event.h"
#include "cards.h"
#include "test_support.h"

#include <QByteArray>
#include <QDataStream>
#include <QIODevice>

// 把一个 GameEvent 打包成 SvrEvent payload（subType 前缀 + 序列化事件）
static QByteArray makeEventPayload(const GameEvent &event)
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    // handleServerMessage 的 payload 不含 subType（subType 单独传参）
    GameSerialization::writeEvent(out, event);
    return payload;
}

// 便捷：构造事件
static GameEvent evt(GameEventType type, int seat, int value = 0, const Cards &cards = Cards())
{
    GameEvent e;
    e.type = type;
    e.seat = seat;
    e.value = value;
    e.cards = cards;
    return e;
}

// SvrEvent subType 常量（与 networkdata.h NetMsg::GameSub::SvrEvent = 21 一致）
static constexpr quint8 kSvrEvent = 21;

bool testRemoteViewUpdatesOnRoundStarted()
{
    RemoteGameSession session(nullptr);
    session.setLocalSeat(0);

    session.handleServerMessage(kSvrEvent, makeEventPayload(
        evt(GameEventType::RoundStarted, 0, 1)));

    EXPECT_EQ(session.viewState().phase, GamePhase::CallingLord);
    EXPECT_EQ(session.viewState().currentSeat, 0);
    EXPECT_EQ(session.viewState().roundNumber, 1);
    return true;
}

bool testRemoteViewStoresOwnHandOnly()
{
    RemoteGameSession session(nullptr);
    session.setLocalSeat(1);

    session.handleServerMessage(kSvrEvent, makeEventPayload(
        evt(GameEventType::RoundStarted, 0, 1)));

    // 自己的私有发牌
    Cards myHand;
    myHand.add(Card(Card::Card_3, Card::Spade));
    myHand.add(Card(Card::Card_A, Card::Heart));
    session.handleServerMessage(kSvrEvent, makeEventPayload(
        evt(GameEventType::PrivateHandDealt, 1, 2, myHand)));

    EXPECT_TRUE(session.viewState().players[1].hand == myHand);
    // 他人手牌应为空（客户端看不到）
    EXPECT_TRUE(session.viewState().players[0].hand.isEmpty());
    EXPECT_TRUE(session.viewState().players[2].hand.isEmpty());
    return true;
}

bool testRemoteViewIgnoresOtherSeatPrivateHand()
{
    RemoteGameSession session(nullptr);
    session.setLocalSeat(0);

    // 收到 seat 2 的私有发牌（理论上不该发到这，但即使发了也不能存）
    Cards otherHand;
    otherHand.add(Card(Card::Card_K, Card::Club));
    session.handleServerMessage(kSvrEvent, makeEventPayload(
        evt(GameEventType::PrivateHandDealt, 2, 1, otherHand)));

    // seat 2 手牌仍为空（本地座位是 0，只存自己的）
    EXPECT_TRUE(session.viewState().players[2].hand.isEmpty());
    return true;
}

bool testRemoteViewTracksTurnChange()
{
    RemoteGameSession session(nullptr);
    session.setLocalSeat(0);

    session.handleServerMessage(kSvrEvent, makeEventPayload(
        evt(GameEventType::TurnChanged, 2)));
    EXPECT_EQ(session.viewState().currentSeat, 2);
    return true;
}

bool testRemoteViewTracksLordSelected()
{
    RemoteGameSession session(nullptr);
    session.setLocalSeat(1);

    session.handleServerMessage(kSvrEvent, makeEventPayload(
        evt(GameEventType::LordSelected, 1, 2)));

    EXPECT_EQ(session.viewState().phase, GamePhase::Playing);
    EXPECT_EQ(session.viewState().players[1].role, PlayerRole::Lord);
    EXPECT_EQ(session.viewState().players[0].role, PlayerRole::Farmer);
    EXPECT_EQ(session.viewState().players[2].role, PlayerRole::Farmer);
    EXPECT_EQ(session.viewState().multiplier, 2);
    return true;
}

bool testRemoteViewRemovesOwnCardsOnPlay()
{
    RemoteGameSession session(nullptr);
    session.setLocalSeat(0);

    // 先给自己发牌
    Cards myHand;
    myHand.add(Card(Card::Card_3, Card::Spade));
    myHand.add(Card(Card::Card_4, Card::Heart));
    session.handleServerMessage(kSvrEvent, makeEventPayload(
        evt(GameEventType::PrivateHandDealt, 0, 2, myHand)));

    // 自己出一张
    Cards played;
    played.add(Card(Card::Card_3, Card::Spade));
    session.handleServerMessage(kSvrEvent, makeEventPayload(
        evt(GameEventType::CardsPlayed, 0, 0, played)));

    // 本地手牌应只剩一张
    EXPECT_EQ(session.viewState().players[0].hand.cardCount(), 1);
    EXPECT_TRUE(session.viewState().pendingCards == played);
    EXPECT_EQ(session.viewState().pendingSeat, 0);
    return true;
}

bool testRemoteViewTracksScoreAndFinish()
{
    RemoteGameSession session(nullptr);
    session.setLocalSeat(0);

    session.handleServerMessage(kSvrEvent, makeEventPayload(
        evt(GameEventType::ScoreChanged, 0, 6)));
    EXPECT_EQ(session.viewState().players[0].score, 6);

    session.handleServerMessage(kSvrEvent, makeEventPayload(
        evt(GameEventType::RoundFinished, 0)));
    EXPECT_EQ(session.viewState().phase, GamePhase::RoundFinished);
    EXPECT_EQ(session.viewState().winnerSeat, 0);
    return true;
}

bool testRemoteViewClearsPendingOnTrickReset()
{
    // Regression: double-pass clears pending server-side but the client only
    // saw TurnChanged before. A TrickReset event must clear the client's
    // pending cards/seat so leader detection works on the next trick.
    RemoteGameSession session(nullptr);
    session.setLocalSeat(0);

    Cards played;
    played.add(Card(Card::Card_7, Card::Spade));
    session.handleServerMessage(kSvrEvent, makeEventPayload(
        evt(GameEventType::CardsPlayed, 2, 0, played)));
    EXPECT_TRUE(!session.viewState().pendingCards.isEmpty());
    EXPECT_EQ(session.viewState().pendingSeat, 2);

    // Two passes -> trick reset, lead returns to seat 2
    session.handleServerMessage(kSvrEvent, makeEventPayload(
        evt(GameEventType::TrickReset, 2)));

    EXPECT_TRUE(session.viewState().pendingCards.isEmpty());
    EXPECT_EQ(session.viewState().pendingSeat, kInvalidSeat);
    return true;
}

bool testRemoteViewTracksOpponentHandCounts()
{
    // handCount must start at 17, jump to 20 for the lord, and decrement as
    // opponents play (opponent hands are otherwise invisible to the client).
    RemoteGameSession session(nullptr);
    session.setLocalSeat(0);

    session.handleServerMessage(kSvrEvent, makeEventPayload(
        evt(GameEventType::RoundStarted, 0, 1)));
    EXPECT_EQ(session.viewState().players[1].handCount, 17);
    EXPECT_EQ(session.viewState().players[2].handCount, 17);

    // seat 1 becomes lord (+3 bottom cards)
    Cards bottom;
    bottom.add(Card(Card::Card_3, Card::Spade));
    bottom.add(Card(Card::Card_4, Card::Spade));
    bottom.add(Card(Card::Card_5, Card::Spade));
    session.handleServerMessage(kSvrEvent, makeEventPayload(
        evt(GameEventType::LordSelected, 1, 3, bottom)));
    EXPECT_EQ(session.viewState().players[1].handCount, 20);

    // seat 1 plays two cards
    Cards play;
    play.add(Card(Card::Card_9, Card::Spade));
    play.add(Card(Card::Card_9, Card::Heart));
    session.handleServerMessage(kSvrEvent, makeEventPayload(
        evt(GameEventType::CardsPlayed, 1, 0, play)));
    EXPECT_EQ(session.viewState().players[1].handCount, 18);
    return true;
}

bool testRemoteViewSurvivesUnsetLocalSeat()
{
    // Regression for the -1 OOB write: applying events before setLocalSeat
    // (m_localSeat == kInvalidSeat) with seat=-1 must not write players[-1].
    RemoteGameSession session(nullptr);  // localSeat defaults to kInvalidSeat

    Cards hand;
    hand.add(Card(Card::Card_3, Card::Spade));
    // seat -1 with unset local seat: guard must skip the store, no crash.
    session.handleServerMessage(kSvrEvent, makeEventPayload(
        evt(GameEventType::PrivateHandDealt, kInvalidSeat, 0, hand)));
    session.handleServerMessage(kSvrEvent, makeEventPayload(
        evt(GameEventType::CardsPlayed, kInvalidSeat, 0, hand)));

    // Reaching here without crashing is the assertion.
    EXPECT_TRUE(true);
    return true;
}

int main()
{
    const TestCase tests[] = {
        {"Remote view updates on RoundStarted", testRemoteViewUpdatesOnRoundStarted},
        {"Remote view stores own hand only", testRemoteViewStoresOwnHandOnly},
        {"Remote view ignores other seat private hand", testRemoteViewIgnoresOtherSeatPrivateHand},
        {"Remote view tracks turn change", testRemoteViewTracksTurnChange},
        {"Remote view tracks lord selected", testRemoteViewTracksLordSelected},
        {"Remote view removes own cards on play", testRemoteViewRemovesOwnCardsOnPlay},
        {"Remote view tracks score and finish", testRemoteViewTracksScoreAndFinish},
        {"Remote view clears pending on trick reset", testRemoteViewClearsPendingOnTrickReset},
        {"Remote view tracks opponent hand counts", testRemoteViewTracksOpponentHandCounts},
        {"Remote view survives unset local seat", testRemoteViewSurvivesUnsetLocalSeat},
    };
    return runTests(tests, static_cast<int>(std::size(tests)));
}
