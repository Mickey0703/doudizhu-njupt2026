#ifndef PLAYHAND_H
#define PLAYHAND_H

#include "card.h"
#include "cards.h"

/**
 * @brief 牌型识别与比较类 —— 判断一手牌的"牌型"并比较大小
 *
 * 工作流程：
 * 1. 构造时传入 Cards，内部调用 classify() 按张数分类(1/2/3/4张)
 * 2. 然后 judgeCardType() 用 16 种牌型规则依次匹配
 * 3. 成功后记录 m_type(牌型)、m_pt(主点数)、m_extra(附加信息如顺子长度)
 * 4. 外部通过 canBeat() 比较两手牌的大小
 *
 * 斗地主牌型体系（支持炸弹/火箭带牌的特殊变体）：
 *   单张 / 对子 / 三张 / 三带一 / 三带二 /
 *   飞机 / 飞机带单 / 飞机带双 /
 *   连对 / 顺子 /
 *   炸弹 / 炸弹带单 / 炸弹带对 / 炸弹带两单 /
 *   火箭(王炸) / 火箭带单 / 火箭带对 / 火箭带两单
 */
class PlayHand
{
public:
    /// 牌型枚举
    enum HandType
    {
        Hand_Unknown,                ///< 未知/不合法牌型

        Hand_Pass,                   ///< 不出（过）

        Hand_Single,                 ///< 单张
        Hand_Pair,                   ///< 对子

        Hand_Triple,                 ///< 三张（三不带）
        Hand_Triple_Single,          ///< 三带一
        Hand_Triple_Pair,            ///< 三带二

        Hand_Plane,                  ///< 飞机（555_666 不含翅膀）
        Hand_Plane_Two_Single,       ///< 飞机带单（555_666_3_4）
        Hand_Plane_Two_Pair,         ///< 飞机带双（555_666_33_44）

        Hand_Seq_Pair,               ///< 连对（33_44_55_... 至少3连对）
        Hand_Seq_Single,             ///< 顺子（34567_... 至少5连单）

        Hand_Bomb,                   ///< 炸弹（4张同点数）
        Hand_Bomb_Single,            ///< 炸弹带1个单张
        Hand_Bomb_Pair,              ///< 炸弹带1对
        Hand_Bomb_Two_Single,        ///< 炸弹带2个单张

        Hand_Bomb_Jokers,            ///< 火箭/王炸（大王+小王）
        Hand_Bomb_Jokers_Single,     ///< 王炸带1个单张
        Hand_Bomb_Jokers_Pair,       ///< 王炸带1对
        Hand_Bomb_Jokers_Two_Single  ///< 王炸带2个单张
    };

    PlayHand();
    explicit PlayHand(const Cards &cards);
    explicit PlayHand(HandType type, Card::CardPoint pt, int extra);

    HandType getHandType() const;
    Card::CardPoint getCardPoint() const; ///< 牌型主点数（对比较大小起决定作用）
    int getExtra() const;                ///< 额外信息（顺子/连对/飞机的长度）

    /// 比较两手牌：this 是否能打过 other
    bool canBeat(const PlayHand &other) const;

    /// 牌型枚举 → 中文名称
    static QString handTypeName(HandType type);

private:
    // ============ 分类 ============

    /// 将牌按张数分为 m_oneCard / m_twoCard / m_threeCard / m_fourCard
    void classify(const Cards &cards);

    /// 依次匹配 16 种牌型规则，设置 m_type & m_pt
    void judgeCardType();

    // ============ 各牌型判断方法 ============

    bool isPass();
    bool isSingle();
    bool isPair();
    bool isTriple();
    bool isTripleSingle();
    bool isTriplePair();
    bool isPlane();
    bool isPlaneTwoSingle();
    bool isPlaneTwoPair();
    bool isSeqPair();
    bool isSeqSingle();
    bool isBomb();
    bool isBombSingle();
    bool isBombPair();
    bool isBombTwoSingle();
    bool isBombJokers();
    bool isBombJokersSingle();
    bool isBombJokersPair();
    bool isBombJokersTwoSingle();

    // ============ 成员变量 ============

    HandType m_type;                   ///< 识别出的牌型
    Card::CardPoint m_pt;              ///< 主点数（比较大小的关键字段）
    int m_extra;                       ///< 附加信息（如顺子长度、飞机翅膀数）

    /// 按张数分类后的点数数组
    QVector<Card::CardPoint> m_oneCard;   ///< 只出现1张的点数列表
    QVector<Card::CardPoint> m_twoCard;   ///< 出现2张的点数列表
    QVector<Card::CardPoint> m_threeCard; ///< 出现3张的点数列表
    QVector<Card::CardPoint> m_fourCard;  ///< 出现4张的点数列表
};

#endif // PLAYHAND_H
