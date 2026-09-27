#include "theme_manager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
{
}

bool ThemeManager::loadTheme(const QString &dirPath)
{
    QDir dir(dirPath);
    if (!dir.exists())
    {
        qWarning() << "ThemeManager: directory not found:" << dirPath;
        return false;
    }

    QFile configFile(dir.filePath("theme.json"));
    if (!configFile.open(QIODevice::ReadOnly))
    {
        qWarning() << "ThemeManager: cannot open theme.json in" << dirPath;
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(configFile.readAll());
    configFile.close();
    QJsonObject obj = doc.object();

    m_config.name        = obj["name"].toString();
    m_config.displayName = obj["displayName"].toString();
    m_config.description = obj["description"].toString();
    m_config.cardBack    = obj["cardBack"].toString();
    m_config.cardFaceDir  = obj["cardFaceDir"].toString();
    m_config.cardFaceSuffix = obj["cardFaceSuffix"].toString();
    m_config.cardNaming  = obj["cardNaming"].toString();
    m_config.bgm         = obj["bgm"].toString();

    QJsonArray rnames = obj["robotNames"].toArray();
    m_config.robotNames.clear();
    for (auto v : rnames) m_config.robotNames << v.toString();

    QJsonArray rgender = obj["robotGender"].toArray();
    m_config.robotGender.clear();
    for (auto v : rgender) m_config.robotGender << v.toString();

    m_dir = dir.absolutePath() + "/";
    m_cardFaceCache.clear();
    m_stickers.clear();

    // Load card back
    QString backPath = m_dir + m_config.cardBack;
    m_cardBack = QPixmap(backPath);

    // Load stickers
    QDir stickerDir(m_dir + "stickers");
    if (stickerDir.exists())
    {
        QStringList filters;
        filters << "*.png" << "*.jpg" << "*.jpeg";
        QStringList files = stickerDir.entryList(filters, QDir::Files, QDir::Name);
        for (const auto &f : files)
        {
            QPixmap p(stickerDir.filePath(f));
            if (!p.isNull())
                m_stickers.append(p);
        }
    }

    // Load chaos.json (optional)
    QFile chaosFile(dir.filePath("chaos.json"));
    if (chaosFile.exists() && chaosFile.open(QIODevice::ReadOnly))
    {
        QJsonDocument cdoc = QJsonDocument::fromJson(chaosFile.readAll());
        chaosFile.close();
        QJsonObject cobj = cdoc.object();
        m_chaosConfig.disappearChance = cobj["disappearChance"].toDouble(0.0);
        m_chaosConfig.transformChance = cobj["transformChance"].toDouble(0.0);
        QJsonArray dt = cobj["disappearTiming"].toArray();
        m_chaosConfig.disappearTiming.clear();
        for (auto v : dt) m_chaosConfig.disappearTiming << v.toString();
        QJsonArray tt = cobj["transformTiming"].toArray();
        m_chaosConfig.transformTiming.clear();
        for (auto v : tt) m_chaosConfig.transformTiming << v.toString();
    }
    else
    {
        m_chaosConfig = ChaosConfig();
    }

    // Register SFX paths (skip 0-byte placeholder files)
    m_sfxPaths.clear();
    QString sfxDir = m_dir + "sfx/";
    QStringList events = {"call_lord_1","call_lord_2","call_lord_3",
                          "bomb","rocket","win","lose","bgm"};
    for (const auto &ev : events)
    {
        QString p = sfxDir + ev;
        auto validSfx = [](const QString &path) {
            return QFile::exists(path) && QFileInfo(path).size() > 0;
        };
        if (validSfx(p + ".mp3"))
            m_sfxPaths << (p + ".mp3");
        else if (validSfx(p + ".wav"))
            m_sfxPaths << (p + ".wav");
        else
            m_sfxPaths << QString();    // 空字符串 → 静默跳过
    }

    return true;
}

void ThemeManager::clear()
{
    m_dir.clear();
    m_config = ThemeConfig();
    m_chaosConfig = ChaosConfig();
    m_cardBack = QPixmap();
    m_cardFaceCache.clear();
    m_stickers.clear();
    m_sfxPaths.clear();
}

bool ThemeManager::isLoaded() const
{
    return !m_config.name.isEmpty();
}

ThemeConfig ThemeManager::config() const
{
    return m_config;
}

ChaosConfig ThemeManager::chaosConfig() const
{
    return m_chaosConfig;
}

QPixmap ThemeManager::cardFace(Card::CardPoint pt, Card::CardSuit suit) const
{
    QString fname = resolveCardName(pt, suit);
    if (m_cardFaceCache.contains(fname))
        return m_cardFaceCache[fname];

    QString path = m_dir + m_config.cardFaceDir + fname;
    QPixmap pix(path);
    if (pix.isNull())
    {
        qWarning() << "ThemeManager: card face not found:" << path;
        return QPixmap();
    }
    m_cardFaceCache[fname] = pix;
    return pix;
}

QPixmap ThemeManager::cardBack() const
{
    return m_cardBack;
}

QString ThemeManager::sfxPath(const QString &eventType) const
{
    static QMap<QString,int> idxMap = {
        {"call_lord_1",0},{"call_lord_2",1},{"call_lord_3",2},
        {"bomb",3},{"rocket",4},{"win",5},{"lose",6},{"bgm",7}
    };
    int idx = idxMap.value(eventType, -1);
    if (idx < 0 || idx >= m_sfxPaths.size()) return QString();
    return m_sfxPaths[idx];
}

QString ThemeManager::bgmPath() const
{
    return sfxPath("bgm");
}

QVector<QPixmap> ThemeManager::stickers() const
{
    return m_stickers;
}

QPixmap ThemeManager::sticker(int index) const
{
    if (index < 0 || index >= m_stickers.size())
        return QPixmap();
    return m_stickers[index];
}

QString ThemeManager::resolveCardName(Card::CardPoint pt, Card::CardSuit suit) const
{
    return cardFileName(pt, suit, m_config.cardNaming, m_config.cardFaceSuffix);
}

QString ThemeManager::cardFileName(Card::CardPoint pt, Card::CardSuit suit,
                                    const QString &naming, const QString &suffix)
{
    if (pt == Card::Card_SJ) return "SJ" + suffix;
    if (pt == Card::Card_BJ) return "BJ" + suffix;

    QString n = naming;
    n.replace("{point}", pointToStr(pt));
    n.replace("{suit_letter}", suitToLetter(suit));
    return n + suffix;
}

QString ThemeManager::suitToLetter(Card::CardSuit suit)
{
    switch (suit) {
    case Card::Diamond: return "D";
    case Card::Club:    return "C";
    case Card::Heart:   return "H";
    case Card::Spade:   return "S";
    default: return "X";
    }
}

QString ThemeManager::pointToStr(Card::CardPoint pt)
{
    switch (pt) {
    case Card::Card_3:  return "3";  case Card::Card_4:  return "4";
    case Card::Card_5:  return "5";  case Card::Card_6:  return "6";
    case Card::Card_7:  return "7";  case Card::Card_8:  return "8";
    case Card::Card_9:  return "9";  case Card::Card_10: return "10";
    case Card::Card_J:  return "J";  case Card::Card_Q:  return "Q";
    case Card::Card_K:  return "K";  case Card::Card_A:  return "A";
    case Card::Card_2:  return "2";
    case Card::Card_SJ: return "SJ";
    case Card::Card_BJ: return "BJ";
    default: return "?";
    }
}

QStringList ThemeManager::availableThemes(const QString &themesRootDir)
{
    QDir root(themesRootDir);
    QStringList names;
    QStringList entries = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const auto &e : entries)
    {
        if (QFile::exists(root.filePath(e + "/theme.json")))
            names << e;
    }
    return names;
}
