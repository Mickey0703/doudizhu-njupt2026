#include "card.h"
#include <QtCore/qhashfunctions.h>

// ============ 构造 ============

Card::Card()
    : m_point(Card_Begin), m_suit(Suit_Begin)
{
    // 默认构造为哨兵值（非法牌）。渲染层 cardToImageIndex 的边界检查会将其
    // 视为越界并渲染占位牌，绝不读到未初始化内存。
}

Card::Card(CardPoint point, CardSuit suit)
{
    setPoint(point);
    setSuit(suit);
}

// ============ 属性设置/获取 ============

void Card::setPoint(CardPoint point)
{
    m_point = point;
}

void Card::setSuit(CardSuit suit)
{
    m_suit = suit;
}

Card::CardSuit Card::getsuit() const
{
    return m_suit;
}

Card::CardPoint Card::getpoint() const
{
    return m_point;
}

// ============ 运算符重载 ============

bool Card::operator==(const Card &other) const
{
    // 两张牌相等：点数相同 且 花色相同
    return m_point == other.m_point && m_suit == other.m_suit;
}

bool Card::operator!=(const Card &other) const
{
    return !(*this == other);
}

// ============ 哈希函数 ============

size_t qHash(const Card &card, size_t seed)
{
    // 将点数和花色的哈希值做异或运算，保证相同的牌得到相同的哈希值
    return qHash(static_cast<int>(card.getpoint()), seed)
           ^ qHash(static_cast<int>(card.getsuit()), seed);
}

// ============ 排序比较函数 ============

bool lessSort(const Card &c1, const Card &c2)
{
    // 升序：先比较点数，点数相同则比较花色
    if (c1.getpoint() == c2.getpoint())
    {
        return c1.getsuit() < c2.getsuit();
    }
    else
    {
        return c1.getpoint() < c2.getpoint();
    }
}

bool greaterSort(const Card &c1, const Card &c2)
{
    // 降序：先比较点数，点数相同则比较花色
    if (c1.getpoint() == c2.getpoint())
    {
        return c1.getsuit() > c2.getsuit();
    }
    else
    {
        return c1.getpoint() > c2.getpoint();
    }
}
