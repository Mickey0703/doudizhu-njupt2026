#include "game_serialization.h"

namespace GameSerialization
{

[[maybe_unused]] static quint8 encodeCard(const Card &card)
{
    return static_cast<quint8>(card.getpoint() * 10 + card.getsuit());
}

[[maybe_unused]] static Card decodeCard(quint8 code)
{
    const int point = code / 10;
    const int suit = code % 10;

    const bool joker = (point == Card::Card_SJ || point == Card::Card_BJ)
                       && suit == Card::Suit_Begin;
    const bool normal = (point >= Card::Card_3 && point <= Card::Card_2)
                        && (suit >= Card::Diamond && suit <= Card::Spade);
    if (!joker && !normal)
        return Card();

    return Card(static_cast<Card::CardPoint>(point),
                static_cast<Card::CardSuit>(suit));
}

void writeCards(QDataStream &out, const Cards &cards)
{
    const CardList list = cards.toCardList(Cards::Asc);
    if (list.size() > 54) {
        out.setStatus(QDataStream::WriteFailed);
        return;
    }
    out << static_cast<quint8>(list.size());
    for (const Card &card : list)
        out << encodeCard(card);
}

Cards readCards(QDataStream &in)
{
    Cards cards;
    quint8 count = 0;
    in >> count;
    if (in.status() != QDataStream::Ok)
        return cards;
    if (count > 54) {
        in.setStatus(QDataStream::ReadCorruptData);
        return cards;
    }
    for (int i = 0; i < count; ++i) {
        quint8 code = 0;
        in >> code;
        if (in.status() != QDataStream::Ok)
            break;
        const Card card = decodeCard(code);
        if (cards.contains(card)) {
            in.setStatus(QDataStream::ReadCorruptData);
            break;
        }
        // Preserve the decoder's sentinel for malformed physical card codes;
        // ownership checks reject it, without casting out-of-range enums.
        cards.add(card);
    }
    return cards;
}

void writeCommand(QDataStream &out, const GameCommand &command)
{
    out << static_cast<quint8>(command.type)
        << static_cast<qint32>(command.seat) << static_cast<qint32>(command.bid);
    writeCards(out, command.cards);
}

GameCommand readCommand(QDataStream &in)
{
    GameCommand command;
    quint8 type = 0;
    qint32 seat = kInvalidSeat, bid = 0;
    in >> type >> seat >> bid;
    if (in.status() != QDataStream::Ok)
        return command;
    if (type > static_cast<quint8>(GameCommandType::ChaosVanish)) {
        in.setStatus(QDataStream::ReadCorruptData);
        return command;
    }
    const Cards cards = readCards(in);
    // A truncated body must not turn into a valid partial play/vanish.
    if (in.status() != QDataStream::Ok)
        return command;
    // Non-play commands do not inspect their cards. Reject decoder sentinels
    // here so a malformed body cannot become a valid bid or pass.
    if (cards.contains(Card())) {
        in.setStatus(QDataStream::ReadCorruptData);
        return command;
    }
    command.type = static_cast<GameCommandType>(type);
    command.seat = seat;
    command.bid = bid;
    command.cards = cards;
    return command;
}

void writeEvent(QDataStream &out, const GameEvent &event)
{
    out << static_cast<quint8>(event.type)
        << static_cast<qint32>(event.seat) << static_cast<qint32>(event.value);
    writeCards(out, event.cards);
}

GameEvent readEvent(QDataStream &in)
{
    GameEvent event;
    quint8 type = 0;
    qint32 seat = kInvalidSeat, value = 0;
    in >> type >> seat >> value;
    if (in.status() != QDataStream::Ok)
        return event;
    if (type > static_cast<quint8>(GameEventType::CardVanished)) {
        in.setStatus(QDataStream::ReadCorruptData);
        return event;
    }
    const Cards cards = readCards(in);
    if (in.status() != QDataStream::Ok)
        return event;
    if (cards.contains(Card())) {
        in.setStatus(QDataStream::ReadCorruptData);
        return event;
    }
    event.type = static_cast<GameEventType>(type);
    event.seat = seat;
    event.value = value;
    event.cards = cards;
    return event;
}

} // namespace GameSerialization
