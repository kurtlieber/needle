#include "playlists.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QRandomGenerator>
#include <QSaveFile>

#include "haystackclient.h"
#include "engineclient.h"

static const int kReadTimeoutMs = 10 * 60 * 1000;
static const QStringList kFilters{"all", "fresh", "progress", "done"};
static const QStringList kSorts{"spotify", "left", "name"};

static QString num(int v) { return QLocale(QLocale::English).toString(v); }
static QString songs(int v) { return num(v) + (v == 1 ? " song" : " songs"); }

// ---- the model ----

int PlaylistModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_ids.size();
}

QVariant PlaylistModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_ids.size())
        return {};
    const QString id = m_ids[index.row()];
    auto it = m_tiles.find(id);
    if (it == m_tiles.end())
        it = m_tiles.insert(id, m_owner->tile(id));
    static const auto names = roleNames();
    return it->value(QString::fromLatin1(names.value(role)));
}

QHash<int, QByteArray> PlaylistModel::roleNames() const
{
    return {{IdRole, "id"}, {NameRole, "name"}, {CoverRole, "coverSource"}, {MineRole, "mine"}, {OwnerRole, "owner"},
            {SongsRole, "songs"}, {ReviewedRole, "reviewed"}, {RatedRole, "rated"}, {LeftRole, "left"},
            {StarsRole, "stars"}, {PassedRole, "passed"}, {DislikedRole, "disliked"}, {AverageRole, "average"},
            {StageRole, "stage"}, {ReadableRole, "readable"}, {MetaRole, "meta"}, {SelectedRole, "selected"},
            {ArtistsRole, "artists"}, {ReadingRole, "reading"}};
}

void PlaylistModel::reset(const QStringList &ids)
{
    beginResetModel();
    m_ids = ids;
    m_tiles.clear();
    endResetModel();
}

void PlaylistModel::refresh()
{
    m_tiles.clear();
    if (!m_ids.isEmpty())
        emit dataChanged(index(0), index(m_ids.size() - 1));
}

// ---- the picker ----

Playlists::Playlists(HaystackClient *haystack, EngineClient *engine, const QString &stateDir, QObject *parent)
    : QObject(parent), m_haystack(haystack), m_engine(engine), m_file(stateDir + "/playlist-review.json"), m_model(this)
{
    QDir().mkpath(stateDir);
    load();
    m_readWatchdog.setSingleShot(true);
    m_readWatchdog.setInterval(kReadTimeoutMs);
    connect(&m_readWatchdog, &QTimer::timeout, this, [this] {
        qWarning("needle-engine never finished reading playlist %s", qPrintable(m_reading));
        m_reading.clear();
        emit changed();
        readNext();
    });
    if (m_haystack) {
        connect(m_haystack, &HaystackClient::stateArrived, this, &Playlists::applyState);
        connect(m_haystack, &HaystackClient::verdictAdded, this, &Playlists::verdictAdded);
        connect(m_haystack, &HaystackClient::playlistArrived, this, &Playlists::playlistArrived);
        connect(m_haystack, &HaystackClient::playlistFailed, this, &Playlists::playlistFailed);
        connect(m_haystack, &HaystackClient::playlistTracksSent, this, [this](const QString &id, int status) {
            if (status == 200) {
                m_haystack->refresh();             // the snapshot that knows its songs
            } else if (status == 0) {            // haystack didn't answer: read it again after the next snapshot
                const QString snap = m_library.item(id).value("snapshot_id").toString();
                m_tried.remove(id + '@' + snap);
            }                                    // refused: not again this run (the warning says why)
        });
    }
    if (m_engine) {
        connect(m_engine, &EngineClient::authChanged, this, [this] {
            if (!m_reading.isEmpty() && m_engine->auth() != "ok") {
                // the engine stopped mid-read: read that one again once it's back
                m_tried.remove(m_reading + '@' + m_library.item(m_reading).value("snapshot_id").toString());
                m_reading.clear();
                m_readWatchdog.stop();
            }
            m_model.refresh();
            emit changed();
            readNext();
        });
        connect(m_engine, &EngineClient::playlistRead, this, [this](const QString &id, const QJsonObject &o) {
            if (id == m_reading) {
                m_reading.clear();
                m_readWatchdog.stop();
            }
            if (m_haystack)
                m_haystack->sendPlaylistTracks(id, {{"snapshot_id", o.value("snapshot_id")}, {"tracks", o.value("tracks")}});
            m_model.refresh();
            emit changed();
            readNext();
        });
        connect(m_engine, &EngineClient::playlistReadFailed, this, [this](const QString &id, const QString &) {
            if (id == m_reading) {
                m_reading.clear();
                m_readWatchdog.stop();
            }
            m_model.refresh();
            emit changed();
            readNext();
        });
    }
    if (m_haystack && !m_haystack->cachedState().isEmpty())
        applyState(m_haystack->cachedState());
}

