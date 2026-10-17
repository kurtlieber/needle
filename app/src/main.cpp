// needle: a Spotify review app. Plays the Review queues (Discoveries, Rate your likes, and the playlists the
// listener picks) through its own librespot engine, shows each track's Readings against their Baseline, and sends
// their Verdicts to haystack.

#include <QCommandLineParser>
#include <QDir>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequence>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QUrl>

#include "config.h"
#include "engineclient.h"
#include "haystackclient.h"
#include "insight.h"
#include "playlists.h"
#include "prefs.h"
#include "review.h"
#include "spectrumitem.h"
#include "theme.h"

static QString dir(QStandardPaths::StandardLocation loc, const QString &fallback)
{
    QString d = QStandardPaths::writableLocation(loc);
    return d.isEmpty() ? QDir::homePath() + fallback : d;
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("needle");
    app.setOrganizationName("needle");
    app.setApplicationVersion(QStringLiteral(NEEDLE_VERSION));

    // Matches the window to needle.desktop, so the compositor (Wayland app_id) and the launcher pick up the icon.
    app.setDesktopFileName("needle");
    app.setWindowIcon(QIcon::fromTheme("needle"));
    QQuickStyle::setStyle("Material");

    // Archivo (OFL), bundled: the wordmark and BPM use its width axis
    if (QFontDatabase::addApplicationFont(":/fonts/Archivo[wdth,wght].ttf") < 0)
        qWarning("could not load the Archivo font");

    const QString home = QDir::homePath();
    const QString state = qEnvironmentVariable("XDG_STATE_HOME", home + "/.local/state") + "/needle";
    const QString cache = dir(QStandardPaths::GenericCacheLocation, "/.cache") + "/needle";
    const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR", QDir::tempPath());
    const QString configDir = dir(QStandardPaths::GenericConfigLocation, "/.config") + "/needle";
    const Config config = Config::load(configDir + "/config.toml", configDir);
    for (const auto &p : config.problems)
        qWarning("%s", qPrintable(p));

    Theme theme;
    Insight insight;
    Prefs prefs;
    HaystackClient haystack(config.servers, config.tokenFile, cache, state);
    EngineClient engine(EngineClient::findEngine(), runtime + "/needle/engine.sock");
    Review review(&haystack, &engine);
    Playlists playlists(&haystack, &engine, state);
    QObject::connect(&playlists, &Playlists::sessionReady, &review, &Review::startPlaylistReview);
    playlists.restore();

    qmlRegisterType<SpectrumItem>("Needle", 1, 0, "Spectrum");
    QQmlApplicationEngine qml;
    auto *ctx = qml.rootContext();
    ctx->setContextProperty("theme", &theme);
    ctx->setContextProperty("insight", &insight);
    ctx->setContextProperty("prefs", &prefs);
    ctx->setContextProperty("haystack", &haystack);
    ctx->setContextProperty("engine", &engine);
    ctx->setContextProperty("review", &review);
    ctx->setContextProperty("playlists", &playlists);
    qml.load(QUrl("qrc:/Main.qml"));
    if (qml.rootObjects().isEmpty())
        return 1;

    engine.start();
    haystack.refresh();

    // Development: render one frame to a file and quit (screenshots against the mockups, without a window
    // on screen when run with QT_QPA_PLATFORM=offscreen).
    QCommandLineParser args;
    args.addOptions({{"grab", "Save a screenshot to <file> after <delay> ms, then quit.", "file"},
                     {"delay", "Milliseconds to wait before --grab.", "ms", "6000"},
                     {"size", "Window size, WxH.", "size"},
                     {"press", "Press these keys (comma-separated, e.g. 4,P,Shift+5) 3 s after start.", "keys"}});
    args.process(app);
    auto *win = qobject_cast<QQuickWindow *>(qml.rootObjects().first());
    if (args.isSet("size")) {
        const auto wh = args.value("size").split('x');
        win->resize(wh.value(0).toInt(), wh.value(1).toInt());
    }
    if (args.isSet("press")) {
        const QStringList keys = args.value("press").split(',', Qt::SkipEmptyParts);
        for (int i = 0; i < keys.size(); ++i)
            QTimer::singleShot(3000 + 700 * i, win, [win, k = keys[i]] {
                const QKeyCombination combo = QKeySequence::fromString(k)[0];
                QKeyEvent press(QEvent::KeyPress, combo.key(), combo.keyboardModifiers(), QKeySequence(combo.key()).toString());
                // straight to the item with the key handler: a window that isn't active has no focus item
                QObject *target = win->findChild<QQuickItem *>("keys");
                QCoreApplication::sendEvent(target ? target : static_cast<QObject *>(win), &press);
                qInfo("pressed %s", qPrintable(k));
            });
    }
    if (args.isSet("grab")) {
        const QString file = args.value("grab");
        QTimer::singleShot(args.value("delay").toInt(), &app, [win, file] {
            const bool ok = win->grabWindow().save(file);
            qInfo("grab %s: %s", qPrintable(file), ok ? "saved" : "failed");
            QCoreApplication::exit(ok ? 0 : 1);
        });
    }
    return app.exec();
}
