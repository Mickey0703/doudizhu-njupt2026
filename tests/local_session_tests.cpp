#include "game_core/local_session.h"
#include "game_core/deck.h"
#include "player.h"
#include "strategy.h"
#include "cards.h"
#include "test_support.h"

#include <cstdlib>

bool testSessionStartsThreePlayerGame()
{
    LocalSession session;
    session.setPlayerType(0, PlayerType::User);
    session.setPlayerType(1, PlayerType::AI);
    session.setPlayerType(2, PlayerType::AI);

    const bool started = session.startRound();
    EXPECT_TRUE(started);
    EXPECT_EQ(session.currentPhase(), GamePhase::CallingLord);
    return true;
}

bool testSessionAcceptsUserCommands()
{
    LocalSession session;
    session.setPlayerType(0, PlayerType::User);
    session.setPlayerType(1, PlayerType::AI);
    session.setPlayerType(2, PlayerType::AI);
    session.startRound();

    // User at seat 0 bids
    const CommandResult result = session.executeCommand(GameCommand::callLord(0, 1));
    EXPECT_TRUE(result.accepted);
    return true;
}

bool testSessionRejectsCommandsForAISeats()
{
    LocalSession session;
    session.setPlayerType(0, PlayerType::User);
    session.setPlayerType(1, PlayerType::AI);
    session.setPlayerType(2, PlayerType::AI);
    session.startRound();

    // Try to command AI seat directly
    const CommandResult result = session.executeCommand(GameCommand::callLord(1, 1));
    EXPECT_TRUE(!result.accepted);
    return true;
}

bool testSessionTriggersAIDecisions()
{
    LocalSession session;
    session.setPlayerType(0, PlayerType::User);
    session.setPlayerType(1, PlayerType::AI);
    session.setPlayerType(2, PlayerType::AI);
    session.setAIEnabled(false);  // step AI manually, no timer chaining
    session.startRound(12345);    // fixed seed for deterministic behavior

    const int seatBefore = session.currentSeat();
    // User passes turn to AI at seat 1
    session.executeCommand(GameCommand::callLord(0, 0));

    // Manually trigger AI instead of waiting for timer
    session.triggerAIMove();

    // The AI must have acted: either the turn advanced (bidding continues)
    // or the bid concluded and we moved into the Playing phase. A high AI
    // bid can end CallingLord immediately, so we don't hard-code seat 2.
    EXPECT_TRUE(session.currentSeat() != seatBefore
                || session.currentPhase() == GamePhase::Playing);
    return true;
}

bool testSessionCompletesFullRound()
{
    LocalSession session;
    session.setPlayerType(0, PlayerType::User);
    session.setPlayerType(1, PlayerType::AI);
    session.setPlayerType(2, PlayerType::AI);
    session.startRound();

    // Manually trigger AI for bidding phase
    int iterations = 0;
    while (session.currentPhase() == GamePhase::CallingLord && iterations++ < 10) {
        if (session.currentPlayerType() == PlayerType::User) {
            session.executeCommand(GameCommand::callLord(session.currentSeat(), 0));
        } else {
            session.triggerAIMove();
        }
    }

    // Should reach Playing phase or void
    EXPECT_TRUE(session.currentPhase() == GamePhase::Playing
                || session.currentPhase() == GamePhase::Waiting);
    return true;
}

bool testAllAIRoundPlaysToCompletion()
{
    // Regression for the AI dangling-pointer / null-next-player crashes and
    // the leader-cannot-pass deadlock. An all-AI round must drive the whole
    // game (bidding -> playing -> a winner) without crashing or stalling.
    // Runs several seeds so we exercise many hand shapes and follow/lead paths.
    for (quint32 seed = 1; seed <= 200; ++seed) {
        LocalSession session;  // all seats AI by default
        // Drive synchronously: disable timer-based scheduling and step manually.
        session.setAIEnabled(false);
        const bool started = session.startRound(seed);
        EXPECT_TRUE(started);

        int steps = 0;
        while (session.currentPhase() != GamePhase::RoundFinished
               && session.currentPhase() != GamePhase::Waiting
               && steps++ < 500) {
            session.triggerAIMove();
        }

        // The round must terminate (not stall) within a sane step budget.
        EXPECT_TRUE(session.currentPhase() == GamePhase::RoundFinished
                    || session.currentPhase() == GamePhase::Waiting);
    }
    return true;
}

