#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

// Verdicts and plays waiting for haystack (ADR 0004, ADR 0006). Persisted at every change, atomically, so a
// verdict given offline, or right before a crash, is never lost; haystack ignores replays by id.
class Outbox {
public:
    explicit Outbox(const QString &path);

    void addVerdict(const QJsonObject &v);
    void addPlay(const QJsonObject &p);
    // Drop what haystack answered for (accepted, or rejected as malformed: retrying can't fix those).
    void settle(const QStringList &ids);

    QJsonArray verdicts() const { return m_verdicts; }
    QJsonArray plays() const { return m_plays; }
    int pendingVerdicts() const { return m_verdicts.size(); }
    int size() const { return m_verdicts.size() + m_plays.size(); }
    QString error() const { return m_error; }

private:
    void save();

    QString m_path;
    QJsonArray m_verdicts, m_plays;
    QString m_error;
};
