#include "playhand.h"
#include "cards.h"
#include "card.h"
#include "test_support.h"

#include <cstdlib>

// ---- helpers -------------------------------------------------------------

// Add `count` cards of the given point using distinct suits (count <= 4).
static void addN(Cards &cards, Card::CardPoint pt, int count)
{
    const Card::CardSuit suits[] = {Card::Diamond, Card::Club, Card::Heart, Card::Spade};
    for (int i = 0; i < count; ++i)
        cards.add(Card(pt, suits[i]));
}

static Cards single(Card::CardPoint pt)
{
    Cards c;
    c.add(Card(pt, Card::Spade));
    return c;
}

// ---- #7 plane-with-two-pairs ---------------------------------------------

bool testPlaneTwoPairDetectedNotAsTwoSingle()
{
    // 999 TTT 33 44 = plane(2) + two pairs = 10 cards. Must be Hand_Plane_Two_Pair,
    // NOT Hand_Plane_Two_Single (the old singleCount miscount misclassified it).
    Cards hand;
    addN(hand, Card::Card_9, 3);
    addN(hand, Card::Card_10, 3);
    addN(hand, Card::Card_3, 2);
    addN(hand, Card::Card_4, 2);
    PlayHand h(hand);
    EXPECT_EQ(h.getHandType(), PlayHand::Hand_Plane_Two_Pair);
    return true;
}

bool testPlaneTwoSingleStillDetected()
{
    // 999 TTT + 3 + 4 = plane(2) + two singles = 8 cards.
    Cards hand;
    addN(hand, Card::Card_9, 3);
    addN(hand, Card::Card_10, 3);
    hand.add(Card(Card::Card_3, Card::Spade));
    hand.add(Card(Card::Card_4, Card::Spade));
    PlayHand h(hand);
    EXPECT_EQ(h.getHandType(), PlayHand::Hand_Plane_Two_Single);
    return true;
}

bool testPlaneTwoSingleCannotBeatPlaneTwoPair()
{
    // Regression for #7: an 8-card plane-two-single must not beat a 10-card
    // plane-two-pair (different hand types, canBeat requires same type).
    Cards twoSingle;
    addN(twoSingle, Card::Card_9, 3);
    addN(twoSingle, Card::Card_10, 3);
    twoSingle.add(Card(Card::Card_3, Card::Spade));
    twoSingle.add(Card(Card::Card_4, Card::Spade));

    Cards twoPair;
    addN(twoPair, Card::Card_5, 3);
    addN(twoPair, Card::Card_6, 3);
    addN(twoPair, Card::Card_3, 2);
    addN(twoPair, Card::Card_4, 2);

    PlayHand a(twoSingle);
    PlayHand b(twoPair);
    EXPECT_TRUE(!a.canBeat(b));
    return true;
}

// ---- #17 pure plane length via m_extra -----------------------------------

bool testPureePlaneRecordsExtra()
{
    // 333 444 555 -> plane of 3 triples. m_extra must equal 3.
    Cards hand;
    addN(hand, Card::Card_3, 3);
    addN(hand, Card::Card_4, 3);
    addN(hand, Card::Card_5, 3);
    PlayHand h(hand);
    EXPECT_EQ(h.getHandType(), PlayHand::Hand_Plane);
    EXPECT_EQ(h.getExtra(), 3);
    return true;
}

bool testDifferentLengthPlanesCannotBeat()
{
    // Regression for #17: a 2-triple plane and a 3-triple plane have different
    // m_extra, so canBeat must return false (cannot compare unlike lengths).
    Cards shortPlane;   // 999 TTT (extra 2)
    addN(shortPlane, Card::Card_9, 3);
    addN(shortPlane, Card::Card_10, 3);

    Cards longPlane;    // 333 444 555 (extra 3)
    addN(longPlane, Card::Card_3, 3);
    addN(longPlane, Card::Card_4, 3);
    addN(longPlane, Card::Card_5, 3);

    PlayHand hi(shortPlane);  // higher points but shorter
    PlayHand lo(longPlane);
    EXPECT_TRUE(!hi.canBeat(lo));
    EXPECT_TRUE(!lo.canBeat(hi));
    return true;
}

