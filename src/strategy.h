#ifndef STRATEGY_H
#define STRATEGY_H

#include "player.h"
#include "playhand.h"

/**
 * @brief AI 出牌策略类 —— 为机器人玩家决策出什么牌
 *
 * 核心思想：
 * - 首出(fistPlay)：尽可能地消耗单张（拆顺子），优先打出最长连对/飞机
 * - 跟出(getGreaterCards)：找到能打过上家的最小牌型
 * - 是否要打(whetherToBeat)：队友不出炸弹、不拆2/王
 *
 * 这是一个"贪心"策略，不是搜索算法，优点是速度快，缺点是可能不是全局最优。
 */
class Strategy
{
public:
    Strategy(Player *player, const Cards &cards);

    // ============ 公开接口 ============

    /// 决策入口：视情况调用 firstPlay() 或 getGreaterCards()
    Cards makeStrategy();

    /// 自由出牌（本轮没人出牌 / 自己是新一轮的首次出牌者）
    Cards firstPlay();

    /// 找一副能打过 hand 的牌型（优先打最小的）
    Cards getGreaterCards(PlayHand hand);

    /// 判断是否值得打（队友不打、不浪费炸弹/大牌等）
    bool whetherToBeat(Cards &cs);

    // ============ 工具方法 ============

    /// 找指定点数 point 的 count 张牌（count=1~4）
    Cards findSamePointCards(Card::CardPoint point, int count);

    /// 找所有恰好出现 count 次的点数对应的牌组
    QVector<Cards> findCardsByCount(int count);

    /// 获取区间 [begin, end) 内的所有牌
    Cards getRangeCards(Card::CardPoint begin, Card::CardPoint end);

    /// 找符合给定牌型的所有组合（beat=true 时要求点数 > hand）
    QVector<Cards> findCardType(PlayHand hand, bool beat);

    // ============ 顺子拆分 ============

    /// 递归拆分顺子，穷举所有顺子组合方案
    void pickSeqSingles(QVector<QVector<Cards>> &allSeqRecord,
                        const QVector<Cards> &seqSingle, const Cards &cards);

    /// 选最优顺子拆分方案（拆完后剩余单张牌点数总和最小）
    QVector<Cards> pickOptimalSeqSingles();

private:
    // 函数指针类型：用于 getBaseSeqPair / getBaseSeqSingle 的回调
    using function = Cards (Strategy::*)(Card::CardPoint point);

    /// 查找牌型的参数结构
    struct CardInfo
    {
        Card::CardPoint begin;   ///< 起始点数
        Card::CardPoint end;     ///< 终止点数
        int extra;               ///< 额外参数（长度等）
        bool beat;               ///< 是否需要比某牌大
        int number;              ///< 每节的张数（1=顺子, 2=连对）
        int base;                ///< 基础长度（顺子5, 连对3）
        function getSeq;         ///< 获取基础序列的函数指针
    };

    // ============ 内部查找方法 ============

    /// 找所有恰好 number 张的点数的牌组（点数 >= point）
    QVector<Cards> getCards(Card::CardPoint point, int number);

    /// 找三带一 或 三带二
    QVector<Cards> getTripleSingleOrPair(Card::CardPoint begin, PlayHand::HandType type);

    /// 找飞机（不带翅膀）。triples = 连续三张的节数（≥2）；跟牌时须与上家等长。
    QVector<Cards> getPlane(Card::CardPoint begin, int triples = 2);

    /// 找飞机带单 或 飞机带双。triples = 飞机主体节数（决定翅膀数量）。
    QVector<Cards> getPlane2SingleOr2Pair(Card::CardPoint begin, PlayHand::HandType type,
                                          int triples = 2);

    /// 找连对 或 顺子
    QVector<Cards> getSepPairOrSeqSingle(CardInfo &info);

    /// 基础3连对（作为连对查找起点）
    Cards getBaseSeqPair(Card::CardPoint point);

    /// 基础5顺子（作为顺子查找起点）
    Cards getBaseSeqSingle(Card::CardPoint point);

    /// 找炸弹
    QVector<Cards> getBomb(Card::CardPoint begin);

    // ============ 成员变量 ============

    Player *m_player;   ///< 所属玩家（用于获取上下家、角色信息）
    Cards m_cards;      ///< 当前手牌
};

#endif // STRATEGY_H
