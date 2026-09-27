#include "gamecontrol_adapter.h"
#include "game_core/game_command.h"
#include "game_core/game_event.h"

GameControlAdapter::GameControlAdapter(QObject *parent)
    : QObject(parent)
    , m_session(nullptr)
    , m_robotLeft(nullptr)
    , m_robotRight(nullptr)
    , m_user(nullptr)
    , m_currPlayer(nullptr)
    , m_pendPlayer(nullptr)
    , m_chaosEngine(nullptr)
    , m_sessionActive(false)
{
    m_session = new LocalSession(this);
    setupConnections();
}

GameControlAdapter::~GameControlAdapter()
{
    delete m_robotLeft;
    delete m_robotRight;
    delete m_user;
}

void GameControlAdapter::setupConnections()
{
    connect(m_session, &LocalSession::eventEmitted,
            this, &GameControlAdapter::onSessionEvent);
    connect(m_session, &LocalSession::roundFinished,
            this, &GameControlAdapter::onSessionRoundFinished);
    // Chaos transform happens inside the session (AI path). Forward it out,
    // mapping seat → Player, so GamePanel can show the "变异" banner.
    connect(m_session, &LocalSession::chaosCardsTransformed, this,
            [this](int seat, const Card &from, const Card &to) {
        emit chaosCardsTransformed(seatToPlayer(seat), from, to);
    });
}

void GameControlAdapter::playerInit()
{
    // Create Player objects (used only as data containers, not for logic)
    m_robotLeft = new Robot(this);
    m_robotLeft->setDirection(Player::Left);
    m_robotLeft->setType(Player::Robot);

    m_robotRight = new Robot(this);
    m_robotRight->setDirection(Player::Right);
    m_robotRight->setType(Player::Robot);

    m_user = new UserPlayer(this);
    m_user->setDirection(Player::Left);  // User doesn't have direction
    m_user->setType(Player::User);

    // Wire the human player's UI-driven actions into the engine. When the user
    // clicks a bid/play/pass button GamePanel calls m_user->grabLordBet()/
    // playHand(), which emit these signals; without these connections the
    // command never reaches LocalSession and the game stalls on the user's turn.
    // Only the user is wired: AI seats are driven internally by LocalSession,
    // so connecting robots here would double-issue their moves.
    connect(m_user, &Player::notifyGrabLordBet, this, &GameControlAdapter::onGrabBet);
    connect(m_user, &Player::notifyPlayHand,    this, &GameControlAdapter::onPlayHand);

    // Set up circular linked list (for compatibility)
    m_robotLeft->setPrevPlayer(m_user);
    m_robotLeft->setNextPlayer(m_robotRight);

    m_robotRight->setPrevPlayer(m_robotLeft);
    m_robotRight->setNextPlayer(m_user);

    m_user->setPrevPlayer(m_robotRight);
    m_user->setNextPlayer(m_robotLeft);

    // Configure session: user is seat 0, AIs are seats 1 and 2
    m_session->setPlayerType(0, PlayerType::User);
    m_session->setPlayerType(1, PlayerType::AI);
    m_session->setPlayerType(2, PlayerType::AI);
}

void GameControlAdapter::resetCardData()
{
    // Start a new round in the session
    m_session->setAIEnabled(true);
    m_sessionActive = true; // initial events must see an active presentation
    m_sessionActive = m_session->startRound();

    if (m_sessionActive) {
        // Sync initial state to Player objects
        syncStateToPlayers();

        // Emit game status changed
        emit gameStatusChanged(DispatchCard);
    }
}

void GameControlAdapter::startLordCard()
{
    // Trigger bidding phase - already started by resetCardData
    if (m_sessionActive && m_session->currentPhase() == GamePhase::CallingLord) {
        emit gameStatusChanged(CallingLord);

        // gameStatusChanged(CallingLord) just hid all turn buttons in the UI.
        // Re-announce whose turn it is so the first bidder's UI is correct.
        // If the current seat is AI, trigger its move; otherwise surface the
        // bid buttons for the human player (TurnChanged handles every later
        // turn, but the opening turn happens before this status change).
        if (m_session->currentPlayerType() == PlayerType::AI) {
            m_session->triggerAIMove();
        } else if (Player *cur = seatToPlayer(m_session->currentSeat())) {
            emit playerStatusChanged(cur, ThinkingForCallLord);
        }
    }
}

void GameControlAdapter::becomeLord(Player *player, int bet)
{
    // bet 是兼容旧 GameControl API 的保留参数：真正的倍数由引擎/LordSelected 事件管理，
    // 此处仅更新 Player 对象角色，故不使用 bet。签名不能改，显式标注消除告警。
    Q_UNUSED(bet);
    // This is called when lord is selected - update Player objects
    if (player) {
        player->setRole(Player::Lord);
        emit playerStatusChanged(player, PickingCard);
    }
}

void GameControlAdapter::disable()
{
    m_sessionActive = false;
    m_session->setAIEnabled(false);
}

