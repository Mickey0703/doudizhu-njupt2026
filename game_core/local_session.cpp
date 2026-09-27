#include "local_session.h"
#include "deck.h"
#include "ai_policy.h"

#include <QTimer>
#include <QDebug>
#include <QRandomGenerator>
#include <random>

namespace {
// AI 每步行动前的延迟（毫秒）。给玩家留出看清对手出牌的时间。
constexpr int kAIMoveDelayMs = 1200;
}

LocalSession::LocalSession(QObject *parent)
    : QObject(parent)
    , m_aiEnabled(true)
{
    // Default: all AI players
    for (int i = 0; i < kPlayerCount; ++i) {
        m_playerTypes[i] = PlayerType::AI;
    }
}

LocalSession::~LocalSession()
{
}

void LocalSession::setPlayerType(int seat, PlayerType type)
{
    if (!isValidSeat(seat))
        return;

    m_playerTypes[seat] = type;
}

PlayerType LocalSession::playerType(int seat) const
{
    if (!isValidSeat(seat))
        return PlayerType::User;
    return m_playerTypes[seat];
}

bool LocalSession::startRound(quint32 seed)
{
    if (seed == 0)
        seed = QRandomGenerator::global()->generate();
    const CommandResult result = m_engine.startRound(Deck::shuffled(seed), 0);
    if (!result.accepted)
        return false;
    processEvents(result.events);
    if (m_chaosActive && m_chaosDisappearOnDraw)
        for (int seat = 0; seat < kPlayerCount; ++seat)
            maybeVanishOnDeal(seat);
    return true;
}

CommandResult LocalSession::executeCommand(const GameCommand &command)
{
    if (!isValidSeat(command.seat))
        return CommandResult::rejected(GameError::InvalidSeat);
    if (m_playerTypes[command.seat] == PlayerType::AI)
        return CommandResult::rejected(GameError::NotCurrentPlayer);
    const CommandResult result = m_engine.execute(command);
    if (result.accepted)
        processEvents(result.events);
    return result;
}

GamePhase LocalSession::currentPhase() const
{
    return m_engine.state().phase;
}

int LocalSession::currentSeat() const
{
    return m_engine.state().currentSeat;
}

PlayerType LocalSession::currentPlayerType() const
{
    const int seat = currentSeat();
    if (!isValidSeat(seat))
        return PlayerType::User;
    return m_playerTypes[seat];
}

GameState LocalSession::projectedStateFor(int seat) const
{
    return m_engine.projectedStateFor(seat);
}

const GameState &LocalSession::fullState() const
{
    return m_engine.state();
}

void LocalSession::setAIEnabled(bool enabled)
{
    m_aiEnabled = enabled;
}

void LocalSession::triggerAIMove()
{
    const int seat = currentSeat();
    if (!isValidSeat(seat))
        return;

    if (m_playerTypes[seat] == PlayerType::AI) {
        executeAIMove(seat);
    }
}

void LocalSession::processEvents(const QVector<GameEvent> &events)
{
    for (const GameEvent &event : events) {
        if (event.type == GameEventType::RoundStarted) {
            if (auto *timer = findChild<QTimer *>(QStringLiteral("localSessionRedeal"), Qt::FindDirectChildrenOnly))
                timer->stop();
        }
        emit eventEmitted(event);
        if (event.type == GameEventType::RoundFinished)
            emit roundFinished(event.seat);
        if (event.type == GameEventType::RoundVoided) {
            auto *timer = findChild<QTimer *>(QStringLiteral("localSessionRedeal"), Qt::FindDirectChildrenOnly);
            if (!timer) {
                timer = new QTimer(this);
                timer->setObjectName(QStringLiteral("localSessionRedeal"));
                timer->setSingleShot(true);
                connect(timer, &QTimer::timeout, this, [this] {
                    if (currentPhase() == GamePhase::Waiting)
                        startRound();
                });
            }
            timer->start(kAIMoveDelayMs);
        }
    }
    scheduleAIMove();
}

void LocalSession::scheduleAIMove()
{
    // One owned, reusable timer prevents duplicate moves and cancels an old
    // callback after manual stepping, a restart, or a change to a user turn.
    auto *timer = findChild<QTimer *>(QStringLiteral("localSessionAI"), Qt::FindDirectChildrenOnly);
    if (timer)
        timer->stop();
    const int seat = currentSeat();
    const GamePhase phase = currentPhase();
    if (!m_aiEnabled || !isValidSeat(seat) || m_playerTypes[seat] != PlayerType::AI
        || (phase != GamePhase::CallingLord && phase != GamePhase::Playing))
        return;
    if (!timer) {
        timer = new QTimer(this);
        timer->setObjectName(QStringLiteral("localSessionAI"));
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, this, [this] {
            if (m_aiEnabled)
                executeAIMove(currentSeat());
        });
    }
    timer->start(kAIMoveDelayMs);
}

