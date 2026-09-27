#ifndef GAME_RECORD_MODEL_H
#define GAME_RECORD_MODEL_H
#include "game_core/game_event.h"
#include <QObject>
#include <QStringList>
#include <array>

// Read-only projection of public events. Never submits a command or writes state.
class GameRecordModel : public QObject
{
    Q_OBJECT
public:
    explicit GameRecordModel(QObject *parent = nullptr) : QObject(parent) {}
    void beginSession(const std::array<int, 3> &scores = {});
    void observe(const GameEvent &event);
    int score(int seat) const { return isValidSeat(seat) ? m_scores[seat] : 0; }
    int multiplier() const { return m_multiplier; }
    int roundNumber() const { return m_round; }
    const QStringList &entries() const { return m_entries; }
signals:
    void changed();
private:
    std::array<int, 3> m_scores{};
    QStringList m_entries;
    int m_multiplier = 1;
    int m_round = 0;
};
#endif
