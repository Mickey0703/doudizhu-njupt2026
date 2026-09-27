#ifndef CARD_H
#define CARD_H

#include <QVector>
#include <cstddef>

/**
 * @brief 扑克牌类 —— 描述单张扑克牌的花色和点数
 *
 * 点数顺序(从小到大): 3,4,5,6,7,8,9,10,J,Q,K,A,2,小王,大王
 * 花色顺序: 方块(Diamond) < 梅花(Club) < 红桃(Heart) < 黑桃(Spade)
 */
class Card
{
public:
    /// 花色枚举
    enum CardSuit
    {
        Suit_Begin,   ///< 花色起始标记(非真实花色)
        Diamond,      ///< 方块 ♦
        Club,         ///< 梅花 ♣
        Heart,        ///< 红桃 ♥
        Spade,        ///< 黑桃 ♠
        Suit_End,     ///< 花色结束标记(用于遍历)
    };

    /// 点数枚举 (从小到大排列，符合斗地主规则)
    enum CardPoint
    {
        Card_Begin,   ///< 点数起始标记
        Card_3,       ///< 3
        Card_4,       ///< 4
        Card_5,       ///< 5
        Card_6,       ///< 6
        Card_7,       ///< 7
        Card_8,       ///< 8
        Card_9,       ///< 9
        Card_10,      ///< 10
        Card_J,       ///< J
        Card_Q,       ///< Q
        Card_K,       ///< K
        Card_A,       ///< A
        Card_2,       ///< 2
        Card_SJ,      ///< 小王(Small Joker)
        Card_BJ,      ///< 大王(Big Joker)
        Card_End,     ///< 点数结束标记(用于遍历)
    };

    Card();
    Card(CardPoint point, CardSuit suit);

    void setPoint(CardPoint point);
    void setSuit(CardSuit suit);
    CardPoint getpoint() const;
    CardSuit getsuit() const;

    /// 相等比较：花色的点数都相同才算相等
    bool operator==(const Card &other) const;
    bool operator!=(const Card &other) const;

private:
    CardPoint m_point;   ///< 点数
    CardSuit m_suit;     ///< 花色
};

// ============ 排序和哈希辅助函数 ============

/// 升序排序：先比点数，点数相同再比花色
bool lessSort(const Card &c1, const Card &c2);

/// 降序排序：先比点数，点数相同再比花色
bool greaterSort(const Card &c1, const Card &c2);

/// 哈希函数：允许 Card 存入 QSet / QHash
size_t qHash(const Card &card, size_t seed = 0);

/// CardList 别名 —— 有序的牌列表
using CardList = QVector<Card>;

#endif // CARD_H