Playlists::~Playlists() = default;

QString Playlists::name(const QString &id) const
{
    const QString n = m_library.item(id).value("name").toString();
    return n.isEmpty() ? m_fetched.value(id).value("name").toString() : n;
}

void Playlists::applyState(const QVariantMap &state)
{
    m_library.setState(state.value("playlists").toMap());
    QStringList keep;
    for (const auto &id : std::as_const(m_selected))
        if (m_library.readable(id))
            keep << id;
    m_selected = keep;
    rebuildModel(true);
    emit changed();
    emit selectionChanged();
    readNext();
}

QStringList Playlists::ordered(const QStringList &ids) const
{
    const QStringList all = m_library.ids();
    QStringList out = ids;
    std::sort(out.begin(), out.end(), [&](const QString &a, const QString &b) {
        if (m_sort == "left") {
            const int la = m_library.progress(a).left(), lb = m_library.progress(b).left();
            if (la != lb)
                return la > lb;
        } else if (m_sort == "name") {
            const int c = QString::localeAwareCompare(name(a).toCaseFolded(), name(b).toCaseFolded());
            if (c != 0)
                return c < 0;
        }
        return all.indexOf(a) < all.indexOf(b);
    });
    return out;
}

void Playlists::rebuildModel(bool keepCurrent)
{
    const QString was = keepCurrent ? m_model.idAt(m_current) : QString();
    QStringList ids;
    for (const auto &id : m_library.ids()) {
        if (m_filter != "all" && m_library.stage(id) != m_filter)
            continue;
        if (!m_nameFilter.isEmpty() && !name(id).contains(m_nameFilter, Qt::CaseInsensitive))
            continue;
        ids << id;
    }
    ids = ordered(ids);
    m_model.reset(ids);
    const int at = was.isEmpty() ? -1 : ids.indexOf(was);
    m_current = at >= 0 ? at : std::clamp(m_current, 0, std::max(0, int(ids.size()) - 1));
    if (!keepCurrent)
        m_current = 0;
    emit viewChanged();
    emit currentChanged();
}

QVariantMap Playlists::tile(const QString &id) const
{
    const auto p = m_library.item(id);
    const auto pr = m_library.progress(id);
    const bool readable = m_library.readable(id);
    const bool reading = m_reading == id;
    QString meta;
    if (!readable) {
        if (m_library.needsEngine(id))
            meta = reading ? QStringLiteral("Reading its songs from Spotify…")
                   : m_engine && m_engine->auth() == "ok" ? QStringLiteral("Waiting to read its songs")
                   : QStringLiteral("Sign in to Spotify to read its songs");
        else
            meta = p.value("tracks").isNull() ? QStringLiteral("haystack hasn't read it yet") : QStringLiteral("No songs");
    } else if (pr.left() == 0) {
        meta = QStringLiteral("All reviewed");
    } else if (pr.reviewed()) {
        meta = QStringLiteral("%1 of %2 to review").arg(num(pr.left()), num(pr.songs));
    } else {
        meta = songs(pr.songs);
    }
    QVariantList stars;
    for (int i = 0; i < 5; ++i)
        stars << pr.stars[i];
    const QString cover = p.value("cover").toString();
    return {{"id", id}, {"name", p.value("name")}, {"coverSource", m_haystack ? m_haystack->cover(cover) : cover},
            {"mine", p.value("mine").toBool()}, {"owner", p.value("owner")}, {"songs", pr.songs},
            {"reviewed", pr.reviewed()}, {"rated", pr.rated()}, {"left", pr.left()}, {"stars", stars},
            {"passed", pr.passed}, {"disliked", pr.disliked}, {"average", pr.average()},
            {"stage", m_library.stage(id)}, {"readable", readable}, {"meta", meta},
            {"selected", m_selected.contains(id)}, {"artists", p.value("artists")}, {"reading", reading}};
}

