#include "strategy.h"
#include <QMap>
#include <functional>

// ============ 枚举辅助函数 ============

/// 枚举值前进1（用于循环遍历 CardPoint）
static inline Card::CardPoint nextPt(Card::CardPoint p)
{
    return static_cast<Card::CardPoint>(static_cast<int>(p) + 1);
}

/// 枚举值后退1
static inline Card::CardPoint prevPt(Card::CardPoint p)
{
    return static_cast<Card::CardPoint>(static_cast<int>(p) - 1);
}

// ============ 构造 ============

Strategy::Strategy(Player *player, const Cards &cards)
    : m_player(player), m_cards(cards)
{
}

// ============ 决策入口 ============

Cards Strategy::makeStrategy()
{
    Player *pendPlayer = m_player->getPendPlayer();
    Cards pendCards = m_player->getPendCards();

    // 如果自己是新一轮首次出牌者（没人出牌或上家是自己）
    if (pendPlayer == m_player || pendPlayer == nullptr)
    {
        return firstPlay();
    }
    else
    {
        // 跟牌模式：找能打过上家的牌
        PlayHand type(pendCards);
        Cards beatCards = getGreaterCards(type);

        if (whetherToBeat(beatCards))
            return beatCards;

        // 策略决定不打
        return Cards();
    }
}

// ============ 自由出牌（首轮） ============

