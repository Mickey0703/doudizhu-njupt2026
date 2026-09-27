#include "avatar_picker.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QPainter>

AvatarPicker::AvatarPicker(AvatarStore *store, QWidget *parent)
    : QDialog(parent), m_store(store)
{
    setWindowTitle(QStringLiteral("🖼️ 选择头像"));
    setFixedSize(360, 300);
    setStyleSheet("background:#2a2a2a;border-radius:8px;");
    buildUI();
}

void AvatarPicker::buildUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12,12,12,12);

    auto *title = new QLabel(QStringLiteral("选择你的头像"), this);
    title->setStyleSheet("color:gold;font-size:14px;font-weight:bold;");
    title->setAlignment(Qt::AlignCenter);
    root->addWidget(title);

    auto *grid = new QGridLayout;
    grid->setSpacing(10);
    QVector<AvatarInfo> avatars = m_store->list();
    int cols = 4;
    for (int i = 0; i < avatars.size(); ++i) {
        QPixmap pix = m_store->getAvatar(avatars[i].id);
        if (pix.isNull()) continue;

        QPixmap circle(64,64);
        circle.fill(Qt::transparent);
        QPainter p(&circle);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(pix.scaled(64,64,Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation));
        p.setPen(QPen(QColor(100,100,100),1));
        p.drawEllipse(0,0,64,64);
        p.end();

        auto *btn = new QPushButton(this);
        btn->setIcon(QIcon(circle));
        btn->setIconSize(QSize(64,64));
        btn->setFixedSize(72,72);
        btn->setFlat(true);
        btn->setStyleSheet("QPushButton{border:2px solid transparent;border-radius:36px;}"
                          "QPushButton:hover{border-color:gold;background:rgba(255,215,0,30);}");

        QString aid = avatars[i].id;
        connect(btn, &QPushButton::clicked, this, [this,aid]() {
            m_selectedId = aid;
            emit avatarSelected(aid);
            accept();
        });
        grid->addWidget(btn, i/cols, i%cols, Qt::AlignCenter);
    }
    root->addLayout(grid);
    root->addStretch();
}

QString AvatarPicker::selectedAvatarId() const { return m_selectedId; }
