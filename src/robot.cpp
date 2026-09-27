#include "robot.h"

// ============ 构造 ============

Robot::Robot(QObject *parent)
    : Player(parent)
{
    m_type = Player::Robot; // 标记为 AI 玩家
}

// AI 决策不在此类：单机由 LocalSession 驱动、统一走 AIPolicy（见 game_core/ai_policy.cpp），
// 服务器掉线托管复用同一份。历史上的 prepare*/think* 决策路径已废弃删除。
