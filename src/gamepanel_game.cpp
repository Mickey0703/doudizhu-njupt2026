// Game presentation and menu start/restart. Commands, hints and countdown are delegated to GameActionController.

#include "gamepanel.h"
#include "playhand.h"


#include <QMessageBox>
#include <QTimer>
#include <QDir>
#include <QRandomGenerator>
#include <QCoreApplication>

// ===================================================================
// 游戏流程
// ===================================================================

void GamePanel::onStartGame()
{
    if (m_online->isOnline())
    {
        if (!m_online->isHost())
            return;
        if (m_online->playerCount() < 3)
        {
            m_statusLabel->setText(QStringLiteral("等待 3 名玩家到齐"));
            updateRoomStartVisibility();
            return;
        }
        if (m_online->isConnected())
            m_online->sendStartGame();
    }

    m_btnStart->hide();
    m_btnBackMenu->show();   // 对战中一直显示返回按钮
    m_passStreak = 0;

    // 服务器权威联机：房主只发开局请求，发牌/叫分/出牌全部由服务器 GameEngine
    // 裁决并以 SvrEvent 下发。客户端不跑本地引擎，直接返回，等待服务器事件驱动 UI。
    if (m_online->isOnline())
        return;

    // 加载选中的主题（耄耋模式已在 onChaosMode 加载好 maodie 主题+混沌配置，
    // 不能再用 VS-AI 遗留的下拉框主题覆盖它）
    if (!m_chaosEngine->isActive() && !m_themeNames.isEmpty() && m_themeCombo->currentIndex() >= 0)
    {
        QString themeId = m_themeCombo->currentData().toString();
        QString themesDir = QCoreApplication::applicationDirPath() + "/../themes/";
        QString themePath = themesDir + themeId;
        if (!QDir(themePath).exists())
            themePath = QCoreApplication::applicationDirPath() + "/themes/" + themeId;

        if (m_themeManager->loadTheme(themePath))
        {
            ThemeConfig cfg = m_themeManager->config();
            m_stickerStore->loadFromTheme(m_themeManager);
            m_chatPanel->setStickerStore(m_stickerStore);

            if (!m_online->isOnline())
            {
                m_leftNameLabel->setText(cfg.robotNames.value(0, "机器人A"));
                m_rightNameLabel->setText(cfg.robotNames.value(1, "机器人B"));
            }

            // 随机机器人头像
            m_leftBotAvatarId = m_avatarStore->list().isEmpty() ? ""
                : m_avatarStore->list()[QRandomGenerator::global()->bounded(m_avatarStore->list().size())].id;
            m_rightBotAvatarId = m_avatarStore->list().isEmpty() ? ""
                : m_avatarStore->list()[QRandomGenerator::global()->bounded(m_avatarStore->list().size())].id;
        }
    }

    // BGM: 优先用主题 BGM 路径，否则走默认曼波
    if (m_themeManager->isLoaded() && !m_themeManager->bgmPath().isEmpty())
        m_musicPlayer->playBGM(m_themeManager->bgmPath());
    else
        m_musicPlayer->playBGM(MusicPlayer::BGM_Playing);

    // 重置牌堆并发牌 —— resetCardData() 内部经 LocalSession::startRound()
    // 完成正确发牌、syncStateToPlayers()，并 emit gameStatusChanged(DispatchCard)；
    // 后者已同步触发 updateAllPlayerCards/clearPlayArea/clearLordCards，无需再手动 emit，
    // 也不要在此追加任何逐张发牌逻辑（历史上那样会注入未初始化的无效 Card 致渲染越界崩溃）。
    m_gameControl->resetCardData();

    // 延迟500ms后开始叫地主（让UI先渲染）—— 仅单机路径；联机已在上方 early-return。
    QTimer::singleShot(500, m_gameControl, &GameControlAdapter::startLordCard);
}

void GamePanel::onRestartGame()
{
    m_btnRestart->hide();
    m_btnBackMenu->hide();
    clearPlayArea();
    clearLordCards();
    onStartGame(); // 复用开赛逻辑
}

