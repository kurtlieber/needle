#include "review.h"

#include <QJsonObject>
#include <QLocale>
#include <QUuid>

#include "haystackclient.h"
#include "engineclient.h"

static const qint64 kPlayCountsMs = 5000;

Review::Review(HaystackClient *haystack, EngineClient *engine, QObject *parent)
    : QObject(parent), m_haystack(haystack), m_engine(engine)
{
    m_measureRefresh.setSingleShot(true);
    m_measureRefresh.setInterval(20000);
    if (m_haystack) {
        connect(&m_measureRefresh, &QTimer::timeout, this, [this] {
            // a Playlist review's cards aren't in the snapshot: ask for the one on screen, for two minutes at most
            if (m_queue != "playlists")
                m_haystack->refresh();
            else if (++m_measureChecks <= 20)
                m_haystack->fetchTrack(q().currentId());
        });
        connect(m_haystack, &HaystackClient::stateArrived, this, &Review::applyState);
        connect(m_haystack, &HaystackClient::trackArrived, this, &Review::updateCard);
        if (!m_haystack->cachedState().isEmpty())
            applyState(m_haystack->cachedState());
    }
    if (m_engine) {
        connect(m_engine, &EngineClient::positionChanged, this, [this] {
            if (m_engine->trackId() == m_loaded && m_engine->playing())
                m_maxPosition = std::max(m_maxPosition, m_engine->positionMs());
        });
        connect(m_engine, &EngineClient::ended, this, [this] {
            leaveTrack(true);
            move(true, true);
        });
        // Spotify can't play it here: on to the next one, playing only if this one was meant to
        connect(m_engine, &EngineClient::unavailable, this, [this] { move(true, m_loadPlays); });
        connect(m_engine, &EngineClient::mpris, this, [this](const QString &a) {
            if (a == "next")
                next();
            else if (a == "previous")
                back();
        });
        connect(m_engine, &EngineClient::authChanged, this, [this] {
            if (m_engine->auth() == "ok" && m_loaded.isEmpty())
                loadCurrent(false);
        });
    }
}

static QVariantMap withVerdict(QVariantMap card, const QVariantMap &v)
{
    card.insert("verdict", v);
    return card;
}

void Review::applyState(const QVariantMap &state)
{
    const auto queues = state.value("queues").toMap();
    const QStringList features = state.value("server").toMap().value("features").toStringList();
    QVariantList disc = queues.value("discoveries").toMap().value("items").toList();
    // only a haystack that lists the feature has the queue (its docs/api.md)
    QVariantList calibrate = features.contains("queue.calibrate")
                                 ? queues.value("calibrate").toMap().value("items").toList() : QVariantList{};
    QVariantList likes = queues.value("likes").toMap().value("items").toList();
    // verdicts haystack hasn't got yet still count here
    QHash<QString, QVariantMap> pending;
    if (m_haystack)
        for (const auto &v : m_haystack->pendingVerdicts()) {
            const auto o = v.toObject();
            pending.insert(o.value("track").toString(),
                           {{"kind", o.value("kind").toString()}, {"stars", o.value("stars").toVariant()},
                            {"given_at", o.value("given_at").toString()}});
        }
    for (auto *list : {&disc, &calibrate, &likes})
        for (auto &c : *list) {
            const QString id = c.toMap().value("id").toString();
            if (pending.contains(id))
                c = withVerdict(c.toMap(), pending.value(id));
        }
    const QString before = q().currentId();
    m_discoveries.setCards(disc);
    m_calibrate.setCards(calibrate);
    m_likes.setCards(likes);
    m_baseline = state.value("baseline").toMap().value("traits").toMap();
    m_features = features;
    if (m_queue == "calibrate" && !hasCalibrate()) {
        leaveTrack(false);
        m_queue = "discoveries";        // a haystack rolled back to before 0.5.0
    }
    m_nextBatch = QDateTime::fromString(queues.value("discoveries").toMap().value("next_batch_at").toString(), Qt::ISODate);
    const bool first = !m_hasState;
    m_hasState = true;
    // open on Discoveries while it has tracks waiting, else on Calibrate, else on Rate your likes
    if (first && !m_discoveries.hasCurrent()) {
        if (m_calibrate.hasCurrent())
            m_queue = "calibrate";
        else if (m_likes.hasCurrent())
            m_queue = "likes";
    }
    emit changed();
    if (q().currentId() != before || first)
        loadCurrent(false);
    prefetch();
    measureSoon();
}

