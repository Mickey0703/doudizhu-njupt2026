#ifndef STICKER_STORE_H
#define STICKER_STORE_H

#include <QObject>
#include <QPixmap>
#include <QVector>

class ThemeManager;

class StickerStore : public QObject
{
    Q_OBJECT
public:
    explicit StickerStore(QObject *parent = nullptr);
    void loadFromTheme(const ThemeManager *theme);
    QVector<QPixmap> all() const;
    QPixmap get(int index) const;
    int count() const;
    bool isLoaded() const;
    void loadFromDisk(const QString &themeDir);

private:
    QVector<QPixmap> m_stickers;
};

#endif
