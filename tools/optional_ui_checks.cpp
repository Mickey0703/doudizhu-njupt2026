#include "gamepanel.h"
#include "card_table_view.h"
#include "game_action_controller.h"
#include "game_record_model.h"
#include "scoreboard_dialog.h"
#include "cardpanel.h"
#include "game_core/state_strategy.h"
#include <QApplication>
#include <QTableWidget>
#include <QTest>
#include <QFontDatabase>
#include <QFile>
#include <iostream>
#include <stdexcept>

#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (false)

static void viewOwnsSelectionAndWidgets()
{
    QWidget layer;
    layer.resize(1024,720);
    layer.show();
    CardTableView view(&layer);
    Cards hand;
    const Card first(Card::Card_3,Card::Diamond), second(Card::Card_4,Card::Spade);
    hand << first << second;
    view.setCardBack(CardPanel::loadCardBack());
    view.setHand(hand);
    view.toggleSelection(-1);
    view.toggleSelection(999);
    CHECK(view.selectedCards().isEmpty());
    view.selectCards(Cards(first));
    CHECK(view.selectedCards() == Cards(first));
    bool highlighted = false;
    for (auto *panel : layer.findChildren<CardPanel *>())
        if (panel->getCard() == first) highlighted = panel->isSelected();
    CHECK(highlighted);
    view.toggleSelection(1); // Desc order: 3 is index 1
    CHECK(view.selectedCards().isEmpty());
    view.setOpponentCount(CardTableView::Side::Left,54);
    view.setOpponentCount(CardTableView::Side::Right,0);
    view.showPlayed(hand,true);
    layer.resize(1300,720);
    view.relayout();
    QTest::qWait(450);
    int animatedCards = 0;
    for (auto *panel : layer.findChildren<CardPanel *>()) {
        if (!panel->graphicsEffect()) continue;
        ++animatedCards;
        const int expectedX = (1300 - (2 * (75 + 8) - 8)) / 2
            + (panel->getCard() == second ? 0 : 83);
        CHECK(panel->pos() == QPoint(expectedX,390));
    }
    CHECK(animatedCards == 2);
    view.clearPlayed();
    view.showBottom(hand);
    view.clearBottom();
    view.clearHands();
    CHECK(view.handCards().isEmpty() && view.selectedCards().isEmpty());
    QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
    CHECK(layer.findChildren<CardPanel *>().isEmpty());
}

static void publicRecordProjection()
{
    GameRecordModel records;
    records.observe({GameEventType::RoundStarted,0,1,{}});
    const auto before = records.entries();
    records.observe({GameEventType::PrivateHandDealt,1,17,Cards(Card(Card::Card_BJ,Card::Suit_Begin))});
    CHECK(records.entries() == before);
    records.observe({GameEventType::LordSelected,0,3,{}});
    records.observe({GameEventType::MultiplierChanged,0,6,{}});
    records.observe({GameEventType::ScoreChanged,0,12,{}});
    records.observe({GameEventType::ScoreChanged,1,-6,{}});
    records.observe({GameEventType::ScoreChanged,2,-6,{}});
    CHECK(records.score(0)+records.score(1)+records.score(2) == 0);
    CHECK(records.roundNumber() == 1 && records.multiplier() == 6);
    ScoreboardDialog dialog(&records);
    dialog.setSeatNames({QStringLiteral("地主"),QStringLiteral("左农民"),QStringLiteral("右农民")});
    auto *table = dialog.findChild<QTableWidget *>(QStringLiteral("scoreTable"));
    CHECK(table && table->item(0,2)->text() == QStringLiteral("12"));
    for (int i=0;i<400;++i) records.observe({GameEventType::PlayerPassed,1,0,{}});
    CHECK(records.entries().size() == 300);
    CHECK(records.score(0) == 12);
}

