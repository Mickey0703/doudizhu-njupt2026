#include "player.h"

// ============ 构造 ============

Player::Player(QObject *parent)
    : QObject(parent)
{
}

Player::Player(QString name, QObject *parent)
    : QObject(parent), m_name(name)
{
}

// ============ 基本信息 getter/setter ============

void Player::setName(QString name) { m_name = name; }
QString Player::getName() const { return m_name; }

void Player::setRole(Role role) { m_role = role; }
Player::Role Player::getRole() const { return m_role; }

void Player::setGender(Gender gender) { m_gender = gender; }
Player::Gender Player::getGender() const { return m_gender; }

void Player::setDirection(Direction direction) { m_direction = direction; }
Player::Direction Player::getDirection() const { return m_direction; }

void Player::setType(Type type) { m_type = type; }
Player::Type Player::getType() const { return m_type; }

void Player::setScore(int score) { m_score = score; }
int Player::getScore() const { return m_score; }

void Player::setWin(bool flag) { m_isWin = flag; }
bool Player::isWin() const { return m_isWin; }

// ============ 玩家链表 ============

void Player::setPrevPlayer(Player *player) { m_prev = player; }
void Player::setNextPlayer(Player *player) { m_next = player; }
Player *Player::getPrevPlayer() const { return m_prev; }
Player *Player::getNextPlayer() const { return m_next; }

// ============ 叫地主 ============

void Player::grabLordBet(int point)
{
    // 发射信号，由 GameControl::onGrabBet 处理叫分逻辑
    emit notifyGrabLordBet(this, point);
}

// ============ 手牌操作 ============

void Player::storeDispatchCard(const Card &card)
{
    m_cards.add(card);
}

void Player::storeDispatchCard(const Cards &cards)
{
    m_cards.add(cards);
}

Cards Player::getCards() const
{
    return m_cards;
}

void Player::clearCards()
{
    m_cards.clear();
}

void Player::playHand(const Cards &cards)
{
    // 从手牌中移除打出的牌，并通知外部
    m_cards.remove(cards);
    emit notifyPlayHand(this, cards);
}

void Player::removeCard(const Card &card)
{
    m_cards.remove(card);
}

// ============ 待出牌信息 ============

Player *Player::getPendPlayer() const
{
    return m_pendPlayer;
}

Cards Player::getPendCards() const
{
    return m_pendCards;
}

void Player::storePendingInfo(Player *player, const Cards &cards)
{
    // 记录本轮谁出了什么牌，供 Strategy 决策时参考
    m_pendPlayer = player;
    m_pendCards = cards;
}
