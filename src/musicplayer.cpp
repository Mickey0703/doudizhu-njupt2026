#include "musicplayer.h"
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDebug>

// ============ 音效目录辅助函数 ============

/// 获取 WAV 音效目录 —— 多路径搜索
/// 优先级：构建目录 > 项目资源目录 > 源目录
static QString sfxDir()
{
    // 构建目录下的 resources/music/wav/
    QString buildPath = QCoreApplication::applicationDirPath() + "/resources/music/wav/";
    if (QDir(buildPath).exists())
        return buildPath;

    // 可执行文件同级 images → 回退路径
    QString alt1 = QCoreApplication::applicationDirPath() + "/music/wav/";
    if (QDir(alt1).exists())
        return alt1;

    // 源目录：从构建目录向上查找项目根目录
    QString srcPath = QCoreApplication::applicationDirPath() + "/../resources/music/wav/";
    if (QDir(srcPath).exists())
        return QDir(srcPath).absolutePath() + "/";

    // 最后回退到构建目录（即使不存在，也允许后续 QFile::exists 检查）
    return buildPath;
}

// ============ 构造 ============

MusicPlayer::MusicPlayer(QObject *parent)
    : QObject(parent)
    , m_bgmPlayer(new QMediaPlayer(this))
    , m_bgmOutput(new QAudioOutput(this))
{
    m_bgmPlayer->setAudioOutput(m_bgmOutput);
    m_bgmOutput->setVolume(0.4);   // 默认 BGM 音量 40%
    initSFX();
}

// ==================== BGM ====================

void MusicPlayer::playBGM(BGM bgm)
{
    // 主菜单与对局用不同曲目，避免进出游戏时听感无变化
    QString file;
    switch (bgm)
    {
    case BGM_MainMenu:
        file = "threeManBo.wav";
        break;
    case BGM_Playing:
        file = "manbo.wav";
        break;
    default:
        return;
    }
    const QString path = "qrc:/music/" + file;

    // 避免重复播放同一首
    if (m_bgmPlayer->source() == QUrl(path) &&
        m_bgmPlayer->playbackState() == QMediaPlayer::PlayingState)
        return;

    m_bgmPlayer->stop();
    m_bgmPlayer->setSource(QUrl(path));

    // QRC 加载失败时，尝试文件系统路径
    m_bgmPlayer->play();
    if (m_bgmPlayer->error() != QMediaPlayer::NoError)
    {
        // 在多个位置搜索 BGM 文件
        QString musicDir = sfxDir();
        QDir d(musicDir);
        d.cdUp(); // wav → music
        QStringList candidates = {
            d.absolutePath() + "/" + file,
            QCoreApplication::applicationDirPath() + "/resources/music/" + file,
            QCoreApplication::applicationDirPath() + "/../resources/music/" + file,
        };
        for (const auto &alt : candidates)
        {
            QString abs = QDir(alt).absolutePath();
            if (QFile::exists(abs))
            {
                m_bgmPlayer->setSource(QUrl::fromLocalFile(abs));
                m_bgmPlayer->play();
                break;
            }
        }
    }
    m_bgmPlayer->setLoops(QMediaPlayer::Infinite);
}

void MusicPlayer::playBGM(const QString &path)
{
    if (path.isEmpty() || !QFile::exists(path)) return;
    if (m_bgmPlayer->source() == QUrl::fromLocalFile(path) &&
        m_bgmPlayer->playbackState() == QMediaPlayer::PlayingState)
        return;
    m_bgmPlayer->stop();
    m_bgmPlayer->setSource(QUrl::fromLocalFile(path));
    m_bgmPlayer->setLoops(QMediaPlayer::Infinite);
    m_bgmPlayer->play();
}

void MusicPlayer::stopBGM()
{
    m_bgmPlayer->stop();
}

void MusicPlayer::pauseBGM()
{
    m_bgmPlayer->pause();
}

void MusicPlayer::resumeBGM()
{
    m_bgmPlayer->play();
}

// ==================== SFX ====================

