#include "insight.h"

#include <QLocale>
#include <algorithm>
#include <cmath>

const QList<TraitInfo> &Insight::traits()
{
    // The order is the order of the summary sentence ("Slower, darker and more instrumental").
    static const QList<TraitInfo> t{
        {"tempo", "Tempo", "BPM", "slow", "fast", "slower", "faster", 60, 180, 30, 5, 0},
        {"valence", "Valence", "", "sad", "happy", "darker", "happier", 0, 1, .25, .05, 2},
        {"energy", "Energy", "", "calm", "intense", "calmer", "more intense", 0, 1, .25, .05, 2},
        {"danceability", "Danceability", "", "loose", "danceable", "looser", "more danceable", 0, 1, .25, .05, 2},
        {"acousticness", "Acousticness", "", "electronic", "acoustic", "more electronic", "more acoustic", 0, 1, .25, .05, 2},
        {"instrumentalness", "Instrumentalness", "", "vocal", "instrumental", "more vocal", "more instrumental", 0, 1, .25, .05, 2},
        {"loudness", "Loudness", "LUFS", "quiet", "loud", "quieter", "louder", -20, 0, 5, 1, 1},
    };
    return t;
}

const TraitInfo *Insight::trait(const QString &key)
{
    for (const auto &t : traits())
        if (t.key == key)
            return &t;
    return nullptr;
}

static bool number(const QVariant &v, double *out)
{
    if (!v.isValid() || v.isNull())
        return false;
    bool ok = false;
    *out = v.toDouble(&ok);
    return ok && std::isfinite(*out);
}

QVariantMap Insight::summary(const QVariantMap &readings, const QVariantMap &baseline)
{
    if (readings.isEmpty())
        return {{"lead", ""}, {"tail", ""}};
    QStringList parts;
    bool compared = false;
    for (const auto &t : traits()) {
        const auto b = baseline.value(t.key).toMap();
        double song, lo, hi, n;
        if (!number(readings.value(t.key), &song) || !number(b.value("lo"), &lo) || !number(b.value("hi"), &hi))
            continue;
        // A usual range from a handful of likes collapses to a point and calls every track different.
        if (!number(b.value("n"), &n) || n < kMinBaselineTracks)
            continue;
        compared = true;
        if (song < lo)
            parts << t.lower;
        else if (song > hi)
            parts << t.higher;
    }
    if (!compared)
        return {{"lead", ""}, {"tail", "Too few of your likes are measured yet to compare."}};
    if (parts.isEmpty())
        return {{"lead", ""}, {"tail", "Right in your usual range on every trait."}};
    QString list = parts.size() == 1 ? parts.first() : parts.mid(0, parts.size() - 1).join(", ") + " and " + parts.last();
    list[0] = list[0].toUpper();
    return {{"lead", list}, {"tail", " than what you usually like."}};
}

QString Insight::tempoComparison(const QVariant &song, const QVariant &yours)
{
    double s, y;
    if (!number(song, &s) || !number(yours, &y))
        return {};
    const int diff = int(std::lround(s)) - int(std::lround(y));
    if (std::abs(diff) <= 2)
        return QStringLiteral("About your usual tempo.");
    return QStringLiteral("This one is %1 BPM %2.").arg(std::abs(diff)).arg(diff < 0 ? "slower" : "faster");
}

QString Insight::format(const QString &key, const QVariant &value)
{
    double v;
    const auto *t = trait(key);
    if (!t || !number(value, &v))
        return QStringLiteral("–");
    QString s = QString::number(t->decimals ? v : std::round(v), 'f', t->decimals);
    if (s.startsWith('-'))
        s.replace(0, 1, QChar(0x2212));
    return s == QStringLiteral("−0.0") || s == QStringLiteral("−0.00") ? s.mid(1) : s;
}

double Insight::position(const QString &key, const QVariant &value)
{
    double v;
    const auto *t = trait(key);
    if (!t || !number(value, &v))
        return 0;
    return std::clamp((v - t->min) / (t->max - t->min), 0.0, 1.0);
}

