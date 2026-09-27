#include "game_core/deck.h"
#include "game_core/game_engine.h"
#include "test_support.h"

#include <cstdlib>

class GameEngineTestAccess
{
public:
    static GameState &state(GameEngine &engine)
    {
        return engine.m_state;
    }
};

GameEngine startedEngine()
{
    GameEngine engine;
    const CommandResult result = engine.startRound(Deck::shuffled(42u), 0);
    if (!result.accepted)
        std::abort();
    return engine;
}

GameEngine playingEngine()
{
    GameEngine engine = startedEngine();
    if (!engine.execute(GameCommand::callLord(0, 3)).accepted)
        std::abort();
    return engine;
}

Cards oneCardFrom(const GameEngine &engine, int seat)
{
    const Card card = engine.state().players[seat].hand.toCardList(Cards::Asc).first();
    return Cards(card);
}

bool testStartRoundDeals17CardsAnd3BottomCards()
{
    const GameEngine engine = startedEngine();
    const GameState &state = engine.state();
    EXPECT_EQ(state.phase, GamePhase::CallingLord);
    EXPECT_EQ(state.players[0].hand.cardCount(), 17);
    EXPECT_EQ(state.players[1].hand.cardCount(), 17);
    EXPECT_EQ(state.players[2].hand.cardCount(), 17);
    EXPECT_EQ(state.bottomCards.cardCount(), 3);
    EXPECT_EQ(state.currentSeat, 0);
    return true;
}

bool testOnlyCurrentPlayerMayBid()
{
    GameEngine engine = startedEngine();
    const GameState before = engine.state();
    const CommandResult result = engine.execute(GameCommand::callLord(1, 1));
    EXPECT_TRUE(!result.accepted);
    EXPECT_EQ(result.error, GameError::NotCurrentPlayer);
    EXPECT_TRUE(engine.state().players[0].hand == before.players[0].hand);
    EXPECT_EQ(engine.state().currentSeat, before.currentSeat);
    return true;
}

bool testBidThreeSelectsLordImmediately()
{
    GameEngine engine = startedEngine();
    const CommandResult result = engine.execute(GameCommand::callLord(0, 3));
    EXPECT_TRUE(result.accepted);
    EXPECT_EQ(engine.state().phase, GamePhase::Playing);
    EXPECT_EQ(engine.state().players[0].role, PlayerRole::Lord);
    EXPECT_EQ(engine.state().players[0].hand.cardCount(), 20);
    EXPECT_EQ(engine.state().multiplier, 3);
    EXPECT_EQ(engine.state().currentSeat, 0);
    return true;
}

bool testHighestBidderBecomesLordAfterThreeBids()
{
    GameEngine engine = startedEngine();
    EXPECT_TRUE(engine.execute(GameCommand::callLord(0, 1)).accepted);
    EXPECT_TRUE(engine.execute(GameCommand::callLord(1, 2)).accepted);
    EXPECT_TRUE(engine.execute(GameCommand::callLord(2, 0)).accepted);
    EXPECT_EQ(engine.state().phase, GamePhase::Playing);
    EXPECT_EQ(engine.state().players[1].role, PlayerRole::Lord);
    EXPECT_EQ(engine.state().multiplier, 2);
    return true;
}

bool testThreePassesVoidTheRound()
{
    GameEngine engine = startedEngine();
    const int beforeRound = engine.state().roundNumber;
    EXPECT_TRUE(engine.execute(GameCommand::callLord(0, 0)).accepted);
    EXPECT_TRUE(engine.execute(GameCommand::callLord(1, 0)).accepted);
    const CommandResult result = engine.execute(GameCommand::callLord(2, 0));
    EXPECT_TRUE(result.accepted);
    EXPECT_EQ(engine.state().phase, GamePhase::Waiting);
    EXPECT_EQ(engine.state().currentSeat, kInvalidSeat);
    EXPECT_EQ(engine.state().roundNumber, beforeRound - 1);
    return true;
}

