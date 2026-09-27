#include "playhand.h"

#include <QMap>
#include <algorithm>

PlayHand::PlayHand()
    : m_type(Hand_Unknown), m_pt(Card::Card_Begin), m_extra(0)
{
}

PlayHand::PlayHand(const Cards &cards)
    : m_type(Hand_Unknown), m_pt(Card::Card_Begin), m_extra(0)
{
    classify(cards);
    judgeCardType();
}

PlayHand::PlayHand(HandType type, Card::CardPoint pt, int extra)
    : m_type(type), m_pt(pt), m_extra(extra)
{
}

PlayHand::HandType PlayHand::getHandType() const { return m_type; }
Card::CardPoint PlayHand::getCardPoint() const { return m_pt; }
int PlayHand::getExtra() const { return m_extra; }

void PlayHand::classify(const Cards &cards)
{
    m_oneCard.clear();
    m_twoCard.clear();
    m_threeCard.clear();
    m_fourCard.clear();
    QMap<Card::CardPoint, int> counts;
    for (const Card &card : cards.toCardList(Cards::Asc))
        ++counts[card.getpoint()];
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
        const auto point = it.key();
        const int count = it.value();
        if (point < Card::Card_3 || point >= Card::Card_End || count > 4
            || (point >= Card::Card_SJ && count != 1)) {
            // A sentinel prevents malformed input from looking like a pass.
            m_oneCard = {Card::Card_Begin};
            m_twoCard.clear();
            m_threeCard.clear();
            m_fourCard.clear();
            return;
        }
        switch (count) {
        case 1: m_oneCard.append(point); break;
        case 2: m_twoCard.append(point); break;
        case 3: m_threeCard.append(point); break;
        case 4: m_fourCard.append(point); break;
        }
    }
}

void PlayHand::judgeCardType()
{
    m_type = Hand_Unknown;
    m_pt = Card::Card_Begin;
    m_extra = 0;
    if (m_oneCard.contains(Card::Card_Begin))
        return;
    // Exact shapes first; attachments never gain a pure bomb's priority.
    const auto judges = {&PlayHand::isPass, &PlayHand::isSingle,
        &PlayHand::isBombJokers, &PlayHand::isPair, &PlayHand::isTriple,
        &PlayHand::isTripleSingle, &PlayHand::isTriplePair,
        &PlayHand::isPlane, &PlayHand::isPlaneTwoPair,
        &PlayHand::isPlaneTwoSingle, &PlayHand::isSeqPair,
        &PlayHand::isSeqSingle, &PlayHand::isBomb,
        &PlayHand::isBombSingle, &PlayHand::isBombPair,
        &PlayHand::isBombTwoSingle, &PlayHand::isBombJokersSingle,
        &PlayHand::isBombJokersPair, &PlayHand::isBombJokersTwoSingle};
    for (auto judge : judges)
        if ((this->*judge)())
            return;
}

