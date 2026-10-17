// Theme watching follows Omacut (MIT, David Heinemeier Hansson).
#include "theme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace {
// Only for a machine without Omarchy: a neutral dark palette and the Omarchy fallback accent.
const QHash<QString, QString> &fallback()
{
    static const QHash<QString, QString> f{
        {"background", "#121212"}, {"dark_background", "#0d0d0d"}, {"darker_background", "#090909"},
        {"lighter_background", "#1e1e1e"}, {"foreground", "#bebebe"}, {"bright_foreground", "#e6e6e6"},
        {"dark_foreground", "#555555"}, {"light_foreground", "#8a8a8d"}, {"accent", "#FFD60A"},
        {"selection", "#2a2a2a"}, {"muted", "#333333"}, {"red", "#D35F5F"}};
    return f;
}
} // namespace

Theme::Theme(const QString &directory, QObject *parent)
    : QObject(parent),
      m_directory(directory.isEmpty() ? QDir::homePath() + "/.local/state/omarchy/current" : directory)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Theme::reload);
    reload();
}

QHash<QString, QString> Theme::parse(const QByteArray &toml)
{
    QHash<QString, QString> out;
    static const QRegularExpression line(R"re(^\s*([a-z_0-9]+)\s*=\s*["']([^"']*)["'])re",
                                         QRegularExpression::MultilineOption);
    static const QRegularExpression hex("^#[0-9a-fA-F]{6}$");
    auto it = line.globalMatch(QString::fromUtf8(toml));
    while (it.hasNext()) {
        const auto m = it.next();
        const QString key = m.captured(1), value = m.captured(2);
        if (key == "mode" || hex.match(value).hasMatch())
            out.insert(key, value);
    }
    return out;
}

QColor Theme::token(const QString &name) const
{
    const auto v = m_tokens.value(name);
    return QColor(v.isEmpty() ? fallback().value(name, "#ff00ff") : v);
}

void Theme::reload()
{
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);

    // A theme switch replaces symlinks and files, so watch the parents too and re-arm after every change.
    // Watching the nearest existing ancestor means installing Omarchy later works without a restart.
    QString ancestor = m_directory;
    while (!QFileInfo::exists(ancestor) && ancestor != "/")
        ancestor = QFileInfo(ancestor).absolutePath();
    QStringList candidates{ancestor, QFileInfo(ancestor).absolutePath(), m_directory, m_directory + "/theme",
                           m_directory + "/theme/colors.toml"};
    candidates.removeDuplicates();
    for (const auto &path : candidates)
        if (QFileInfo::exists(path))
            m_watcher.addPath(path);

    QFile file(m_directory + "/theme/colors.toml");
    QHash<QString, QString> tokens;
    if (file.open(QIODevice::ReadOnly))
        tokens = parse(file.readAll());
    const bool dark = tokens.value("mode", "dark") != "light";
    if (tokens != m_tokens || dark != m_dark) {
        m_tokens = tokens;
        m_dark = dark;
        emit changed();
    }
}
