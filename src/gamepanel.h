#ifndef GAMEPANEL_H
#define GAMEPANEL_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QMap>
#include <QLineEdit>
#include <QComboBox>
#include"cardpanel.h"
#include "card.h"
#include "cards.h"
#include "gamecontrol_adapter.h"
#include "musicplayer.h"
#include "chatpanel.h"
#include "online_room_controller.h"
#include "card_table_view.h"
#include "game_action_controller.h"
#include "game_record_model.h"
#include "scoreboard_dialog.h"

#include "theme_manager.h"
#include "chaos_engine.h"
#include "sticker_store.h"
#include "avatar_store.h"

class CardPanel;
class QWidget;
class QMovie;

QT_BEGIN_NAMESPACE
namespace Ui { class GamePanel; }
QT_END_NAMESPACE

/**
 * Window composition and presentation callbacks. Card selection/rendering,
 * room/network ownership, user commands/countdown, and public history live in
 * dedicated components. The window never owns a second authoritative state.
 */
class GamePanel : public QMainWindow
{
    Q_OBJECT

public:
    explicit GamePanel(QWidget *parent = nullptr);
    ~GamePanel() override;

protected:
    /// 窗口尺寸变化时重排 UI 和背景
    void resizeEvent(QResizeEvent *event) override;

private slots:
    // ============ 菜单按钮 ============
    void onVsAI();
    void onCreateRoom();
    void onJoinRoom();

    // ============ 游戏流程控制 ============
    void onStartGame();
    void onRestartGame();
    void onBackToMenu();

    // ============ 游戏状态回调 ============
    void onGameStatusChanged(GameControlAdapter::GameStatus status);
    void onPlayerStatusChanged(Player *player, GameControlAdapter::PlayerStatus status);
    void onNotifyCallLord(Player *player, int bet, bool isFirst);
    void onNotifyPlayHand(Player *player, const Cards &cards);

    // ============ 用户操作 ============
    void onCardSelected(int cardId);   ///< 选牌
    void onBtnPlay();                  ///< 出牌
    void onBtnPass();                  ///< 不出
    void onBtnHint();                  ///< 提示
    void onBtnCallLord(int bet);       ///< 叫地主

    void onCountdownChanged(int seconds);
    void onIdleWarning();
    void onRemoteChat(int playerId, const QString &text);
    void onShowScoreboard();
    void showActionResult(const GameActionResult &result);

    // ============ 聊天 ============
    void onChatSend(const QString &msg);

    // ============ Relay 回调 ============
    void onRelayConnected();
    void onRelayDisconnected();
    void onRoomCreated(quint32 roomId);
    void onRoomJoined(quint32 roomId, int myPid, const QList<RemotePlayer> &players);
    void onRoomJoinFailed(const QString &reason);
    void onRemotePlayerJoined(int playerId, const QString &name, const QString &avatarId);
    void onRemotePlayerLeft(int playerId);
    void onRoomDissolved(const QString &reason);
    void onRelayGameStarted();
    void onRelayError(const QString &error);

    // ============ 服务器权威事件（RemoteGameSession） ============
    void onRemoteGameEvent(const GameEvent &event);
    void onRemoteCommandRejected(GameError error);
    void onChaosMode();
    void onAvatarSelect();
    void onChaosStickerPopup(const QPixmap &sticker, QPoint pos, int durationMs);
    void onChaosShake(int intensity);


private:
    // ============ 初始化 ============
    void initUI();           ///< 创建所有 UI 控件
    void initConnections();  ///< 连接 GameControl 的信号
    void initGame();         ///< 创建 GameControl、MusicPlayer 等
    void initBackground();   ///< 加载并设置全屏背景图
    void applyBackgroundScaled();///< 按当前窗口尺寸等比铺满背景（初始 + resize 共用）

    // ============ 模式切换 ============
    void enterGameMode();    ///< 进入游戏模式
    void enterMenuMode();    ///< 进入主菜单模式

    // ============ 卡牌渲染 ============
    void updateAllPlayerCards();              ///< 刷新所有玩家手牌显示
    void updateUserCards();                   ///< 刷新用户手牌
    void updateRobotCards(Player *robot);     ///< 刷新机器人手牌（最多显示6张背面）
    void updatePlayArea(Player *player, const Cards &cards); ///< 刷新出牌区（带动画）
    void updateLordCards();                   ///< 刷新底牌区
    void updateLordIcons();                   ///< 刷新地主图标
    void clearPlayArea();                     ///< 清空出牌区
    void clearLordCards();                    ///< 清空底牌区

    // ============ 布局编排 ============
    void arrangeUserCards();                  ///< 排列用户手牌（选中上移15px）
    void arrangeRobotCards(Player *robot);    ///< 排列机器人手牌（背面堆叠）
    void arrangePlayArea();                   ///< 排列出牌区
    void arrangeLordArea();                   ///< 排列底牌区
    void arrangeMenuButtons();                ///< 排列主菜单按钮

    // ============ 按钮显隐 ============
    void setCallButtonsVisible(bool v);   ///< 叫地主按钮
    void setPlayButtonsVisible(bool v);   ///< 出牌/不出/提示按钮
    void setMenuButtonsVisible(bool v);   ///< 主菜单按钮
    void raiseGameButtons();              ///< 确保游戏按钮在最上层（不被卡牌遮挡）

