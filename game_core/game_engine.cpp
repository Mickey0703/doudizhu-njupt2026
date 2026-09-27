#include "game_engine.h"
#include "deck.h"
#include "playhand.h"

const GameState &GameEngine::state() const
{
    return m_state;
}

CommandResult GameEngine::startRound(const CardList &deck, int firstSeat)
{
    if (!isValidSeat(firstSeat))
        return CommandResult::rejected(GameError::InvalidSeat);
    if (!Deck::isValid(deck))
        return CommandResult::rejected(GameError::InvalidDeck);

    GameState fresh;
    fresh.roundNumber = m_state.roundNumber + 1;
    fresh.phase = GamePhase::CallingLord;
    fresh.currentSeat = firstSeat;
    for (int seat = 0; seat < kPlayerCount; ++seat) {
        fresh.players[seat].score = m_state.players[seat].score;
        fresh.players[seat].connected = m_state.players[seat].connected;
    }
    // Deal cyclically in fixed seat order; firstSeat controls bidding only.
    for (int i = 0; i < 51; ++i)
        fresh.players[i % kPlayerCount].hand.add(deck[i]);
    for (int i = 51; i < 54; ++i)
        fresh.bottomCards.add(deck[i]);
    for (auto &player : fresh.players)
        player.handCount = player.hand.cardCount();
    m_state = fresh;

    CommandResult result;
    result.accepted = true;
    result.events.append({GameEventType::RoundStarted, firstSeat, m_state.roundNumber, {}});
    for (int seat = 0; seat < kPlayerCount; ++seat)
        result.events.append({GameEventType::PrivateHandDealt, seat, 17, m_state.players[seat].hand});
    result.events.append({GameEventType::TurnChanged, firstSeat, 0, {}});
    return result;
}

CommandResult GameEngine::execute(const GameCommand &command)
{
    if (m_state.phase == GamePhase::RoundFinished)
        return CommandResult::rejected(GameError::RoundAlreadyFinished);

    CommandResult result;
    switch (command.type) {
    case GameCommandType::CallLord:
        result = executeCallLord(command);
        break;
    case GameCommandType::PlayCards:
        result = executePlayCards(command);
        break;
    case GameCommandType::Pass:
        result = executePass(command);
        break;
    case GameCommandType::ChaosVanish:
        result = executeChaosVanish(command);
        break;
    default:
        return CommandResult::rejected(GameError::InvalidPhase);
    }

    if (result.accepted) {
        Q_ASSERT(validateState() == GameError::None);
    }
    return result;
}

CommandResult GameEngine::executeCallLord(const GameCommand &command)
{
    if (m_state.phase != GamePhase::CallingLord)
        return CommandResult::rejected(GameError::InvalidPhase);
    if (!isValidSeat(command.seat))
        return CommandResult::rejected(GameError::InvalidSeat);
    if (command.seat != m_state.currentSeat)
        return CommandResult::rejected(GameError::NotCurrentPlayer);
    if (command.bid < 0 || command.bid > 3)
        return CommandResult::rejected(GameError::InvalidBid);
    if (command.bid != 0 && command.bid <= m_state.highestBid)
        return CommandResult::rejected(GameError::BidNotHighEnough);

    CommandResult result;
    result.accepted = true;
    ++m_state.bidCount;
    if (command.bid > 0) {
        m_state.highestBid = command.bid;
        m_state.highestBidder = command.seat;
    }
    result.events.append({GameEventType::BidAccepted, command.seat, command.bid, {}});
    if (command.bid == 3 || (m_state.bidCount == kPlayerCount && m_state.highestBid > 0)) {
        selectLord(m_state.highestBidder, m_state.highestBid, &result.events);
    } else if (m_state.bidCount == kPlayerCount) {
        // A void deal does not count as a completed/numbered round.
        GameState waiting;
        waiting.roundNumber = m_state.roundNumber - 1;
        for (int seat = 0; seat < kPlayerCount; ++seat) {
            waiting.players[seat].score = m_state.players[seat].score;
            waiting.players[seat].connected = m_state.players[seat].connected;
        }
        m_state = waiting;
        result.events.append({GameEventType::RoundVoided, kInvalidSeat, m_state.roundNumber, {}});
    } else {
        m_state.currentSeat = nextSeat(command.seat);
        result.events.append({GameEventType::TurnChanged, m_state.currentSeat, 0, {}});
    }
    return result;
}