bool PlayHand::isPass() {
    if (!(m_oneCard.isEmpty() && m_twoCard.isEmpty() && m_threeCard.isEmpty() && m_fourCard.isEmpty()))
        return false;
    m_type = Hand_Pass;
    m_pt = Card::Card_Begin;
    return true;
}
bool PlayHand::isSingle() {
    if (!(m_oneCard.size() == 1 && m_twoCard.isEmpty() && m_threeCard.isEmpty() && m_fourCard.isEmpty()))
        return false;
    m_type = Hand_Single;
    m_pt = m_oneCard.first();
    return true;
}
bool PlayHand::isPair() {
    if (!(m_oneCard.isEmpty() && m_twoCard.size() == 1 && m_threeCard.isEmpty() && m_fourCard.isEmpty()))
        return false;
    m_type = Hand_Pair;
    m_pt = m_twoCard.first();
    return true;
}
bool PlayHand::isTriple() {
    if (!(m_oneCard.isEmpty() && m_twoCard.isEmpty() && m_threeCard.size() == 1 && m_fourCard.isEmpty()))
        return false;
    m_type = Hand_Triple;
    m_pt = m_threeCard.first();
    return true;
}
bool PlayHand::isTripleSingle() {
    if (!(m_oneCard.size() == 1 && m_twoCard.isEmpty() && m_threeCard.size() == 1 && m_fourCard.isEmpty()))
        return false;
    m_type = Hand_Triple_Single;
    m_pt = m_threeCard.first();
    return true;
}
bool PlayHand::isTriplePair() {
    if (!(m_oneCard.isEmpty() && m_twoCard.size() == 1 && m_threeCard.size() == 1 && m_fourCard.isEmpty()))
        return false;
    m_type = Hand_Triple_Pair;
    m_pt = m_threeCard.first();
    return true;
}
bool PlayHand::isPlane() {
    if (!(m_oneCard.isEmpty() && m_twoCard.isEmpty() && m_fourCard.isEmpty()) || m_threeCard.size() < 2
        || m_threeCard.last() >= Card::Card_2)
        return false;
    for (int i = 1; i < m_threeCard.size(); ++i)
        if (m_threeCard[i] != m_threeCard[i - 1] + 1)
            return false;
    m_type = Hand_Plane;
    m_pt = m_threeCard.first();
    m_extra = static_cast<int>(m_threeCard.size());
    return true;
}
bool PlayHand::isPlaneTwoSingle() {
    if (!(m_fourCard.isEmpty() && m_oneCard.size() + 2 * m_twoCard.size() == m_threeCard.size()) || m_threeCard.size() < 2
        || m_threeCard.last() >= Card::Card_2)
        return false;
    for (int i = 1; i < m_threeCard.size(); ++i)
        if (m_threeCard[i] != m_threeCard[i - 1] + 1)
            return false;
    m_type = Hand_Plane_Two_Single;
    m_pt = m_threeCard.first();
    m_extra = static_cast<int>(m_threeCard.size());
    return true;
}
bool PlayHand::isPlaneTwoPair() {
    if (!(m_oneCard.isEmpty() && m_fourCard.isEmpty() && m_twoCard.size() == m_threeCard.size()) || m_threeCard.size() < 2
        || m_threeCard.last() >= Card::Card_2)
        return false;
    for (int i = 1; i < m_threeCard.size(); ++i)
        if (m_threeCard[i] != m_threeCard[i - 1] + 1)
            return false;
    m_type = Hand_Plane_Two_Pair;
    m_pt = m_threeCard.first();
    m_extra = static_cast<int>(m_threeCard.size());
    return true;
}
bool PlayHand::isSeqPair() {
    if (!(m_oneCard.isEmpty() && m_threeCard.isEmpty() && m_fourCard.isEmpty()) || m_twoCard.size() < 3
        || m_twoCard.last() >= Card::Card_2)
        return false;
    for (int i = 1; i < m_twoCard.size(); ++i)
        if (m_twoCard[i] != m_twoCard[i - 1] + 1)
            return false;
    m_type = Hand_Seq_Pair;
    m_pt = m_twoCard.first();
    m_extra = static_cast<int>(m_twoCard.size());
    return true;
}
bool PlayHand::isSeqSingle() {
    if (!(m_twoCard.isEmpty() && m_threeCard.isEmpty() && m_fourCard.isEmpty()) || m_oneCard.size() < 5
        || m_oneCard.last() >= Card::Card_2)
        return false;
    for (int i = 1; i < m_oneCard.size(); ++i)
        if (m_oneCard[i] != m_oneCard[i - 1] + 1)
            return false;
    m_type = Hand_Seq_Single;
    m_pt = m_oneCard.first();
    m_extra = static_cast<int>(m_oneCard.size());
    return true;
}
bool PlayHand::isBomb() {
    if (!(m_oneCard.isEmpty() && m_twoCard.isEmpty() && m_threeCard.isEmpty() && m_fourCard.size() == 1))
        return false;
    m_type = Hand_Bomb;
    m_pt = m_fourCard.first();
    return true;
}
bool PlayHand::isBombSingle() {
    if (!(m_oneCard.size() == 2 && m_twoCard.isEmpty() && m_threeCard.isEmpty() && m_fourCard.size() == 1))
        return false;
    m_type = Hand_Bomb_Single;
    m_pt = m_fourCard.first();
    return true;
}
bool PlayHand::isBombPair() {
    if (!(m_oneCard.isEmpty() && m_twoCard.size() == 2 && m_threeCard.isEmpty() && m_fourCard.size() == 1))
        return false;
    m_type = Hand_Bomb_Pair;
    m_pt = m_fourCard.first();
    return true;
}
bool PlayHand::isBombTwoSingle() {
    if (!(m_oneCard.isEmpty() && m_twoCard.size() == 1 && m_threeCard.isEmpty() && m_fourCard.size() == 1))
        return false;
    m_type = Hand_Bomb_Two_Single;
    m_pt = m_fourCard.first();
    return true;
}
bool PlayHand::isBombJokers() {
    if (!(m_oneCard.size() == 2 && m_oneCard.contains(Card::Card_SJ) && m_oneCard.contains(Card::Card_BJ) && m_twoCard.isEmpty() && m_threeCard.isEmpty() && m_fourCard.isEmpty()))
        return false;
    m_type = Hand_Bomb_Jokers;
    m_pt = Card::Card_BJ;
    return true;
}
bool PlayHand::isBombJokersSingle() {
    if (!(m_oneCard.size() == 3 && m_oneCard.contains(Card::Card_SJ) && m_oneCard.contains(Card::Card_BJ) && m_twoCard.isEmpty() && m_threeCard.isEmpty() && m_fourCard.isEmpty()))
        return false;
    m_type = Hand_Bomb_Jokers_Single;
    m_pt = Card::Card_BJ;
    return true;
}
bool PlayHand::isBombJokersPair() {
    if (!(m_oneCard.size() == 2 && m_oneCard.contains(Card::Card_SJ) && m_oneCard.contains(Card::Card_BJ) && m_twoCard.size() == 1 && m_threeCard.isEmpty() && m_fourCard.isEmpty()))
        return false;
    m_type = Hand_Bomb_Jokers_Pair;
    m_pt = Card::Card_BJ;
    return true;
}
bool PlayHand::isBombJokersTwoSingle() {
    if (!(m_oneCard.size() == 4 && m_oneCard.contains(Card::Card_SJ) && m_oneCard.contains(Card::Card_BJ) && m_twoCard.isEmpty() && m_threeCard.isEmpty() && m_fourCard.isEmpty()))
        return false;
    m_type = Hand_Bomb_Jokers_Two_Single;
    m_pt = Card::Card_BJ;
    return true;
}

