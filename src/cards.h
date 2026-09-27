#ifndef CARDS_H
#define CARDS_H

#include "card.h"
#include <QSet>
#include <QVector>

/**
 * @brief 牌集合类 —— 以 QSet<Card> 为底层容器管理一组牌
 *
 * 特点：
 * - 内部用 QSet 存储，自动去重（同一花色+点数的牌不会重复）
 * - 支持批量增删、流式操作(<<)、随机取牌
 * - 提供点数统计、最大/最小点数查询
 * - 输出时可选择升序(Asc)、降序(Desc)或无排序(NoSort)
 */
class Cards
{
public:
    /// 排序方式
    enum SortType
    {
        Asc,     ///< 升序排列 (lessSort)
        Desc,    ///< 降序排列 (greaterSort)
        NoSort   ///< 不排序
    };

    Cards();
    explicit Cards(const Card &card);

    // ============ 添加牌 ============

    /// 添加单张牌
    void add(const Card &card);
    /// 合并另一组牌（并集）
    void add(const Cards &cards);
    /// 合并多组牌
    void add(const QVector<Cards> &cardsArray);

    /// 流式操作符：add 的语法糖
    Cards &operator<<(const Card &card);
    Cards &operator<<(const Cards &cards);

    // ============ 移除牌 ============

    /// 移除单张牌
    void remove(const Card &card);
    /// 移除另一组牌（差集）
    void remove(const Cards &cards);
    /// 移除多组牌
    void remove(const QVector<Cards> &cardsArray);

    // ============ 查询 ============

    /// 牌的数量
    int cardCount() const;
    /// 牌集是否为空
    bool isEmpty() const;
    /// 清空所有牌
    void clear();

    /// 获取最大点数
    Card::CardPoint maxPoint() const;
    /// 获取最小点数
    Card::CardPoint minPoint() const;
    /// 统计某一指定点数的牌有多少张
    int pointCount(Card::CardPoint point) const;
    /// 是否包含某张牌
    bool contains(const Card &card) const;
    /// 是否包含另一组全部牌
    bool contains(const Cards &cards) const;

    /// 相等性比较
    bool operator==(const Cards &other) const;
    bool operator!=(const Cards &other) const;

    // ============ 操作 ============

    /// 转换为有序 CardList，按指定排序方式
    CardList toCardList(SortType type = Desc) const;

private:
    QSet<Card> m_cards;   ///< 底层牌集合（无序、去重）
};

#endif // CARDS_H