QVariantMap Playlists::stageCounts() const
{
    QVariantMap out{{"all", m_library.size()}, {"fresh", 0}, {"progress", 0}, {"done", 0}};
    for (const auto &id : m_library.ids()) {
        const QString s = m_library.stage(id);
        out.insert(s, out.value(s).toInt() + 1);
    }
    return out;
}

void Playlists::setFilter(const QString &f)
{
    if (f == m_filter || !kFilters.contains(f))
        return;
    m_filter = f;
    rebuildModel(true);
}

void Playlists::setSort(const QString &s)
{
    if (s == m_sort || !kSorts.contains(s))
        return;
    m_sort = s;
    rebuildModel(true);
    save();
}

QString Playlists::sortLabel() const
{
    return m_sort == "left" ? QStringLiteral("Most to review") : m_sort == "name" ? QStringLiteral("Name")
                                                                                 : QStringLiteral("Spotify order");
}

void Playlists::setNameFilter(const QString &f)
{
    if (f == m_nameFilter)
        return;
    m_nameFilter = f;
    rebuildModel(false);
}

void Playlists::cycleFilter()
{
    setFilter(kFilters.value((kFilters.indexOf(m_filter) + 1) % kFilters.size()));
}

void Playlists::cycleSort()
{
    setSort(kSorts.value((kSorts.indexOf(m_sort) + 1) % kSorts.size()));
}

void Playlists::setCurrentIndex(int i)
{
    i = std::clamp(i, 0, std::max(0, shown() - 1));
    if (i == m_current)
        return;
    m_current = i;
    emit currentChanged();
    emit selectionChanged();            // the dock's hint names the playlist in focus
}

void Playlists::move(int delta)
{
    setCurrentIndex(m_current + delta);
}

QVariantMap Playlists::current() const
{
    const QString id = m_model.idAt(m_current);
    if (id.isEmpty())
        return {};
    QVariantMap t = tile(id);
    t.insert("index", m_current);
    t.insert("spaceText", m_selected.contains(id) ? QStringLiteral("Take out of this review")
                                                 : QStringLiteral("Add to this review"));
    t.insert("enterText", m_selected.isEmpty() ? QStringLiteral("Review only this playlist")
                          : QStringLiteral("Start the review of %1 playlist%2")
                                .arg(m_selected.size()).arg(m_selected.size() == 1 ? "" : "s"));
    return t;
}

QVariantMap Playlists::selectionInfo() const
{
    const QStringList ids = ordered(m_selected);
    const auto s = m_library.selection(ids);
    const int toReview = m_skip ? s.songs - s.withVerdict : s.songs;
    QVariantList covers;
    for (const auto &id : ids.mid(0, 4))
        covers << tile(id).value("coverSource");
    QString line1, line2;
    bool canStart = false;
    if (ids.isEmpty()) {
        const auto c = current();
        const bool ok = c.value("readable").toBool() && (!m_skip || c.value("left").toInt() > 0);
        line1 = QStringLiteral("Nothing picked yet");
        line2 = ok ? QStringLiteral("Enter reviews %1 on its own. Space picks more than one.").arg(c.value("name").toString())
                   : QStringLiteral("Space picks the playlist in focus, and A picks every one shown.");
        canStart = ok;
    } else {
        const QString lists = QStringLiteral("%1 playlist%2").arg(ids.size()).arg(ids.size() == 1 ? "" : "s");
        QStringList more;
        if (toReview == 0) {
            line1 = lists + QStringLiteral(" · nothing left to review");
            more << QStringLiteral("Every song has a verdict: turn off Skip songs with a verdict to hear them again");
        } else {
            line1 = lists + QStringLiteral(" · ") + songs(toReview) + (m_skip ? QStringLiteral(" to review") : QString());
            if (s.withVerdict)
                more << (m_skip ? QStringLiteral("%1 already have a verdict").arg(num(s.withVerdict))
                                : QStringLiteral("%1 already have a verdict and play again").arg(num(s.withVerdict)));
            if (s.overlap)
                more << QStringLiteral("%1 %2 on more than one and play%3 once")
                            .arg(num(s.overlap), s.overlap == 1 ? "is" : "are", s.overlap == 1 ? "s" : "");
        }
        line2 = more.join(QStringLiteral(" · "));
        canStart = toReview > 0;
    }
    return {{"count", ids.size()}, {"songs", s.songs}, {"toReview", toReview}, {"withVerdict", s.withVerdict},
            {"overlap", s.overlap}, {"covers", covers}, {"line1", line1}, {"line2", line2}, {"canStart", canStart}};
}

