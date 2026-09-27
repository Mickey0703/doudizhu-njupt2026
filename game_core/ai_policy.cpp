#include "ai_policy.h"

#include "game_command.h"
#include "playhand.h"
#include "state_strategy.h"


#include <QDebug>

namespace AIPolicy {

GameCommand decideCommand(const GameEngine &engine, int seat)
{
    return StateStrategy::decideCommand(engine.state(), seat);
}

CommandResult performMove(GameEngine &engine, int seat)
{
    if (!isValidSeat(seat))
        return CommandResult::rejected(GameError::InvalidSeat);

    const GameState &state = engine.state();
    if (state.currentSeat != seat)
        return CommandResult::rejected(GameError::InvalidSeat);

    const GamePhase phase = state.phase;
    CommandResult result;

    if (phase != GamePhase::CallingLord && phase != GamePhase::Playing)
        return CommandResult::rejected(GameError::InvalidPhase);
    result = engine.execute(decideCommand(engine, seat));

    if (result.accepted)
        return result;

    // 被拒兜底：保持牌局流动，绝不卡死。
    qWarning() << "AIPolicy: move rejected for seat" << seat
               << "error" << static_cast<int>(result.error) << "- applying fallback";

    if (phase == GamePhase::CallingLord) {
        return engine.execute(GameCommand::callLord(seat, 0)); // 强制不叫
    }

    // 出牌阶段：先试过牌
    CommandResult passResult = engine.execute(GameCommand::pass(seat));
    if (passResult.accepted)
        return passResult;

    // 过牌也被拒（领出者不能过）：出最小单张避免永久停滞。
    const CardList sorted = engine.state().players[seat].hand.toCardList(Cards::Asc);
    if (!sorted.isEmpty()) {
        Cards single;
        single.add(sorted.first());
        return engine.execute(GameCommand::playCards(seat, single));
    }
    return passResult; // 无牌可出（异常），返回被拒结果
}

} // namespace AIPolicy