// ============ 游戏状态回调 ============

void GamePanel::onGameStatusChanged(GameControlAdapter::GameStatus s)
{
    setCallButtonsVisible(false);
    setPlayButtonsVisible(false);

    switch (s)
    {
    case GameControlAdapter::DispatchCard:
        m_statusLabel->setText(QStringLiteral("发牌中..."));
        break;
    case GameControlAdapter::CallingLord:
        m_statusLabel->setText(QStringLiteral("叫地主阶段"));
        m_musicPlayer->playSFX(MusicPlayer::SFX_DealCard);
        break;
    case GameControlAdapter::PlayingHand:
        m_statusLabel->setText(QStringLiteral("出牌阶段"));
        // 此时 becomeLord 已设置角色，可以正确显示地主图标和底牌
        updateLordCards();
        updateLordIcons();
        break;
    }

    updateAllPlayerCards();
    clearPlayArea();
    // PlayingHand 阶段由 case 内 updateLordCards() 显示底牌，不能在这里清除
    if (s != GameControlAdapter::PlayingHand)
        clearLordCards();
}

void GamePanel::onNotifyCallLord(Player *player, int bet, bool isFirst)
{
    Q_UNUSED(isFirst);

    Player *p = findPlayerPtr(player);
    if (!p)
        return;

    // 状态栏显示叫分结果
    QString txt = (bet == 0) ? QStringLiteral("不叫") : QString::number(bet) + QStringLiteral("分");
    m_statusLabel->setText(p->getName() + ": " + txt);

    // 播放对应叫分音效
    switch (bet)
    {
    case 0: m_musicPlayer->playSFX(MusicPlayer::SFX_NoCall);    break;
    case 1: m_musicPlayer->playSFX(MusicPlayer::SFX_CallLord1); break;
    case 2: m_musicPlayer->playSFX(MusicPlayer::SFX_CallLord2); break;
    case 3: m_musicPlayer->playSFX(MusicPlayer::SFX_CallLord3); break;
    }

    // 混沌：叫3分弹窗
    if (bet == 3)
        m_chaosEngine->checkCallLordTrigger(bet);

    // 主题音效：主题只提供 call_lord_1/2/3，没有"不叫"音效。bet==0 时不放主题音
    // （非主题的 SFX_NoCall 上面已播），不能再错误映射到 call_lord_1（那是叫1分的音）。
    if (m_themeManager->isLoaded() && bet >= 1)
    {
        QString sfxKey = (bet == 1) ? "call_lord_1"
                         : (bet == 2) ? "call_lord_2"
                         : "call_lord_3";
        QString path = m_themeManager->sfxPath(sfxKey);
        if (!path.isEmpty())
            m_musicPlayer->playEventSFX(path);
    }

    m_chatPanel->addSystemMessage(QStringLiteral("🎯 %1: %2").arg(p->getName(), txt));
}

void GamePanel::onNotifyPlayHand(Player *player, const Cards &cards)
{
    Player *p = findPlayerPtr(player);
    if (!p)
        return;

    if (cards.isEmpty())
    {
        m_passStreak++;
        if (m_chaosEngine->isActive())
            m_chaosEngine->checkPassStreak(m_passStreak);

        // 不出
        m_statusLabel->setText(p->getName() + QStringLiteral(": 不出"));
        m_musicPlayer->playSFX(MusicPlayer::SFX_Pass1);
        m_chatPanel->addSystemMessage(QStringLiteral("🚫 %1: 不出").arg(p->getName()));
    }
    else
    {
        m_passStreak = 0;

        // 有出牌：显示牌型并播放对应音效
        PlayHand h(cards);
        m_statusLabel->setText(p->getName() + ": " + PlayHand::handTypeName(h.getHandType()));
        m_musicPlayer->playSFXForHandType(h.getHandType());
        m_chatPanel->addSystemMessage(
            QStringLiteral("🃏 %1: %2(%3张)")
                .arg(p->getName(), PlayHand::handTypeName(h.getHandType()))
                .arg(cards.cardCount()));

        // 混沌特殊牌型检测
        if (m_chaosEngine->isActive())
        {
            m_chaosEngine->checkSpecialTrigger(cards, h.getHandType());
        }

        // 主题炸弹/火箭音效
        if (m_themeManager->isLoaded())
        {
            QString sfxKey;
            if (h.getHandType() == PlayHand::Hand_Bomb) sfxKey = "bomb";
            else if (h.getHandType() == PlayHand::Hand_Bomb_Jokers) sfxKey = "rocket";
            if (!sfxKey.isEmpty())
            {
                QString path = m_themeManager->sfxPath(sfxKey);
                if (!path.isEmpty()) m_musicPlayer->playEventSFX(path);
            }
        }
    }

    updatePlayArea(player, cards);
    updateAllPlayerCards();
}

