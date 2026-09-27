#ifndef AVATAR_STORE_H
#define AVATAR_STORE_H

#include <QObject>
#include <QPixmap>
#include <QVector>
#include <QString>
#include <QMap>

struct AvatarInfo
{
    QString id;
    QString name;
    QString file;
};

class AvatarStore : public QObject
{
    Q_OBJECT
public:
    explicit AvatarStore(QObject *parent = nullptr);
    bool load(const QString &dirname);
    QVector<AvatarInfo> list() const;
    QPixmap getAvatar(const QString &id) const;
    QPixmap getDefaultAvatar() const;
    QPixmap getRandomAvatar() const;
    bool isLoaded() const;

private:
    QString m_dir;
    QVector<AvatarInfo> m_list;
    QMap<QString, QPixmap> m_cache;
};

#endif
