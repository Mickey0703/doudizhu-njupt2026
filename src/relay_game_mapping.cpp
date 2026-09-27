#include "relay_game_mapping.h"

namespace {

bool isValidRelayPlayerId(int playerId)
{
    return playerId >= 1 && playerId <= 3;
}

} // namespace

int nextRelayPlayerId(int playerId)
{
    if (!isValidRelayPlayerId(playerId))
        return -1;
    return playerId == 3 ? 1 : playerId + 1;
}

int previousRelayPlayerId(int playerId)
{
    if (!isValidRelayPlayerId(playerId))
        return -1;
    return playerId == 1 ? 3 : playerId - 1;
}

RelaySeat relaySeatForPlayerId(int localPlayerId, int playerId)
{
    if (!isValidRelayPlayerId(localPlayerId) || !isValidRelayPlayerId(playerId))
        return RelaySeat::Unknown;

    if (playerId == localPlayerId)
        return RelaySeat::User;
    if (playerId == nextRelayPlayerId(localPlayerId))
        return RelaySeat::RightOpponent;
    if (playerId == previousRelayPlayerId(localPlayerId))
        return RelaySeat::LeftOpponent;

    return RelaySeat::Unknown;
}

int playerIdForRelaySeat(int localPlayerId, RelaySeat seat)
{
    if (!isValidRelayPlayerId(localPlayerId))
        return -1;

    switch (seat) {
    case RelaySeat::User:
        return localPlayerId;
    case RelaySeat::RightOpponent:
        return nextRelayPlayerId(localPlayerId);
    case RelaySeat::LeftOpponent:
        return previousRelayPlayerId(localPlayerId);
    case RelaySeat::Unknown:
        break;
    }

    return -1;
}
