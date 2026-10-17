#pragma once

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariantMap>

#include "outbox.h"

class QNetworkReply;

// needle's side of haystack's HTTP API (haystack's docs/api.md, ADR 0004 and ADR 0006): one snapshot of the Review state,
// cached on disk so Review works offline, and an outbox of verdicts and plays flushed whenever haystack answers.
// Tries the servers in the order given (Config), the next one when one doesn't answer. A Playlist review fetches
// each picked playlist's cards once, and keeps them too.
class HaystackClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(int pending READ pending NOTIFY statusChanged)
    Q_PROPERTY(QStringList servers READ servers CONSTANT)
    Q_PROPERTY(QString tokenFile READ tokenFile CONSTANT)   // as shown: ~ for the home folder

public:
    HaystackClient(const QStringList &baseUrls, const QString &tokenPath, const QString &cacheDir,
                 const QString &stateDir, QObject *parent = nullptr);

    QString status() const { return m_status; }
    QString statusText() const;
    int pending() const { return m_outbox.pendingVerdicts(); }
    QStringList servers() const { return m_bases; }
    QString tokenFile() const;
    QVariantMap cachedState() const { return m_state; }
    QJsonArray pendingVerdicts() const { return m_outbox.verdicts(); }

    void addVerdict(const QJsonObject &v);
    void addPlay(const QJsonObject &p);

    // A local file for a cover if it was fetched before, else the URL itself (and fetch it for next time).
    Q_INVOKABLE QString cover(const QString &url);
    void prefetchCovers(const QStringList &urls);

    // A playlist with a card for each of its songs (GET /v1/playlists/{id}), saved for offline use.
    void fetchPlaylist(const QString &id);
    QVariantMap cachedPlaylist(const QString &id) const;
    // The songs needle's engine read for a playlist haystack can't list (ADR 0008); a fresh snapshot follows.
    void sendPlaylistTracks(const QString &id, const QJsonObject &body);
    // One track's card (GET /v1/tracks/{id}); haystack starts measuring it when it has no Readings yet.
    void fetchTrack(const QString &id);

public slots:
    void refresh();
    void flush();

signals:
    void stateArrived(const QVariantMap &state);
    void statusChanged();
    void verdictAdded(const QJsonObject &verdict);
    void playlistArrived(const QString &id, const QVariantMap &playlist);
    void playlistFailed(const QString &id, int httpStatus);
    // httpStatus: 200 stored, 0 no answer (worth another try), anything else refused
    void playlistTracksSent(const QString &id, int httpStatus);
    void trackArrived(const QVariantMap &card);

private:
    void fetchState();
    QNetworkReply *send(const QString &method, const QString &path, const QByteArray &body = {});
    bool readToken();
    void setStatus(const QString &s);
    void networkFailed(QNetworkReply *reply, std::function<void()> retry);
    void postBatch(const QString &path, const QString &key, const QJsonArray &items, std::function<void()> then);
    // One request, on the next server when one doesn't answer; fail gets the HTTP status (0: no answer).
    void call(const QString &method, const QString &path, const QByteArray &body,
              std::function<void(const QByteArray &)> ok, std::function<void(int)> fail);
    QString playlistPath(const QString &id) const;
    QString coverPath(const QString &url) const;
    void fetchNextCover();

    QStringList m_bases;
    int m_base = 0;
    int m_failures = 0;
    QString m_tokenPath, m_token, m_cacheDir;
    QNetworkAccessManager m_nam;
    Outbox m_outbox;
    QVariantMap m_state;
    QString m_status = "syncing";
    QString m_spotifyError;
    int m_spotifyPending = 0;
    bool m_flushing = false;
    QTimer m_refreshTimer, m_retryTimer;
    QStringList m_coverQueue;
    QSet<QString> m_coverBusy;
};
