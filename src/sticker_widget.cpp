#include "sticker_widget.h"
#include "sticker_store.h"
#include <QPushButton>

StickerWidget::StickerWidget(StickerStore *store, QWidget *parent)
    : QWidget(parent, Qt::Popup), m_store(store)
{
    setFixedSize(220, 160);
    setStyleSheet("background:#1e1e1e;border:2px solid #555;border-radius:6px;");
    m_grid = new QGridLayout(this);
    m_grid->setContentsMargins(4,4,4,4);
    m_grid->setSpacing(4);
    buildGrid();
}

void StickerWidget::buildGrid()
{
    QLayoutItem *child;
    while ((child = m_grid->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }
    if (!m_store || !m_store->isLoaded()) return;
    int cols = 4;
    for (int i = 0; i < m_store->count(); ++i) {
        QPixmap pix = m_store->get(i);
        QPixmap thumb = pix.scaled(48,48,Qt::KeepAspectRatio,Qt::SmoothTransformation);
        auto *btn = new QPushButton(this);
        btn->setIcon(QIcon(thumb));
        btn->setIconSize(QSize(48,48));
        btn->setFixedSize(48,48);
        btn->setFlat(true);
        btn->setStyleSheet("QPushButton{border:1px solid transparent;border-radius:4px;}"
                          "QPushButton:hover{border-color:gold;background:rgba(255,215,0,30);}");
        int idx = i;
        connect(btn, &QPushButton::clicked, this, [this,idx]() { emit stickerSelected(idx); hide(); });
        m_grid->addWidget(btn, i/cols, i%cols);
    }
}

void StickerWidget::refresh() { buildGrid(); }
void StickerWidget::focusOutEvent(QFocusEvent *event) { Q_UNUSED(event); hide(); }