QVariantMap Review::current() const
{
    auto c = q().current();
    if (!c.isEmpty() && m_haystack)
        c.insert("coverSource", m_haystack->cover(c.value("cover").toString()));
    return c;
}

QVariantMap Review::upNext() const
{
    auto c = q().upNext();
    if (!c.isEmpty() && m_haystack) {
        // The full-size art: the Up next card shows it at 120 px (cover_small is 64 px), and it is the same
        // image the track shows once it plays, so it is cached by then.
        const QString big = c.value("cover").toString();
        c.insert("coverSource", m_haystack->cover(big.isEmpty() ? c.value("cover_small").toString() : big));
    }
    return c;
}

int Review::position(const ReviewQueue &queue)
{
    return std::min(queue.size(), queue.reviewed() - (queue.currentReviewed() ? 1 : 0) + 1);
}

int Review::discoveriesPosition() const
{
    return position(m_discoveries);
}

QString Review::nextBatchText() const
{
    if (!m_nextBatch.isValid())
        return {};
    const QDateTime local = m_nextBatch.toLocalTime();
    const int days = QDate::currentDate().daysTo(local.date());
    const QString time = QLocale().toString(local.time(), "HH:mm");
    if (days == 0)
        return QStringLiteral("today at %1").arg(time);
    if (days == 1)
        return QStringLiteral("tomorrow at %1").arg(time);
    return QStringLiteral("%1 at %2").arg(QLocale().toString(local.date(), "dddd"), time);
}

void Review::setQueue(const QString &name)
{
    const bool offered = name == "discoveries" || name == "likes" || name == "playlists"
                         || (name == "calibrate" && hasCalibrate());
    if (name == m_queue || !offered)
        return;
    const bool wasPlaying = m_engine && m_engine->playing();
    leaveTrack(false);
    m_queue = name;
    if (q().currentReviewed())
        q().next();
    emit changed();
    if (!q().hasCurrent() && m_engine)
        m_engine->pause();              // nothing of this queue to play (the playlist picker, a finished queue)
    loadCurrent(wasPlaying);
    prefetch();
    measureSoon();
}

QString Review::tabNext() const
{
    if (m_queue == "discoveries")
        return hasCalibrate() ? "calibrate" : "likes";
    return m_queue == "calibrate" ? "likes" : m_queue == "likes" ? "playlists" : "discoveries";
}

QString Review::doneNext() const
{
    const bool calibrate = hasCalibrate() && calibrateLeft() > 0;
    if (m_queue == "discoveries")
        return calibrate ? "calibrate" : likesLeft() > 0 ? "likes" : "";
    if (m_queue == "calibrate")
        return likesLeft() > 0 ? "likes" : discoveriesLeft() > 0 ? "discoveries" : "";
    if (m_queue == "likes")
        return discoveriesLeft() > 0 ? "discoveries" : calibrate ? "calibrate" : "";
    return {};
}

void Review::switchQueue()
{
    setQueue(tabNext());
}

void Review::startPlaylistReview(const QVariantList &cards, bool revisit, const QStringList &done, bool activate)
{
    if (activate)
        leaveTrack(false);
    m_playlists.reset(cards, revisit, done);
    m_measureAsked.clear();
    if (!activate) {
        emit changed();
        return;
    }
    m_queue = "playlists";
    emit changed();
    loadCurrent(true);
    prefetch();
    measureSoon();
}

void Review::updateCard(const QVariantMap &card)
{
    const QString id = card.value("id").toString();
    if (!m_playlists.updateCard(card))
        return;
    if (m_queue == "playlists" && q().currentId() == id) {
        emit changed();
        measureSoon();
    }
}

