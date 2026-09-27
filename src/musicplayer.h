#ifndef MUSICPLAYER_H
#define MUSICPLAYER_H

#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QSoundEffect>
#include <QMap>
#include <QStringList>
#include <QRandomGenerator>
#include "playhand.h"

/**
 * @brief 音乐/音效播放器 —— 管理 BGM 和 SFX
 *
 * 架构：
 * - BGM：使用 QMediaPlayer（支持循环播放）
 * - SFX：使用 QSoundEffect（低延迟、适合短音效）
 *
 * 音效文件按性别分类（Man_* 和 Woman_*），
 * 通过 setGender() 切换男女声播报。
 * 每个 SFX 枚举可能对应多个文件（随机选择增加变化感）。
 */
class MusicPlayer : public QObject
{
    Q_OBJECT

public:
    explicit MusicPlayer(QObject *parent = nullptr);

    // ============ BGM 背景音乐 ============

    enum BGM
    {
        BGM_None,      ///< 无
        BGM_MainMenu,  ///< 主菜单背景音乐
        BGM_Playing,   ///< 游戏中对战背景音乐
    };

    void playBGM(BGM bgm);
    void playBGM(const QString &path);   ///< 播放指定路径的 BGM（优先用于主题 BGM）
    void stopBGM();
    void pauseBGM();
    void resumeBGM();

    // ============ SFX 音效 ============

    enum SFX
    {
        SFX_CallLord1, SFX_CallLord2, SFX_CallLord3, // 叫地主 1/2/3 分
        SFX_NoCall,                                    // 不叫
        SFX_NoRob,                                     // 不抢
        SFX_PlayCard,                                  // 出牌通用
        SFX_Pass1, SFX_Pass2, SFX_Pass3, SFX_Pass4,   // 不要（多个变体）
        SFX_Bomb, SFX_Rocket,                          // 炸弹 / 火箭
        SFX_ShunZi, SFX_LianDui, SFX_FeiJi,            // 顺子 / 连对 / 飞机
        SFX_SanDaiYi, SFX_SanDaiYiDui,                 // 三带一 / 三带二
        SFX_SiDaiEr, SFX_SiDaiLiangDui,                // 四带二 / 四带两对
        SFX_DuiZi1, SFX_DuiZi2,                        // 对子（多个变体）
        SFX_DanPai1, SFX_DanPai2,                      // 单张（多个变体）
        SFX_SelectCard,                                // 选牌点击
        SFX_Win, SFX_Lose,                             // 输赢
        SFX_Spring,                                    // 春天
        SFX_DealCard,                                  // 发牌
    };

    /// 播放指定音效
    void playSFX(SFX sfx);

    /// 根据 PlayHand::HandType 自动选择对应音效
    static SFX sfxForHandType(PlayHand::HandType handType);
    void playSFXForHandType(PlayHand::HandType handType);

    // ============ 设置 ============

    void setVolume(int volume);     ///< 设置 BGM 音量 (0-100)
    void setSFXVolume(int volume);  ///< 设置音效音量 (0-100)
    void setMuted(bool muted);      ///< 静音
    void setGender(bool isMale);    ///< 设置性别（true=男声, false=女声）

    // ============ 主题音效支持 ============
    void playEventSFX(const QString &path);
    void setBGMVolume(int volume);

private:
    // BGM
    QMediaPlayer *m_bgmPlayer; ///< 背景音乐播放器
    QAudioOutput *m_bgmOutput; ///< 背景音乐音频输出

    // SFX
    QMap<SFX, QStringList> m_sfxMap;       ///< 音效枚举 → 文件路径列表
    bool m_isMale = true;                  ///< 当前性别（控制声音选择）
    qreal m_sfxVolume = 0.7;               ///< 音效音量 (0.0-1.0)
    bool m_muted = false;                  ///< 是否静音（同时作用于 BGM 与 SFX）

    /// 初始化所有音效文件路径
    void initSFX();

    /// 播放一个一次性音效：每次新建 QSoundEffect，播完自动销毁。
    /// 避免复用单个实例时快速连续音效互相截断。
    void playOneShot(const QString &path);

    /// 从列表中随机选一个文件路径
    QString randomFromList(const QStringList &list);
};

#endif // MUSICPLAYER_H