void Playlists::setSkipVerdicts(bool on)
{
    if (on == m_skip)
        return;
    m_skip = on;
    save();
    emit selectionChanged();
}

void Playlists::setShuffle(bool on)
{
    if (on == m_shuffle)
        return;
    m_shuffle = on;
    save();
    emit selectionChanged();
}

void Playlists::toggle(int row)
{
    const QString id = m_model.idAt(row);
    if (id.isEmpty())
        return;
    m_current = row;
    if (!m_library.readable(id)) {
        const QString n = name(id);
        if (m_library.needsEngine(id))
            m_notice = m_engine && m_engine->auth() == "ok"
                       ? QStringLiteral("needle is reading the songs of %1 from Spotify. It can be picked in a moment.").arg(n)
                       : QStringLiteral("Sign in to Spotify, and needle reads the songs of %1 so it can be picked.").arg(n);
        else
            m_notice = m_library.item(id).value("tracks").isNull()
                       ? QStringLiteral("haystack hasn't read %1 yet. Try again in a few minutes.").arg(n)
                       : QStringLiteral("%1 has no songs.").arg(n);
        emit pickerChanged();
        emit currentChanged();
        return;
    }
    if (m_selected.contains(id))
        m_selected.removeAll(id);
    else
        m_selected << id;
    m_notice.clear();
    m_model.refresh();
    emit pickerChanged();
    emit currentChanged();
    emit selectionChanged();
}

void Playlists::pickAll()
{
    QStringList shownIds;
    for (int i = 0; i < m_model.rowCount(); ++i)
        if (m_library.readable(m_model.idAt(i)))
            shownIds << m_model.idAt(i);
    const bool all = std::all_of(shownIds.cbegin(), shownIds.cend(), [this](const QString &id) { return m_selected.contains(id); });
    for (const auto &id : shownIds) {
        if (all)
            m_selected.removeAll(id);
        else if (!m_selected.contains(id))
            m_selected << id;
    }
    m_model.refresh();
    emit currentChanged();
    emit selectionChanged();
}

void Playlists::clearSelection()
{
    m_selected.clear();
    m_model.refresh();
    emit currentChanged();
    emit selectionChanged();
}

void Playlists::openPicker()
{
    if (m_selected.isEmpty())
        for (const auto &id : m_session.playlists)
            if (m_library.readable(id))
                m_selected << id;
    m_pickerOpen = true;
    m_notice.clear();
    m_model.refresh();
    emit pickerChanged();
    emit selectionChanged();
    emit currentChanged();
}

void Playlists::closePicker()
{
    if (!m_pickerOpen)
        return;
    m_pickerOpen = false;
    emit pickerChanged();
}

// ---- starting a review ----

void Playlists::start()
{
    if (m_starting)
        return;
    QStringList ids;
    for (const auto &id : ordered(m_selected))     // the order of the wall
        if (m_library.readable(id))
            ids << id;
    if (ids.isEmpty() && m_selected.isEmpty() && m_library.readable(m_model.idAt(m_current)))
        ids << m_model.idAt(m_current);
    if (ids.isEmpty())
        return;
    const auto s = m_library.selection(ids);
    if (m_skip && s.songs - s.withVerdict <= 0) {
        m_notice = QStringLiteral("Every song there has a verdict already. Turn off Skip songs with a verdict to hear them again.");
        emit pickerChanged();
        return;
    }
    m_starting = true;
    m_notice.clear();
    m_starts = ids;
    m_waiting = ids;
    m_fetched.clear();
    emit pickerChanged();
    for (const auto &id : ids) {
        if (m_haystack)
            m_haystack->fetchPlaylist(id);
        else
            playlistFailed(id, 0);
    }
}

