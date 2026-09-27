#include "scoreboard_dialog.h"
#include "game_record_model.h"
#include <QLabel>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QDialogButtonBox>

ScoreboardDialog::ScoreboardDialog(GameRecordModel *model, QWidget *parent)
    : QDialog(parent), m_model(model)
{
    setWindowTitle(QStringLiteral("计分板与对局记录"));
    setObjectName(QStringLiteral("scoreboardDialog"));
    resize(520, 520);
    setMinimumSize(400, 350);
    setStyleSheet("QDialog{background:#20242a;color:#f4f4f4;} QLabel{color:#f4f4f4;}"
                  "QTableWidget,QListWidget{background:#2c3138;color:#f4f4f4;font-size:14px;}"
                  "QHeaderView::section{background:#353c45;color:#f4f4f4;padding:6px;}"
                  "QPushButton{padding:8px 16px;}");
    auto *layout = new QVBoxLayout(this);
    m_summary = new QLabel(this);
    layout->addWidget(m_summary);
    m_table = new QTableWidget(3, 3, this);
    m_table->setObjectName(QStringLiteral("scoreTable"));
    m_table->setHorizontalHeaderLabels({QStringLiteral("座位"),QStringLiteral("玩家"),QStringLiteral("累计分")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setMaximumHeight(150);
    layout->addWidget(m_table);
    layout->addWidget(new QLabel(QStringLiteral("最近 300 条事件（不记录私有牌面）"), this));
    m_history = new QListWidget(this);
    m_history->setObjectName(QStringLiteral("eventHistory"));
    layout->addWidget(m_history);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::hide);
    layout->addWidget(buttons);
    connect(model, &GameRecordModel::changed, this, &ScoreboardDialog::refresh);
    refresh();
}
void ScoreboardDialog::setSeatNames(const QStringList &names) { m_names = names; refresh(); }
void ScoreboardDialog::refresh()
{
    m_summary->setText(QStringLiteral("第 %1 局 · 当前倍数 %2 · 总分 %3")
        .arg(m_model->roundNumber()).arg(m_model->multiplier())
        .arg(m_model->score(0)+m_model->score(1)+m_model->score(2)));
    for (int seat = 0; seat < 3; ++seat) {
        const QStringList row{QString::number(seat+1),m_names.value(seat,QStringLiteral("玩家 %1").arg(seat+1)),
                              QString::number(m_model->score(seat))};
        for (int column = 0; column < 3; ++column)
            m_table->setItem(seat,column,new QTableWidgetItem(row[column]));
    }
    m_history->clear();
    m_history->addItems(m_model->entries());
    m_history->scrollToBottom();
}
