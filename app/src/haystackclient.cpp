#include "haystackclient.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUrl>

static const int kRefreshMs = 5 * 60 * 1000;
static const int kRetryMs = 30 * 1000;
static const int kTimeoutMs = 8000;

HaystackClient::HaystackClient(const QStringList &baseUrls, const QString &tokenPath, const QString &cacheDir,
                           const QString &stateDir, QObject *parent)
    : QObject(parent), m_bases(baseUrls), m_tokenPath(tokenPath), m_cacheDir(cacheDir),
      m_outbox(stateDir + "/outbox.json")
{
    QDir().mkpath(cacheDir + "/covers");
    QDir().mkpath(cacheDir + "/playlists");
    QFile f(cacheDir + "/state.json");
    if (f.open(QIODevice::ReadOnly))
        m_state = QJsonDocument::fromJson(f.readAll()).object().toVariantMap();
    m_refreshTimer.setInterval(kRefreshMs);
    connect(&m_refreshTimer, &QTimer::timeout, this, &HaystackClient::refresh);
    m_refreshTimer.start();
    m_retryTimer.setSingleShot(true);
    m_retryTimer.setInterval(kRetryMs);
    connect(&m_retryTimer, &QTimer::timeout, this, &HaystackClient::refresh);
}

bool HaystackClient::readToken()
{
    QFile f(m_tokenPath);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    m_token = QString::fromUtf8(f.readAll()).trimmed();
    return !m_token.isEmpty();
}

QString HaystackClient::tokenFile() const
{
    const QString home = QDir::homePath() + '/';
    return m_tokenPath.startsWith(home) ? "~/" + m_tokenPath.mid(home.size()) : m_tokenPath;
}

QString HaystackClient::statusText() const
{
    const int n = pending();
    const QString waiting = n ? QStringLiteral(", %1 verdict%2 waiting").arg(n).arg(n == 1 ? "" : "s") : QString();
    if (m_status == "no_token")
        return QStringLiteral("No haystack token (%1)").arg(tokenFile());
    if (m_status == "unauthorised")
        return QStringLiteral("haystack refused the token") + waiting;
    if (m_status == "offline")
        return QStringLiteral("Offline") + (n ? waiting : QStringLiteral(", using saved queues"));
    if (m_status == "syncing")
        return QStringLiteral("Syncing with haystack");
    if (m_spotifyPending && !m_spotifyError.isEmpty())
        return QStringLiteral("Synced with haystack, Spotify writes waiting");
    return QStringLiteral("Synced with haystack");
}

void HaystackClient::setStatus(const QString &s)
{
    if (s != m_status) {
        m_status = s;
    }
    emit statusChanged();
}

QNetworkReply *HaystackClient::send(const QString &method, const QString &path, const QByteArray &body)
{
    QNetworkRequest rq(QUrl(m_bases.value(m_base) + path));
    rq.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
    rq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    rq.setTransferTimeout(kTimeoutMs);
    return m_nam.sendCustomRequest(rq, method.toUtf8(), body);
}

void HaystackClient::networkFailed(QNetworkReply *reply, std::function<void()> retry)
{
    const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (code == 401 || code == 403) {
        setStatus("unauthorised");
        return;
    }
    // no answer at all: try the next server, once round, then call it offline
    if (code == 0 && ++m_failures < m_bases.size()) {
        m_base = (m_base + 1) % m_bases.size();
        retry();
        return;
    }
    m_failures = 0;
    setStatus("offline");
    m_retryTimer.start();
}

void HaystackClient::refresh()
{
    if (!readToken()) {
        setStatus("no_token");
        return;
    }
    if (m_outbox.size()) {
        flush();                 // send first, so the snapshot already includes our verdicts
        return;
    }
    fetchState();
}

void HaystackClient::fetchState()
{
    auto *r = send("GET", "/v1/state");
    connect(r, &QNetworkReply::finished, this, [this, r] {
        r->deleteLater();
        if (r->error() != QNetworkReply::NoError) {
            networkFailed(r, [this] { fetchState(); });
            return;
        }
        m_failures = 0;
        const QByteArray body = r->readAll();
        const auto doc = QJsonDocument::fromJson(body);
        if (!doc.isObject()) {
            setStatus("offline");
            return;
        }
        QSaveFile f(m_cacheDir + "/state.json");
        if (f.open(QIODevice::WriteOnly)) {
            f.write(body);
            f.commit();
        }
        m_state = doc.object().toVariantMap();
        const auto sp = m_state.value("spotify").toMap();
        m_spotifyPending = sp.value("pending_writes").toInt();
        m_spotifyError = sp.value("error").toString();
        setStatus("synced");
        emit stateArrived(m_state);
    });
}

void HaystackClient::postBatch(const QString &path, const QString &key, const QJsonArray &items, std::function<void()> then)
{
    if (items.isEmpty()) {
        then();
        return;
    }
    auto *r = send("POST", path, QJsonDocument(QJsonObject{{key, items}}).toJson(QJsonDocument::Compact));
    connect(r, &QNetworkReply::finished, this, [this, r, path, key, items, then] {
        r->deleteLater();
        if (r->error() != QNetworkReply::NoError) {
            m_flushing = false;
            networkFailed(r, [this] { flush(); });
            return;
        }
        m_failures = 0;
        const auto o = QJsonDocument::fromJson(r->readAll()).object();
        QStringList done;
        for (const auto &v : o.value("accepted").toArray())
            done << v.toString();
        for (const auto &v : o.value("rejected").toArray()) {
            qWarning("haystack rejected %s: %s", qPrintable(v.toObject().value("id").toString()),
                     qPrintable(v.toObject().value("error").toString()));
            done << v.toObject().value("id").toString();
        }
        m_outbox.settle(done);
        then();
    });
}

