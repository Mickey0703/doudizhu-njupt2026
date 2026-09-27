#include "cards.h"

// ============ 构造 ============

Cards::Cards() {}
Cards::Cards(const Card &card) { add(card); }

// ============ 添加 ============

void Cards::add(const Card &card)
{
    m_cards.insert(card);
}

void Cards::add(const Cards &cards)
{
    // QSet::unite 执行并集操作
    m_cards.unite(cards.m_cards);
}

void Cards::add(const QVector<Cards> &cardsArray)
{
    for (const auto &c : cardsArray)
        add(c);
}

Cards &Cards::operator<<(const Card &card)
{
    add(card);
    return *this;
}

Cards &Cards::operator<<(const Cards &cards)
{
    add(cards);
    return *this;
}

// ============ 移除 ============

void Cards::remove(const Card &card)
{
    m_cards.remove(card);
}

void Cards::remove(const Cards &cards)
{
    // QSet::subtract 执行差集操作
    m_cards.subtract(cards.m_cards);
}

void Cards::remove(const QVector<Cards> &cardsArray)
{
    for (const auto &c : cardsArray)
        remove(c);
}

// ============ 查询 ============

int Cards::cardCount() const { return m_cards.size(); }
bool Cards::isEmpty() const { return m_cards.isEmpty(); }
void Cards::clear() { m_cards.clear(); }

Card::CardPoint Cards::maxPoint() const
{
    // 遍历所有牌，找到最大点数
    Card::CardPoint max = Card::Card_Begin;
    if (!m_cards.isEmpty())
    {
        for (auto it = m_cards.begin(); it != m_cards.end(); ++it)
            if (it->getpoint() > max)
                max = it->getpoint();
    }
    return max;
}

Card::CardPoint Cards::minPoint() const
{
    // 遍历所有牌，找到最小点数
    Card::CardPoint min = Card::Card_End;
    if (!m_cards.isEmpty())
    {
        for (auto it = m_cards.begin(); it != m_cards.end(); ++it)
            if (it->getpoint() < min)
                min = it->getpoint();
    }
    return min;
}

int Cards::pointCount(Card::CardPoint point) const
{
    // 统计指定点数的牌张数
    int count = 0;
    for (auto it = m_cards.begin(); it != m_cards.end(); ++it)
        if (it->getpoint() == point)
            count++;
    return count;
}

bool Cards::contains(const Card &card) const
{
    return m_cards.contains(card);
}

bool Cards::contains(const Cards &cards) const
{
    // QSet::contains(QSet) 检查是否包含所有元素
    return m_cards.contains(cards.m_cards);
}

bool Cards::operator==(const Cards &other) const
{
    return m_cards == other.m_cards;
}

bool Cards::operator!=(const Cards &other) const
{
    return !(*this == other);
}

// ============ 操作 ============

CardList Cards::toCardList(SortType type) const
{
    // 将 QSet 转为 QVector，并可选排序
    CardList list;
    for (auto it = m_cards.begin(); it != m_cards.end(); ++it)
        list << *it;

    if (type == Asc)
        std::sort(list.begin(), list.end(), lessSort);
    else if (type == Desc)
        std::sort(list.begin(), list.end(), greaterSort);

    return list;
}
