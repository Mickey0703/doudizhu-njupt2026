#include "card_table_view.h"
#include "cardpanel.h"
#include <QLabel>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QGraphicsOpacityEffect>

CardTableView::CardTableView(QWidget *layer) : QObject(layer), m_layer(layer)
{
    m_passLabel = new QLabel(QStringLiteral("不出"), layer);
    m_passLabel->setFont(QFont("Microsoft YaHei", 11));
    m_passLabel->setStyleSheet("color:yellow;background:rgba(0,0,0,120);border-radius:6px;padding:4px 12px;");
    m_passLabel->setAlignment(Qt::AlignCenter);
    m_passLabel->hide();
}

void CardTableView::setCardBack(const QPixmap &back, const QPixmap &themeBack)
{
    m_back = back;
    m_themeBack = themeBack;
}

CardPanel *CardTableView::createCard(const Card &card, bool front)
{
    auto *panel = new CardPanel(m_layer);
    panel->setFixedSize(kWidth, kHeight);
    panel->setCard(card);
    if (front) panel->setFrontPixmap(CardPanel::loadCardPixmap(card));
    panel->setBackPixmap(m_themeBack.isNull() ? m_back : m_themeBack);
    panel->setFrontSide(front);
    panel->show();
    return panel;
}

void CardTableView::discard(QVector<CardPanel *> &panels)
{
    for (auto *panel : panels) {
        panel->hide(); // deleteLater alone leaves stale cards visible this frame.
        panel->deleteLater();
    }
    panels.clear();
}

void CardTableView::setHand(const Cards &hand)
{
    discard(m_userPanels);
    m_hand = hand.toCardList(Cards::Desc);
    m_selected.clear();
    for (int index = 0; index < m_hand.size(); ++index) {
        auto *panel = createCard(m_hand[index], true);
        connect(panel, &CardPanel::clicked, this, [this, index] { emit cardClicked(index); });
        m_userPanels.append(panel);
    }
    relayout();
}

void CardTableView::setOpponentCount(Side side, int count)
{
    auto &panels = m_opponents[side == Side::Left ? 0 : 1];
    discard(panels);
    for (int i = 0; i < qBound(0, count, 6); ++i)
        panels.append(createCard(Card(), false));
    relayout();
}

void CardTableView::showPlayed(const Cards &cards, bool animate)
{
    clearPlayed();
    if (cards.isEmpty()) {
        m_passLabel->show();
        relayout();
        return;
    }
    for (const auto &card : cards.toCardList(Cards::Desc))
        m_played.append(createCard(card, true));
    relayout();
    if (animate && m_layer) {
        const QPoint center = m_layer->rect().center();
        for (auto *panel : m_played) {
            const QRect destination = panel->geometry();
            const QRect origin(center.x() - kWidth / 4, center.y() - kHeight / 4,
                               kWidth / 2, kHeight / 2);
            // Every animation belongs to its card, so clearing the table also
            // cancels animation work. QWidget fixed size precludes scale;
            // animate position and opacity without fighting its constraints.
            auto *opacity = new QGraphicsOpacityEffect(panel);
            panel->setGraphicsEffect(opacity);
            auto *group = new QParallelAnimationGroup(panel);
            auto *position = new QPropertyAnimation(panel, "pos", group);
            position->setDuration(360);
            position->setStartValue(origin.topLeft());
            position->setEndValue(destination.topLeft());
            position->setEasingCurve(QEasingCurve::OutCubic);
            auto *fade = new QPropertyAnimation(opacity, "opacity", group);
            fade->setDuration(280);
            fade->setStartValue(0.0);
            fade->setEndValue(1.0);
            group->start(QAbstractAnimation::DeleteWhenStopped);
        }
    }
}

void CardTableView::showBottom(const Cards &cards)
{
    clearBottom();
    for (const auto &card : cards.toCardList(Cards::Desc))
        m_bottom.append(createCard(card, true));
    relayout();
}

void CardTableView::clearPlayed() { discard(m_played); m_passLabel->hide(); }
void CardTableView::clearBottom() { discard(m_bottom); }
void CardTableView::clearHands()
{
    discard(m_userPanels);
    for (auto &panels : m_opponents) discard(panels);
    m_hand.clear();
    m_selected.clear();
}
void CardTableView::hideHands()
{
    for (auto *panel : m_userPanels) panel->hide();
    for (const auto &panels : m_opponents)
        for (auto *panel : panels) panel->hide();
}

void CardTableView::toggleSelection(int index)
{
    if (index < 0 || index >= m_hand.size()) return;
    if (m_selected.contains(index)) m_selected.remove(index);
    else m_selected.insert(index);
    relayout();
}
void CardTableView::clearSelection() { m_selected.clear(); relayout(); }
void CardTableView::selectCards(const Cards &cards)
{
    m_selected.clear();
    for (int i = 0; i < m_hand.size(); ++i)
        if (cards.contains(m_hand[i])) m_selected.insert(i);
    relayout();
}
Cards CardTableView::selectedCards() const
{
    Cards cards;
    for (int index : m_selected)
        if (index >= 0 && index < m_hand.size()) cards.add(m_hand[index]);
    return cards;
}

void CardTableView::relayout()
{
    if (!m_layer) return;
    const int w = m_layer->width(), h = m_layer->height();
    const int handWidth = m_userPanels.size() * kOverlap + kWidth - kOverlap;
    const int start = qMax(5, (w - handWidth) / 2);
    for (int i = 0; i < m_userPanels.size(); ++i) {
        const bool selected = m_selected.contains(i);
        m_userPanels[i]->setSelected(selected);
        m_userPanels[i]->move(start + i * kOverlap, h - kHeight - 80 - (selected ? 15 : 0));
        m_userPanels[i]->raise();
    }
    for (int side = 0; side < 2; ++side)
        for (int i = 0; i < m_opponents[side].size(); ++i) {
            m_opponents[side][i]->move((side == 0 ? 20 : w - 215) + i * 10, 72);
            m_opponents[side][i]->raise();
        }
    const int playWidth = m_played.size() * (kWidth + 8) - 8;
    for (int i = 0; i < m_played.size(); ++i) {
        const QPoint destination(qMax(10, (w - playWidth) / 2) + i * (kWidth + 8), h / 2 + 30);
        bool animating = false;
        for (auto *animation : m_played[i]->findChildren<QPropertyAnimation *>()) {
            if (animation->targetObject() == m_played[i] && animation->propertyName() == "pos"
                && animation->state() == QAbstractAnimation::Running) {
                animation->setEndValue(destination);
                animating = true;
            }
        }
        if (!animating) m_played[i]->move(destination);
        m_played[i]->raise();
    }
    for (int i = 0; i < m_bottom.size(); ++i)
        m_bottom[i]->move(w / 2 - 130 + i * (kWidth + 6), h / 2 - kHeight - 30);
    m_passLabel->setGeometry(w / 2 - 200, h / 2 + kHeight + 30, 400, 32);
    m_passLabel->raise();
    emit layersChanged();
}