void LocalSession::executeAIMove(int seat)
{
    if (!isValidSeat(seat) || seat != currentSeat() || m_playerTypes[seat] != PlayerType::AI
        || (currentPhase() != GamePhase::CallingLord && currentPhase() != GamePhase::Playing))
        return;
    CommandResult result;
    if (m_chaosActive && m_chaosTransformOnPlay && currentPhase() == GamePhase::Playing) {
        GameCommand command = AIPolicy::decideCommand(m_engine, seat);
        if (command.type == GameCommandType::PlayCards)
            command.cards = maybeTransformPlay(seat, command.cards);
        result = m_engine.execute(command);
        // A transformed combination may be illegal; the rejected command has
        // no state effect, so the regular policy can safely supply a fallback.
        if (!result.accepted)
            result = AIPolicy::performMove(m_engine, seat);
    } else {
        result = AIPolicy::performMove(m_engine, seat);
    }
    if (result.accepted)
        processEvents(result.events);
}

void LocalSession::setChaosParams(bool active, double disappearChance,
                                  double transformChance, bool disappearOnDraw,
                                  bool transformOnPlay, bool disappearOnIdle)
{
    m_chaosActive = active;
    m_chaosDisappearChance = disappearChance;
    m_chaosTransformChance = transformChance;
    m_chaosDisappearOnDraw = disappearOnDraw;
    m_chaosTransformOnPlay = transformOnPlay;
    m_chaosDisappearOnIdle = disappearOnIdle;
}

bool LocalSession::rollChaos(double chance) const
{
    if (chance <= 0.0) return false;
    if (chance >= 1.0) return true;
    return QRandomGenerator::global()->bounded(10000) < static_cast<int>(chance * 10000);
}

bool LocalSession::vanishRandomCard(int seat)
{
    if (!m_chaosActive || !isValidSeat(seat))
        return false;

    const CardList list = m_engine.state().players[seat].hand.toCardList(Cards::NoSort);
    if (list.isEmpty())
        return false;

    const Card victim = list[QRandomGenerator::global()->bounded(list.size())];
    Cards toVanish;
    toVanish.add(victim);

    const CommandResult result = m_engine.execute(GameCommand::chaosVanish(seat, toVanish));
    if (result.accepted) {
        processEvents(result.events);
        return true;
    }
    return false;
}

bool LocalSession::tryIdleVanish(int seat)
{
    if (!m_chaosActive || !m_chaosDisappearOnIdle || !isValidSeat(seat))
        return false;
    if (!rollChaos(m_chaosDisappearChance))
        return false;
    return vanishRandomCard(seat);
}

void LocalSession::maybeVanishOnDeal(int seat)
{
    if (!isValidSeat(seat))
        return;

    const CardList list = m_engine.state().players[seat].hand.toCardList(Cards::NoSort);
    Cards toVanish;
    for (const Card &c : list) {
        // Independent per-card roll (the onDraw disappearance rule).
        if (rollChaos(m_chaosDisappearChance))
            toVanish.add(c);
    }
    if (toVanish.isEmpty())
        return;

    const CommandResult result = m_engine.execute(GameCommand::chaosVanish(seat, toVanish));
    if (result.accepted)
        processEvents(result.events);
}

Cards LocalSession::transformPlayerPlay(const Cards &intended)
{
    // Player shares the same authoritative transform as the AI. The human is the
    // current seat when this runs (called from onBtnPlay on the player's turn);
    // the transformed cards are then played as a normal command (executeCommand
    // does not re-transform, so no double-apply).
    return maybeTransformPlay(currentSeat(), intended);
}

Cards LocalSession::maybeTransformPlay(int seat, const Cards &intended)
{
    if (!m_chaosActive || !m_chaosTransformOnPlay || intended.isEmpty()
        || !isValidSeat(seat))
        return intended;

    if (!rollChaos(m_chaosTransformChance))
        return intended;

    // Pick a random played card to mutate, and a random replacement from the
    // rest of the hand (not already among the played cards).
    CardList outList = intended.toCardList(Cards::NoSort);
    const CardList hand = m_engine.state().players[seat].hand.toCardList(Cards::NoSort);
    if (outList.isEmpty() || hand.isEmpty())
        return intended;

    const int outIdx = QRandomGenerator::global()->bounded(outList.size());
    const Card from = outList[outIdx];

    Card to;
    bool found = false;
    for (int attempt = 0; attempt < 20; ++attempt) {
        const Card cand = hand[QRandomGenerator::global()->bounded(hand.size())];
        if (!intended.contains(cand)) {
            to = cand;
            found = true;
            break;
        }
    }
    if (!found)
        return intended;

    outList[outIdx] = to;
    Cards result;
    for (const Card &c : outList)
        result.add(c);

    emit chaosCardsTransformed(seat, from, to);
    return result;
}
