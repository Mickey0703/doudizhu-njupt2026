#ifndef RELAY_SERVER_ROOM_GAME_H
#define RELAY_SERVER_ROOM_GAME_H

#include "game_core/game_engine.h"
#include "game_core/game_command.h"
#include "game_core/game_event.h"

#include <QtGlobal>

/**
 * @brief 服务器端房间游戏封装 —— 持有权威 GameEngine
 *
 * 服务器为每个房间维护一个 ServerRoomGame 实例，它是该房间游戏状态的
 * 唯一权威来源。客户端只发送意图命令，由此处验证并执行。
 *
 * 座位约定：GameEngine 使用 0/1/2；房间 playerId 使用 1/2/3。
 * 转换由 RelayServer 负责（seat = playerId - 1）。
 */
class ServerRoomGame
{
public:
    ServerRoomGame() = default;

    /// 用给定种子开始新一轮（洗牌+发牌）。返回引擎结果（含发牌事件）。
    CommandResult startRound(quint32 seed, int firstSeat = 0);

    /// 应用来自某座位的命令。强制 command.seat = seat（不信任客户端载荷）。
    CommandResult applyCommand(int seat, const GameCommand &command);

    /// 只读访问权威状态
    const GameState &state() const { return m_engine.state(); }

    /// 该座位的私有投影视图（隐藏他人手牌）
    GameState viewFor(int seat) const { return m_engine.projectedStateFor(seat); }

    /// 是否有正在进行的一局
    bool isActive() const { return m_active; }

    /// 把某座位标记为 AI 托管（该座位玩家掉线后由服务器 AI 代打本局）
    void setSeatAI(int seat, bool ai);
    /// 该座位当前是否处于 AI 托管
    bool isSeatAI(int seat) const;
    /// 是否存在任一被托管的座位
    bool hasAISeat() const;
    /// 清空所有 AI 托管标记（新一局开始时调用）
    void clearAISeats();

    /// 为当前座位执行一步 AI 行动（掉线托管驱动）。返回引擎结果（含事件）。
    /// 仅当 isActive() 且当前座位处于 AI 托管时才应调用。
    CommandResult stepAI();

private:
    GameEngine m_engine;
    bool m_active = false;
    bool m_aiSeats[kPlayerCount] = {false, false, false};
};

#endif // RELAY_SERVER_ROOM_GAME_H
