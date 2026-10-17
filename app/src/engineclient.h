#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QLocalSocket>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantMap>

// needle's playback engine (needle-engine, Rust + librespot, ADR 0003), started and owned by the app and
// spoken to over a Unix socket, one JSON object per line. It also reads the playlists haystack can't (ADR 0008).
class EngineClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString auth READ auth NOTIFY authChanged)            // starting needed pending ok error missing
    Q_PROPERTY(QString authMessage READ authMessage NOTIFY authChanged)
    Q_PROPERTY(QString user READ user NOTIFY authChanged)
    Q_PROPERTY(QString playState READ playState NOTIFY playStateChanged) // stopped loading playing paused
    Q_PROPERTY(bool playing READ playing NOTIFY playStateChanged)
    Q_PROPERTY(QString trackId READ trackId NOTIFY playStateChanged)
    Q_PROPERTY(qint64 positionMs READ positionMs NOTIFY positionChanged)
    Q_PROPERTY(qint64 durationMs READ durationMs NOTIFY playStateChanged)
    Q_PROPERTY(QByteArray bands READ bands NOTIFY spectrumChanged)
    Q_PROPERTY(QByteArray peaks READ peaks NOTIFY spectrumChanged)

public:
    explicit EngineClient(const QString &program, const QString &socketPath, QObject *parent = nullptr);
    ~EngineClient() override;

    static QString findEngine();

    QString auth() const { return m_auth; }
    QString authMessage() const { return m_authMessage; }
    QString user() const { return m_user; }
    QString playState() const { return m_state; }
    bool playing() const { return m_state == "playing"; }
    QString trackId() const { return m_trackId; }
    qint64 positionMs() const;
    qint64 durationMs() const { return m_durationMs; }
    QByteArray bands() const { return m_bands; }
    QByteArray peaks() const { return m_peaks; }

    void start();
    bool connected() const { return m_socket.state() == QLocalSocket::ConnectedState; }
    // Read a playlist's songs and their metadata: playlistRead or playlistReadFailed follows.
    void readPlaylist(const QString &id) { send({{"cmd", QStringLiteral("playlist")}, {"id", id}}); }
    // For tests: feed one line as if the engine had sent it.
    void handleLine(const QByteArray &line);

public slots:
    void load(const QString &id, bool play = true);
    void play() { send(QVariantMap{{"cmd", QStringLiteral("play")}}); }
    void pause() { send(QVariantMap{{"cmd", QStringLiteral("pause")}}); }
    void toggle() { send(QVariantMap{{"cmd", QStringLiteral("toggle")}}); }
    void seek(qint64 ms);
    void seekBy(qint64 deltaMs);
    void login() { send(QVariantMap{{"cmd", QStringLiteral("login")}}); }
    void setSpectrum(bool on);

signals:
    void authChanged();
    void playStateChanged();
    void positionChanged();
    void spectrumChanged();
    void ended(const QString &id);
    void unavailable(const QString &id);
    void mpris(const QString &action);
    void error(const QString &message);
    void playlistRead(const QString &id, const QJsonObject &playlist);
    void playlistReadFailed(const QString &id, const QString &message);

private:
    void send(const QVariantMap &cmd);
    void connectSocket();

    QString m_program, m_socketPath;
    QProcess m_process;
    QLocalSocket m_socket;
    QTimer m_connectRetry, m_ticker;
    int m_restarts = 0;
    bool m_quitting = false;
    bool m_spectrum = false;

    QString m_auth = "starting", m_authMessage, m_user;
    QString m_state = "stopped", m_trackId;
    qint64 m_positionMs = 0, m_durationMs = 0;
    QElapsedTimer m_since;
    QByteArray m_bands, m_peaks;
};