bool PlayHand::canBeat(const PlayHand &other) const
{
    if (m_type == Hand_Unknown || m_type == Hand_Pass
        || other.m_type == Hand_Unknown)
        return false;
    if (other.m_type == Hand_Pass)
        return true;
    if (other.m_type == Hand_Bomb_Jokers)
        return false;
    if (m_type == Hand_Bomb_Jokers)
        return true;
    if (m_type == Hand_Bomb && other.m_type != Hand_Bomb)
        return true;
    if (other.m_type == Hand_Bomb && m_type != Hand_Bomb)
        return false;
    return m_type == other.m_type && m_extra == other.m_extra
           && m_pt > other.m_pt;
}

QString PlayHand::handTypeName(HandType type)
{
    switch (type) {
    case Hand_Unknown: return QStringLiteral("未知");
    case Hand_Pass: return QStringLiteral("过牌");
    case Hand_Single: return QStringLiteral("单张");
    case Hand_Pair: return QStringLiteral("对子");
    case Hand_Triple: return QStringLiteral("三张");
    case Hand_Triple_Single: return QStringLiteral("三带一");
    case Hand_Triple_Pair: return QStringLiteral("三带二");
    case Hand_Plane: return QStringLiteral("飞机");
    case Hand_Plane_Two_Single: return QStringLiteral("飞机带单");
    case Hand_Plane_Two_Pair: return QStringLiteral("飞机带双");
    case Hand_Seq_Pair: return QStringLiteral("连对");
    case Hand_Seq_Single: return QStringLiteral("顺子");
    case Hand_Bomb: return QStringLiteral("炸弹");
    case Hand_Bomb_Single: return QStringLiteral("四带两单");
    case Hand_Bomb_Pair: return QStringLiteral("四带两对");
    case Hand_Bomb_Two_Single: return QStringLiteral("四带一对");
    case Hand_Bomb_Jokers: return QStringLiteral("火箭");
    case Hand_Bomb_Jokers_Single: return QStringLiteral("火箭带单");
    case Hand_Bomb_Jokers_Pair: return QStringLiteral("火箭带对");
    case Hand_Bomb_Jokers_Two_Single: return QStringLiteral("火箭带两单");
    }
    return QStringLiteral("未知");
}