// ---- #18 invalid 7-card four-with-two ------------------------------------

bool testSevenCardFourWithTwoRejected()
{
    // 4444 + 3 + 55 = 7 cards. Not a valid Dou Dizhu hand. Must be Unknown.
    Cards hand;
    addN(hand, Card::Card_4, 4);
    hand.add(Card(Card::Card_3, Card::Spade));
    addN(hand, Card::Card_5, 2);
    PlayHand h(hand);
    EXPECT_EQ(h.getHandType(), PlayHand::Hand_Unknown);
    return true;
}

bool testFourWithTwoSinglesStillValid()
{
    // 4444 + 3 + 5 = 6 cards, valid four-with-two-singles.
    Cards hand;
    addN(hand, Card::Card_4, 4);
    hand.add(Card(Card::Card_3, Card::Spade));
    hand.add(Card(Card::Card_5, Card::Spade));
    PlayHand h(hand);
    EXPECT_EQ(h.getHandType(), PlayHand::Hand_Bomb_Single);
    return true;
}

bool testFourWithTwoPairsStillValid()
{
    // 4444 + 33 + 55 = 8 cards, valid four-with-two-pairs.
    Cards hand;
    addN(hand, Card::Card_4, 4);
    addN(hand, Card::Card_3, 2);
    addN(hand, Card::Card_5, 2);
    PlayHand h(hand);
    EXPECT_EQ(h.getHandType(), PlayHand::Hand_Bomb_Pair);
    return true;
}

// ---- sanity: core comparisons unaffected ---------------------------------

bool testBombBeatsNonBombAndRocketBeatsAll()
{
    Cards bomb;   addN(bomb, Card::Card_5, 4);
    Cards pair;   addN(pair, Card::Card_K, 2);
    Cards rocket; rocket.add(Card(Card::Card_SJ, Card::Suit_Begin));
                  rocket.add(Card(Card::Card_BJ, Card::Suit_Begin));

    PlayHand b(bomb), p(pair), r(rocket);
    EXPECT_TRUE(b.canBeat(p));       // bomb > pair
    EXPECT_TRUE(!p.canBeat(b));      // pair !> bomb
    EXPECT_TRUE(r.canBeat(b));       // rocket > bomb
    EXPECT_TRUE(!b.canBeat(r));      // bomb !> rocket
    return true;
}

bool testStraightSamLengthComparesByPoint()
{
    Cards lo; for (int p = Card::Card_3; p <= Card::Card_7; ++p) lo.add(Card(static_cast<Card::CardPoint>(p), Card::Spade));
    Cards hi; for (int p = Card::Card_4; p <= Card::Card_8; ++p) hi.add(Card(static_cast<Card::CardPoint>(p), Card::Spade));
    PlayHand a(lo), b(hi);
    EXPECT_TRUE(b.canBeat(a));
    EXPECT_TRUE(!a.canBeat(b));
    return true;
}

int main()
{
    const TestCase tests[] = {
        {"Plane two-pair detected (not two-single)", testPlaneTwoPairDetectedNotAsTwoSingle},
        {"Plane two-single still detected", testPlaneTwoSingleStillDetected},
        {"Plane two-single cannot beat plane two-pair", testPlaneTwoSingleCannotBeatPlaneTwoPair},
        {"Pure plane records extra length", testPureePlaneRecordsExtra},
        {"Different-length planes cannot beat", testDifferentLengthPlanesCannotBeat},
        {"Seven-card four-with-two rejected", testSevenCardFourWithTwoRejected},
        {"Four-with-two-singles still valid", testFourWithTwoSinglesStillValid},
        {"Four-with-two-pairs still valid", testFourWithTwoPairsStillValid},
        {"Bomb/rocket comparisons intact", testBombBeatsNonBombAndRocketBeatsAll},
        {"Straight same-length compares by point", testStraightSamLengthComparesByPoint},
    };
    return runTests(tests, static_cast<int>(std::size(tests)));
}