void Playlists::playlistArrived(const QString &id, const QVariantMap &playlist)
{
    if (!m_waiting.contains(id))
        return;
    m_fetched.insert(id, playlist);
    m_waiting.removeAll(id);
    if (m_waiting.isEmpty())
        finishStart();
}

void Playlists::playlistFailed(const QString &id, int status)
{
    if (!m_waiting.contains(id))
        return;
    // offline: the copy fetched last time does
    const QVariantMap saved = m_haystack ? m_haystack->cachedPlaylist(id) : QVariantMap{};
    if (!saved.isEmpty()) {
        playlistArrived(id, saved);
        return;
    }
    m_starting = false;
    m_waiting.clear();
    m_notice = status == 0 ? QStringLiteral("Can't reach haystack, and %1 isn't saved on this computer yet.").arg(name(id))
                           : QStringLiteral("haystack couldn't send %1 (HTTP %2).").arg(name(id)).arg(status);
    emit pickerChanged();
}

static void addCards(QHash<QString, QVariantMap> &byId, QStringList *order, const QString &playlist,
                     const QVariantList &items)
{
    for (const auto &v : items) {
        QVariantMap c = v.toMap();
        const QString id = c.value("id").toString();
        if (id.isEmpty() || byId.contains(id))
            continue;
        QStringList on = c.value("playlists").toStringList();
        if (!on.contains(playlist))
            on << playlist;
        c.insert("playlists", on);
        byId.insert(id, c);
        if (order)
            *order << id;
    }
}

void Playlists::finishStart()
{
    QHash<QString, QVariantMap> byId;
    QStringList songs;
    for (const auto &id : std::as_const(m_starts))
        addCards(byId, &songs, id, m_fetched.value(id).value("items").toList());
    if (m_skip)
        songs.erase(std::remove_if(songs.begin(), songs.end(), [&](const QString &t) {
                        return !byId.value(t).value("verdict").toMap().isEmpty() || m_library.hasVerdict(t);
                    }), songs.end());
    m_starting = false;
    if (songs.isEmpty()) {
        m_notice = QStringLiteral("Every song there has a verdict already. Turn off Skip songs with a verdict to hear them again.");
        emit pickerChanged();
        return;
    }
    m_session = PlaylistSession{};
    m_session.playlists = m_starts;
    m_session.skipVerdicts = m_skip;
    m_session.shuffle = m_shuffle;
    m_session.seed = QRandomGenerator::global()->generate64();
    m_session.tracks = PlaylistSession::order(songs, m_shuffle, m_session.seed);
    m_session.startedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    m_cards = byId;
    m_selected = m_starts;
    save();
    m_pickerOpen = false;
    emit pickerChanged();
    emit sessionChanged();
    emit selectionChanged();
    m_model.refresh();
    emit sessionReady(sessionCards(), !m_session.skipVerdicts, {}, true);
}

QVariantList Playlists::sessionCards() const
{
    QHash<QString, QString> names;
    for (const auto &id : m_library.ids())
        names.insert(id, name(id));
    for (auto it = m_fetched.cbegin(); it != m_fetched.cend(); ++it)
        if (!names.contains(it.key()))
            names.insert(it.key(), it.value().value("name").toString());
    // the saved cards are as old as the review: the verdicts given since are in the snapshot (or still on their way)
    QHash<QString, QVariantMap> cards = m_cards;
    for (auto it = cards.begin(); it != cards.end(); ++it) {
        const auto v = m_library.verdict(it.key());
        if (!v.isEmpty() && v.value("given_at").toString() >= it->value("verdict").toMap().value("given_at").toString())
            it->insert("verdict", v);
    }
    return m_session.cards(cards, names);
}

