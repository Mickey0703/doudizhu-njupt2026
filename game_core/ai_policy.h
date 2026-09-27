#ifndef GAME_CORE_AI_POLICY_H
#define GAME_CORE_AI_POLICY_H

#include "game_engine.h"

/**
 * @brief 共享 AI 决策策略 —— 单机与服务器托管复用同一套逻辑。
 *
 * 把"给定引擎当前状态，为某座位算一步合法行动并执行"抽成自由函数，
 * 避免单机(LocalSession)与服务器掉线托管(ServerRoomGame)各写一份 AI。
 *
 * 决策来源为纯状态 StateStrategy（确定性牌型枚举和剩余手数估计），与
 * LocalSession、服务器托管和投影状态提示共享，且不依赖旧 Player 环。
 */
namespace AIPolicy {

/**
 * @brief 为 seat 计算并在 engine 上执行一步 AI 行动（叫分或出牌）。
 *
 * 内部包含与单机一致的兜底：叫分被拒→改不叫(0)；出牌被拒→改过牌；
 * 领出者不能过则出最小单张，保证不会因非法候选而卡死。
 *
 * 调用方负责：调用前确认 currentSeat == seat；调用后路由返回的 events。
 * 本函数不做任何异步调度，纯同步执行一步。
 *
 * @param engine 权威引擎（会被 execute 改变状态）
 * @param seat   要行动的座位 (0-2)
 * @return 被接受的命令结果（含事件）；若无法行动返回 rejected 结果
 */
CommandResult performMove(GameEngine &engine, int seat);

/**
 * @brief 只决策不执行：为 seat 算出一步 AI 行动命令（叫分/出牌/过），不改引擎。
 *
 * 供需要在"决策"与"执行"之间插入处理的调用方使用（如单机耄耋模式在 AI 出牌
 * 前做混沌变形）。若执行被拒，调用方应回退到 performMove 走完整兜底逻辑。
 *
 * @param engine 权威引擎（只读，不改状态）
 * @param seat   要行动的座位 (0-2)
 * @return 建议命令；座位/相位非法时返回一个 seat 非法的默认命令
 */
GameCommand decideCommand(const GameEngine &engine, int seat);

} // namespace AIPolicy

#endif // GAME_CORE_AI_POLICY_H