// Helper: add `count` cards of the given point with distinct suits.
static void addTriple(Cards &cards, Card::CardPoint pt, int count)
{
    const Card::CardSuit suits[] = {Card::Diamond, Card::Club, Card::Heart, Card::Spade};
    for (int i = 0; i < count; ++i)
        cards.add(Card(pt, suits[i]));
}

bool testStrategyTripleFollowMoreTriplesThanWings()
{
    // Regression for the OOB in Strategy::getTripleSingleOrPair: the loop
    // paired every triple with cardsArray.at(i) using the triple count as the
    // bound, so when triples outnumber available single/pair wings, at(i) read
    // past the end (Q_ASSERT abort in debug, UB in release). Build a hand with
    // three triples but only one loose single, then follow a triple-single.
    Player self;
    Player next;
    Player prev;
    self.setNextPlayer(&next);
    self.setPrevPlayer(&prev);
    next.setNextPlayer(&prev);
    prev.setNextPlayer(&self);
    self.setRole(Player::Lord);
    next.setRole(Player::Farmer);
    prev.setRole(Player::Farmer);
    next.storeDispatchCard(Cards());  // non-empty next hand not required here

    Cards hand;
    addTriple(hand, Card::Card_9, 3);
    addTriple(hand, Card::Card_10, 3);
    addTriple(hand, Card::Card_J, 3);
    hand.add(Card(Card::Card_4, Card::Spade));  // the single lone wing
    self.storeDispatchCard(hand);

    // Pending: a triple-single led by the opponent at point 8 (888 + 3).
    Cards pending;
    addTriple(pending, Card::Card_8, 3);
    pending.add(Card(Card::Card_3, Card::Heart));
    self.storePendingInfo(&next, pending);

    // Must not crash. Before the fix this aborted/UB inside getGreaterCards.
    Strategy strategy(&self, hand);
    Cards result = strategy.makeStrategy();

    // A valid follow (999+4 style) or an intentional pass are both acceptable;
    // the point of the test is that it returns without crashing.
    EXPECT_TRUE(result.cardCount() == 0 || result.cardCount() == 4);
    return true;
}

bool testSessionProvidesProjectedState()
{
    LocalSession session;
    session.setPlayerType(0, PlayerType::User);
    session.setPlayerType(1, PlayerType::AI);
    session.setPlayerType(2, PlayerType::AI);
    session.startRound();

    const GameState projected = session.projectedStateFor(0);
    EXPECT_EQ(projected.players[0].hand.cardCount(), 17);
    EXPECT_TRUE(projected.players[1].hand.isEmpty());
    EXPECT_TRUE(projected.players[2].hand.isEmpty());
    return true;
}

int main()
{
    const TestCase tests[] = {
        {"Session starts three player game", testSessionStartsThreePlayerGame},
        {"Session accepts user commands", testSessionAcceptsUserCommands},
        {"Session rejects commands for AI seats", testSessionRejectsCommandsForAISeats},
        {"Session triggers AI decisions", testSessionTriggersAIDecisions},
        {"Session completes full round", testSessionCompletesFullRound},
        {"All-AI round plays to completion", testAllAIRoundPlaysToCompletion},
        {"Strategy triple-follow more triples than wings", testStrategyTripleFollowMoreTriplesThanWings},
        {"Session provides projected state", testSessionProvidesProjectedState},
    };
    return runTests(tests, static_cast<int>(std::size(tests)));
}