bool testPlayRejectsCardsNotOwnedWithoutMutation()
{
    GameEngine engine = playingEngine();
    const GameState before = engine.state();
    Cards foreign(Card(Card::Card_BJ, Card::Diamond));
    const CommandResult result = engine.execute(GameCommand::playCards(0, foreign));
    EXPECT_TRUE(!result.accepted);
    EXPECT_EQ(result.error, GameError::CardsNotOwned);
    EXPECT_TRUE(engine.state().players[0].hand == before.players[0].hand);
    EXPECT_EQ(engine.state().currentSeat, before.currentSeat);
    return true;
}

bool testValidPlayRemovesCardsAndAdvancesTurn()
{
    GameEngine engine = playingEngine();
    const Cards played = oneCardFrom(engine, 0);
    const int beforeCount = engine.state().players[0].hand.cardCount();
    const CommandResult result = engine.execute(GameCommand::playCards(0, played));
    EXPECT_TRUE(result.accepted);
    EXPECT_EQ(engine.state().players[0].hand.cardCount(), beforeCount - 1);
    EXPECT_TRUE(engine.state().pendingCards == played);
    EXPECT_EQ(engine.state().pendingSeat, 0);
    EXPECT_EQ(engine.state().currentSeat, 1);
    return true;
}

bool testLeaderCannotPass()
{
    GameEngine engine = playingEngine();
    const CommandResult result = engine.execute(GameCommand::pass(0));
    EXPECT_TRUE(!result.accepted);
    EXPECT_EQ(result.error, GameError::CannotPassWhenLeading);
    return true;
}

bool testTwoPassesClearPendingAndReturnLead()
{
    GameEngine engine = playingEngine();
    const Cards played = oneCardFrom(engine, 0);
    EXPECT_TRUE(engine.execute(GameCommand::playCards(0, played)).accepted);
    EXPECT_TRUE(engine.execute(GameCommand::pass(1)).accepted);
    EXPECT_TRUE(engine.execute(GameCommand::pass(2)).accepted);
    EXPECT_TRUE(engine.state().pendingCards.isEmpty());
    EXPECT_EQ(engine.state().pendingSeat, kInvalidSeat);
    EXPECT_EQ(engine.state().currentSeat, 0);
    return true;
}

bool testLordWinsAtMultiplierThree()
{
    GameEngine engine;
    GameState &state = GameEngineTestAccess::state(engine);

    // Setup minimal valid Playing state with valid deck
    const CardList deck = Deck::shuffled(100u);
    state.phase = GamePhase::Playing;
    state.roundNumber = 1;
    state.multiplier = 3;
    state.currentSeat = 0;

    // Roles: seat 0 is lord
    state.players[0].role = PlayerRole::Lord;
    state.players[1].role = PlayerRole::Farmer;
    state.players[2].role = PlayerRole::Farmer;

    // Distribute 54 cards: 1 in seat 0 hand, rest distributed
    state.players[0].hand.add(deck[0]);
    for (int i = 1; i < 27; ++i)
        state.players[1].hand.add(deck[i]);
    for (int i = 27; i < 54; ++i)
        state.players[2].hand.add(deck[i]);

    // This test isolates BASE multiplier scoring, so suppress 春天: mark a
    // farmer as having already played, otherwise "lord wins while no farmer
    // ever played" triggers spring doubling (covered separately below).
    state.players[1].playsMade = 1;

    // Lord plays last card and wins
    const Cards lastCard(deck[0]);
    const CommandResult result = engine.execute(GameCommand::playCards(0, lastCard));
    EXPECT_TRUE(result.accepted);
    EXPECT_EQ(state.phase, GamePhase::RoundFinished);
    EXPECT_EQ(state.winnerSeat, 0);
    EXPECT_EQ(state.players[0].score, 6);   // +2*3
    EXPECT_EQ(state.players[1].score, -3);  // -3
    EXPECT_EQ(state.players[2].score, -3);  // -3
    return true;
}

