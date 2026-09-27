#ifndef CARD_TABLE_VIEW_H
#define CARD_TABLE_VIEW_H

#include "cards.h"
#include <QObject>
#include <QPointer>
#include <QPixmap>
#include <QSet>
#include <array>

class QWidget;
class QLabel;
class CardPanel;

// Owns presentation and selection only. No Player, GameEngine or transport.
class CardTableView : public QObject
{
    Q_OBJECT
public:
    enum class Side { Left, Right };
    explicit CardTableView(QWidget *layer);
    void setCardBack(const QPixmap &back, const QPixmap &themeBack = {});
    void setHand(const Cards &hand);
    void setOpponentCount(Side side, int count);
    void showPlayed(const Cards &cards, bool animate);
    void showBottom(const Cards &cards);
    void clearPlayed();
    void clearBottom();
    void clearHands();
    void hideHands();
    void relayout();
    void toggleSelection(int index);
    void clearSelection();
    void selectCards(const Cards &cards);
    Cards selectedCards() const;
    const CardList &handCards() const { return m_hand; }
signals:
    void cardClicked(int index);
    void layersChanged();
private:
    CardPanel *createCard(const Card &card, bool front);
    static void discard(QVector<CardPanel *> &panels);
    QPointer<QWidget> m_layer;
    QLabel *m_passLabel;
    QPixmap m_back;
    QPixmap m_themeBack;
    CardList m_hand;
    QSet<int> m_selected;
    QVector<CardPanel *> m_userPanels;
    std::array<QVector<CardPanel *>, 2> m_opponents;
    QVector<CardPanel *> m_played;
    QVector<CardPanel *> m_bottom;
    static constexpr int kWidth = 75;
    static constexpr int kHeight = 107;
    static constexpr int kOverlap = 26;
};

#endif
