#include "sticker_store.h"
#include "theme_manager.h"

StickerStore::StickerStore(QObject *parent) : QObject(parent) {}

void StickerStore::loadFromTheme(const ThemeManager *theme)
{
    if (!theme) { m_stickers.clear(); return; }
    m_stickers = theme->stickers();
}

QVector<QPixmap> StickerStore::all() const { return m_stickers; }
QPixmap StickerStore::get(int index) const {
    if (index < 0 || index >= m_stickers.size()) return QPixmap();
    return m_stickers[index];
}
int StickerStore::count() const { return m_stickers.size(); }
bool StickerStore::isLoaded() const { return !m_stickers.isEmpty(); }

void StickerStore::loadFromDisk(const QString &themeDir)
{
    ThemeManager tmp;
    if (tmp.loadTheme(themeDir))
        m_stickers = tmp.stickers();
}