QString Insight::albumLine(const QVariantMap &card)
{
    static const QHash<QString, QString> kinds{{"single", "Single"}, {"album", "Album"}, {"compilation", "Compilation"}};
    const QString kind = kinds.value(card.value("album_type").toString());
    const int year = card.value("year").toInt();
    QStringList parts;
    if (!kind.isEmpty())
        parts << kind;
    if (year > 0)
        parts << QString::number(year);
    return parts.join(", ");
}

QString Insight::why(const QVariantMap &why, const QString &strongColor, const QStringList &features)
{
    if (why.isEmpty())
        return {};
    QString out;
    const auto like = why.value("sounds_like").toMap();
    if (!like.isEmpty()) {
        out = QStringLiteral("Picked because it sounds like <b><font color=\"%1\">%2</font></b> by %3")
                  .arg(strongColor, like.value("title").toString().toHtmlEscaped(),
                       like.value("artist").toString().toHtmlEscaped());
        double sim;
        if (number(why.value("similarity"), &sim))
            out += QStringLiteral(" (similarity %1)").arg(sim, 0, 'f', 2);
        out += ". ";
    }
    // display text from haystack, shown as it comes (never parsed)
    const QString reason = why.value("reasons").toList().value(0).toString();
    if (features.contains("why.reasons") && !reason.isEmpty())
        out += QStringLiteral("Learned from your Verdicts: %1. ").arg(reason.toHtmlEscaped());
    const QString source = why.value("source_label").toString();
    if (!source.isEmpty()) {
        out += QStringLiteral("Found through %1").arg(source.toHtmlEscaped());
        out += why.value("exploration").toBool() ? QStringLiteral(", as one of this week’s exploration picks.")
                                                 : QStringLiteral(".");
    }
    return out.trimmed();
}

QString Insight::confidence(const QVariantMap &why, const QStringList &features)
{
    double p;
    if (!features.contains("rerank") || !number(why.value("confidence"), &p) || p < 0 || p > 1)
        return {};
    return QStringLiteral("Confidence %1%").arg(qRound(p * 100));
}

static QString names(const QStringList &list, const QString &strongColor)
{
    QStringList b;
    for (const auto &n : list)
        b << QStringLiteral("<b><font color=\"%1\">%2</font></b>").arg(strongColor, n.toHtmlEscaped());
    if (b.size() > 3) {
        const int more = b.size() - 2;
        b = b.mid(0, 2);
        b << QStringLiteral("%1 more").arg(more);
    }
    if (b.size() == 1)
        return b.first();
    const QString last = b.takeLast();
    return b.join(", ") + " and " + last;
}

QString Insight::playlistLine(const QVariantMap &card, const QString &strongColor)
{
    const QString from = card.value("fromPlaylist").toString();
    if (from.isEmpty())
        return {};
    const int lists = card.value("reviewPlaylists").toInt();
    QString out = QStringLiteral("From %1").arg(names({from}, strongColor));
    out += lists > 1 ? QStringLiteral(", one of the %1 playlists in this review.").arg(lists) : QStringLiteral(".");
    const QStringList also = card.value("alsoOn").toStringList();
    if (!also.isEmpty())
        out += QStringLiteral(" It’s also on %1.").arg(names(also, strongColor));
    if (card.value("liked").toBool())
        out += QStringLiteral(" It’s in your Liked Songs.");
    else if (card.value("in_library").toBool())
        out += QStringLiteral(" It’s in your music library already.");
    return out;
}

QString Insight::time(qint64 ms)
{
    if (ms < 0)
        ms = 0;
    const qint64 s = (ms + 500) / 1000;
    return QStringLiteral("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0'));
}

static QVariantMap toMap(const TraitInfo &t)
{
    return {{"key", t.key}, {"name", t.name}, {"unit", t.unit}, {"lowPole", t.lowPole}, {"highPole", t.highPole},
            {"min", t.min}, {"max", t.max}, {"major", t.major}, {"minor", t.minor}};
}

QVariantList Insight::traitList() const
{
    QVariantList out;
    for (const auto &t : traits())
        out << toMap(t);
    return out;
}

QVariantList Insight::scaleTraitList() const
{
    QVariantList out;
    for (const auto &t : traits())
        if (t.key != "tempo")       // tempo is the big BPM readout, not a scale
            out << toMap(t);
    return out;
}