void MusicPlayer::initSFX()
{
    // 音效目录
    QString dir = sfxDir();

    // 辅助 Lambda：补全文件名和扩展名
    auto f = [&](const QString &name)
    { return dir + name + ".wav"; };

    // ---- 注册所有音效 ----
    // 每个 SFX 通常有男女两个版本（Spec* 为特殊音效不分性别）
    m_sfxMap[SFX_CallLord1]    = { f("Man_Rob1"), f("Woman_Rob1") };
    m_sfxMap[SFX_CallLord2]    = { f("Man_Rob2"), f("Woman_Rob2") };
    m_sfxMap[SFX_CallLord3]    = { f("Man_Rob3"), f("Woman_Rob3") };
    m_sfxMap[SFX_NoCall]       = { f("Man_bujiabei"), f("Woman_bujiabei") };
    m_sfxMap[SFX_NoRob]        = { f("Man_NoRob"), f("Woman_NoRob") };
    m_sfxMap[SFX_Bomb]         = { f("Man_zhadan"), f("Woman_zhadan") };
    m_sfxMap[SFX_Rocket]       = { f("Man_wangzha"), f("Woman_wangzha") };
    m_sfxMap[SFX_ShunZi]       = { f("Man_shunzi"), f("Woman_shunzi") };
    m_sfxMap[SFX_LianDui]      = { f("Man_liandui"), f("Woman_liandui") };
    m_sfxMap[SFX_FeiJi]        = { f("Man_feiji"), f("Woman_feiji") };
    m_sfxMap[SFX_SanDaiYi]     = { f("Man_sandaiyi"), f("Woman_sandaiyi") };
    m_sfxMap[SFX_SanDaiYiDui]  = { f("Man_sandaiyidui"), f("Woman_sandaiyidui") };
    m_sfxMap[SFX_SiDaiEr]      = { f("Man_sidaier"), f("Woman_sidaier") };
    m_sfxMap[SFX_SiDaiLiangDui]= { f("Man_sidailiangdui"), f("Woman_sidailiangdui") };
    m_sfxMap[SFX_Pass1]        = { f("Man_buyao1"), f("Woman_buyao1") };
    m_sfxMap[SFX_Pass2]        = { f("Man_buyao2"), f("Woman_buyao2") };
    m_sfxMap[SFX_Pass3]        = { f("Man_buyao3"), f("Woman_buyao3") };
    m_sfxMap[SFX_Pass4]        = { f("Man_buyao4"), f("Woman_buyao4") };
    m_sfxMap[SFX_DanPai1]      = { f("Man_dani1"), f("Woman_dani1") };
    m_sfxMap[SFX_DanPai2]      = { f("Man_dani2"), f("Woman_dani2") };
    m_sfxMap[SFX_DuiZi1]       = { f("Man_dui1"), f("Woman_dui1") };
    m_sfxMap[SFX_DuiZi2]       = { f("Man_dui2"), f("Woman_dui2") };
    m_sfxMap[SFX_SelectCard]   = { f("SpecSelectCard") };
    m_sfxMap[SFX_DealCard]     = { f("SpecOk") };
    m_sfxMap[SFX_Win]          = { f("SpecBeanMore") };
    m_sfxMap[SFX_Lose]         = { f("SpecBeanLess") };
    m_sfxMap[SFX_Spring]       = { f("Man_baojing1"), f("Woman_baojing1") };
    // 通用出牌音效：sfxForHandType 对未归类牌型的默认返回值。
    // 之前未注册，playSFX(SFX_PlayCard) 直接静默返回。
    m_sfxMap[SFX_PlayCard]     = { f("Special_give") };
}

void MusicPlayer::playSFX(SFX sfx)
{
    if (!m_sfxMap.contains(sfx))
        return;

    QStringList &files = m_sfxMap[sfx];
    if (files.isEmpty())
        return;

    // 根据性别选择索引：男=0，女=1（单文件版本统一用 idx=0）
    int idx = 0;
    if (files.size() >= 2)
    {
        idx = m_isMale ? 0 : 1;
    }

    QString path = files[idx];

    // 如果文件不存在，尝试列表中的其他文件
    if (!QFile::exists(path))
    {
        for (const auto &f : files)
        {
            if (QFile::exists(f))
            {
                path = f;
                break;
            }
        }
    }

    playOneShot(path);
}

void MusicPlayer::playOneShot(const QString &path)
{
    if (m_muted || path.isEmpty() || !QFile::exists(path))
        return;
    auto *fx = new QSoundEffect(this);
    fx->setVolume(m_sfxVolume);
    fx->setSource(QUrl::fromLocalFile(path));
    connect(fx, &QSoundEffect::playingChanged, fx, [fx]() {
        if (!fx->isPlaying()) fx->deleteLater();
    });
    fx->play();
}

MusicPlayer::SFX MusicPlayer::sfxForHandType(PlayHand::HandType handType)
{
    switch (handType)
    {
    case PlayHand::Hand_Single:
        return SFX_DanPai1;
    case PlayHand::Hand_Pair:
        return SFX_DuiZi1;
    case PlayHand::Hand_Seq_Single:
        return SFX_ShunZi;
    case PlayHand::Hand_Seq_Pair:
        return SFX_LianDui;
    case PlayHand::Hand_Plane:
    case PlayHand::Hand_Plane_Two_Single:
    case PlayHand::Hand_Plane_Two_Pair:
        return SFX_FeiJi;
    case PlayHand::Hand_Triple:
    case PlayHand::Hand_Triple_Single:
        return SFX_SanDaiYi;
    case PlayHand::Hand_Triple_Pair:
        return SFX_SanDaiYiDui;
    case PlayHand::Hand_Bomb:
    case PlayHand::Hand_Bomb_Single:
    case PlayHand::Hand_Bomb_Pair:
    case PlayHand::Hand_Bomb_Two_Single:
        return SFX_Bomb;
    case PlayHand::Hand_Bomb_Jokers:
        return SFX_Rocket;
    default:
        return SFX_PlayCard;
    }
}

void MusicPlayer::playSFXForHandType(PlayHand::HandType handType)
{
    if (handType == PlayHand::Hand_Unknown || handType == PlayHand::Hand_Pass)
        return;
    playSFX(sfxForHandType(handType));
}

// ============ 音量控制 ============

void MusicPlayer::setVolume(int volume)
{
    m_bgmOutput->setVolume(qBound(0, volume, 100) / 100.0);
}

void MusicPlayer::setSFXVolume(int volume)
{
    m_sfxVolume = qBound(0, volume, 100) / 100.0;
}

void MusicPlayer::setMuted(bool muted)
{
    m_muted = muted;
    m_bgmOutput->setMuted(muted);
}

void MusicPlayer::setGender(bool isMale)
{
    m_isMale = isMale;
}

// ============ 主题音效支持 ============

void MusicPlayer::playEventSFX(const QString &path)
{
    if (path.isEmpty()) return;
    QFileInfo fi(path);
    if (!fi.exists() || fi.size() == 0) return;   // 跳过空占位文件
    playOneShot(path);
}

void MusicPlayer::setBGMVolume(int volume)
{
    m_bgmOutput->setVolume(qBound(0, volume, 100) / 100.0);
}

