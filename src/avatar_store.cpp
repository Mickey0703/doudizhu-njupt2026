#include "avatar_store.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QDebug>

AvatarStore::AvatarStore(QObject *parent) : QObject(parent) {}

bool AvatarStore::load(const QString &dirname)
{
    QDir dir(dirname);
    if (!dir.exists()) { qWarning() << "AvatarStore: directory not found:" << dirname; return false; }
    m_dir = dir.absolutePath() + "/";
    m_list.clear(); m_cache.clear();

    QFile idxFile(m_dir + "avatar_index.json");
    if (!idxFile.open(QIODevice::ReadOnly)) { qWarning() << "AvatarStore: cannot open avatar_index.json"; return false; }

    QJsonDocument doc = QJsonDocument::fromJson(idxFile.readAll());
    idxFile.close();
    QJsonArray arr = doc.array();
    for (auto v : arr) {
        QJsonObject obj = v.toObject();
        AvatarInfo info;
        info.id = obj["id"].toString();
        info.name = obj["name"].toString();
        info.file = obj["file"].toString();
        m_list.append(info);
    }

    for (const auto &info : m_list) {
        QString path = m_dir + info.file;
        QPixmap pix(path);
        if (pix.isNull()) { qWarning() << "AvatarStore: cannot load:" << path; continue; }
        m_cache[info.id] = pix;
    }
    return !m_cache.isEmpty();
}

bool AvatarStore::isLoaded() const { return !m_cache.isEmpty(); }
QVector<AvatarInfo> AvatarStore::list() const { return m_list; }
QPixmap AvatarStore::getAvatar(const QString &id) const { return m_cache.value(id); }
QPixmap AvatarStore::getDefaultAvatar() const { return m_list.isEmpty() ? QPixmap() : m_cache.value(m_list.first().id); }
QPixmap AvatarStore::getRandomAvatar() const {
    if (m_list.isEmpty()) return QPixmap();
    int idx = QRandomGenerator::global()->bounded(m_list.size());
    return m_cache.value(m_list[idx].id);
}
