#ifndef PLAYER_H
#define PLAYER_H

#include <QObject>
#include "cards.h"

/**
 * @brief 玩家基类 —— 所有玩家(Robot/UserPlayer)的公共抽象
 *
 * 职责：
 * - 存储玩家基本信息（姓名、性别、角色、方向、类型、分数）
 * - 维护手牌(m_cards)和待出牌信息(m_pendCards/m_pendPlayer)
 * - 构建玩家双向链表(m_prev/m_next)以支持轮流操作
 */
class Player : public QObject
{
    Q_OBJECT
public:
    /// 角色：地主 或 农民
    enum Role { Lord, Farmer };

    /// 性别：男/女（影响语音播报）
    enum Gender { Man, Woman };

    /// 座位方向（相对于玩家视角）
    enum Direction { Left, Right };

    /// 玩家类型：AI机器人、真人用户、未知
    enum Type { Robot, User, UnKnow };

    explicit Player(QObject *parent = nullptr);
    explicit Player(QString name, QObject *parent = nullptr);

    // ============ 基本信息 getter/setter ============

    void setName(QString name);
    QString getName() const;

    void setRole(Role role);
    Role getRole() const;

    void setGender(Gender gender);
    Gender getGender() const;

    void setDirection(Direction direction);
    Direction getDirection() const;

    void setType(Type type);
    Type getType() const;

    void setScore(int score);
    int getScore() const;

    void setWin(bool flag);
    bool isWin() const;

    // ============ 玩家链表（轮流顺序） ============

    void setPrevPlayer(Player *player);
    void setNextPlayer(Player *player);
    Player *getPrevPlayer() const;
    Player *getNextPlayer() const;

    // ============ 叫地主 ============

    /// 发出叫分信号 → GameControl::onGrabBet
    void grabLordBet(int point);

    // ============ 手牌操作 ============

    /// 接收发牌（存入手牌）
    void storeDispatchCard(const Card &card);
    void storeDispatchCard(const Cards &cards);

    /// 获取手牌
    Cards getCards() const;
    /// 清空手牌
    void clearCards();
    /// 出牌：从手牌中移除并发射 notifyPlayHand 信号
    void playHand(const Cards &cards);
    /// 移除单张牌（不发射信号，用于网络同步接收方静默更新手牌）
    void removeCard(const Card &card);

    // ============ 待出牌信息（上家出的牌） ============

    Player *getPendPlayer() const;
    Cards getPendCards() const;

    /// 记录当前牌桌上的"待跟牌"信息（谁出的、出的什么）
    void storePendingInfo(Player *player, const Cards &cards);

signals:
    /// 发出叫分请求
    void notifyGrabLordBet(Player *player, int bet);
    /// 发出出牌请求
    void notifyPlayHand(Player *player, const Cards &card);

protected:
    int m_score = 0;              ///< 累计分数
    QString m_name;               ///< 玩家姓名
    Role m_role = Farmer;         ///< 角色：默认为农民
    Gender m_gender = Man;        ///< 性别：默认男
    Direction m_direction = Left; ///< 座位方向
    Type m_type = UnKnow;         ///< 玩家类型
    bool m_isWin = false;         ///< 本局是否获胜
    Player *m_prev = nullptr;     ///< 上一个玩家（链表）
    Player *m_next = nullptr;     ///< 下一个玩家（链表）
    Cards m_cards;                ///< 手牌
    Cards m_pendCards;            ///< 上家出的牌（待跟牌）
    Player *m_pendPlayer = nullptr; ///< 出待跟牌的玩家
};

#endif // PLAYER_H