CommandResult GameControlAdapter::executeCommand(const GameCommand &command)
{
    if (!m_sessionActive || !m_userInputDrivesEngine)
        return CommandResult::rejected(GameError::InvalidPhase);
    const auto result = m_session->executeCommand(command);
    syncStateToPlayers();
    return result;
}

void GameControlAdapter::onGrabBet(Player *player, int bet)
{
    // Online modes route bids through the network (host authority / server
    // authority), not the local engine. Only drive LocalSession in single-player.
    if (!m_userInputDrivesEngine)
        return;

    const int seat = playerToSeat(player);
    if (seat == -1 || !m_sessionActive)
        return;

    // Execute bid command
    const CommandResult result = m_session->executeCommand(
        GameCommand::callLord(seat, bet));

    if (result.accepted) {
        // Events will be handled in onSessionEvent
        syncStateToPlayers();
    }
}

void GameControlAdapter::onPlayHand(Player *player, const Cards &card)
{
    // Online modes route plays through the network, not the local engine.
    if (!m_userInputDrivesEngine)
        return;

    const int seat = playerToSeat(player);
    if (seat == -1 || !m_sessionActive)
        return;

    CommandResult result;

    if (card.isEmpty()) {
        // Pass
        result = m_session->executeCommand(GameCommand::pass(seat));
    } else {
        // Play cards
        result = m_session->executeCommand(GameCommand::playCards(seat, card));
    }

    // Player::playHand() already removed the cards from the Player cache before
    // emitting. If the engine rejected the command the cache is now out of sync
    // with authoritative state, so re-assert engine truth in both cases.
    // (accepted: events also sync; rejected: this restores the removed cards.)
    syncStateToPlayers();
}

void GameControlAdapter::syncStateToPlayers()
{
    if (!m_sessionActive)
        return;

    const GameState &state = m_session->fullState();

    // Sync hands - only if changed to avoid unnecessary operations
    if (m_user && state.players[0].hand != m_user->getCards()) {
        m_user->clearCards();
        m_user->storeDispatchCard(state.players[0].hand);
    }
    if (m_robotLeft && state.players[1].hand != m_robotLeft->getCards()) {
        m_robotLeft->clearCards();
        m_robotLeft->storeDispatchCard(state.players[1].hand);
    }
    if (m_robotRight && state.players[2].hand != m_robotRight->getCards()) {
        m_robotRight->clearCards();
        m_robotRight->storeDispatchCard(state.players[2].hand);
    }

    // Sync roles
    if (state.phase == GamePhase::Playing || state.phase == GamePhase::RoundFinished) {
        if (m_user)
            m_user->setRole(state.players[0].role == PlayerRole::Lord ? Player::Lord : Player::Farmer);
        if (m_robotLeft)
            m_robotLeft->setRole(state.players[1].role == PlayerRole::Lord ? Player::Lord : Player::Farmer);
        if (m_robotRight)
            m_robotRight->setRole(state.players[2].role == PlayerRole::Lord ? Player::Lord : Player::Farmer);
    }

    // Sync pending cards
    m_pendCards = state.pendingCards;
    if (isValidSeat(state.pendingSeat)) {
        m_pendPlayer = seatToPlayer(state.pendingSeat);
    } else {
        m_pendPlayer = nullptr;
    }

    // Sync current player
    if (isValidSeat(state.currentSeat)) {
        m_currPlayer = seatToPlayer(state.currentSeat);
    } else {
        m_currPlayer = nullptr;
    }
}

void GameControlAdapter::onSessionEvent(const GameEvent &event)
{
    if (!m_sessionActive)
        return;
    // Renderers must see the already-applied authoritative state, not the
    // previous Player cache. Observation is read-only (scoreboard/history).
    syncStateToPlayers();
    emit gameEventObserved(event);
    emitEventAsSignals(event);

    // Note: AI triggering is handled by LocalSession internally
    // scheduleAIMove() already chains AI moves, no need to trigger here
}

void GameControlAdapter::onSessionRoundFinished(int winnerSeat)
{
    Player *winner = seatToPlayer(winnerSeat);
    if (winner) {
        emit gameStatusChanged(PlayingHand);
        emit playerStatusChanged(winner, Winning);
    }

    m_sessionActive = false;
}