Cards Strategy::firstPlay()
{
    // 如果整副手牌已经是合法牌型 → 直接全出（胜利）
    PlayHand hand(m_cards);
    if (hand.getHandType() != PlayHand::Hand_Unknown)
        return m_cards;

    // ---- 第1步：尝试拆顺子以减少单张数量 ----
    QVector<Cards> optimalSeq = pickOptimalSeqSingles();
    if (!optimalSeq.isEmpty())
    {
        // 判断拆顺子是否有收益：拆完后剩余单张更少
        int baseNum = findCardsByCount(1).size();
        Cards save = m_cards;
        save.remove(optimalSeq);
        int lastNum = Strategy(m_player, save).findCardsByCount(1).size();
        if (baseNum > lastNum)
            return optimalSeq[0]; // 有收益则出第一个顺子
    }

    // ---- 第2步：分析剩余手牌结构 ----
    bool hasPlane, hasTriple, hasPair;
    hasPair = hasTriple = hasPlane = false;
    Cards backup = m_cards;

    // 先排除炸弹（不影响常规牌型分析）
    QVector<Cards> bombArray = findCardType(
        PlayHand(PlayHand::Hand_Bomb, Card::Card_Begin, 0), false);
    backup.remove(bombArray);

    // 检测是否有飞机
    QVector<Cards> planeArray = Strategy(m_player, backup).findCardType(
        PlayHand(PlayHand::Hand_Plane, Card::Card_Begin, 0), false);
    if (!planeArray.isEmpty())
    {
        hasPlane = true;
        backup.remove(planeArray);
    }

    // 检测是否有三张
    QVector<Cards> seqTripleArray = Strategy(m_player, backup).findCardType(
        PlayHand(PlayHand::Hand_Triple, Card::Card_Begin, 0), false);
    if (!seqTripleArray.isEmpty())
    {
        hasTriple = true;
        backup.remove(seqTripleArray);
    }

    // 检测是否有连对
    QVector<Cards> seqPairArray = Strategy(m_player, backup).findCardType(
        PlayHand(PlayHand::Hand_Seq_Pair, Card::Card_Begin, 0), false);
    if (!seqPairArray.isEmpty())
    {
        hasPair = true;
        backup.remove(seqPairArray);
    }

    // ---- 第3步：按优先级出牌 ----
    // 优先级：最长连对 > 飞机(带翅膀) > 三张(带单/对) > 单张/对子

    if (hasPair)
    {
        // 出最长的连对
        Cards maxPair;
        for (int i = 0; i < seqPairArray.size(); ++i)
            if (seqPairArray[i].cardCount() > maxPair.cardCount())
                maxPair = seqPairArray[i];
        return maxPair;
    }

    if (hasPlane)
    {
        // 尝试给飞机配上翅膀
        bool twoPairFound = false;
        QVector<Cards> pairArray;
        for (Card::CardPoint pt = Card::Card_3; pt <= Card::Card_10; pt = nextPt(pt))
        {
            Cards pair = Strategy(m_player, backup).findSamePointCards(pt, 2);
            if (!pair.isEmpty())
            {
                pairArray.push_back(pair);
                if (pairArray.size() == 2)
                {
                    twoPairFound = true;
                    break;
                }
            }
        }
        if (twoPairFound)
        {
            // 飞机带两对
            Cards tmp = planeArray[0];
            tmp.add(pairArray);
            return tmp;
        }
        else
        {
            // 尝试飞机带两单
            bool twoSingleFound = false;
            QVector<Cards> singleArray;
            for (Card::CardPoint pt = Card::Card_3; pt <= Card::Card_10; pt = nextPt(pt))
            {
                if (backup.pointCount(pt) == 1)
                {
                    Cards single = Strategy(m_player, backup).findSamePointCards(pt, 1);
                    if (!single.isEmpty())
                    {
                        singleArray.push_back(single);
                        if (singleArray.size() == 2)
                        {
                            twoSingleFound = true;
                            break;
                        }
                    }
                }
            }
            if (twoSingleFound)
            {
                Cards tmp = planeArray[0];
                tmp.add(singleArray);
                return tmp;
            }
            // 裸飞机
            return planeArray[0];
        }
    }

    if (hasTriple)
    {
        // 尝试给三张配上翅膀（优先单张，其次对子）
        if (PlayHand(seqTripleArray[0]).getCardPoint() < Card::Card_A)
        {
            for (Card::CardPoint pt = Card::Card_3; pt <= Card::Card_A; pt = nextPt(pt))
            {
                int pc = backup.pointCount(pt);
                if (pc == 1)
                {
                    Cards s = Strategy(m_player, backup).findSamePointCards(pt, 1);
                    Cards tmp = seqTripleArray[0];
                    tmp.add(s);
                    return tmp; // 三带一
                }
                else if (pc == 2)
                {
                    Cards p = Strategy(m_player, backup).findSamePointCards(pt, 2);
                    Cards tmp = seqTripleArray[0];
                    tmp.add(p);
                    return tmp; // 三带二
                }
            }
        }
        return seqTripleArray[0]; // 裸三张
    }

    // ---- 第4步：处理剩余散牌 ----
    // 如果下家只剩1张且是敌对阵营 → 出大牌顶
    // 否则 → 从小牌开始出
    Player *nextPlayer = m_player->getNextPlayer();
    if (nextPlayer->getCards().cardCount() == 1 &&
        m_player->getRole() != nextPlayer->getRole())
    {
        // 下家快赢了，从大到小出牌拦截
        for (Card::CardPoint pt = prevPt(Card::Card_End);
             pt >= Card::Card_3; pt = prevPt(pt))
        {
            int pc = backup.pointCount(pt);
            if (pc == 1)
                return Strategy(m_player, backup).findSamePointCards(pt, 1);
            else if (pc == 2)
                return Strategy(m_player, backup).findSamePointCards(pt, 2);
        }
    }
    else
    {
        // 正常情况：从小到大出
        for (Card::CardPoint pt = Card::Card_3; pt < Card::Card_End; pt = nextPt(pt))
        {
            int pc = backup.pointCount(pt);
            if (pc == 1)
                return Strategy(m_player, backup).findSamePointCards(pt, 1);
            else if (pc == 2)
                return Strategy(m_player, backup).findSamePointCards(pt, 2);
        }
    }
    return Cards();
}

// ============ 找能打过上家的牌 ============

