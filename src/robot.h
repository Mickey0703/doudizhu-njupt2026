#ifndef ROBOT_H
#define ROBOT_H

#include "player.h"

/**
 * @brief AI 机器人玩家 —— 继承 Player，仅作为左右机器人座位的类型标记。
 *
 * 注意：AI 的实际决策不在此类。单机由 LocalSession 驱动，出牌/叫分逻辑统一走
 * game_core/ai_policy.cpp 的 AIPolicy（叫分权重 + Strategy 贪心）；服务器掉线托管
 * 复用同一份。此类历史上的 prepareXxx/thinkXxx 虚函数决策路径已废弃并删除。
 */
class Robot : public Player
{
    Q_OBJECT
public:
    using Player::Player; ///< 继承 Player 的全部构造函数

    explicit Robot(QObject *parent = nullptr);
};

#endif // ROBOT_H
