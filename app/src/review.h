#pragma once

#include <QDateTime>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariantMap>

#include "reviewqueue.h"

class HaystackClient;
class EngineClient;

// A Review session: the Review queues (Discoveries, Calibrate when haystack offers it, Rate your likes, and the
// Playlist review once one is started), the current track, and what the verdict keys do. Pass and Dislike advance
// at once; a plain Rating keeps the track playing; Shift+digit rates and moves on. Verdicts go to haystack through
// the outbox and show at once, online or not.
class Review : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString queue READ queue WRITE setQueue NOTIFY changed)
    Q_PROPERTY(QVariantMap current READ current NOTIFY changed)
    Q_PROPERTY(QVariantMap upNext READ upNext NOTIFY changed)
    Q_PROPERTY(bool hasState READ hasState NOTIFY changed)
    Q_PROPERTY(bool queueDone READ queueDone NOTIFY changed)
    Q_PROPERTY(int discoveriesTotal READ discoveriesTotal NOTIFY changed)
    Q_PROPERTY(int discoveriesPosition READ discoveriesPosition NOTIFY changed)
    Q_PROPERTY(int discoveriesLeft READ discoveriesLeft NOTIFY changed)
    Q_PROPERTY(int likesLeft READ likesLeft NOTIFY changed)
    Q_PROPERTY(bool hasCalibrate READ hasCalibrate NOTIFY changed)
    Q_PROPERTY(int calibrateTotal READ calibrateTotal NOTIFY changed)
    Q_PROPERTY(int calibratePosition READ calibratePosition NOTIFY changed)
    Q_PROPERTY(int calibrateLeft READ calibrateLeft NOTIFY changed)
    Q_PROPERTY(bool hasPlaylistReview READ hasPlaylistReview NOTIFY changed)
    Q_PROPERTY(int playlistsTotal READ playlistsTotal NOTIFY changed)
    Q_PROPERTY(int playlistsPosition READ playlistsPosition NOTIFY changed)
    Q_PROPERTY(int playlistsLeft READ playlistsLeft NOTIFY changed)
    Q_PROPERTY(QString nextBatchText READ nextBatchText NOTIFY changed)
    Q_PROPERTY(QVariantMap baseline READ baseline NOTIFY changed)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY changed)
    Q_PROPERTY(QStringList features READ features NOTIFY changed)
    Q_PROPERTY(QString tabNext READ tabNext NOTIFY changed)
    Q_PROPERTY(QString doneNext READ doneNext NOTIFY changed)
    Q_PROPERTY(bool canSwap READ canSwap NOTIFY changed)

public:
    Review(HaystackClient *haystack, EngineClient *engine, QObject *parent = nullptr);

    QString queue() const { return m_queue; }
    void setQueue(const QString &q);
    QVariantMap current() const;
    QVariantMap upNext() const;
    bool hasState() const { return m_hasState; }
    bool queueDone() const { return m_hasState && !q().hasCurrent(); }
    int discoveriesTotal() const { return m_discoveries.size(); }
    int discoveriesPosition() const;
    int discoveriesLeft() const { return m_discoveries.unreviewed(); }
    int likesLeft() const { return m_likes.unreviewed(); }
    // haystack 0.5.0 on (feature queue.calibrate): the tracks it is least sure about. Without the feature there
    // is no Calibrate queue: no tab, and Tab goes round the other three.
    bool hasCalibrate() const { return m_features.contains("queue.calibrate"); }
    int calibrateTotal() const { return m_calibrate.size(); }
    int calibratePosition() const { return position(m_calibrate); }
    int calibrateLeft() const { return m_calibrate.unreviewed(); }
    bool hasPlaylistReview() const { return m_playlists.size() > 0; }
    int playlistsTotal() const { return m_playlists.size(); }
    int playlistsPosition() const { return position(m_playlists); }
    int playlistsLeft() const { return m_playlists.unreviewed(); }
    QString nextBatchText() const;
    QVariantMap baseline() const { return m_baseline; }
    bool canGoBack() const { return q().canGoBack(); }
    // The snapshot's server.features (haystack's docs/api.md): a screen element that needs a newer haystack shows
    // only when its feature is listed. Empty for a haystack older than the server object.
    QStringList features() const { return m_features; }
    // The queue Tab goes to (switchQueue), and the one a finished queue's notice offers: the next with something
    // left to review, or "" when none has.
    QString tabNext() const;
    QString doneNext() const;
    // haystack 0.9.0 on (features card.original and swaps): the current track is a Possible cover with its Original
    // found on Spotify, in Rate your likes or a Playlist review. Discoveries and Calibrate are passed or rated as
    // they are (haystack's ADR 0022).
    bool canSwap() const;

    // For tests and the clock: the snapshot handling without the network.
    void applyState(const QVariantMap &state);

public slots:
    void rate(int stars, bool advance = false);
    void pass();
    void dislike();
    void next();
    void back();
    // Discoveries -> Calibrate (when offered) -> Rate your likes -> Playlists -> Discoveries
    void switchQueue();
    // A Playlist review starts (activate: switch to it and play) or comes back after a restart.
    void startPlaylistReview(const QVariantList &cards, bool revisit, const QStringList &done, bool activate);
    void updateCard(const QVariantMap &card);
    // Swap the current Possible cover for its Original: haystack puts the Original in its place in the listener's
    // playlists, and here it takes the cover's place and plays. Nothing without canSwap.
    void swap();

signals:
    void changed();

private:
    ReviewQueue &q()
    {
        return m_queue == "likes"       ? m_likes
               : m_queue == "playlists" ? m_playlists
               : m_queue == "calibrate" ? m_calibrate
                                        : m_discoveries;
    }
    const ReviewQueue &q() const
    {
        return m_queue == "likes"       ? m_likes
               : m_queue == "playlists" ? m_playlists
               : m_queue == "calibrate" ? m_calibrate
                                        : m_discoveries;
    }
    static int position(const ReviewQueue &queue);
    void give(const QString &kind, int stars, bool advance);
    void move(bool forward, bool play);
    void loadCurrent(bool play);
    void leaveTrack(bool completed);
    void prefetch();
    void measureSoon();
    bool waiting(const QVariantMap &card) const;
    bool waitsOnSnapshot() const;

    HaystackClient *m_haystack;
    EngineClient *m_engine;
    ReviewQueue m_discoveries, m_calibrate, m_likes, m_playlists;
    QString m_queue = "discoveries";
    bool m_hasState = false;
    QVariantMap m_baseline;
    QStringList m_features;
    QDateTime m_nextBatch;
    QString m_loaded;
    bool m_loadPlays = false;          // the current track was loaded to play (not just to show)
    QDateTime m_playStarted;
    qint64 m_maxPosition = 0;
    QTimer m_measureRefresh;
    QSet<QString> m_measureAsked;      // playlist songs haystack was asked to measure
    int m_measureChecks = 0;           // asks for the current one's Readings or cover check so far
};