bool testFarmerWinsAtMultiplierTwo()
{
    GameEngine engine;
    GameState &state = GameEngineTestAccess::state(engine);

    const CardList deck = Deck::shuffled(200u);
    state.phase = GamePhase::Playing;
    state.roundNumber = 1;
    state.multiplier = 2;
    state.currentSeat = 1;

    // Roles: seat 0 is lord
    state.players[0].role = PlayerRole::Lord;
    state.players[1].role = PlayerRole::Farmer;
    state.players[2].role = PlayerRole::Farmer;

    // Give seat 1 (farmer) one card to play
    state.players[1].hand.add(deck[0]);
    for (int i = 1; i < 27; ++i)
        state.players[0].hand.add(deck[i]);
    for (int i = 27; i < 54; ++i)
        state.players[2].hand.add(deck[i]);

    // This test isolates BASE multiplier scoring, so suppress 反春天: mark the
    // lord as having played more than once, otherwise "farmer wins while the
    // lord played at most once" triggers anti-spring doubling (covered below).
    state.players[0].playsMade = 2;

    // Farmer plays last card and wins
    const Cards lastCard(deck[0]);
    const CommandResult result = engine.execute(GameCommand::playCards(1, lastCard));
    EXPECT_TRUE(result.accepted);
    EXPECT_EQ(state.phase, GamePhase::RoundFinished);
    EXPECT_EQ(state.winnerSeat, 1);
    EXPECT_EQ(state.players[0].score, -4);  // -2*2
    EXPECT_EQ(state.players[1].score, 2);   // +2
    EXPECT_EQ(state.players[2].score, 2);   // +2
    return true;
}

bool testSpringDoublesWhenLordWinsAndFarmersNeverPlayed()
{
    // 春天：地主获胜且两个农民全程未出过一张牌 → 倍数翻倍。
    GameEngine engine;
    GameState &state = GameEngineTestAccess::state(engine);

    const CardList deck = Deck::shuffled(400u);
    state.phase = GamePhase::Playing;
    state.roundNumber = 1;
    state.multiplier = 3;
    state.currentSeat = 0;

    state.players[0].role = PlayerRole::Lord;
    state.players[1].role = PlayerRole::Farmer;
    state.players[2].role = PlayerRole::Farmer;

    state.players[0].hand.add(deck[0]);
    for (int i = 1; i < 27; ++i)
        state.players[1].hand.add(deck[i]);
    for (int i = 27; i < 54; ++i)
        state.players[2].hand.add(deck[i]);

    // 两个农民 playsMade 均为 0（默认）→ 触发春天。
    const Cards lastCard(deck[0]);
    const CommandResult result = engine.execute(GameCommand::playCards(0, lastCard));
    EXPECT_TRUE(result.accepted);
    EXPECT_EQ(state.phase, GamePhase::RoundFinished);
    EXPECT_EQ(state.winnerSeat, 0);
    // 倍数 3 → 春天翻倍为 6。地主 +2*6=12，每个农民 -6。
    EXPECT_EQ(state.players[0].score, 12);
    EXPECT_EQ(state.players[1].score, -6);
    EXPECT_EQ(state.players[2].score, -6);
    return true;
}

bool testAntiSpringDoublesWhenFarmerWinsAndLordPlayedOnce()
{
    // 反春天：农民获胜且地主只出过开局那一手（playsMade <= 1）→ 倍数翻倍。
    GameEngine engine;
    GameState &state = GameEngineTestAccess::state(engine);

    const CardList deck = Deck::shuffled(500u);
    state.phase = GamePhase::Playing;
    state.roundNumber = 1;
    state.multiplier = 2;
    state.currentSeat = 1;

    state.players[0].role = PlayerRole::Lord;
    state.players[1].role = PlayerRole::Farmer;
    state.players[2].role = PlayerRole::Farmer;

    state.players[1].hand.add(deck[0]);
    for (int i = 1; i < 27; ++i)
        state.players[0].hand.add(deck[i]);
    for (int i = 27; i < 54; ++i)
        state.players[2].hand.add(deck[i]);

    // 地主 playsMade 默认 0（<=1）→ 农民获胜触发反春天。
    const Cards lastCard(deck[0]);
    const CommandResult result = engine.execute(GameCommand::playCards(1, lastCard));
    EXPECT_TRUE(result.accepted);
    EXPECT_EQ(state.phase, GamePhase::RoundFinished);
    EXPECT_EQ(state.winnerSeat, 1);
    // 倍数 2 → 反春天翻倍为 4。地主 -2*4=-8，每个农民 +4。
    EXPECT_EQ(state.players[0].score, -8);
    EXPECT_EQ(state.players[1].score, 4);
    EXPECT_EQ(state.players[2].score, 4);
    return true;
}

