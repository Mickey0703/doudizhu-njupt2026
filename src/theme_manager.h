#ifndef THEME_MANAGER_H
#define THEME_MANAGER_H

#include <QObject>
#include <QPixmap>
#include <QString>
#include <QMap>
#include <QVector>
#include "card.h"

/// 主题配置结构体（从 theme.json 反序列化）
struct ThemeConfig
{
    QString name;
    QString displayName;
    QString description;
    QStringList robotNames;
    QStringList robotGender;
    QString cardBack;
    QString cardFaceDir;
    QString cardFaceSuffix;
    QString cardNaming;
    QString bgm;
};

/// 混沌配置（仅耄耋主题，从 chaos.json 反序列化）
struct ChaosConfig
{
    double disappearChance = 0.0;
    double transformChance = 0.0;
    QStringList disappearTiming;
    QStringList transformTiming;
};

class ThemeManager : public QObject
{
    Q_OBJECT
public:
    explicit ThemeManager(QObject *parent = nullptr);

    bool loadTheme(const QString &dirPath);
    /// 卸载当前主题，isLoaded() 恢复 false，牌背/牌面/BGM/音效全部回退默认。
    /// 联机模式进房时调用，避免上一局单机主题残留到联机（各端主题不同步）。
    void clear();
    bool isLoaded() const;
    ThemeConfig config() const;
    ChaosConfig chaosConfig() const;

    QPixmap cardFace(Card::CardPoint pt, Card::CardSuit suit) const;
    QPixmap cardBack() const;

    QString sfxPath(const QString &eventType) const;
    QString bgmPath() const;

    QVector<QPixmap> stickers() const;
    QPixmap sticker(int index) const;

    static QString cardFileName(Card::CardPoint pt, Card::CardSuit suit,
                                const QString &naming, const QString &suffix);
    static QStringList availableThemes(const QString &themesRootDir);

private:
    QString resolveCardName(Card::CardPoint pt, Card::CardSuit suit) const;
    static QString suitToLetter(Card::CardSuit suit);
    static QString pointToStr(Card::CardPoint pt);

    QString m_dir;
    ThemeConfig m_config;
    ChaosConfig m_chaosConfig;
    QPixmap m_cardBack;
    mutable QMap<QString, QPixmap> m_cardFaceCache;
    QVector<QPixmap> m_stickers;
    QStringList m_sfxPaths;
};

#endif // THEME_MANAGER_H