Cards Strategy::getGreaterCards(PlayHand type)
{
    Player *pendPlayer = m_player->getPendPlayer();

    // 如果上家是敌对阵营且手牌 ≤ 3 张 → 优先出炸弹拦截
    if (pendPlayer && pendPlayer->getRole() != m_player->getRole() &&
        pendPlayer->getCards().cardCount() <= 3)
    {
        QVector<Cards> bombs = findCardsByCount(4);
        for (int i = 0; i < bombs.size(); ++i)
        {
            if (PlayHand(bombs[i]).canBeat(type))
                return bombs[i];
        }
        // 没有炸弹就用火箭
        Cards sj = findSamePointCards(Card::Card_SJ, 1);
        Cards bj = findSamePointCards(Card::Card_BJ, 1);
        if (!sj.isEmpty() && !bj.isEmpty())
        {
            Cards j;
            j << sj << bj;
            return j;
        }
    }

    // 常规跟牌：尝试在不破坏顺子的前提下跟牌
    Player *nextPlayer = m_player->getNextPlayer();
    Cards remain = m_cards;
    remain.remove(Strategy(m_player, remain).pickOptimalSeqSingles());

    // Lambda: 在给定牌集中找最小能打过的牌
    auto beatCard = [=](const Cards &cards) -> Cards
    {
        QVector<Cards> beatCardsArray = Strategy(m_player, cards).findCardType(type, true);

        // Safety net: verify each candidate genuinely beats the pending hand and
        // is a well-formed hand. Guards against any generator producing a
        // wrong-length or malformed candidate (which the engine would reject and
        // degrade into a forced pass). `type` is the pending PlayHand.
        auto valid = [&](const Cards &c) {
            const PlayHand h(c);
            return h.getHandType() != PlayHand::Hand_Unknown && h.canBeat(type);
        };

        if (!beatCardsArray.isEmpty())
        {
            // 如果下家是对手且快赢了 → 出最大的压制
            if (m_player->getRole() != nextPlayer->getRole() &&
                nextPlayer->getCards().cardCount() <= 2)
            {
                for (int i = beatCardsArray.size() - 1; i >= 0; --i)
                    if (valid(beatCardsArray[i]))
                        return beatCardsArray[i];
            }
            else
            {
                for (const Cards &c : beatCardsArray)
                    if (valid(c))
                        return c;
            }
        }
        return Cards();
    };

    // 优先在不拆顺子的牌集中找
    Cards cs = beatCard(remain);
    if (!cs.isEmpty())
        return cs;

    // 不行就用全部手牌找
    cs = beatCard(m_cards);
    if (!cs.isEmpty())
        return cs;

    // 实在打不过
    return Cards();
}

// ============ 是否要打 ============

bool Strategy::whetherToBeat(Cards &cs)
{
    if (cs.isEmpty())
        return false;

    Player *pendPlayer = m_player->getPendPlayer();

    // 上家是队友：帮忙但不浪费大牌
    if (m_player->getRole() == pendPlayer->getRole())
    {
        Cards left = m_cards;
        left.remove(cs);

        // 打完这把就赢 → 打
        if (PlayHand(left).getHandType() != PlayHand::Hand_Unknown)
            return true;
        // 别拿2和大小王压队友
        Card::CardPoint basePoint = PlayHand(cs).getCardPoint();
        if (basePoint == Card::Card_2 || basePoint == Card::Card_SJ ||
            basePoint == Card::Card_BJ)
            return false;
    }
    else
    {
        // 上家是对手：别浪费2的三带一/三带二
        PlayHand myHand(cs);
        if ((myHand.getHandType() == PlayHand::Hand_Triple_Single ||
             myHand.getHandType() == PlayHand::Hand_Triple_Pair) &&
            myHand.getCardPoint() == Card::Card_2)
            return false;
        // 上家牌多时别随便拆2的对子
        if (myHand.getHandType() == PlayHand::Hand_Pair &&
            myHand.getCardPoint() == Card::Card_2 &&
            pendPlayer->getCards().cardCount() >= 10 &&
            m_player->getCards().cardCount() >= 5)
            return false;
    }
    return true;
}

// ============ 工具方法 ============