bool testScoresAreZeroSum()
{
    GameEngine engine;
    GameState &state = GameEngineTestAccess::state(engine);

    const CardList deck = Deck::shuffled(300u);
    state.phase = GamePhase::Playing;
    state.roundNumber = 1;
    state.multiplier = 4;
    state.currentSeat = 2;

    state.players[0].role = PlayerRole::Lord;
    state.players[1].role = PlayerRole::Farmer;
    state.players[2].role = PlayerRole::Farmer;

    state.players[2].hand.add(deck[0]);
    for (int i = 1; i < 27; ++i)
        state.players[0].hand.add(deck[i]);
    for (int i = 27; i < 54; ++i)
        state.players[1].hand.add(deck[i]);

    const Cards lastCard(deck[0]);
    const CommandResult result = engine.execute(GameCommand::playCards(2, lastCard));
    EXPECT_TRUE(result.accepted);

    const int sum = state.players[0].score + state.players[1].score + state.players[2].score;
    EXPECT_EQ(sum, 0);
    return true;
}

bool testCumulativeScoresSurviveNextRound()
{
    GameEngine engine;
    GameState &state = GameEngineTestAccess::state(engine);

    const CardList deck = Deck::shuffled(400u);
    state.phase = GamePhase::Playing;
    state.roundNumber = 1;
    state.currentSeat = 0;
    state.players[0].role = PlayerRole::Lord;
    state.players[1].role = PlayerRole::Farmer;
    state.players[2].role = PlayerRole::Farmer;
    state.multiplier = 1;

    state.players[0].hand.add(deck[0]);
    for (int i = 1; i < 27; ++i)
        state.players[1].hand.add(deck[i]);
    for (int i = 27; i < 54; ++i)
        state.players[2].hand.add(deck[i]);

    // Lord wins
    const Cards lastCard(deck[0]);
    EXPECT_TRUE(engine.execute(GameCommand::playCards(0, lastCard)).accepted);
    EXPECT_EQ(state.phase, GamePhase::RoundFinished);

    const int score0 = state.players[0].score;
    const int score1 = state.players[1].score;
    const int score2 = state.players[2].score;

    // Start new round
    const CommandResult result = engine.startRound(Deck::shuffled(99u), 1);
    EXPECT_TRUE(result.accepted);

    // Scores should be preserved
    EXPECT_EQ(state.players[0].score, score0);
    EXPECT_EQ(state.players[1].score, score1);
    EXPECT_EQ(state.players[2].score, score2);

    // Other state should be reset
    EXPECT_EQ(state.phase, GamePhase::CallingLord);
    EXPECT_EQ(state.multiplier, 1);
    EXPECT_EQ(state.winnerSeat, kInvalidSeat);
    EXPECT_TRUE(state.pendingCards.isEmpty());
    EXPECT_EQ(state.highestBid, 0);
    return true;
}

bool testValidateStateAfterStart()
{
    const GameEngine engine = startedEngine();
    EXPECT_EQ(engine.validateState(), GameError::None);
    return true;
}

bool testValidateStateAfterBid()
{
    GameEngine engine = startedEngine();
    EXPECT_TRUE(engine.execute(GameCommand::callLord(0, 1)).accepted);
    EXPECT_EQ(engine.validateState(), GameError::None);
    return true;
}

