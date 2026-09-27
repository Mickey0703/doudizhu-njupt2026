#ifndef AVATAR_PICKER_H
#define AVATAR_PICKER_H

#include <QDialog>
#include "avatar_store.h"

class AvatarPicker : public QDialog
{
    Q_OBJECT
public:
    explicit AvatarPicker(AvatarStore *store, QWidget *parent = nullptr);
    QString selectedAvatarId() const;
signals:
    void avatarSelected(const QString &avatarId);
private:
    void buildUI();
    AvatarStore *m_store;
    QString m_selectedId;
};

#endif
