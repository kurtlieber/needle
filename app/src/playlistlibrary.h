#pragma once

#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// The listener's Spotify playlists as the picker shows them (the cover wall, mockups/playlists-a-wall.png): each one's
// progress from the current Verdicts of its songs, and what a Playlist review of some of them would hold. Built
// from the "playlists" part of /v1/state, which carries every playlist's song ids but no cards, so all of this
// works offline. Pure: no network, no QML.
class PlaylistLibrary {
public:
    struct Progress {
        int songs = 0;
        int stars[5] = {0, 0, 0, 0, 0};   // 1★ .. 5★
        int passed = 0, disliked = 0;
        int rated() const { return stars[0] + stars[1] + stars[2] + stars[3] + stars[4]; }
        int reviewed() const { return rated() + passed + disliked; }
        int left() const { return songs - reviewed(); }
        double average() const;            // over the Ratings; 0 with none
    };

    // A Playlist review of some playlists, before it starts.
    struct Selection {
        int playlists = 0;
        int songs = 0;          // each once, however many of the playlists it is on
        int withVerdict = 0;
        int overlap = 0;        // songs on more than one of them
    };

    void setState(const QVariantMap &playlists);
    // A verdict given here that haystack may not have yet; the newer one (by given_at) wins.
    void applyVerdict(const QString &track, const QVariantMap &verdict);

    QVariantList items() const { return m_items; }
    int size() const { return m_items.size(); }
    QVariantMap item(const QString &id) const { return m_byId.value(id); }
    QStringList ids() const;
    // Its songs are known (it can be picked); false until haystack (or needle's engine) has read them.
    bool readable(const QString &id) const;
    // Spotify refuses haystack this playlist's songs and the engine hasn't sent the current version yet.
    bool needsEngine(const QString &id) const;
    QStringList songs(const QString &id) const;
    Progress progress(const QString &id) const;
    // all | fresh (not started) | progress (in progress) | done
    QString stage(const QString &id) const;
    QVariantMap verdict(const QString &track) const { return m_verdicts.value(track); }
    bool hasVerdict(const QString &track) const { return m_verdicts.contains(track); }

    Selection selection(const QStringList &ids) const;
    // The picked playlists' songs, in the order given and then each playlist's own, each once.
    QStringList union_(const QStringList &ids) const;

private:
    QVariantList m_items;
    QHash<QString, QVariantMap> m_byId;
    QHash<QString, QStringList> m_songs;
    QHash<QString, QVariantMap> m_verdicts;
    QHash<QString, QVariantMap> m_local;        // given here, maybe not on haystack yet
};

// A Playlist review in progress: which playlists, the switches it started with, and its songs in play order,
// frozen when it starts (a fresh snapshot doesn't reshuffle it). Saved, so it survives a restart.
struct PlaylistSession {
    QStringList playlists;
    bool skipVerdicts = true;
    bool shuffle = true;
    quint64 seed = 0;
    QStringList tracks;
    QStringList done;        // with skipVerdicts off: the songs given a verdict in this review
    QString startedAt;

    bool isEmpty() const { return tracks.isEmpty(); }
    QJsonObject toJson() const;
    static PlaylistSession fromJson(const QJsonObject &o);

    // Shuffle by a hash of each song and the session's seed: the same order every time, and a song keeps its
    // place even if the list around it changes.
    static QStringList order(const QStringList &songs, bool shuffle, quint64 seed);
    // The session's cards, in play order: each card as haystack sent it plus where it comes from in this review
    // (fromPlaylist, the first picked playlist it is on), the other playlists it is on (alsoOn, names) and how
    // many playlists the review has (reviewPlaylists). Songs with no card are left out.
    QVariantList cards(const QHash<QString, QVariantMap> &cardsById, const QHash<QString, QString> &names) const;
};
