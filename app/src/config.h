#pragma once

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

// Where haystack is: ~/.config/needle/config.toml, written by hand (needle never writes it; Prefs keeps what needle
// changes itself), then NEEDLE_SERVER (comma-separated) and NEEDLE_TOKEN_FILE over it. Read once at start.
//
//   [haystack]
//   servers = ["http://192.168.1.10:8517", "http://10.20.0.10:8517"]   # tried in order when one doesn't answer
//   token_file = "~/.config/needle/token"                               # the API's bearer token, one line (0600)
//
// A small subset of TOML: [tables], key = "string" or 'string' or an array of them (over several lines too), and
// # comments. A file that doesn't parse is ignored as a whole, with a warning; unknown keys are only warned about.
struct Config {
    QStringList servers{"http://127.0.0.1:8517"};
    QString tokenFile;          // absolute
    QStringList problems;       // what was wrong with the file or the environment, for the log

    // configDir: needle's own folder (~/.config/needle), for the default token file and relative paths
    static Config load(const QString &path, const QString &configDir,
                       const QProcessEnvironment &env = QProcessEnvironment::systemEnvironment());
    static Config parse(const QString &toml, const QString &configDir);
};
