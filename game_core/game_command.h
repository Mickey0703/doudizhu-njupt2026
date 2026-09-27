#ifndef GAME_CORE_GAME_COMMAND_H
#define GAME_CORE_GAME_COMMAND_H

#include "cards.h"
#include "game_types.h"

enum class GameCommandType {
    CallLord,
    PlayCards,
    Pass,
    // Chaos (耄耋) mode only: remove one card from a seat's hand out of turn
    // (deal-time / idle-timeout disappearance). The randomness is decided by the
    // session/ChaosEngine; the engine just applies it deterministically so the
    // authoritative GameState stays the single source of truth (no resurrection
    // on next sync). Must stay last to preserve serialized enum values above.
    ChaosVanish
};

struct GameCommand
{
    GameCommandType type = GameCommandType::Pass;
    int seat = kInvalidSeat;
    int bid = 0;
    Cards cards;

    static GameCommand callLord(int seat, int bid)
    {
        GameCommand command;
        command.type = GameCommandType::CallLord;
        command.seat = seat;
        command.bid = bid;
        return command;
    }

    static GameCommand playCards(int seat, const Cards &cards)
    {
        GameCommand command;
        command.type = GameCommandType::PlayCards;
        command.seat = seat;
        command.cards = cards;
        return command;
    }

    static GameCommand pass(int seat)
    {
        GameCommand command;
        command.type = GameCommandType::Pass;
        command.seat = seat;
        return command;
    }

    static GameCommand chaosVanish(int seat, const Cards &cards)
    {
        GameCommand command;
        command.type = GameCommandType::ChaosVanish;
        command.seat = seat;
        command.cards = cards;
        return command;
    }
};

#endif // GAME_CORE_GAME_COMMAND_H
