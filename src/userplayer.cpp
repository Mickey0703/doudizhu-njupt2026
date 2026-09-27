#include "userplayer.h"

UserPlayer::UserPlayer(QObject *parent)
    : Player(parent)
{
    m_type = Player::User; // 标记为真人用户
}
