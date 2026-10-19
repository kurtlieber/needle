#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// What the screen says about a track's Readings next to the listener's Baseline: the Trait table (names, poles, scale
// ranges), the summary sentence, the tempo comparison and number formatting. Pure, so it is tested without QML.
struct TraitInfo {
    QString key, name, unit, lowPole, highPole, lower, higher;
    double min, max, major, minor;
    int decimals;
};

class Insight : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList traits READ traitList CONSTANT)
    Q_PROPERTY(QVariantList scaleTraits READ scaleTraitList CONSTANT)

public:
    using QObject::QObject;

    static const QList<TraitInfo> &traits();
    static const TraitInfo *trait(const QString &key);

    // A Trait joins the summary only when its Baseline rests on at least this many measured likes.
    static constexpr int kMinBaselineTracks = 20;

    // {lead: "Slower, darker and more instrumental", tail: " than what you usually like."}; lead is empty when
    // every Trait is in its usual range or no Trait has a full enough Baseline, and both are empty with no
    // Readings.
    Q_INVOKABLE static QVariantMap summary(const QVariantMap &readings, const QVariantMap &baseline);
    // "This one is 24 BPM slower." / "About your usual tempo." / "" without a Reading or Baseline.
    Q_INVOKABLE static QString tempoComparison(const QVariant &song, const QVariant &yours);
    // 0.14, −7.1 (a real minus sign), 102
    Q_INVOKABLE static QString format(const QString &key, const QVariant &value);
    // 0..1 position of a value on the Trait's scale, clamped.
    Q_INVOKABLE static double position(const QString &key, const QVariant &value);
    // "Single, 2018" / "Album, 2011" / "2018"
    Q_INVOKABLE static QString albumLine(const QVariantMap &card);
    // "Picked because it sounds like <b>Cradles</b> by Sub Urban (similarity 0.96). Found through Deezer artist
    // radio, as one of this week's exploration picks." (rich text, escaped); "" without a reason. With the feature
    // why.reasons and a reason on the card, the re-ranker's first one follows the first sentence: "Learned from your
    // Verdicts: close to your 4-5★ tracks."
    Q_INVOKABLE static QString why(const QVariantMap &why, const QString &strongColor,
                                   const QStringList &features = {});
    // "Confidence 81%" for a Discoveries pick the re-ranker ordered (feature rerank); "" otherwise.
    Q_INVOKABLE static QString confidence(const QVariantMap &why, const QStringList &features);
    // A Playlist review's song: "From <b>All the Dubstep</b>, one of the 3 playlists in this review. It's also on
    // <b>EDM</b>. It's in your Liked Songs." (rich text, escaped); "" for a card from another queue.
    Q_INVOKABLE static QString playlistLine(const QVariantMap &card, const QString &strongColor);
    Q_INVOKABLE static QString time(qint64 ms);

    QVariantList traitList() const;
    QVariantList scaleTraitList() const;
};