void GamePanel::onPlayerStatusChanged(Player *player, GameControlAdapter::PlayerStatus s)
{
    Player *p = findPlayerPtr(player);
    if (!p)
        return;

    // ---- 游戏结束 ----
    if (s == GameControlAdapter::Winning)
    {
        m_actions->stopCountdown();
        m_timerLabel->hide();

        bool isLordWin = (player->getRole() == Player::Lord);
        QString r = isLordWin ? QStringLiteral("👑地主胜利!") : QStringLiteral("🌾农民胜利!");
        QString msg = QStringLiteral("%1\n%2").arg(p->getName(), r);
        m_statusLabel->setText(msg);
        m_musicPlayer->stopBGM();

        // 判定玩家输赢 → 播放对应音效
        if ((p == m_gameControl->getUserPlayer()) ||
            (p->getRole() == Player::Farmer &&
             m_gameControl->getUserPlayer()->getRole() == Player::Farmer))
            m_musicPlayer->playSFX(MusicPlayer::SFX_Win);
        else
            m_musicPlayer->playSFX(MusicPlayer::SFX_Lose);

        clearPlayArea();
        m_chatPanel->addSystemMessage(QStringLiteral("🏆 ") + msg);

        // 延迟弹出结果对话框。以 this 作 context object：若窗口在 300ms 内销毁，
        // Qt 自动取消回调，避免悬垂 this。
        QTimer::singleShot(300, this, [msg, this]()
                           { QMessageBox::information(this, QStringLiteral("游戏结束"), msg); });

        // 显示再来一局和返回菜单按钮
        m_btnRestart->show();
        m_btnBackMenu->show();
        setPlayButtonsVisible(false);
        setCallButtonsVisible(false);
        return;
    }

    // ---- 叫地主/出牌等待 ----
    m_actions->stopCountdown();
    m_timerLabel->hide();

    bool isUser = (p == m_gameControl->getUserPlayer());
    switch (s)
    {
    case GameControlAdapter::ThinkingForCallLord:
        if (isUser)
        {
            // 用户回合 → 显示叫分按钮
            setCallButtonsVisible(true);
            m_statusLabel->setText(QStringLiteral("请选择叫分"));
        }
        else
        {
            setCallButtonsVisible(false);
            setPlayButtonsVisible(false);
            m_statusLabel->setText(p->getName() + QStringLiteral(" 思考中..."));
        }
        break;

    case GameControlAdapter::ThinkingForPlayHand:
        if (isUser)
        {
            // 用户回合 → 显示出牌按钮
            setPlayButtonsVisible(true);
            m_statusLabel->setText(QStringLiteral("请出牌"));
        }
        else
        {
            setPlayButtonsVisible(false);
            setCallButtonsVisible(false);
            m_statusLabel->setText(p->getName() + QStringLiteral(" 思考中..."));
        }
        break;

    default:
        break;
    }

    // 启动25秒倒计时——仅当轮到人类玩家真正需要操作（叫分/出牌）时。
    // AI 回合或其它状态不该起计时，否则倒计时会在对手思考时空转，
    // 归零还会误触发混沌消牌等基于超时的逻辑。
    const bool userMustAct = isUser &&
        (s == GameControlAdapter::ThinkingForCallLord ||
         s == GameControlAdapter::ThinkingForPlayHand);
    if (userMustAct)
    {
        m_actions->startCountdown();
    }
}