void GameControlAdapter::emitEventAsSignals(const GameEvent &event)
{
    Player *player = seatToPlayer(event.seat);

    switch (event.type) {
    case GameEventType::RoundStarted:
        emit gameStatusChanged(DispatchCard);
        break;

    case GameEventType::PrivateHandDealt:
        // Dealt to every seat in a loop; this is not a turn signal. Whose turn
        // it is to bid is announced by startLordCard() / TurnChanged, so do not
        // toggle turn buttons here (doing so left the human's bid buttons hidden
        // because the last robot's emit overrode the user's).
        break;

    case GameEventType::BidAccepted:
        if (player) {
            // Third parameter: true if this is a new high bid, false if pass (bid=0)
            const bool isRealBid = (event.value > 0);
            emit notifyGrabLordBet(player, event.value, isRealBid);
        }
        break;

    case GameEventType::LordSelected:
        if (player) {
            becomeLord(player, event.value);
            emit gameStatusChanged(PlayingHand);
        }
        break;

    case GameEventType::CardsPlayed:
        if (player) {
            emit notifyPlayHand(player, event.cards);
            emit pendingInfo(player, event.cards);
        }
        break;

    case GameEventType::PlayerPassed:
        if (player) {
            emit notifyPlayHand(player, Cards());
        }
        break;

    case GameEventType::TurnChanged:
        // Check if pending was cleared (new round started by leader)
        if (m_pendPlayer != nullptr && m_session->fullState().pendingCards.isEmpty()) {
            emit pendingInfo(nullptr, Cards());
        }
        // Announce whose turn it is so the UI shows the right buttons (bid
        // buttons while calling lord, play buttons while playing). This is the
        // single source of turn truth for the human player; AI seats keep
        // moving via LocalSession's internal scheduleAIMove().
        if (player) {
            switch (m_session->currentPhase()) {
            case GamePhase::CallingLord:
                emit playerStatusChanged(player, ThinkingForCallLord);
                break;
            case GamePhase::Playing:
                emit playerStatusChanged(player, ThinkingForPlayHand);
                break;
            default:
                break;
            }
        }
        break;

    case GameEventType::TrickReset:
        // 引擎在两家连续过牌、一轮结束时专门发此事件（TurnChanged 传达不了这一点）。
        // 通知 UI 清空桌面：领出者将开新一轮，桌面不应再停留在上一手被压/被过的牌，
        // 否则玩家会误以为下一手小牌「压」过了上一手大牌。TurnChanged 里那处基于
        // m_pendPlayer 的清空判断在单机不可靠——onSessionEvent 每事件都会 sync，
        // PlayerPassed 处理时 m_pendPlayer 已被置空，故必须用本事件兜底。
        emit pendingInfo(nullptr, Cards());
        break;

    case GameEventType::RoundFinished:
        // Handled in onSessionRoundFinished
        break;

    case GameEventType::ScoreChanged:
        // Update player scores
        if (player) {
            player->setScore(m_session->fullState().players[event.seat].score);
        }
        break;

    case GameEventType::RoundVoided:
        emit gameStatusChanged(DispatchCard);
        break;

    case GameEventType::MultiplierChanged:
        // Multiplier changed (bomb played) - could emit notification
        break;

    case GameEventType::CardVanished:
        // Chaos mode: cards vanished from event.seat's hand. The hand itself is
        // re-rendered by syncStateToPlayers() (authoritative state already
        // updated). Drive the banner/effect per vanished card.
        if (player) {
            const CardList gone = event.cards.toCardList(Cards::NoSort);
            for (const Card &c : gone)
                emit chaosCardDisappeared(player, c);
        }
        break;

    default:
        break;
    }
}

Player *GameControlAdapter::seatToPlayer(int seat) const
{
    switch (seat) {
    case 0: return m_user;
    case 1: return m_robotLeft;
    case 2: return m_robotRight;
    default: return nullptr;
    }
}

int GameControlAdapter::playerToSeat(Player *player) const
{
    if (player == m_user)
        return 0;
    if (player == m_robotLeft)
        return 1;
    if (player == m_robotRight)
        return 2;
    return -1;
}

Cards GameControlAdapter::getSurplusCards() const
{
    // In new architecture, lord cards are in the GameState
    if (m_sessionActive) {
        return m_session->fullState().bottomCards;
    }
    return Cards();
}

void GameControlAdapter::setCurrentPlayer(Player *player)
{
    // This is only used in Relay mode to sync external state
    // The session manages currentPlayer internally
    m_currPlayer = player;
}

void GameControlAdapter::syncPendingInfo(Player *player, const Cards &cards)
{
    // This is only used in Relay mode to sync external state
    // The session manages pending info internally
    m_pendPlayer = player;
    m_pendCards = cards;

    // Emit signal for UI update
    emit pendingInfo(player, cards);
}

void GameControlAdapter::setChaosParams(bool active, double disappearChance,
                                        double transformChance, bool disappearOnDraw,
                                        bool transformOnPlay, bool disappearOnIdle)
{
    m_session->setChaosParams(active, disappearChance, transformChance,
                              disappearOnDraw, transformOnPlay, disappearOnIdle);
}

Cards GameControlAdapter::maybeTransformPlayerCards(const Cards &sel)
{
    // Player transform shares the authoritative session path (same as AI). The
    // transformed cards are returned so onBtnPlay can pre-validate + play them;
    // the banner fires via chaosCardsTransformed → this adapter → GamePanel.
    return m_session->transformPlayerPlay(sel);
}

bool GameControlAdapter::tryIdleVanishFor(Player *player)
{
    // Idle-timeout disappearance. Decision (roll) + execution both live in the
    // session now (authoritative state); the old cache-only path let the next
    // syncStateToPlayers() "resurrect" the card. The CardVanished event fired by
    // the session re-renders the hand; GamePanel adds the "发呆" banner on true.
    const int seat = playerToSeat(player);
    if (seat == -1)
        return false;
    return m_session->tryIdleVanish(seat);
}
