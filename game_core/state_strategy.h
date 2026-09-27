#ifndef GAME_CORE_STATE_STRATEGY_H
#define GAME_CORE_STATE_STRATEGY_H
#include "game_state.h"
#include "game_command.h"

// Pure, deterministic policy. Only the acting seat's hand and public information
// are inspected, so authoritative and projected states yield identical commands.
namespace StateStrategy {
GameCommand decideCommand(const GameState &state, int seat);
}
#endif