bool testValidateStateAfterPlay()
{
    GameEngine engine = playingEngine();
    const Cards played = oneCardFrom(engine, 0);
    EXPECT_TRUE(engine.execute(GameCommand::playCards(0, played)).accepted);
    EXPECT_EQ(engine.validateState(), GameError::None);
    return true;
}

bool testValidateStateAfterPass()
{
    GameEngine engine = playingEngine();
    const Cards played = oneCardFrom(engine, 0);
    EXPECT_TRUE(engine.execute(GameCommand::playCards(0, played)).accepted);
    EXPECT_TRUE(engine.execute(GameCommand::pass(1)).accepted);
    EXPECT_EQ(engine.validateState(), GameError::None);
    return true;
}

bool testValidateStateAfterRoundFinish()
{
    GameEngine engine;
    GameState &state = GameEngineTestAccess::state(engine);

    const CardList deck = Deck::shuffled(500u);
    state.phase = GamePhase::Playing;
    state.roundNumber = 1;
    state.multiplier = 1;
    state.currentSeat = 0;
    state.players[0].role = PlayerRole::Lord;
    state.players[1].role = PlayerRole::Farmer;
    state.players[2].role = PlayerRole::Farmer;

    state.players[0].hand.add(deck[0]);
    for (int i = 1; i < 27; ++i)
        state.players[1].hand.add(deck[i]);
    for (int i = 27; i < 54; ++i)
        state.players[2].hand.add(deck[i]);

    const Cards lastCard(deck[0]);
    EXPECT_TRUE(engine.execute(GameCommand::playCards(0, lastCard)).accepted);
    EXPECT_EQ(engine.validateState(), GameError::None);
    return true;
}

bool testValidateStateRejectsDuplicateCards()
{
    GameEngine engine = startedEngine();
    GameState &state = GameEngineTestAccess::state(engine);

    // Corrupt state by duplicating a card
    const Card dupCard = state.players[0].hand.toCardList(Cards::Asc).first();
    state.players[1].hand.add(dupCard);

    EXPECT_EQ(engine.validateState(), GameError::InvalidDeck);
    return true;
}

bool testProjectionPreservesOwnHand()
{
    const GameEngine engine = playingEngine();
    const GameState &full = engine.state();
    const GameState projected = engine.projectedStateFor(0);

    EXPECT_TRUE(projected.players[0].hand == full.players[0].hand);
    EXPECT_EQ(projected.players[0].hand.cardCount(), 20);
    return true;
}

bool testProjectionHidesOtherHands()
{
    const GameEngine engine = playingEngine();
    const GameState projected = engine.projectedStateFor(0);

    EXPECT_TRUE(projected.players[1].hand.isEmpty());
    EXPECT_TRUE(projected.players[2].hand.isEmpty());
    return true;
}

bool testProjectionPreservesRevealedBottomCards()
{
    const GameEngine engine = playingEngine();
    const GameState &full = engine.state();
    const GameState projected = engine.projectedStateFor(0);

    EXPECT_TRUE(projected.revealedBottomCards == full.revealedBottomCards);
    EXPECT_EQ(projected.revealedBottomCards.cardCount(), 3);
    return true;
}

bool testProjectionHidesUnrevealedBottomCards()
{
    const GameEngine engine = startedEngine();
    const GameState &full = engine.state();
    const GameState projected = engine.projectedStateFor(0);

    EXPECT_EQ(full.bottomCards.cardCount(), 3);
    EXPECT_TRUE(projected.bottomCards.isEmpty());
    return true;
}