    /// 在 gamecontrol 中查找对应的实际 Player 指针
    Player *findPlayerPtr(Player *p);
    int localRelayPlayerId() const;
    Player *playerForRelayId(int playerId) const;
    int relayIdForPlayer(Player *player) const;
    QString relayNameForPlayerId(int playerId) const;
    void configureOnlinePlayers();
    void resetOnlineState();
    void resetThemeForOnline();               ///< 进房时卸载单机主题/混沌，避免残留漏进联机
    void updateRoomStartVisibility();
    void applyRemoteTurnUI(Player *current);  ///< 依服务器视图阶段刷新当前玩家的操作 UI
    int remoteHandCountFor(Player *player) const; ///< 联机模式下某玩家剩余牌数（来自服务器视图）
    void renderAvatarTo(QLabel *label, const QString &avatarId, const QColor &ring); ///< 将头像绘成圆形贴到座位标签

    Ui::GamePanel *ui;
    CardTableView *m_tableView = nullptr;
    OnlineRoomController *m_online = nullptr;
    GameActionController *m_actions = nullptr;
    GameRecordModel *m_records = nullptr;
    ScoreboardDialog *m_scoreboard = nullptr;
    QPushButton *m_btnScoreboard = nullptr;

    // ============ 背景 ============
    QLabel *m_bgLabel = nullptr;      ///< 全屏背景 Label
    QPixmap m_bgSource;     ///< 原始背景图（用于 resize 缩放裁剪）

    // ============ 核心组件 ============
    GameControlAdapter *m_gameControl = nullptr;   ///< 游戏逻辑控制器（新架构）
    MusicPlayer *m_musicPlayer = nullptr;          ///< 音乐/音效播放器


    // ============ 层级容器 ============
    QWidget *m_cardLayer = nullptr;    ///< 卡牌层（背景之上，按钮和卡牌均挂载于此层）

    // ============ 玩家信息面板 ============
    QLabel *m_leftNameLabel = nullptr;
    QLabel *m_rightNameLabel = nullptr;
    QLabel *m_userNameLabel = nullptr;
    QLabel *m_userAvatar = nullptr;      QLabel *m_leftAvatar = nullptr;    QLabel *m_rightAvatar = nullptr;
    QLabel *m_userLordIcon = nullptr;    QLabel *m_leftLordIcon = nullptr;  QLabel *m_rightLordIcon = nullptr;

    // ============ 主菜单按钮 ============
    QPushButton *m_btnVsAI = nullptr;
    QPushButton *m_btnCreateRoom = nullptr;
    QPushButton *m_btnJoinRoom = nullptr;

    // ============ 游戏操作按钮 ============
    QPushButton *m_btnStart = nullptr;
    QPushButton *m_btnRestart = nullptr;
    QPushButton *m_btnBackMenu = nullptr;
    QPushButton *m_btnPlay = nullptr;
    QPushButton *m_btnPass = nullptr;
    QPushButton *m_btnHint = nullptr;
    QPushButton *m_btnCall1 = nullptr;   ///< 叫1分
    QPushButton *m_btnCall2 = nullptr;   ///< 叫2分
    QPushButton *m_btnCall3 = nullptr;   ///< 叫3分
    QPushButton *m_btnNoCall = nullptr;  ///< 不叫

    // ============ 界面辅助 ============
    QLabel *m_timerLabel = nullptr;          ///< 倒计时标签
    QLabel *m_statusLabel = nullptr;         ///< 状态文本（"叫地主阶段"/"请出牌"等）
    QLabel *m_titleLabel = nullptr;          ///< 菜单标题 "🃏 斗 地 主"
    QLabel *m_titleLogo = nullptr; ///< 标题左侧 logo（images/logo/title.gif 动图优先，否则 title.png）
    QMovie *m_titleMovie = nullptr;///< 标题 logo 动图（GIF）播放器；静态图时为空
    bool m_hasTitleLogo = false;   ///< 是否成功加载到标题 logo（动图或静态图），驱动显隐/布局
    ChatPanel *m_chatPanel = nullptr;        ///< 聊天面板
    QLabel *m_roomIdLabel = nullptr;      ///< 房间号显示（房主可见）
    QLabel *m_playerListLabel = nullptr;  ///< 房间内玩家列表
    // ============ 主题/混沌/头像系统 ============
    ThemeManager *m_themeManager = nullptr;
    ChaosEngine *m_chaosEngine = nullptr;
    StickerStore *m_stickerStore = nullptr;
    AvatarStore *m_avatarStore = nullptr;
    QString m_playerAvatarId;
    QString m_leftBotAvatarId;
    QString m_rightBotAvatarId;

    // ============ 主题选择 UI ============
    QPushButton *m_btnChaosMode = nullptr;
    QPushButton *m_btnAvatar = nullptr;
    QPushButton *m_btnConfirmTheme = nullptr;
    QComboBox *m_themeCombo = nullptr;
    QLabel *m_chaosBanner = nullptr;
    QStringList m_themeNames;
    int m_passStreak = 0;

};

#endif // GAMEPANEL_H