CommandResult GameEngine::executePlayCards(const GameCommand &command)
{
    if (m_state.phase != GamePhase::Playing)
        return CommandResult::rejected(GameError::InvalidPhase);
    if (!isValidSeat(command.seat))
        return CommandResult::rejected(GameError::InvalidSeat);
    if (command.seat != m_state.currentSeat)
        return CommandResult::rejected(GameError::NotCurrentPlayer);
    if (!m_state.players[command.seat].hand.contains(command.cards))
        return CommandResult::rejected(GameError::CardsNotOwned);
    const PlayHand hand(command.cards);
    const auto type = hand.getHandType();
    if (type == PlayHand::Hand_Unknown || type == PlayHand::Hand_Pass)
        return CommandResult::rejected(GameError::InvalidHand);
    if (!m_state.pendingCards.isEmpty() && !hand.canBeat(PlayHand(m_state.pendingCards)))
        return CommandResult::rejected(GameError::HandDoesNotBeatPending);

    auto &player = m_state.players[command.seat];
    player.hand.remove(command.cards);
    player.handCount = player.hand.cardCount();
    ++player.playsMade;
    m_state.pendingCards = command.cards;
    m_state.pendingSeat = command.seat;
    m_state.playedCards.add(command.cards);
    m_state.passCount = 0;

    CommandResult result;
    result.accepted = true;
    result.events.append({GameEventType::CardsPlayed, command.seat, player.handCount, command.cards});
    if (type == PlayHand::Hand_Bomb || type == PlayHand::Hand_Bomb_Jokers) {
        m_state.multiplier *= 2;
        result.events.append({GameEventType::MultiplierChanged, command.seat, m_state.multiplier, {}});
    }
    if (player.hand.isEmpty()) {
        finishRound(command.seat, &result.events);
    } else {
        m_state.currentSeat = nextSeat(command.seat);
        result.events.append({GameEventType::TurnChanged, m_state.currentSeat, 0, {}});
    }
    return result;
}

CommandResult GameEngine::executePass(const GameCommand &command)
{
    if (m_state.phase != GamePhase::Playing)
        return CommandResult::rejected(GameError::InvalidPhase);
    if (!isValidSeat(command.seat))
        return CommandResult::rejected(GameError::InvalidSeat);
    if (command.seat != m_state.currentSeat)
        return CommandResult::rejected(GameError::NotCurrentPlayer);
    if (m_state.pendingCards.isEmpty() || m_state.pendingSeat == command.seat)
        return CommandResult::rejected(GameError::CannotPassWhenLeading);

    CommandResult result;
    result.accepted = true;
    result.events.append({GameEventType::PlayerPassed, command.seat, 0, {}});
    if (++m_state.passCount == kPlayerCount - 1) {
        m_state.currentSeat = m_state.pendingSeat;
        m_state.pendingSeat = kInvalidSeat;
        m_state.pendingCards.clear();
        m_state.passCount = 0;
        result.events.append({GameEventType::TrickReset, m_state.currentSeat, 0, {}});
    } else {
        m_state.currentSeat = nextSeat(command.seat);
    }
    result.events.append({GameEventType::TurnChanged, m_state.currentSeat, 0, {}});
    return result;
}

