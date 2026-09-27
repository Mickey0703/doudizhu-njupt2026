#ifndef GAME_CORE_DECK_H
#define GAME_CORE_DECK_H

#include "card.h"

#include <QtGlobal>

class Deck
{
public:
    static CardList standard();
    static CardList shuffled(quint32 seed);
    static bool isValid(const CardList &cards);
};

#endif // GAME_CORE_DECK_H
