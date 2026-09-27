#include "game_action_controller.h"
#include "gamecontrol_adapter.h"
#include "online_room_controller.h"
#include "game_core/state_strategy.h"
#include "playhand.h"

GameActionController::GameActionController(GameControlAdapter *local,
    OnlineRoomController *online, QObject *parent)
    : QObject(parent), m_local(local), m_online(online)
{
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &GameActionController::tick);
}

const GameState &GameActionController::state() const
{
    return m_online->isOnline() ? m_online->viewState() : m_local->state();
}
int GameActionController::localSeat() const
{
    return m_online->isOnline() ? m_online->localSeat() : 0;
}
bool GameActionController::isOurTurn() const
{
    return isValidSeat(localSeat()) && state().currentSeat == localSeat()
           && (state().phase == GamePhase::CallingLord || state().phase == GamePhase::Playing);
}

GameActionResult GameActionController::submit(GameCommand command)
{
    if (!isOurTurn()) return {false, false, GameError::NotCurrentPlayer};
    command.seat = localSeat();
    const GameState &s = state();
    if (command.type == GameCommandType::PlayCards) {
        if (s.phase != GamePhase::Playing) return {false, false, GameError::InvalidPhase};
        if (!s.players[command.seat].hand.contains(command.cards))
            return {false, false, GameError::CardsNotOwned};
        const PlayHand hand(command.cards);
        if (hand.getHandType() == PlayHand::Hand_Unknown || hand.getHandType() == PlayHand::Hand_Pass)
            return {false, false, GameError::InvalidHand};
        if (!s.pendingCards.isEmpty() && !hand.canBeat(PlayHand(s.pendingCards)))
            return {false, false, GameError::HandDoesNotBeatPending};
    } else if (command.type == GameCommandType::Pass) {
        if (s.phase != GamePhase::Playing) return {false, false, GameError::InvalidPhase};
        if (s.pendingCards.isEmpty() || s.pendingSeat == command.seat)
            return {false, false, GameError::CannotPassWhenLeading};
    } else if (command.type == GameCommandType::CallLord) {
        if (s.phase != GamePhase::CallingLord) return {false, false, GameError::InvalidPhase};
        if (command.bid < 0 || command.bid > 3) return {false, false, GameError::InvalidBid};
        if (command.bid > 0 && command.bid <= s.highestBid)
            return {false, false, GameError::BidNotHighEnough};
    } else {
        return {false, false, GameError::InvalidPhase};
    }
    if (m_online->isOnline()) {
        const bool sent = m_online->submit(command);
        return {sent, sent, sent ? GameError::None : GameError::InvalidPhase};
    }
    const auto result = m_local->executeCommand(command);
    return {result.accepted, false, result.error};
}

GameCommand GameActionController::hint() const
{
    if (!isOurTurn()) return GameCommand::pass(kInvalidSeat);
    return StateStrategy::decideCommand(state(), localSeat());
}

void GameActionController::startCountdown(int seconds)
{
    stopCountdown();
    if (!isOurTurn() || seconds <= 0) return;
    m_seconds = seconds;
    m_turnSeat = state().currentSeat;
    m_turnPhase = state().phase;
    m_turnRound = state().roundNumber;
    emit secondsChanged(m_seconds);
    m_timer.start();
}
void GameActionController::stopCountdown()
{
    m_timer.stop();
    m_seconds = 0;
    m_turnSeat = kInvalidSeat;
}
void GameActionController::tick()
{
    if (!isOurTurn() || state().currentSeat != m_turnSeat || state().phase != m_turnPhase
        || state().roundNumber != m_turnRound) {
        stopCountdown();
        return;
    }
    emit secondsChanged(--m_seconds);
    if (m_seconds == 5) emit idleWarning();
    if (m_seconds > 0) return;
    m_timer.stop();
    // A leading player cannot pass. Use the same pure-state hint policy to
    // supply a legal lead; following players can safely pass on timeout.
    const GameCommand command = state().phase == GamePhase::CallingLord
        ? GameCommand::callLord(localSeat(), 0)
        : state().pendingCards.isEmpty() ? hint() : GameCommand::pass(localSeat());
    const auto result = submit(command);
    emit timedOut(result);
}