Cards Strategy::findSamePointCards(Card::CardPoint point, int count)
{
    if (count < 1 || count > 4)
        return Cards();

    // 大小王特殊处理：只有一张，找多张返回空
    if (point == Card::Card_SJ || point == Card::Card_BJ)
    {
        if (count > 1)
            return Cards();
        Card card;
        card.setPoint(point);
        card.setSuit(Card::Suit_Begin); // 王没有花色
        if (m_cards.contains(card))
        {
            Cards c;
            c.add(card);
            return c;
        }
        return Cards();
    }

    // 普通牌：按花色从小到大找 count 张
    int found = 0;
    Cards findCards;
    for (int suit = Card::Diamond; suit <= Card::Spade; ++suit)
    {
        Card card;
        card.setPoint(point);
        card.setSuit(static_cast<Card::CardSuit>(suit));
        if (m_cards.contains(card))
        {
            found++;
            findCards.add(card);
            if (found == count)
                return findCards;
        }
    }
    return Cards(); // 不够 count 张
}

QVector<Cards> Strategy::findCardsByCount(int count)
{
    if (count < 1 || count > 4)
        return {};

    QVector<Cards> cardsArray;
    // 从3遍历到2（不包括大小王），找恰好出现 count 次的点数
    for (Card::CardPoint pt = Card::Card_3; pt < Card::Card_End; pt = nextPt(pt))
    {
        if (m_cards.pointCount(pt) == count)
        {
            Cards cs;
            cs << findSamePointCards(pt, count);
            cardsArray << cs;
        }
    }
    return cardsArray;
}

Cards Strategy::getRangeCards(Card::CardPoint begin, Card::CardPoint end)
{
    Cards rangeCards;
    // 取 [begin, end) 区间内所有牌
    for (Card::CardPoint pt = begin; pt < end; pt = nextPt(pt))
    {
        int cnt = m_cards.pointCount(pt);
        Cards cs = findSamePointCards(pt, cnt);
        rangeCards << cs;
    }
    return rangeCards;
}

QVector<Cards> Strategy::findCardType(PlayHand hand, bool beat)
{
    PlayHand::HandType type = hand.getHandType();
    Card::CardPoint point = hand.getCardPoint();
    int extra = hand.getExtra();

    // beat 模式下从 point+1 开始找，否则从3开始
    Card::CardPoint beginPoint = beat ? nextPt(point) : Card::Card_3;

    switch (type)
    {
    case PlayHand::Hand_Single:
        return getCards(beginPoint, 1);
    case PlayHand::Hand_Pair:
        return getCards(beginPoint, 2);
    case PlayHand::Hand_Triple:
        return getCards(beginPoint, 3);
    case PlayHand::Hand_Triple_Single:
        return getTripleSingleOrPair(beginPoint, PlayHand::Hand_Single);
    case PlayHand::Hand_Triple_Pair:
        return getTripleSingleOrPair(beginPoint, PlayHand::Hand_Pair);
    case PlayHand::Hand_Plane:
        // beat 模式下 extra = 上家飞机节数，须等长跟牌；自由模式默认 2 节。
        return getPlane(beginPoint, beat ? extra : 2);
    case PlayHand::Hand_Plane_Two_Single:
        return getPlane2SingleOr2Pair(beginPoint, PlayHand::Hand_Single, beat ? extra : 2);
    case PlayHand::Hand_Plane_Two_Pair:
        return getPlane2SingleOr2Pair(beginPoint, PlayHand::Hand_Pair, beat ? extra : 2);
    case PlayHand::Hand_Seq_Pair:
    {
        // 连对查找参数
        CardInfo info;
        info.begin = beginPoint;
        info.end = Card::Card_Q;  // 最大只能到Q（QQ_KK_AA）
        info.number = 2;          // 每组2张
        info.base = 3;            // 至少3组
        info.extra = extra;
        info.beat = beat;
        info.getSeq = &Strategy::getBaseSeqPair;
        return getSepPairOrSeqSingle(info);
    }
    case PlayHand::Hand_Seq_Single:
    {
        // 顺子查找参数
        CardInfo info;
        info.begin = beginPoint;
        info.end = Card::Card_10; // 最大到10（10_J_Q_K_A）
        info.number = 1;          // 每组1张
        info.base = 5;            // 至少5组
        info.extra = extra;
        info.beat = beat;
        info.getSeq = &Strategy::getBaseSeqSingle;
        return getSepPairOrSeqSingle(info);
    }
    case PlayHand::Hand_Bomb:
        return getBomb(beginPoint);
    default:
        return {};
    }
}