QVariantList Playlists::sessionPlaylists() const
{
    QVariantList out;
    for (const auto &id : m_session.playlists) {
        const auto t = tile(id);
        out << QVariantMap{{"id", id}, {"name", name(id)}, {"coverSource", t.value("coverSource")}};
    }
    return out;
}

void Playlists::restore()
{
    if (m_session.isEmpty())
        return;
    if (m_haystack)
        for (const auto &v : m_haystack->pendingVerdicts())
            verdictAdded(v.toObject());
    QHash<QString, QVariantMap> byId;
    for (const auto &id : std::as_const(m_session.playlists)) {
        const auto saved = m_haystack ? m_haystack->cachedPlaylist(id) : QVariantMap{};
        m_fetched.insert(id, saved);
        addCards(byId, nullptr, id, saved.value("items").toList());
    }
    if (byId.isEmpty()) {                 // the saved cards are gone: start over
        m_session = PlaylistSession{};
        save();
        emit sessionChanged();
        return;
    }
    m_cards = byId;
    if (m_selected.isEmpty())
        m_selected = m_session.playlists;
    emit sessionChanged();
    emit selectionChanged();
    emit sessionReady(sessionCards(), !m_session.skipVerdicts, m_session.done, false);
}

void Playlists::verdictAdded(const QJsonObject &v)
{
    const QString track = v.value("track").toString();
    QVariantMap verdict{{"kind", v.value("kind").toString()}, {"given_at", v.value("given_at").toString()}};
    if (v.contains("stars"))
        verdict.insert("stars", v.value("stars").toInt());
    m_library.applyVerdict(track, verdict);
    if (m_cards.contains(track))
        m_cards[track].insert("verdict", verdict);
    if (!m_session.isEmpty() && !m_session.skipVerdicts && m_session.tracks.contains(track)
        && !m_session.done.contains(track)) {
        m_session.done << track;
        save();
    }
    m_model.refresh();
    emit changed();
    emit currentChanged();
    emit selectionChanged();
}

// ---- playlists only needle's engine can read ----

void Playlists::readNext()
{
    if (!m_reading.isEmpty() || !m_engine || m_engine->auth() != "ok" || !m_engine->connected())
        return;
    if (m_haystack && m_haystack->status() != "synced")
        return;
    for (const auto &id : m_library.ids()) {
        const QString key = id + '@' + m_library.item(id).value("snapshot_id").toString();
        if (!m_library.needsEngine(id) || m_tried.contains(key))
            continue;
        m_tried.insert(key);
        m_reading = id;
        m_readWatchdog.start();
        m_engine->readPlaylist(id);
        m_model.refresh();
        emit changed();
        return;
    }
}

QString Playlists::readingText() const
{
    int waiting = 0;
    for (const auto &id : m_library.ids())
        waiting += m_library.needsEngine(id) && id != m_reading;
    if (!m_reading.isEmpty())
        return QStringLiteral("Reading %1 from Spotify%2").arg(name(m_reading),
               waiting ? QStringLiteral(", %1 more to go").arg(waiting) : QString());
    if (waiting && (!m_engine || m_engine->auth() != "ok"))
        return QStringLiteral("Sign in to Spotify so needle can read the %1 playlist%2 you follow.")
            .arg(waiting).arg(waiting == 1 ? "" : "s");
    return {};
}

// ---- saved between runs ----

void Playlists::save() const
{
    QJsonObject o{{"picker", QJsonObject{{"skip_verdicts", m_skip}, {"shuffle", m_shuffle}, {"sort", m_sort}}}};
    if (!m_session.isEmpty())
        o.insert("session", m_session.toJson());
    QSaveFile f(m_file);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
        f.commit();
    }
}

void Playlists::load()
{
    QFile f(m_file);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const auto o = QJsonDocument::fromJson(f.readAll()).object();
    const auto picker = o.value("picker").toObject();
    m_skip = picker.value("skip_verdicts").toBool(true);
    m_shuffle = picker.value("shuffle").toBool(true);
    if (kSorts.contains(picker.value("sort").toString()))
        m_sort = picker.value("sort").toString();
    m_session = PlaylistSession::fromJson(o.value("session").toObject());
}
