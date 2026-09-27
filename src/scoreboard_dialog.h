#ifndef SCOREBOARD_DIALOG_H
#define SCOREBOARD_DIALOG_H
#include <QDialog>
#include <QStringList>
class QLabel;
class QTableWidget;
class QListWidget;
class GameRecordModel;

class ScoreboardDialog : public QDialog
{
    Q_OBJECT
public:
    ScoreboardDialog(GameRecordModel *model, QWidget *parent = nullptr);
    void setSeatNames(const QStringList &names);
private:
    void refresh();
    GameRecordModel *m_model;
    QLabel *m_summary;
    QTableWidget *m_table;
    QListWidget *m_history;
    QStringList m_names;
};
#endif
