#include "engineclient.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

EngineClient::EngineClient(const QString &program, const QString &socketPath, QObject *parent)
    : QObject(parent), m_program(program), m_socketPath(socketPath)
{
    m_since.start();
    m_connectRetry.setSingleShot(true);
    m_connectRetry.setInterval(150);
    connect(&m_connectRetry, &QTimer::timeout, this, &EngineClient::connectSocket);
    // the progress bar moves smoothly between the engine's once-a-second positions
    m_ticker.setInterval(250);
    connect(&m_ticker, &QTimer::timeout, this, &EngineClient::positionChanged);

    connect(&m_socket, &QLocalSocket::readyRead, this, [this] {
        while (m_socket.canReadLine())
            handleLine(m_socket.readLine());
    });
    connect(&m_socket, &QLocalSocket::connected, this, [this] {
        m_restarts = 0;
        if (m_spectrum)
            setSpectrum(true);
    });
    connect(&m_socket, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError) {
        if (m_socket.state() != QLocalSocket::ConnectedState && m_process.state() == QProcess::Running)
            m_connectRetry.start();
    });
    m_process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        if (m_quitting)
            return;
        qWarning("needle-engine exited (%d)", code);
        m_state = "stopped";
        emit playStateChanged();
        m_auth = "starting";                 // signed in again once the new one says so
        m_authMessage.clear();
        emit authChanged();
        if (++m_restarts <= 5) {
            QTimer::singleShot(500 * m_restarts, this, &EngineClient::start);
        } else {
            m_auth = "missing";
            m_authMessage = QStringLiteral("The playback engine keeps stopping (needle-engine exited with %1).").arg(code);
            emit authChanged();
        }
    });
}

EngineClient::~EngineClient()
{
    m_quitting = true;
    if (m_socket.state() == QLocalSocket::ConnectedState) {
        send(QVariantMap{{"cmd", QStringLiteral("quit")}});
        m_socket.flush();
    }
    if (m_process.state() != QProcess::NotRunning && !m_process.waitForFinished(1500)) {
        m_process.terminate();
        m_process.waitForFinished(1000);
    }
}

QString EngineClient::findEngine()
{
    const QString env = qEnvironmentVariable("NEEDLE_ENGINE");
    const QStringList candidates{env, QCoreApplication::applicationDirPath() + "/needle-engine",
                                 "/usr/lib/needle/needle-engine"};
    for (const auto &c : candidates)
        if (!c.isEmpty() && QFileInfo(c).isExecutable())
            return c;
    return {};
}

void EngineClient::start()
{
    if (m_program.isEmpty()) {
        m_auth = "missing";
        m_authMessage = QStringLiteral("needle-engine is not installed (looked next to needle and in /usr/lib/needle).");
        emit authChanged();
        return;
    }
    QDir().mkpath(QFileInfo(m_socketPath).absolutePath());
    m_process.start(m_program, {"--socket", m_socketPath, "--exit-on-disconnect"});
    m_connectRetry.start();
}

void EngineClient::connectSocket()
{
    if (m_socket.state() == QLocalSocket::UnconnectedState)
        m_socket.connectToServer(m_socketPath);
}

void EngineClient::send(const QVariantMap &cmd)
{
    if (m_socket.state() != QLocalSocket::ConnectedState)
        return;
    m_socket.write(QJsonDocument(QJsonObject::fromVariantMap(cmd)).toJson(QJsonDocument::Compact) + '\n');
}

qint64 EngineClient::positionMs() const
{
    qint64 p = m_positionMs + (m_state == "playing" ? m_since.elapsed() : 0);
    return m_durationMs > 0 ? std::min(p, m_durationMs) : p;
}

void EngineClient::load(const QString &id, bool play)
{
    m_trackId = id;
    m_positionMs = 0;
    m_durationMs = 0;
    m_state = "loading";
    m_since.restart();
    emit playStateChanged();
    emit positionChanged();
    send({{"cmd", "load"}, {"id", id}, {"play", play}});
}

void EngineClient::seek(qint64 ms)
{
    m_positionMs = std::max<qint64>(0, ms);
    m_since.restart();
    emit positionChanged();
    send({{"cmd", "seek"}, {"position_ms", m_positionMs}});
}

void EngineClient::seekBy(qint64 deltaMs)
{
    seek(positionMs() + deltaMs);
}

void EngineClient::setSpectrum(bool on)
{
    m_spectrum = on;
    send({{"cmd", "spectrum"}, {"enabled", on}});
}

static QByteArray bytes(const QJsonArray &a)
{
    QByteArray out(a.size(), 0);
    for (int i = 0; i < a.size(); ++i)
        out[i] = char(quint8(a[i].toInt()));
    return out;
}

void EngineClient::handleLine(const QByteArray &line)
{
    const auto o = QJsonDocument::fromJson(line).object();
    const QString ev = o.value("event").toString();
    if (ev == "spectrum") {
        m_bands = bytes(o.value("bands").toArray());
        m_peaks = bytes(o.value("peaks").toArray());
        emit spectrumChanged();
    } else if (ev == "position") {
        if (o.value("id").toString() == m_trackId) {
            m_positionMs = o.value("position_ms").toInteger();
            m_since.restart();
            emit positionChanged();
        }
    } else if (ev == "state") {
        const QString id = o.value("id").toString();
        if (!id.isEmpty() && id != m_trackId)
            return;                        // a late event about the previous track
        m_state = o.value("state").toString();
        m_positionMs = o.value("position_ms").toInteger();
        if (o.contains("duration_ms"))
            m_durationMs = o.value("duration_ms").toInteger();
        m_since.restart();
        m_state == "playing" ? m_ticker.start() : m_ticker.stop();
        emit playStateChanged();
        emit positionChanged();
    } else if (ev == "auth") {
        m_auth = o.value("state").toString();
        m_user = o.value("user").toString();
        m_authMessage = o.value("message").toString();
        emit authChanged();
    } else if (ev == "end") {
        if (o.value("id").toString() == m_trackId)
            emit ended(m_trackId);
    } else if (ev == "unavailable") {
        emit unavailable(o.value("id").toString());
    } else if (ev == "mpris") {
        emit mpris(o.value("action").toString());
    } else if (ev == "playlist") {
        emit playlistRead(o.value("id").toString(), o);
    } else if (ev == "playlist_error") {
        qWarning("needle-engine: playlist %s: %s", qPrintable(o.value("id").toString()),
                 qPrintable(o.value("message").toString()));
        emit playlistReadFailed(o.value("id").toString(), o.value("message").toString());
    } else if (ev == "error") {
        qWarning("needle-engine: %s", qPrintable(o.value("message").toString()));
        emit error(o.value("message").toString());
    }
}
