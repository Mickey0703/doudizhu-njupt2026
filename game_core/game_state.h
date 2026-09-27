#ifndef GAME_CORE_GAME_STATE_H
#define GAME_CORE_GAME_STATE_H

#include "cards.h"
#include "game_types.h"

#include <array>

struct PlayerState
{
    Cards hand;
    PlayerRole role = PlayerRole::Unknown;
    int score = 0;
    bool connected = true;
    // Remaining card count. In the authoritative engine `hand` is always
    // complete so this is informational, but a view-only client (RemoteGameSession)
    // sees only its own hand and relies on handCount for opponents.
    int handCount = 0;
    // Number of times this seat has played cards (passes excluded) this round.
    // Used to detect 春天/反春天 (spring / anti-spring) doubling at round end.
    int playsMade = 0;
};

struct GameState
{
    GamePhase phase = GamePhase::Waiting;
    std::array<PlayerState, kPlayerCount> players;
    Cards bottomCards;
    Cards revealedBottomCards;
    Cards pendingCards;
    Cards playedCards;
    // Chaos (耄耋) mode: cards removed from play by disappearance effects. Kept
    // so the 54-card / no-duplicate invariant in validateState() stays honest.
    Cards vanishedCards;
    int currentSeat = kInvalidSeat;
    int pendingSeat = kInvalidSeat;
    int highestBid = 0;
    int highestBidder = kInvalidSeat;
    int bidCount = 0;
    int multiplier = 1;
    int passCount = 0;
    int winnerSeat = kInvalidSeat;
    int roundNumber = 0;
};

#endif // GAME_CORE_GAME_STATE_H