// ============ 顺子拆分（递归穷举所有拆分方案） ============

void Strategy::pickSeqSingles(QVector<QVector<Cards>> &allSeqRecord,
                              const QVector<Cards> &seqSingle, const Cards &cards)
{
    // 在当前牌集中找所有可能的顺子
    QVector<Cards> allSeq = Strategy(m_player, cards).findCardType(
        PlayHand(PlayHand::Hand_Seq_Single, Card::Card_Begin, 0), false);

    if (allSeq.isEmpty())
    {
        // 没有更多顺子了 → 记录当前拆分方案
        allSeqRecord << seqSingle;
    }
    else
    {
        // 对每个顺子递归拆分
        Cards saveCards = cards;
        for (int i = 0; i < allSeq.size(); ++i)
        {
            Cards aScheme = allSeq.at(i);  // 选一个顺子
            Cards temp = saveCards;
            temp.remove(aScheme);           // 从手牌中移除
            QVector<Cards> seqArray = seqSingle;
            seqArray << aScheme;            // 加入方案
            pickSeqSingles(allSeqRecord, seqArray, temp); // 继续递归
        }
    }
}

// ============ 选最优顺子拆分方案 ============

QVector<Cards> Strategy::pickOptimalSeqSingles()
{
    QVector<QVector<Cards>> seqRecord;  // 所有拆分方案
    QVector<Cards> seqSingles;          // 初始为空
    Cards save = m_cards;

    // 排除炸弹和三张（它们不参与顺子拆分）
    save.remove(findCardsByCount(4));
    save.remove(findCardsByCount(3));

    // 递归穷举所有顺子拆分方案
    pickSeqSingles(seqRecord, seqSingles, save);

    if (seqRecord.isEmpty())
        return {};

    // 对每种方案打分：拆完后剩余单张的手数越少越好
    // 打分公式：Σ(剩余单张点数 + 15)
    QMap<int, int> seqMarks; // 方案索引 → 分数
    for (int i = 0; i < seqRecord.size(); ++i)
    {
        Cards backupCards = m_cards;
        QVector<Cards> seqArray = seqRecord[i];
        backupCards.remove(seqArray);

        // 统计剩余单张
        QVector<Cards> singleArray = Strategy(m_player, backupCards).findCardsByCount(1);
        CardList cardList;
        for (int j = 0; j < singleArray.size(); ++j)
            cardList << singleArray[j].toCardList();

        // 分数 = 单张点数和 + 15×数量（分数越小越好，即越容易出掉）
        int mark = 0;
        for (int j = 0; j < cardList.size(); ++j)
            mark += static_cast<int>(cardList[j].getpoint()) + 15;
        seqMarks.insert(i, mark);
    }

    // 找分数最小的方案（最优）
    int value = 0, comMark = 1000;
    for (auto it = seqMarks.constBegin(); it != seqMarks.constEnd(); ++it)
    {
        if (it.value() < comMark)
        {
            comMark = it.value();
            value = it.key();
        }
    }
    return seqRecord[value];
}

// ============ 内部查找方法 ============

QVector<Cards> Strategy::getCards(Card::CardPoint point, int number)
{
    // 从 point 开始遍历所有点数，找恰好 number 张的
    QVector<Cards> findCardsArray;
    for (Card::CardPoint pt = point; pt < Card::Card_End; pt = nextPt(pt))
    {
        if (m_cards.pointCount(pt) == number)
        {
            Cards cs = findSamePointCards(pt, number);
            findCardsArray << cs;
        }
    }
    return findCardsArray;
}

