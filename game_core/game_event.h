#ifndef GAME_CORE_GAME_EVENT_H
#define GAME_CORE_GAME_EVENT_H

#include "cards.h"
#include "game_types.h"

#include <QVector>

enum class GameEventType {
    RoundStarted,
    PrivateHandDealt,
    TurnChanged,
    BidAccepted,
    RoundVoided,
    LordSelected,
    CardsPlayed,
    PlayerPassed,
    MultiplierChanged,
    ScoreChanged,
    RoundFinished,
    // Emitted when two consecutive passes end a trick and lead returns to the
    // pending seat. The client view holds no engine, so without an explicit
    // event it cannot know pendingCards was cleared (it only sees TurnChanged).
    // event.seat = the seat that regains the lead. Must stay last to preserve
    // the serialized enum values of the events above.
    TrickReset,
    // Chaos (耄耋) mode: one or more cards vanished from event.seat's hand
    // (deal-time / idle-timeout). event.cards = the removed cards. Drives the
    // UI to re-render the shrunken hand + play the "card disappeared" effect.
    // Must stay last to preserve serialized enum values above.
    CardVanished
};

struct GameEvent
{
    GameEventType type = GameEventType::RoundStarted;
    int seat = kInvalidSeat;
    int value = 0;
    Cards cards;
};

struct CommandResult
{
    bool accepted = false;
    GameError error = GameError::None;
    QVector<GameEvent> events;

    static CommandResult rejected(GameError error)
    {
        CommandResult result;
        result.error = error;
        return result;
    }
};

#endif // GAME_CORE_GAME_EVENT_H
