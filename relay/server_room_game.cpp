#include "server_room_game.h"
#include "game_core/deck.h"
#include "game_core/ai_policy.h"

CommandResult ServerRoomGame::startRound(quint32 seed, int firstSeat)
{
    const CardList deck = Deck::shuffled(seed);
    CommandResult result = m_engine.startRound(deck, firstSeat);
    if (result.accepted)
        m_active = true;
    return result;
}

void ServerRoomGame::setSeatAI(int seat, bool ai)
{
    if (isValidSeat(seat))
        m_aiSeats[seat] = ai;
}

bool ServerRoomGame::isSeatAI(int seat) const
{
    return isValidSeat(seat) && m_aiSeats[seat];
}

bool ServerRoomGame::hasAISeat() const
{
    for (bool ai : m_aiSeats)
        if (ai) return true;
    return false;
}

void ServerRoomGame::clearAISeats()
{
    for (bool &ai : m_aiSeats)
        ai = false;
}

CommandResult ServerRoomGame::stepAI()
{
    const int seat = m_engine.state().currentSeat;
    if (!isValidSeat(seat) || !m_aiSeats[seat])
        return CommandResult::rejected(GameError::InvalidSeat);

    CommandResult result = AIPolicy::performMove(m_engine, seat);

    // 与 applyCommand 相同的活跃态复位：一局结束或全员不叫流局后 isActive() 置否。
    if (result.accepted) {
        const GamePhase phase = m_engine.state().phase;
        if (phase == GamePhase::RoundFinished || phase == GamePhase::Waiting)
            m_active = false;
    }
    return result;
}

CommandResult ServerRoomGame::applyCommand(int seat, const GameCommand &command)
{
    // 强制使用会话推导的座位，忽略客户端载荷中的 seat
    GameCommand authoritative = command;
    authoritative.seat = seat;

    CommandResult result = m_engine.execute(authoritative);

    // 一局结束（RoundFinished）或全员不叫流局（引擎置回 Waiting）后标记非活跃，
    // 使 isActive() 正确复位——否则房间在首局后永久锁死（N2），既无法重开也无法加入。
    if (result.accepted) {
        const GamePhase phase = m_engine.state().phase;
        if (phase == GamePhase::RoundFinished || phase == GamePhase::Waiting)
            m_active = false;
    }

    return result;
}