QVector<Cards> Strategy::getTripleSingleOrPair(Card::CardPoint begin,
                                               PlayHand::HandType type)
{
    // 找三带一 或 三带二
    QVector<Cards> findCardArray = getCards(begin, 3); // 先找三张
    if (!findCardArray.isEmpty())
    {
        // 在排除三张后的剩余牌中找翅膀
        Cards remainCards = m_cards;
        remainCards.remove(findCardArray);
        Strategy st(m_player, remainCards);
        QVector<Cards> cardsArray = st.findCardType(
            PlayHand(type, Card::Card_Begin, 0), false);

        // 只能给和翅膀数量一样多的三张配翅膀：findCardArray 的长度是三张
        // 的数量，cardsArray 的长度是可用翅膀的数量，二者不一定相等。
        // 原先用 findCardArray.size() 作循环上界去索引 cardsArray，当三张
        // 多于翅膀时 cardsArray.at(i) 越界（release 下 UB，debug 下断言中止）。
        const int pairable = qMin(findCardArray.size(), cardsArray.size());
        if (pairable > 0)
        {
            for (int i = 0; i < pairable; ++i)
                findCardArray[i].add(cardsArray.at(i));
            // 丢弃配不到翅膀的多余三张，避免返回裸三张冒充三带
            findCardArray.resize(pairable);
        }
        else
        {
            findCardArray.clear();
        }
    }
    return findCardArray;
}

QVector<Cards> Strategy::getPlane(Card::CardPoint begin, int triples)
{
    // 飞机主体 = triples 个连续点数的三张。2 是最小合法飞机长度。
    // 跟牌时必须与上家节数一致，否则牌型不匹配（长飞机曾只生成 2 节 → 非法被拒）。
    if (triples < 2)
        triples = 2;

    QVector<Cards> findCardArray;
    // 每节都要有三张，且最高点不得到达 2（2 不参与顺子/飞机）。
    for (Card::CardPoint pt = begin; pt <= Card::Card_K; pt = nextPt(pt))
    {
        if (static_cast<int>(pt) + triples - 1 >= Card::Card_2)
            break;  // 长度超出 A 上限

        Cards run;
        bool ok = true;
        for (int i = 0; i < triples; ++i)
        {
            Cards trip = findSamePointCards(
                static_cast<Card::CardPoint>(static_cast<int>(pt) + i), 3);
            if (trip.isEmpty())
            {
                ok = false;
                break;
            }
            run << trip;
        }
        if (ok)
            findCardArray << run;
    }
    return findCardArray;
}

QVector<Cards> Strategy::getPlane2SingleOr2Pair(Card::CardPoint begin,
                                                 PlayHand::HandType type,
                                                 int triples)
{
    if (triples < 2)
        triples = 2;

    // 飞机主体候选（每个候选是 triples 节连续三张）
    QVector<Cards> bodies = getPlane(begin, triples);
    QVector<Cards> result;

    // 每个飞机主体需要 triples 个翅膀（单张或对子），翅膀取自去掉主体后的余牌，
    // 且必须逐候选独立计算（不同主体占用的牌不同）。原实现只取固定 2 个翅膀且
    // 所有候选共用，长飞机(≥3节)必然产出非法牌。
    for (const Cards &body : bodies)
    {
        Cards remainCards = m_cards;
        remainCards.remove(body);
        Strategy st(m_player, remainCards);
        QVector<Cards> wings = st.findCardType(
            PlayHand(type, Card::Card_Begin, 0), false);

        if (wings.size() < triples)
            continue;  // 翅膀不够，这个主体不可用

        Cards full = body;
        for (int i = 0; i < triples; ++i)
            full.add(wings[i]);
        result << full;
    }

    return result;
}

