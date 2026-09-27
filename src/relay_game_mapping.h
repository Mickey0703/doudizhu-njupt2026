#ifndef RELAY_GAME_MAPPING_H
#define RELAY_GAME_MAPPING_H

enum class RelaySeat {
    Unknown,
    User,
    RightOpponent,
    LeftOpponent
};

int nextRelayPlayerId(int playerId);
int previousRelayPlayerId(int playerId);

RelaySeat relaySeatForPlayerId(int localPlayerId, int playerId);
int playerIdForRelaySeat(int localPlayerId, RelaySeat seat);

#endif // RELAY_GAME_MAPPING_H