static void panelCompositionAndTimeout()
{
    GamePanel panel;
    panel.show();
    auto *actions = panel.findChild<GameActionController *>();
    auto *cards = panel.findChild<CardTableView *>();
    auto *local = panel.findChild<GameControlAdapter *>();
    auto *session = panel.findChild<LocalSession *>();
    auto *records = panel.findChild<GameRecordModel *>();
    CHECK(actions && cards && local && session && records);
    CHECK(panel.findChild<OnlineRoomController *>());
    CHECK(!actions->isOurTurn());
    CHECK(QMetaObject::invokeMethod(&panel,"onVsAI",Qt::DirectConnection));
    auto *confirm = panel.findChild<QPushButton *>(QStringLiteral("themeConfirmButton"));
    if (confirm && confirm->isVisible()) confirm->click();
    CHECK(session->startRound(42));
    session->setAIEnabled(false);
    const auto call = actions->submit(GameCommand::callLord(0,3));
    CHECK(call.submitted && !call.waitingForServer);
    CHECK(actions->state().phase == GamePhase::Playing);
    CHECK(cards->handCards().size() == 20);
    CHECK(actions->state().revealedBottomCards.cardCount() == 3);
    const auto hint = actions->hint();
    CHECK(hint.type == GameCommandType::PlayCards && !hint.cards.isEmpty());
    cards->selectCards(hint.cards);
    CHECK(cards->selectedCards() == hint.cards);
    const auto rejected = actions->submit(GameCommand::pass(0));
    CHECK(!rejected.submitted && rejected.error == GameError::CannotPassWhenLeading);
    actions->startCountdown(1);
    QTest::qWait(1250);
    CHECK(actions->state().players[0].playsMade == 1);
    CHECK(actions->state().currentSeat == 1 || actions->state().phase == GamePhase::RoundFinished);
    CHECK(QMetaObject::invokeMethod(&panel,"onShowScoreboard",Qt::DirectConnection));
    CHECK(panel.findChild<ScoreboardDialog *>()->isVisible());
    panel.findChild<ScoreboardDialog *>()->grab().save(QCoreApplication::applicationDirPath()+"/optional-scoreboard.png");
    panel.grab().save(QCoreApplication::applicationDirPath()+"/optional-gameplay.png");
    CHECK(QMetaObject::invokeMethod(&panel,"onBackToMenu",Qt::DirectConnection));
    const auto played = local->state().playedCards;
    QTest::qWait(1300);
    CHECK(local->state().playedCards == played);
    CHECK(!panel.findChild<ScoreboardDialog *>()->isVisible());
    // A second round restores AI scheduling and presentation ownership.
    CHECK(QMetaObject::invokeMethod(&panel,"onVsAI",Qt::DirectConnection));
    if (confirm && confirm->isVisible()) confirm->click();
    CHECK(session->startRound(77));
    session->setAIEnabled(false);
    CHECK(actions->state().phase == GamePhase::CallingLord && cards->handCards().size() == 17);
    panel.grab().save(QCoreApplication::applicationDirPath()+"/optional-table.png");
}

int main(int argc,char **argv)
{
    qputenv("QT_QPA_PLATFORM","offscreen");
    QApplication app(argc,argv);
    // The offscreen Windows backend lacks the native system-font discovery
    // used by qwindows. Load fonts for visual QA without changing production UI.
    for (const auto &file : {"C:/Windows/Fonts/msyh.ttc","C:/Windows/Fonts/arial.ttf","C:/Windows/Fonts/seguiemj.ttf"})
        if (QFile::exists(QString::fromLatin1(file))) QFontDatabase::addApplicationFont(QString::fromLatin1(file));
    try {
        viewOwnsSelectionAndWidgets(); std::cout << "PASS: view owns selection, widgets and animation lifetime\n";
        publicRecordProjection(); std::cout << "PASS: scoreboard/public history are read-only, bounded and omit private cards\n";
        panelCompositionAndTimeout(); std::cout << "PASS: composed panel, state hints, legal leading timeout, menu cancellation and restart\n";
    } catch (const std::exception &error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