QVector<Cards> Strategy::getSepPairOrSeqSingle(CardInfo &info)
{
    QVector<Cards> findCardsArray;

    if (info.beat)
    {
        // beat模式：只需要找到一个能大过上家的，从起始点遍历
        for (Card::CardPoint pt = info.begin; pt <= info.end; pt = nextPt(pt))
        {
            bool found = true;
            Cards seqCards;
            for (int i = 0; i < info.extra; ++i)
            {
                Cards cards = findSamePointCards(
                    static_cast<Card::CardPoint>(static_cast<int>(pt) + i), info.number);
                // 顺子/连对的最高点是 pt+extra-1，必须 < Card_2（即最高到 A）。
                // 原判据用 pt+extra >= Card_2，把最高恰为 A 的顺子(如 10JQKA)误拒，
                // 导致 AI 能首出却不能跟牌压制。应比较实际最高点 pt+extra-1。
                if (cards.isEmpty() ||
                    (static_cast<int>(pt) + info.extra - 1 >= Card::Card_2))
                {
                    found = false;
                    seqCards.clear();
                    break;
                }
                seqCards << cards;
            }
            if (found)
            {
                findCardsArray << seqCards;
                return findCardsArray; // beat模式只需要第一个能打的
            }
        }
    }
    else
    {
        // 自由模式：穷举所有可能的连对/顺子
        for (Card::CardPoint pt = info.begin; pt <= info.end; pt = nextPt(pt))
        {
            // 先获取基准序列（3连对或5顺子）
            Cards baseSeq = (this->*info.getSeq)(pt);
            if (baseSeq.isEmpty())
                continue;

            findCardsArray << baseSeq;
            int followed = info.base;
            Cards alreadyFollowed;

            // 向后扩展：尝试添加更多连续的节
            while (true)
            {
                Card::CardPoint fp = static_cast<Card::CardPoint>(
                    static_cast<int>(pt) + followed);
                if (fp >= Card::Card_2)
                    break;
                Cards fc = findSamePointCards(fp, info.number);
                if (fc.isEmpty())
                    break;
                alreadyFollowed << fc;
                Cards newSeq = baseSeq;
                newSeq << alreadyFollowed;
                findCardsArray << newSeq;
                followed++;
            }
        }
    }
    return findCardsArray;
}

Cards Strategy::getBaseSeqPair(Card::CardPoint point)
{
    // 获取从 point 开始的3连对（最小连对）
    Cards c0 = findSamePointCards(point, 2);
    Cards c1 = findSamePointCards(nextPt(point), 2);
    Cards c2 = findSamePointCards(
        static_cast<Card::CardPoint>(static_cast<int>(point) + 2), 2);

    Cards baseSeq;
    if (!c0.isEmpty() && !c1.isEmpty() && !c2.isEmpty())
        baseSeq << c0 << c1 << c2;
    return baseSeq;
}

Cards Strategy::getBaseSeqSingle(Card::CardPoint point)
{
    // 获取从 point 开始的5连顺子（最小顺子）
    Cards c0 = findSamePointCards(point, 1);
    Cards c1 = findSamePointCards(nextPt(point), 1);
    Cards c2 = findSamePointCards(
        static_cast<Card::CardPoint>(static_cast<int>(point) + 2), 1);
    Cards c3 = findSamePointCards(
        static_cast<Card::CardPoint>(static_cast<int>(point) + 3), 1);
    Cards c4 = findSamePointCards(
        static_cast<Card::CardPoint>(static_cast<int>(point) + 4), 1);

    Cards baseSeq;
    if (!c0.isEmpty() && !c1.isEmpty() && !c2.isEmpty() &&
        !c3.isEmpty() && !c4.isEmpty())
        baseSeq << c0 << c1 << c2 << c3 << c4;
    return baseSeq;
}

QVector<Cards> Strategy::getBomb(Card::CardPoint begin)
{
    // 从 begin 开始找所有炸弹
    QVector<Cards> findcardsArray;
    for (Card::CardPoint pt = begin; pt < Card::Card_End; pt = nextPt(pt))
    {
        Cards cs = findSamePointCards(pt, 4);
        if (!cs.isEmpty())
            findcardsArray << cs;
    }
    return findcardsArray;
}
