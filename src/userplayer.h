#ifndef USERPLAYER_H
#define USERPLAYER_H

#include "player.h"

/**
 * @brief 真人用户玩家 —— 继承 Player，仅作为用户座位的类型标记。
 *
 * 用户操作通过 GamePanel 的 GUI 按钮驱动：GameControl 发出
 * ThinkingForCallLord / ThinkingForPlayHand 状态后 GamePanel 显示对应按钮。
 */
class UserPlayer : public Player
{
    Q_OBJECT
public:
    using Player::Player; ///< 继承 Player 的全部构造函数

    explicit UserPlayer(QObject *parent = nullptr);
};

#endif // USERPLAYER_H