CommandResult GameEngine::executeChaosVanish(const GameCommand &command)
{
    // Chaos mode: remove the given cards from the seat's hand, out of turn.
    // Valid in CallingLord (deal-time disappearance) and Playing (idle timeout).
    if (m_state.phase != GamePhase::CallingLord && m_state.phase != GamePhase::Playing)
        return CommandResult::rejected(GameError::InvalidPhase);
    if (!isValidSeat(command.seat))
        return CommandResult::rejected(GameError::InvalidSeat);
    if (command.cards.isEmpty())
        return CommandResult::rejected(GameError::InvalidHand);
    // The cards must actually be in the seat's hand (the session picks from the
    // authoritative hand, but guard against stale picks / duplicates).
    if (!m_state.players[command.seat].hand.contains(command.cards))
        return CommandResult::rejected(GameError::CardsNotOwned);

    m_state.players[command.seat].hand.remove(command.cards);
    m_state.players[command.seat].handCount = m_state.players[command.seat].hand.cardCount();
    // Move the vanished cards to a side pile so the 54-card / no-duplicate
    // invariant in validateState() stays honest (the cards still exist, just
    // out of play). Without this the assert in execute() would fire.
    m_state.vanishedCards.add(command.cards);

    CommandResult result;
    result.accepted = true;
    result.events.append({GameEventType::CardVanished, command.seat, 0, command.cards});
    return result;
}

void GameEngine::selectLord(int seat, int bid, QVector<GameEvent> *events)
{
    for (int s = 0; s < kPlayerCount; ++s)
        m_state.players[s].role = s == seat ? PlayerRole::Lord : PlayerRole::Farmer;
    m_state.revealedBottomCards = m_state.bottomCards;
    m_state.players[seat].hand.add(m_state.bottomCards);
    m_state.players[seat].handCount = m_state.players[seat].hand.cardCount();
    m_state.bottomCards.clear();
    m_state.multiplier = bid;
    m_state.phase = GamePhase::Playing;
    m_state.currentSeat = seat;
    events->append({GameEventType::LordSelected, seat, bid, m_state.revealedBottomCards});
    events->append({GameEventType::TurnChanged, seat, 0, {}});
}

void GameEngine::finishRound(int winnerSeat, QVector<GameEvent> *events)
{
    const bool lordWon = m_state.players[winnerSeat].role == PlayerRole::Lord;
    bool spring = true;
    for (const auto &player : m_state.players) {
        if (lordWon && player.role == PlayerRole::Farmer && player.playsMade != 0)
            spring = false;
        if (!lordWon && player.role == PlayerRole::Lord && player.playsMade > 1)
            spring = false;
    }
    if (spring) {
        m_state.multiplier *= 2;
        events->append({GameEventType::MultiplierChanged, winnerSeat, m_state.multiplier, {}});
    }
    for (int seat = 0; seat < kPlayerCount; ++seat) {
        const int factor = m_state.players[seat].role == PlayerRole::Lord ? 2 : -1;
        m_state.players[seat].score += factor * m_state.multiplier * (lordWon ? 1 : -1);
        events->append({GameEventType::ScoreChanged, seat, m_state.players[seat].score, {}});
    }
    m_state.winnerSeat = winnerSeat;
    m_state.currentSeat = kInvalidSeat;
    m_state.phase = GamePhase::RoundFinished;
    events->append({GameEventType::RoundFinished, winnerSeat, m_state.multiplier, {}});
}

