#include "cards.h"
#include "test_support.h"
#include "game_core/game_state.h"
#include "game_core/deck.h"

bool testCardsQueriesAcceptConstValues()
{
    Cards mutableCards;
    mutableCards.add(Card(Card::Card_3, Card::Spade));
    const Cards cards = mutableCards;

    EXPECT_TRUE(cards.contains(Card(Card::Card_3, Card::Spade)));
    EXPECT_EQ(cards.cardCount(), 1);
    EXPECT_EQ(cards.maxPoint(), Card::Card_3);
    EXPECT_EQ(cards.minPoint(), Card::Card_3);
    EXPECT_EQ(cards.pointCount(Card::Card_3), 1);
    return true;
}

bool testCardsEqualityUsesPhysicalCards()
{
    Cards first;
    first.add(Card(Card::Card_A, Card::Heart));
    Cards second;
    second.add(Card(Card::Card_A, Card::Heart));

    EXPECT_TRUE(first == second);
    return true;
}

bool testGameStateStartsWaiting()
{
    const GameState state;
    EXPECT_EQ(state.phase, GamePhase::Waiting);
    EXPECT_EQ(state.currentSeat, kInvalidSeat);
    EXPECT_EQ(state.pendingSeat, kInvalidSeat);
    EXPECT_EQ(state.multiplier, 1);
    return true;
}

bool testStandardDeckContains54UniqueCards()
{
    const CardList deck = Deck::standard();
    EXPECT_EQ(deck.size(), 54);

    Cards unique;
    for (const Card &card : deck)
        unique.add(card);
    EXPECT_EQ(unique.cardCount(), 54);
    return true;
}

bool testShuffleIsDeterministicForSeed()
{
    const CardList first = Deck::shuffled(12345u);
    const CardList second = Deck::shuffled(12345u);
    EXPECT_EQ(first, second);
    EXPECT_TRUE(first != Deck::shuffled(54321u));
    return true;
}

bool testDeckValidationRejectsDuplicates()
{
    CardList deck = Deck::standard();
    deck[1] = deck[0];
    EXPECT_TRUE(!Deck::isValid(deck));
    return true;
}

bool testDeckValidationRejectsNonStandardCards()
{
    CardList deck = Deck::standard();
    deck[0] = Card(Card::Card_BJ, Card::Spade);
    EXPECT_TRUE(!Deck::isValid(deck));
    return true;
}

int main()
{
    const TestCase tests[] = {
        {"Cards const queries", testCardsQueriesAcceptConstValues},
        {"Cards equality", testCardsEqualityUsesPhysicalCards},
        {"GameState starts waiting", testGameStateStartsWaiting},
        {"Standard deck has 54 unique cards", testStandardDeckContains54UniqueCards},
        {"Shuffle is deterministic for seed", testShuffleIsDeterministicForSeed},
        {"Deck validation rejects duplicates", testDeckValidationRejectsDuplicates},
        {"Deck validation rejects non-standard cards", testDeckValidationRejectsNonStandardCards},
    };
    return runTests(tests, static_cast<int>(std::size(tests)));
}