bool testProjectionPreservesPublicFields()
{
    GameEngine engine = playingEngine();
    const Cards played = oneCardFrom(engine, 0);
    engine.execute(GameCommand::playCards(0, played));

    const GameState &full = engine.state();
    const GameState projected = engine.projectedStateFor(1);

    EXPECT_EQ(projected.phase, full.phase);
    EXPECT_EQ(projected.currentSeat, full.currentSeat);
    EXPECT_EQ(projected.pendingSeat, full.pendingSeat);
    EXPECT_EQ(projected.multiplier, full.multiplier);
    EXPECT_EQ(projected.roundNumber, full.roundNumber);
    EXPECT_EQ(projected.players[0].role, full.players[0].role);
    EXPECT_EQ(projected.players[1].role, full.players[1].role);
    EXPECT_EQ(projected.players[2].role, full.players[2].role);
    EXPECT_EQ(projected.players[0].score, full.players[0].score);
    EXPECT_EQ(projected.players[1].score, full.players[1].score);
    EXPECT_EQ(projected.players[2].score, full.players[2].score);
    return true;
}

bool testProjectionPreservesPlayedAndPendingCards()
{
    GameEngine engine = playingEngine();
    const Cards played = oneCardFrom(engine, 0);
    engine.execute(GameCommand::playCards(0, played));

    const GameState &full = engine.state();
    const GameState projected = engine.projectedStateFor(1);

    EXPECT_TRUE(projected.playedCards == full.playedCards);
    EXPECT_TRUE(projected.pendingCards == full.pendingCards);
    return true;
}

bool testProjectionRejectsInvalidSeat()
{
    const GameEngine engine = playingEngine();
    const GameState projected = engine.projectedStateFor(-1);

    EXPECT_EQ(projected.phase, GamePhase::Waiting);
    EXPECT_EQ(projected.roundNumber, 0);
    return true;
}

int main()
{
    const TestCase tests[] = {
        {"Start round deals cards", testStartRoundDeals17CardsAnd3BottomCards},
        {"Only current player bids", testOnlyCurrentPlayerMayBid},
        {"Bid three selects lord", testBidThreeSelectsLordImmediately},
        {"Highest bidder becomes lord", testHighestBidderBecomesLordAfterThreeBids},
        {"All pass voids round", testThreePassesVoidTheRound},
        {"Play rejects cards not owned", testPlayRejectsCardsNotOwnedWithoutMutation},
        {"Valid play removes cards and advances", testValidPlayRemovesCardsAndAdvancesTurn},
        {"Leader cannot pass", testLeaderCannotPass},
        {"Two passes clear pending", testTwoPassesClearPendingAndReturnLead},
        {"Lord wins at multiplier 3", testLordWinsAtMultiplierThree},
        {"Farmer wins at multiplier 2", testFarmerWinsAtMultiplierTwo},
        {"Spring doubles when lord wins and farmers never played", testSpringDoublesWhenLordWinsAndFarmersNeverPlayed},
        {"Anti-spring doubles when farmer wins and lord played once", testAntiSpringDoublesWhenFarmerWinsAndLordPlayedOnce},
        {"Scores are zero sum", testScoresAreZeroSum},
        {"Cumulative scores survive next round", testCumulativeScoresSurviveNextRound},
        {"Validate state after start", testValidateStateAfterStart},
        {"Validate state after bid", testValidateStateAfterBid},
        {"Validate state after play", testValidateStateAfterPlay},
        {"Validate state after pass", testValidateStateAfterPass},
        {"Validate state after round finish", testValidateStateAfterRoundFinish},
        {"Validate state rejects duplicates", testValidateStateRejectsDuplicateCards},
        {"Projection preserves own hand", testProjectionPreservesOwnHand},
        {"Projection hides other hands", testProjectionHidesOtherHands},
        {"Projection preserves revealed bottom", testProjectionPreservesRevealedBottomCards},
        {"Projection hides unrevealed bottom", testProjectionHidesUnrevealedBottomCards},
        {"Projection preserves public fields", testProjectionPreservesPublicFields},
        {"Projection preserves played and pending", testProjectionPreservesPlayedAndPendingCards},
        {"Projection rejects invalid seat", testProjectionRejectsInvalidSeat},
    };
    return runTests(tests, static_cast<int>(std::size(tests)));
}