void HaystackClient::flush()
{
    if (m_flushing)
        return;
    if (!readToken()) {
        setStatus("no_token");
        return;
    }
    m_flushing = true;
    setStatus("syncing");
    postBatch("/v1/verdicts", "verdicts", m_outbox.verdicts(), [this] {
        postBatch("/v1/plays", "plays", m_outbox.plays(), [this] {
            const bool swaps = m_state.value("server").toMap().value("features").toStringList().contains("swaps");
            postBatch("/v1/swaps", "swaps", swaps ? m_outbox.swaps() : QJsonArray{}, [this] {
                m_flushing = false;
                fetchState();
            });
        });
    });
}

void HaystackClient::addVerdict(const QJsonObject &v)
{
    m_outbox.addVerdict(v);
    emit statusChanged();
    emit verdictAdded(v);
    flush();
}

void HaystackClient::addSwap(const QJsonObject &s)
{
    m_outbox.addSwap(s);
    emit swapAdded(s);
    flush();
}

void HaystackClient::addPlay(const QJsonObject &p)
{
    m_outbox.addPlay(p);
    if (m_status != "offline")
        flush();
}

QString HaystackClient::coverPath(const QString &url) const
{
    const auto h = QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Sha1).toHex();
    return m_cacheDir + "/covers/" + QString::fromLatin1(h) + ".jpg";
}

QString HaystackClient::cover(const QString &url)
{
    if (url.isEmpty())
        return {};
    const QString p = coverPath(url);
    if (QFileInfo::exists(p))
        return QUrl::fromLocalFile(p).toString();
    prefetchCovers({url});
    return url;
}

void HaystackClient::prefetchCovers(const QStringList &urls)
{
    for (const auto &u : urls)
        if (!u.isEmpty() && !m_coverQueue.contains(u) && !m_coverBusy.contains(u) && !QFileInfo::exists(coverPath(u)))
            m_coverQueue << u;
    while (m_coverBusy.size() < 4 && !m_coverQueue.isEmpty())
        fetchNextCover();
}

void HaystackClient::fetchNextCover()
{
    const QString url = m_coverQueue.takeFirst();
    m_coverBusy.insert(url);
    QNetworkRequest rq{QUrl(url)};
    rq.setTransferTimeout(15000);
    auto *r = m_nam.get(rq);
    connect(r, &QNetworkReply::finished, this, [this, r, url] {
        r->deleteLater();
        m_coverBusy.remove(url);
        if (r->error() == QNetworkReply::NoError) {
            QSaveFile f(coverPath(url));
            if (f.open(QIODevice::WriteOnly)) {
                f.write(r->readAll());
                f.commit();
            }
        }
        if (!m_coverQueue.isEmpty())
            fetchNextCover();
    });
}

void HaystackClient::call(const QString &method, const QString &path, const QByteArray &body,
                        std::function<void(const QByteArray &)> ok, std::function<void(int)> fail)
{
    if (!readToken()) {
        setStatus("no_token");
        fail(0);
        return;
    }
    auto *r = send(method, path, body);
    connect(r, &QNetworkReply::finished, this, [this, r, method, path, body, ok, fail] {
        r->deleteLater();
        const int code = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (r->error() == QNetworkReply::NoError) {
            m_failures = 0;
            ok(r->readAll());
            return;
        }
        if (code == 0 && ++m_failures < m_bases.size()) {      // no answer: the next server, once round
            m_base = (m_base + 1) % m_bases.size();
            call(method, path, body, ok, fail);
            return;
        }
        m_failures = 0;
        if (code == 401 || code == 403) {
            setStatus("unauthorised");
        } else if (code == 0) {
            setStatus("offline");
            m_retryTimer.start();
        }
        fail(code);
    });
}

QString HaystackClient::playlistPath(const QString &id) const
{
    static const QRegularExpression ok("^[0-9A-Za-z]{22}$");
    return ok.match(id).hasMatch() ? m_cacheDir + "/playlists/" + id + ".json" : QString();
}

void HaystackClient::fetchPlaylist(const QString &id)
{
    const QString file = playlistPath(id);
    if (file.isEmpty()) {
        emit playlistFailed(id, 404);
        return;
    }
    call("GET", "/v1/playlists/" + id, {}, [this, id, file](const QByteArray &body) {
        const auto doc = QJsonDocument::fromJson(body);
        if (!doc.isObject()) {
            emit playlistFailed(id, 0);
            return;
        }
        QSaveFile f(file);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(body);
            f.commit();
        }
        emit playlistArrived(id, doc.object().toVariantMap());
    }, [this, id](int code) { emit playlistFailed(id, code); });
}

QVariantMap HaystackClient::cachedPlaylist(const QString &id) const
{
    QFile f(playlistPath(id));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object().toVariantMap();
}

void HaystackClient::sendPlaylistTracks(const QString &id, const QJsonObject &body)
{
    call("PUT", "/v1/playlists/" + id + "/tracks", QJsonDocument(body).toJson(QJsonDocument::Compact),
         [this, id](const QByteArray &) { emit playlistTracksSent(id, 200); },
         [this, id](int code) {
             if (code >= 400)
                 qWarning("haystack refused playlist %s's songs (HTTP %d)", qPrintable(id), code);
             emit playlistTracksSent(id, code);
         });
}

void HaystackClient::fetchTrack(const QString &id)
{
    call("GET", "/v1/tracks/" + id, {}, [this](const QByteArray &body) {
        const auto doc = QJsonDocument::fromJson(body);
        if (doc.isObject())
            emit trackArrived(doc.object().toVariantMap());
    }, [](int) {});
}
