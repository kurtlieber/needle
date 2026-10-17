#include "config.h"

#include <QDir>
#include <QFile>
#include <QPair>
#include <QSet>
#include <QUrl>
#include <QVariant>

namespace {

using Settings = QList<QPair<QString, QVariant>>;       // "table.key" -> QString or QStringList, in file order

// Reads the subset of TOML the config uses; read() is false, with error() set, when it doesn't parse.
class Toml {
public:
    explicit Toml(const QString &text) : m_s(text) {}

    bool read(Settings &out)
    {
        QString table;
        QSet<QString> seen;
        for (;;) {
            skip(true);
            if (atEnd())
                return true;
            if (eat('[')) {
                skip(false);
                table = key();
                skip(false);
                if (table.isEmpty() || !eat(']'))
                    return fail("a table header is [name]");
            } else {
                const QString k = key();
                if (k.isEmpty())
                    return fail("expected key = value");
                skip(false);
                if (!eat('='))
                    return fail("expected = after " + k);
                skip(false);
                QVariant v;
                if (!value(v))
                    return false;
                const QString full = table.isEmpty() ? k : table + '.' + k;
                if (seen.contains(full))
                    return fail(full + " is set twice");
                seen.insert(full);
                out.append({full, v});
            }
            skip(false);
            if (!atEnd() && !eat('\n'))
                return fail("expected the end of the line");
        }
    }

    QString error() const { return m_error; }

private:
    bool atEnd() const { return m_i >= m_s.size(); }
    QChar peek() const { return atEnd() ? QChar() : m_s.at(m_i); }

    bool eat(QChar c)
    {
        if (peek() != c)
            return false;
        ++m_i;
        if (c == '\n')
            ++m_line;
        return true;
    }

    // spaces and comments, and line ends too when newlines is set (inside an array)
    void skip(bool newlines)
    {
        while (!atEnd()) {
            const QChar c = peek();
            if (c == ' ' || c == '\t' || c == '\r') {
                ++m_i;
            } else if (c == '#') {
                while (!atEnd() && peek() != '\n')
                    ++m_i;
            } else if (c == '\n' && newlines) {
                eat('\n');
            } else {
                return;
            }
        }
    }

    QString key()
    {
        const int start = m_i;
        const auto bare = [](QChar c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
        };
        while (!atEnd() && bare(peek()))
            ++m_i;
        return m_s.mid(start, m_i - start);
    }

    bool value(QVariant &v)
    {
        if (peek() != '[') {
            QString s;
            if (!string(s))
                return false;
            v = s;
            return true;
        }
        eat('[');
        QStringList list;
        for (;;) {
            skip(true);
            if (eat(']'))
                break;
            QString s;
            if (!string(s))
                return false;
            list << s;
            skip(true);
            if (eat(','))
                continue;
            if (eat(']'))
                break;
            return fail("expected , or ] in the list");
        }
        v = list;
        return true;
    }

    bool string(QString &out)
    {
        const QChar quote = peek();
        if (quote != '"' && quote != '\'')
            return fail("only strings and lists of strings are supported");
        ++m_i;
        for (;;) {
            if (atEnd() || peek() == '\n')
                return fail("the string has no closing quote");
            const QChar c = m_s.at(m_i++);
            if (c == quote)
                return true;
            if (c != '\\' || quote == '\'') {       // 'literal' strings have no escapes
                out += c;
                continue;
            }
            if (!escape(out))
                return false;
        }
    }

    bool escape(QString &out)
    {
        const QChar e = atEnd() ? QChar() : m_s.at(m_i++);
        switch (e.unicode()) {
        case '"': out += '"'; return true;
        case '\\': out += '\\'; return true;
        case 'n': out += '\n'; return true;
        case 't': out += '\t'; return true;
        case 'r': out += '\r'; return true;
        case 'b': out += '\b'; return true;
        case 'f': out += '\f'; return true;
        }
        const int digits = e == 'u' ? 4 : e == 'U' ? 8 : 0;
        bool ok = false;
        const uint code = digits ? m_s.mid(m_i, digits).toUInt(&ok, 16) : 0;
        if (!ok || m_s.mid(m_i, digits).size() != digits || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF))
            return fail("unknown escape in a string");
        m_i += digits;
        const char32_t c = code;
        out += QString::fromUcs4(&c, 1);
        return true;
    }

    bool fail(const QString &why)
    {
        m_error = QStringLiteral("line %1: %2").arg(m_line).arg(why);
        return false;
    }

    QString m_s;
    int m_i = 0;
    int m_line = 1;
    QString m_error;
};

// ~ is the home folder; a relative path is relative to base
QString expand(const QString &path, const QString &base)
{
    if (path == "~" || path.startsWith("~/"))
        return QDir::cleanPath(QDir::homePath() + path.mid(1));
    return QDir::cleanPath(QDir(base).absoluteFilePath(path));
}

// http(s)://host[:port][/path], without the trailing slash
QStringList serverList(const QStringList &raw, QStringList &problems)
{
    QStringList out;
    for (QString s : raw) {
        s = s.trimmed();
        while (s.endsWith('/'))
            s.chop(1);
        const QUrl u(s, QUrl::StrictMode);
        if (!u.isValid() || (u.scheme() != "http" && u.scheme() != "https") || u.host().isEmpty() || u.hasQuery()
            || u.hasFragment() || !u.userInfo().isEmpty()) {
            problems << "not a server address (http://host:port): " + s;
            continue;
        }
        out << s;
    }
    return out;
}

}  // namespace

Config Config::parse(const QString &toml, const QString &configDir)
{
    Config c;
    c.tokenFile = QDir::cleanPath(configDir + "/token");
    Settings settings;
    Toml reader(toml);
    if (!reader.read(settings)) {
        c.problems << reader.error() + " (the whole file is ignored)";
        return c;
    }
    for (const auto &[key, v] : settings) {
        if (key == "haystack.servers") {
            const QStringList raw = v.typeId() == QMetaType::QString ? QStringList{v.toString()} : v.toStringList();
            const QStringList s = serverList(raw, c.problems);
            if (s.isEmpty())
                c.problems << "haystack.servers has no server address in it";
            else
                c.servers = s;
        } else if (key == "haystack.token_file") {
            if (v.typeId() != QMetaType::QString || v.toString().trimmed().isEmpty())
                c.problems << "haystack.token_file is one path";
            else
                c.tokenFile = expand(v.toString().trimmed(), configDir);
        } else {
            c.problems << "unknown setting " + key;
        }
    }
    return c;
}

Config Config::load(const QString &path, const QString &configDir, const QProcessEnvironment &env)
{
    Config c;
    c.tokenFile = QDir::cleanPath(configDir + "/token");
    QFile f(path);
    if (f.open(QIODevice::ReadOnly))
        c = parse(QString::fromUtf8(f.readAll()), configDir);
    else if (f.exists())
        c.problems << f.errorString();
    for (auto &p : c.problems)
        p = path + ": " + p;

    const QString servers = env.value("NEEDLE_SERVER");
    if (!servers.trimmed().isEmpty()) {
        QStringList problems;
        const QStringList s = serverList(servers.split(',', Qt::SkipEmptyParts), problems);
        for (const auto &p : problems)
            c.problems << "NEEDLE_SERVER: " + p;
        if (!s.isEmpty())
            c.servers = s;
    }
    const QString token = env.value("NEEDLE_TOKEN_FILE").trimmed();
    if (!token.isEmpty())
        c.tokenFile = expand(token, QDir::currentPath());
    return c;
}
