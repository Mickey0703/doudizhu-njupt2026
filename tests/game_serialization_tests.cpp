#include "game_core/game_serialization.h"
#include "game_core/game_command.h"
#include "game_core/game_event.h"
#include "cards.h"
#include "test_support.h"

#include <QByteArray>
#include <QDataStream>
#include <QIODevice>

// Helper: round-trip a Cards through serialization
static Cards roundTripCards(const Cards &original)
{
    QByteArray buffer;
    QDataStream out(&buffer, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    GameSerialization::writeCards(out, original);

    QDataStream in(&buffer, QIODevice::ReadOnly);
    in.setVersion(QDataStream::Qt_6_0);
    return GameSerialization::readCards(in);
}

bool testCardsSerializationRoundTrip()
{
    Cards cards;
    cards.add(Card(Card::Card_3, Card::Spade));
    cards.add(Card(Card::Card_A, Card::Heart));
    cards.add(Card(Card::Card_BJ, Card::Suit_Begin));

    const Cards restored = roundTripCards(cards);
    EXPECT_TRUE(restored == cards);
    EXPECT_EQ(restored.cardCount(), 3);
    return true;
}

bool testEmptyCardsSerializationRoundTrip()
{
    Cards empty;
    const Cards restored = roundTripCards(empty);
    EXPECT_TRUE(restored.isEmpty());
    return true;
}

static GameCommand roundTripCommand(const GameCommand &original)
{
    QByteArray buffer;
    QDataStream out(&buffer, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    GameSerialization::writeCommand(out, original);

    QDataStream in(&buffer, QIODevice::ReadOnly);
    in.setVersion(QDataStream::Qt_6_0);
    return GameSerialization::readCommand(in);
}

bool testCallLordCommandRoundTrip()
{
    const GameCommand cmd = GameCommand::callLord(1, 3);
    const GameCommand restored = roundTripCommand(cmd);
    EXPECT_TRUE(restored.type == GameCommandType::CallLord);
    EXPECT_EQ(restored.seat, 1);
    EXPECT_EQ(restored.bid, 3);
    return true;
}

bool testPlayCardsCommandRoundTrip()
{
    Cards cards;
    cards.add(Card(Card::Card_5, Card::Club));
    cards.add(Card(Card::Card_5, Card::Heart));
    const GameCommand cmd = GameCommand::playCards(2, cards);

    const GameCommand restored = roundTripCommand(cmd);
    EXPECT_TRUE(restored.type == GameCommandType::PlayCards);
    EXPECT_EQ(restored.seat, 2);
    EXPECT_TRUE(restored.cards == cards);
    return true;
}

bool testPassCommandRoundTrip()
{
    const GameCommand cmd = GameCommand::pass(0);
    const GameCommand restored = roundTripCommand(cmd);
    EXPECT_TRUE(restored.type == GameCommandType::Pass);
    EXPECT_EQ(restored.seat, 0);
    return true;
}

static GameEvent roundTripEvent(const GameEvent &original)
{
    QByteArray buffer;
    QDataStream out(&buffer, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    GameSerialization::writeEvent(out, original);

    QDataStream in(&buffer, QIODevice::ReadOnly);
    in.setVersion(QDataStream::Qt_6_0);
    return GameSerialization::readEvent(in);
}

bool testEventRoundTripAllTypes()
{
    const GameEventType types[] = {
        GameEventType::RoundStarted,
        GameEventType::PrivateHandDealt,
        GameEventType::TurnChanged,
        GameEventType::BidAccepted,
        GameEventType::RoundVoided,
        GameEventType::LordSelected,
        GameEventType::CardsPlayed,
        GameEventType::PlayerPassed,
        GameEventType::MultiplierChanged,
        GameEventType::ScoreChanged,
        GameEventType::RoundFinished,
    };

    for (const GameEventType type : types) {
        GameEvent event;
        event.type = type;
        event.seat = 2;
        event.value = 42;
        event.cards.add(Card(Card::Card_K, Card::Diamond));

        const GameEvent restored = roundTripEvent(event);
        EXPECT_TRUE(restored.type == type);
        EXPECT_EQ(restored.seat, 2);
        EXPECT_EQ(restored.value, 42);
        EXPECT_TRUE(restored.cards == event.cards);
    }
    return true;
}

// N5：截断的 Cards 包(count 声称 5 张但只给 1 张)不得空转/塞默认牌，
// 只返回真正读到的合法牌。
bool testTruncatedCardsStopsCleanly()
{
    QByteArray buffer;
    QDataStream out(&buffer, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << quint8(5);                                              // 声称 5 张
    out << quint8(Card::Card_7 * 10 + Card::Spade);               // 只给 1 张

    QDataStream in(&buffer, QIODevice::ReadOnly);
    in.setVersion(QDataStream::Qt_6_0);
    const Cards restored = GameSerialization::readCards(in);
    // 读到 1 张合法牌后流耗尽即停，不得因声称的 5 而产出 5 张（含默认牌）。
    EXPECT_EQ(restored.cardCount(), 1);
    EXPECT_TRUE(restored.contains(Card(Card::Card_7, Card::Spade)));
    return true;
}

// N5：命令头部被截断(只给了 type，没给 seat/bid)返回座位非法命令，
// 引擎的座位校验能安全拒绝。
bool testTruncatedCommandHeaderReturnsInvalidSeat()
{
    QByteArray buffer;
    QDataStream out(&buffer, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << quint8(static_cast<quint8>(GameCommandType::PlayCards)); // 只写 type

    QDataStream in(&buffer, QIODevice::ReadOnly);
    in.setVersion(QDataStream::Qt_6_0);
    const GameCommand cmd = GameSerialization::readCommand(in);
    EXPECT_TRUE(!isValidSeat(cmd.seat));
    return true;
}

// N4：非法卡牌码(越界点数/花色)解码为默认哨兵牌，不产生越界枚举。
bool testMalformedCardCodeDecodesToSentinel()
{
    QByteArray buffer;
    QDataStream out(&buffer, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << quint8(1);      // 1 张
    out << quint8(250);    // 非法码：point=25, suit=0，越界

    QDataStream in(&buffer, QIODevice::ReadOnly);
    in.setVersion(QDataStream::Qt_6_0);
    const Cards restored = GameSerialization::readCards(in);
    // 非法码 → 默认 Card（哨兵 Card_Begin/Suit_Begin），不得是越界枚举。
    EXPECT_EQ(restored.cardCount(), 1);
    const Card c = restored.toCardList(Cards::NoSort).first();
    EXPECT_TRUE(c.getpoint() == Card::Card_Begin && c.getsuit() == Card::Suit_Begin);
    return true;
}

int main()
{
    const TestCase tests[] = {
        {"Cards serialization round-trip", testCardsSerializationRoundTrip},
        {"Empty cards round-trip", testEmptyCardsSerializationRoundTrip},
        {"CallLord command round-trip", testCallLordCommandRoundTrip},
        {"PlayCards command round-trip", testPlayCardsCommandRoundTrip},
        {"Pass command round-trip", testPassCommandRoundTrip},
        {"Event round-trip all types", testEventRoundTripAllTypes},
        {"Truncated cards stops cleanly", testTruncatedCardsStopsCleanly},
        {"Truncated command header returns invalid seat", testTruncatedCommandHeaderReturnsInvalidSeat},
        {"Malformed card code decodes to sentinel", testMalformedCardCodeDecodesToSentinel},
    };
    return runTests(tests, static_cast<int>(std::size(tests)));
}
