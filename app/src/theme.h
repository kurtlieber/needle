#pragma once

#include <QColor>
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QTimer>

// The Omarchy theme, every token of colors.toml, followed live (a theme switch replaces files and symlinks,
// so the watcher re-arms after every change). needle draws everything, data included, from these tokens.
class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool dark READ dark NOTIFY changed)
    Q_PROPERTY(QColor background READ background NOTIFY changed)
    Q_PROPERTY(QColor darkBackground READ darkBackground NOTIFY changed)
    Q_PROPERTY(QColor darkerBackground READ darkerBackground NOTIFY changed)
    Q_PROPERTY(QColor lighterBackground READ lighterBackground NOTIFY changed)
    Q_PROPERTY(QColor foreground READ foreground NOTIFY changed)
    Q_PROPERTY(QColor brightForeground READ brightForeground NOTIFY changed)
    Q_PROPERTY(QColor darkForeground READ darkForeground NOTIFY changed)
    Q_PROPERTY(QColor lightForeground READ lightForeground NOTIFY changed)
    Q_PROPERTY(QColor accent READ accent NOTIFY changed)
    Q_PROPERTY(QColor selection READ selection NOTIFY changed)
    Q_PROPERTY(QColor muted READ muted NOTIFY changed)
    Q_PROPERTY(QColor red READ red NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr);

    bool dark() const { return m_dark; }
    QColor background() const { return token("background"); }
    QColor darkBackground() const { return token("dark_background"); }
    QColor darkerBackground() const { return token("darker_background"); }
    QColor lighterBackground() const { return token("lighter_background"); }
    QColor foreground() const { return token("foreground"); }
    QColor brightForeground() const { return token("bright_foreground"); }
    QColor darkForeground() const { return token("dark_foreground"); }
    QColor lightForeground() const { return token("light_foreground"); }
    QColor accent() const { return token("accent"); }
    QColor selection() const { return token("selection"); }
    QColor muted() const { return token("muted"); }
    QColor red() const { return token("red"); }

    QColor token(const QString &name) const;

    // colors.toml -> token -> "#rrggbb" (and "mode"). Unknown or malformed lines are ignored.
    static QHash<QString, QString> parse(const QByteArray &toml);

signals:
    void changed();

private:
    void reload();

    QString m_directory;
    QHash<QString, QString> m_tokens;
    bool m_dark = true;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};
