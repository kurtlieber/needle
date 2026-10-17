#include "playlistlibrary.h"

#include <QJsonArray>

#include <algorithm>

double PlaylistLibrary::Progress::average() const
{
    const int n = rated();
    if (!n)
        return 0;
    int sum = 0;
    for (int i = 0; i < 5; ++i)
        sum += stars[i] * (i + 1);
    return double(sum) / n;
}

void PlaylistLibrary::setState(const QVariantMap &playlists)
{
    m_items = playlists.value("items").toList();
    m_byId.clear();
    m_songs.clear();
    for (const auto &v : std::as_const(m_items)) {
        const auto p = v.toMap();
        const QString id = p.value("id").toString();
        m_byId.insert(id, p);
        if (!p.value("tracks").isNull())
            m_songs.insert(id, p.value("tracks").toStringList());
    }
    m_verdicts.clear();
    const auto vs = playlists.value("verdicts").toMap();
    for (auto it = vs.begin(); it != vs.end(); ++it)
        m_verdicts.insert(it.key(), it.value().toMap());
    // ours win until haystack has caught up with them (haystack's is newer when given elsewhere since)
    for (auto it = m_local.begin(); it != m_local.end(); ++it) {
        const auto theirs = m_verdicts.value(it.key());
        if (theirs.value("given_at").toString() < it.value().value("given_at").toString())
            m_verdicts.insert(it.key(), it.value());
    }
}

void PlaylistLibrary::applyVerdict(const QString &track, const QVariantMap &verdict)
{
    m_local.insert(track, verdict);
    m_verdicts.insert(track, verdict);
}

QStringList PlaylistLibrary::ids() const
{
    QStringList out;
    for (const auto &v : m_items)
        out << v.toMap().value("id").toString();
    return out;
}

bool PlaylistLibrary::readable(const QString &id) const
{
    return !m_songs.value(id).isEmpty();
}

bool PlaylistLibrary::needsEngine(const QString &id) const
{
    const auto p = m_byId.value(id);
    return p.value("reader").toString() == "engine" && !p.value("current").toBool();
}

QStringList PlaylistLibrary::songs(const QString &id) const
{
    return m_songs.value(id);
}

PlaylistLibrary::Progress PlaylistLibrary::progress(const QString &id) const
{
    Progress p;
    const QStringList s = m_songs.value(id);
    p.songs = s.size();
    for (const auto &t : s) {
        const auto it = m_verdicts.constFind(t);
        if (it == m_verdicts.cend())
            continue;
        const QString kind = it->value("kind").toString();
        const int stars = it->value("stars").toInt();
        if (kind == "rating" && stars >= 1 && stars <= 5)
            ++p.stars[stars - 1];
        else if (kind == "pass")
            ++p.passed;
        else if (kind == "dislike")
            ++p.disliked;
    }
    return p;
}

QString PlaylistLibrary::stage(const QString &id) const
{
    const Progress p = progress(id);
    if (!p.songs || !p.reviewed())
        return "fresh";
    return p.left() ? "progress" : "done";
}

QStringList PlaylistLibrary::union_(const QStringList &ids) const
{
    QStringList out;
    QSet<QString> seen;
    for (const auto &id : ids)
        for (const auto &t : m_songs.value(id))
            if (!seen.contains(t)) {
                seen.insert(t);
                out << t;
            }
    return out;
}

PlaylistLibrary::Selection PlaylistLibrary::selection(const QStringList &ids) const
{
    Selection s;
    QHash<QString, int> times;
    for (const auto &id : ids) {
        if (!readable(id))
            continue;
        ++s.playlists;
        for (const auto &t : m_songs.value(id))
            ++times[t];
    }
    s.songs = times.size();
    for (auto it = times.cbegin(); it != times.cend(); ++it) {
        s.withVerdict += m_verdicts.contains(it.key());
        s.overlap += it.value() > 1;
    }
    return s;
}

// ---- a Playlist review ----

QJsonObject PlaylistSession::toJson() const
{
    return {{"playlists", QJsonArray::fromStringList(playlists)}, {"skip_verdicts", skipVerdicts},
            {"shuffle", shuffle}, {"seed", QString::number(seed)}, {"tracks", QJsonArray::fromStringList(tracks)},
            {"done", QJsonArray::fromStringList(done)}, {"started_at", startedAt}};
}

static QStringList strings(const QJsonValue &v)
{
    QStringList out;
    for (const auto &x : v.toArray())
        out << x.toString();
    return out;
}

PlaylistSession PlaylistSession::fromJson(const QJsonObject &o)
{
    PlaylistSession s;
    s.playlists = strings(o.value("playlists"));
    s.skipVerdicts = o.value("skip_verdicts").toBool(true);
    s.shuffle = o.value("shuffle").toBool(true);
    s.seed = o.value("seed").toString().toULongLong();
    s.tracks = strings(o.value("tracks"));
    s.done = strings(o.value("done"));
    s.startedAt = o.value("started_at").toString();
    return s;
}

static quint64 fnv1a(quint64 seed, const QString &s)
{
    quint64 h = 0xcbf29ce484222325ULL ^ seed;
    for (const char c : s.toUtf8()) {
        h ^= quint8(c);
        h *= 0x100000001b3ULL;
    }
    // one round of mixing, so ids that differ only at the end still land far apart
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    return h;
}

QStringList PlaylistSession::order(const QStringList &songs, bool shuffle, quint64 seed)
{
    if (!shuffle)
        return songs;
    QList<QPair<quint64, QString>> keyed;
    keyed.reserve(songs.size());
    for (const auto &s : songs)
        keyed.append({fnv1a(seed, s), s});
    std::sort(keyed.begin(), keyed.end());
    QStringList out;
    out.reserve(keyed.size());
    for (const auto &k : std::as_const(keyed))
        out << k.second;
    return out;
}

QVariantList PlaylistSession::cards(const QHash<QString, QVariantMap> &cardsById,
                                    const QHash<QString, QString> &names) const
{
    QVariantList out;
    for (const auto &t : tracks) {
        auto it = cardsById.constFind(t);
        if (it == cardsById.cend())
            continue;
        QVariantMap c = *it;
        const QStringList on = c.value("playlists").toStringList();
        QString from;
        for (const auto &p : playlists)          // the first picked playlist (in the review's order) it is on
            if (on.contains(p)) {
                from = p;
                break;
            }
        QStringList also;
        for (const auto &p : on)
            if (p != from && names.contains(p))
                also << names.value(p);
        c.insert("fromPlaylist", names.value(from));
        c.insert("alsoOn", also);
        c.insert("reviewPlaylists", playlists.size());
        out << c;
    }
    return out;
}
