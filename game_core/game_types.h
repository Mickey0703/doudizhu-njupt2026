#ifndef GAME_CORE_GAME_TYPES_H
#define GAME_CORE_GAME_TYPES_H

constexpr int kPlayerCount = 3;
constexpr int kInvalidSeat = -1;

enum class GamePhase {
    Waiting,
    Dealing,
    CallingLord,
    Playing,
    RoundFinished
};

enum class PlayerRole {
    Unknown,
    Lord,
    Farmer
};

enum class GameError {
    None,
    InvalidPhase,
    InvalidSeat,
    NotCurrentPlayer,
    InvalidBid,
    BidNotHighEnough,
    CardsNotOwned,
    InvalidHand,
    HandDoesNotBeatPending,
    CannotPassWhenLeading,
    RoundAlreadyFinished,
    InvalidDeck
};

inline bool isValidSeat(int seat)
{
    return seat >= 0 && seat < kPlayerCount;
}

inline int nextSeat(int seat)
{
    return isValidSeat(seat) ? (seat + 1) % kPlayerCount : kInvalidSeat;
}

inline int previousSeat(int seat)
{
    return isValidSeat(seat) ? (seat + kPlayerCount - 1) % kPlayerCount
                             : kInvalidSeat;
}

#endif // GAME_CORE_GAME_TYPES_H