void Review::give(const QString &kind, int stars, bool advance)
{
    const QString id = q().currentId();
    if (id.isEmpty())
        return;
    const QString given = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    QJsonObject v{{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)}, {"track", id}, {"kind", kind},
                  {"given_at", given}, {"queue", m_queue}};
    QVariantMap local{{"kind", kind}, {"given_at", given}};
    if (kind == "rating") {
        v.insert("stars", stars);
        local.insert("stars", stars);
    }
    // the same track can sit in more than one queue (a liked discovery); the others move past it
    for (auto *queue : {&m_discoveries, &m_calibrate, &m_likes, &m_playlists}) {
        queue->applyVerdict(id, local);
        if (queue != &q() && queue->currentId() == id && queue->currentReviewed())
            queue->next();
    }
    if (m_haystack)
        m_haystack->addVerdict(v);
    emit changed();
    if (advance) {
        leaveTrack(false);
        move(true, true);
    }
}

void Review::rate(int stars, bool advance)
{
    if (stars >= 1 && stars <= 5)
        give("rating", stars, advance);
}

void Review::pass() { give("pass", 0, true); }
void Review::dislike() { give("dislike", 0, true); }

void Review::next()
{
    leaveTrack(false);
    move(true, true);
}

void Review::back()
{
    leaveTrack(false);
    move(false, true);
}

void Review::move(bool forward, bool play)
{
    const QString before = q().currentId();
    forward ? q().next() : q().back();
    emit changed();
    if (q().currentId() != before)
        loadCurrent(play);
    else if (!q().hasCurrent() && m_engine)
        m_engine->pause();
    prefetch();
    measureSoon();
}

void Review::loadCurrent(bool play)
{
    const QString id = q().currentId();
    m_loaded.clear();
    m_maxPosition = 0;
    m_measureChecks = 0;
    if (id.isEmpty() || !m_engine || m_engine->auth() != "ok")
        return;
    m_loaded = id;
    m_loadPlays = play;
    m_playStarted = QDateTime::currentDateTimeUtc();
    m_engine->load(id, play);
}

void Review::leaveTrack(bool completed)
{
    if (m_loaded.isEmpty() || !m_haystack)
        return;
    const qint64 played = completed && m_engine ? std::max(m_maxPosition, m_engine->durationMs()) : m_maxPosition;
    if (played >= kPlayCountsMs)
        m_haystack->addPlay({{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)}, {"track", m_loaded},
                           {"started_at", m_playStarted.toString(Qt::ISODateWithMs)}, {"ms_played", played},
                           {"completed", completed}});
    m_maxPosition = 0;
}

void Review::prefetch()
{
    if (!m_haystack)
        return;
    QStringList urls;
    for (const auto *queue : {&m_discoveries, &m_calibrate, &m_likes, &m_playlists})
        for (const auto &c : queue->lookahead(queue == &q() ? 8 : 3)) {
            urls << c.toMap().value("cover").toString() << c.toMap().value("cover_small").toString();
        }
    m_haystack->prefetchCovers(urls);
}

void Review::measureSoon()
{
    // haystack measures tracks without Readings on demand: look again shortly while one is on screen
    const auto unmeasured = [](const QVariantMap &c) {
        const QString m = c.value("measure").toString();
        return m == "pending" || m == "unknown";
    };
    if (m_queue == "playlists" && m_haystack) {
        // the snapshot doesn't carry these cards, so ask for the songs coming up: haystack measures them in turn
        QVariantList soon{q().current()};
        soon += q().lookahead(3);
        for (const auto &v : std::as_const(soon)) {
            const auto c = v.toMap();
            const QString id = c.value("id").toString();
            if (!id.isEmpty() && unmeasured(c) && !m_measureAsked.contains(id)) {
                m_measureAsked.insert(id);
                m_haystack->fetchTrack(id);
            }
        }
    }
    if (unmeasured(q().current())) {
        // one card is cheap to ask for again; the whole snapshot isn't
        m_measureRefresh.setInterval(m_queue == "playlists" ? 6000 : 20000);
        m_measureRefresh.start();
    }
}
