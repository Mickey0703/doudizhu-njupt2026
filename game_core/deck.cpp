#include "deck.h"

#include "cards.h"

#include <algorithm>
#include <random>

CardList Deck::standard()
{
    CardList deck;
    deck.reserve(54);
    for (int point = Card::Card_3; point <= Card::Card_2; ++point)
        for (int suit = Card::Diamond; suit <= Card::Spade; ++suit)
            deck.append(Card(static_cast<Card::CardPoint>(point),
                             static_cast<Card::CardSuit>(suit)));
    deck.append(Card(Card::Card_SJ, Card::Suit_Begin));
    deck.append(Card(Card::Card_BJ, Card::Suit_Begin));
    return deck;
}

CardList Deck::shuffled(quint32 seed)
{
    CardList deck = standard();
    std::mt19937 rng(seed);
    // Explicit Fisher-Yates + rejection sampling: reproducible even across
    // standard libraries whose std::shuffle/distributions differ.
    for (int i = static_cast<int>(deck.size()) - 1; i > 0; --i) {
        const quint32 bound = static_cast<quint32>(i + 1);
        const quint32 threshold = (quint32(0) - bound) % bound;
        quint32 value;
        do { value = static_cast<quint32>(rng()); } while (value < threshold);
        std::swap(deck[i], deck[static_cast<int>(value % bound)]);
    }
    return deck;
}

bool Deck::isValid(const CardList &cards)
{
    if (cards.size() != 54)
        return false;
    Cards remaining;
    for (const Card &card : standard())
        remaining.add(card);
    for (const Card &card : cards) {
        if (!remaining.contains(card))
            return false;
        remaining.remove(card);
    }
    return remaining.isEmpty();
}