GameError GameEngine::validateState() const
{
    // Count physical ownership piles only: pending and revealed bottom cards
    // are public views of cards already present in these piles.
    CardList physical;
    for (const auto &player : m_state.players)
        physical += player.hand.toCardList(Cards::Asc);
    physical += m_state.bottomCards.toCardList(Cards::Asc);
    physical += m_state.playedCards.toCardList(Cards::Asc);
    physical += m_state.vanishedCards.toCardList(Cards::Asc);
    if ((!physical.isEmpty() || m_state.phase != GamePhase::Waiting)
        && !Deck::isValid(physical))
        return GameError::InvalidDeck;
    if (!m_state.playedCards.contains(m_state.pendingCards))
        return GameError::InvalidDeck;
    Cards all;
    for (const Card &card : physical)
        all.add(card);
    if (!all.contains(m_state.revealedBottomCards))
        return GameError::InvalidDeck;

    const auto validOrUnset = [](int seat) { return seat == kInvalidSeat || isValidSeat(seat); };
    if (!validOrUnset(m_state.currentSeat) || !validOrUnset(m_state.pendingSeat)
        || !validOrUnset(m_state.highestBidder) || !validOrUnset(m_state.winnerSeat))
        return GameError::InvalidSeat;
    qint64 scoreSum = 0;
    int lords = 0, farmers = 0;
    for (const auto &player : m_state.players) {
        scoreSum += player.score;
        if (player.playsMade < 0)
            return GameError::InvalidPhase;
        if (player.role == PlayerRole::Lord) ++lords;
        else if (player.role == PlayerRole::Farmer) ++farmers;
        else if (player.role != PlayerRole::Unknown) return GameError::InvalidPhase;
    }
    if (scoreSum != 0 || m_state.multiplier < 1 || m_state.roundNumber < 0
        || m_state.highestBid < 0 || m_state.highestBid > 3
        || m_state.bidCount < 0 || m_state.bidCount > kPlayerCount
        || m_state.passCount < 0 || m_state.passCount >= kPlayerCount - 1)
        return GameError::InvalidPhase;
    if ((m_state.highestBid == 0) != (m_state.highestBidder == kInvalidSeat))
        return GameError::InvalidPhase;
    if (m_state.pendingCards.isEmpty() != (m_state.pendingSeat == kInvalidSeat)
        || (m_state.pendingCards.isEmpty() && m_state.passCount != 0))
        return GameError::InvalidPhase;
    if (!m_state.pendingCards.isEmpty()) {
        const auto type = PlayHand(m_state.pendingCards).getHandType();
        if (type == PlayHand::Hand_Unknown || type == PlayHand::Hand_Pass)
            return GameError::InvalidHand;
    }
    switch (m_state.phase) {
    case GamePhase::Waiting:
        if (m_state.currentSeat != kInvalidSeat || m_state.winnerSeat != kInvalidSeat
            || !physical.isEmpty() || lords != 0 || farmers != 0)
            return GameError::InvalidPhase;
        break;
    case GamePhase::CallingLord:
        if (!isValidSeat(m_state.currentSeat)) return GameError::InvalidSeat;
        if (lords != 0 || farmers != 0 || m_state.bottomCards.cardCount() != 3
            || !m_state.revealedBottomCards.isEmpty() || !m_state.playedCards.isEmpty()
            || !m_state.pendingCards.isEmpty() || m_state.bidCount >= kPlayerCount
            || m_state.highestBid == 3 || m_state.winnerSeat != kInvalidSeat)
            return GameError::InvalidPhase;
        break;
    case GamePhase::Playing:
    case GamePhase::RoundFinished:
        if (lords != 1 || farmers != 2 || !m_state.bottomCards.isEmpty())
            return GameError::InvalidPhase;
        if (m_state.phase == GamePhase::Playing) {
            if (!isValidSeat(m_state.currentSeat)) return GameError::InvalidSeat;
            if (m_state.winnerSeat != kInvalidSeat) return GameError::InvalidPhase;
            if (!m_state.pendingCards.isEmpty()
                && m_state.currentSeat != (m_state.passCount == 0
                    ? nextSeat(m_state.pendingSeat) : previousSeat(m_state.pendingSeat)))
                return GameError::InvalidPhase;
        } else {
            if (!isValidSeat(m_state.winnerSeat)) return GameError::InvalidSeat;
            if (m_state.currentSeat != kInvalidSeat
                || !m_state.players[m_state.winnerSeat].hand.isEmpty())
                return GameError::InvalidPhase;
        }
        break;
    case GamePhase::Dealing:
    default:
        // Dealing is not an externally observable engine state.
        return GameError::InvalidPhase;
    }
    return GameError::None;
}

GameState GameEngine::projectedStateFor(int seat) const
{
    if (!isValidSeat(seat))
        return GameState();
    GameState projected = m_state;
    for (int s = 0; s < kPlayerCount; ++s) {
        projected.players[s].handCount = m_state.players[s].hand.cardCount();
        if (s != seat)
            projected.players[s].hand.clear();
    }
    if (m_state.phase != GamePhase::Playing && m_state.phase != GamePhase::RoundFinished) {
        projected.bottomCards.clear();
        projected.revealedBottomCards.clear();
    }
    return projected;
}