// ===================================================================
// 用户操作
// ===================================================================

void GamePanel::onCardSelected(int id)
{
    if (!m_actions->isOurTurn()) return;
    m_tableView->toggleSelection(id);
    m_musicPlayer->playSFX(MusicPlayer::SFX_SelectCard);
}

void GamePanel::onBtnPlay()
{
    Cards cards = m_tableView->selectedCards();
    if (cards.isEmpty()) return;
    if (m_chaosEngine->isActive() && !m_online->isOnline())
        cards = m_gameControl->maybeTransformPlayerCards(cards);
    showActionResult(m_actions->submit(GameCommand::playCards(m_actions->localSeat(), cards)));
}

void GamePanel::onBtnPass()
{
    showActionResult(m_actions->submit(GameCommand::pass(m_actions->localSeat())));
}

void GamePanel::onBtnHint()
{
    const auto hint = m_actions->hint();
    if (!isValidSeat(hint.seat)) return;
    m_tableView->selectCards(hint.cards);
    if (hint.cards.isEmpty()) m_statusLabel->setText(QStringLiteral("💡 无更大牌可出，建议不出"));
}

void GamePanel::onBtnCallLord(int bet)
{
    showActionResult(m_actions->submit(GameCommand::callLord(m_actions->localSeat(), bet)));
}



void GamePanel::onChatSend(const QString &msg)
{
    m_chatPanel->addMessage(m_gameControl->getUserPlayer()->getName(), msg);
    if (m_online->isOnline() && m_online && m_online->isConnected())
        m_online->sendChat(msg);
}

void GamePanel::showActionResult(const GameActionResult &result)
{
    if (!result.submitted) {
        const QString message = result.error == GameError::CannotPassWhenLeading
            ? QStringLiteral("领出者不能不出，请选择一手牌")
            : result.error == GameError::HandDoesNotBeatPending ? QStringLiteral("打不过上家")
            : result.error == GameError::InvalidHand ? QStringLiteral("无效牌型")
            : QStringLiteral("当前不能执行此操作");
        m_statusLabel->setText(message);
        return;
    }
    m_tableView->clearSelection();
    if (result.waitingForServer) {
        setPlayButtonsVisible(false);
        setCallButtonsVisible(false);
        m_actions->stopCountdown();
        m_timerLabel->hide();
        m_statusLabel->setText(QStringLiteral("等待服务器确认..."));
    }
}
void GamePanel::onCountdownChanged(int seconds)
{
    m_timerLabel->setVisible(seconds > 0);
    m_timerLabel->setText(QString::number(seconds));
    m_timerLabel->setStyleSheet(seconds <= 5
        ? "color:red;background:rgba(0,0,0,180);border-radius:18px;"
        : "color:orange;background:rgba(0,0,0,150);border-radius:18px;");
}
void GamePanel::onIdleWarning()
{
    if (!m_online->isOnline() && m_chaosEngine->isActive() && m_actions->isOurTurn()) {
        Player *user = m_gameControl->getUserPlayer();
        if (m_gameControl->tryIdleVanishFor(user))
            emit m_chaosEngine->chaosTriggered(QStringLiteral("耄耋规则：发呆太久，一张手牌消失了"));
    }
}
void GamePanel::onShowScoreboard()
{
    QStringList names;
    if (m_online->isOnline()) {
        for (int id = 1; id <= 3; ++id) names << relayNameForPlayerId(id);
    } else names << m_userNameLabel->text() << m_leftNameLabel->text() << m_rightNameLabel->text();
    m_scoreboard->setSeatNames(names);
    m_scoreboard->show();
    m_scoreboard->raise();
}
