#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "config.h"
#include "engineclient.h"
#include "haystackclient.h"
#include "insight.h"
#include "outbox.h"
#include "playlistlibrary.h"
#include "playlists.h"
#include "review.h"
#include "reviewqueue.h"
#include "theme.h"

static QVariantMap card(const QString &id, const QVariantMap &verdict = {})
{
    QVariantMap c{{"id", id}, {"title", "Song " + id}};
    if (!verdict.isEmpty())
        c.insert("verdict", verdict);
    return c;
}

static QVariantMap rating(int stars) { return {{"kind", "rating"}, {"stars", stars}}; }

static QString pid(const char *name) { return QString::fromLatin1(name).leftJustified(22, '0'); }

// /v1/state's playlists: Bass (a b c), Dub (c d), Phonk (followed, its songs not read yet); a has 4 stars, c a Pass
static QVariantMap playlistState()
{
    const auto list = [](const char *id, const char *name, const QVariant &tracks, bool current = true) {
        return QVariantMap{{"id", pid(id)}, {"name", name}, {"cover", ""}, {"mine", current}, {"tracks", tracks},
                           {"current", current}, {"reader", current ? "server" : "engine"}, {"snapshot_id", "s1"},
                           {"artists", QStringList{"Skrillex"}}};
    };
    return {{"items", QVariantList{list("bass", "Wide Mouth Bass", QStringList{"a", "b", "c"}),
                                   list("dub", "All the Dubstep", QStringList{"c", "d"}),
                                   list("phonk", "OG Drift Phonk", QVariant(), false)}},
            {"verdicts", QVariantMap{{"a", QVariantMap{{"kind", "rating"}, {"stars", 4}, {"given_at", "2026-10-01T00:00:00Z"}}},
                                     {"c", QVariantMap{{"kind", "pass"}, {"given_at", "2026-10-01T00:00:00Z"}}}}}};
}

// a playlist's cards as haystack sends them; c is on both playlists
static QVariantMap playlistCards(const char *id, const QStringList &tracks)
{
    QVariantList items;
    for (const auto &t : tracks) {
        QVariantMap c = card(t);
        c.insert("playlists", t == "c" ? QStringList{pid("bass"), pid("dub")} : QStringList{pid(id)});
        items << c;
    }
    return {{"id", pid(id)}, {"items", items}};
}

class NeedleTests : public QObject {
    Q_OBJECT

private slots:
    // ---- the theme ----
    void themeReadsEveryToken()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        QFile f(dir.path() + "/theme/colors.toml");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("mode = \"light\"\naccent = \"#112233\"\nlight_foreground = '#8a8a8d'\nred = \"#D35F5F\"\ncolor1 = \"nonsense\"\n");
        f.close();
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QColor("#112233"));
        QCOMPARE(theme.lightForeground(), QColor("#8a8a8d"));
        QCOMPARE(theme.red(), QColor("#D35F5F"));
        QVERIFY(!theme.dark());
    }

    void themeFallsBackWithoutOmarchy()
    {
        QTemporaryDir dir;
        Theme theme(dir.path() + "/missing");
        QCOMPARE(theme.accent(), QColor("#FFD60A"));
        QVERIFY(theme.dark());
        QVERIFY(theme.background().isValid());
    }

    void themeFollowsAThemeSwitch()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        auto write = [&](const char *accent) {
            QFile f(dir.path() + "/theme/colors.toml");
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write(QByteArray("accent = \"") + accent + "\"\n");
        };
        write("#112233");
        Theme theme(dir.path());
        QSignalSpy spy(&theme, &Theme::changed);
        write("#445566");
        QTRY_COMPARE(theme.accent(), QColor("#445566"));
        QVERIFY(spy.count() >= 1);
    }

    // ---- what the screen says ----
    void summaryNamesTheTraitsOutsideTheUsualRange()
    {
        const QVariantMap base{{"tempo", QVariantMap{{"lo", 118}, {"hi", 140}, {"n", 380}}},
                               {"valence", QVariantMap{{"lo", 0.27}, {"hi", 0.55}, {"n", 380}}},
                               {"energy", QVariantMap{{"lo", 0.70}, {"hi", 0.88}, {"n", 380}}},
                               {"instrumentalness", QVariantMap{{"lo", 0.10}, {"hi", 0.61}, {"n", 380}}}};
        const QVariantMap song{{"tempo", 102}, {"valence", 0.14}, {"energy", 0.83}, {"instrumentalness", 0.77}};
        const auto s = Insight::summary(song, base);
        QCOMPARE(s.value("lead").toString(), QString("Slower, darker and more instrumental"));
        QCOMPARE(s.value("tail").toString(), QString(" than what you usually like."));
        const auto one = Insight::summary({{"energy", 0.95}}, base);
        QCOMPARE(one.value("lead").toString(), QString("More intense"));
        QCOMPARE(Insight::summary({{"energy", 0.8}}, base).value("tail").toString(),
                 QString("Right in your usual range on every trait."));
        QVERIFY(Insight::summary({}, base).value("lead").toString().isEmpty());
    }

    void summaryIgnoresATraitWithAThinBaseline()
    {
        // haystack's dev copy before the backfill: 5 measured likes, so every usual range is nearly a point.
        const QVariantMap thin{{"energy", QVariantMap{{"lo", 0.52}, {"hi", 0.53}, {"n", 5}}},
                               {"valence", QVariantMap{{"lo", 0.27}, {"hi", 0.55}, {"n", 380}}}};
        const auto s = Insight::summary({{"energy", 0.10}, {"valence", 0.14}}, thin);
        QCOMPARE(s.value("lead").toString(), QString("Darker"));
        const auto none = Insight::summary({{"energy", 0.10}}, thin);
        QVERIFY(none.value("lead").toString().isEmpty());
        QCOMPARE(none.value("tail").toString(), QString("Too few of your likes are measured yet to compare."));
        QVERIFY(Insight::summary({{"energy", 0.10}}, {{"energy", QVariantMap{{"lo", 0.5}, {"hi", 0.6}}}})
                    .value("lead").toString().isEmpty());   // no n at all: not trusted either
    }

    void numbersReadLikeTheMockup()
    {
        QCOMPARE(Insight::format("valence", 0.14), QString("0.14"));
        QCOMPARE(Insight::format("loudness", -7.1), QString::fromUtf8("−7.1"));
        QCOMPARE(Insight::format("tempo", 101.6), QString("102"));
        QCOMPARE(Insight::format("tempo", QVariant()), QString::fromUtf8("–"));
        QCOMPARE(Insight::tempoComparison(102, 126), QString("This one is 24 BPM slower."));
        QCOMPARE(Insight::tempoComparison(127, 126), QString("About your usual tempo."));
        QCOMPARE(Insight::time(179000), QString("2:59"));
        QCOMPARE(Insight::albumLine({{"album_type", "single"}, {"year", 2018}}), QString("Single, 2018"));
        QCOMPARE(Insight::position("loudness", -25), 0.0);
        QCOMPARE(Insight::position("loudness", -5), 0.75);
    }

    void whyLineEscapesNamesAndMentionsExploration()
    {
        const QVariantMap why{{"sounds_like", QVariantMap{{"title", "Cradles"}, {"artist", "Sub <Urban>"}}},
                              {"similarity", 0.962}, {"source_label", "Deezer artist radio"}, {"exploration", true}};
        const QString s = Insight::why(why, "#bebebe");
        QVERIFY(s.contains("<b><font color=\"#bebebe\">Cradles</font></b> by Sub &lt;Urban&gt; (similarity 0.96)"));
        QVERIFY(s.endsWith(QString::fromUtf8("as one of this week’s exploration picks.")));
        QVERIFY(Insight::why({}, "#fff").isEmpty());
    }

    void whyLineShowsTheFirstReasonOnlyWhenTheServerListsIt()
    {
        const QVariantMap why{{"sounds_like", QVariantMap{{"title", "Cradles"}, {"artist", "Sub Urban"}}},
                              {"similarity", 0.9}, {"source_label", "Deezer artist radio"},
                              {"reasons", QVariantList{"brighter than <you> usually like", "in your usual styles"}}};
        const QString before = Insight::why(why, "#fff");
        QVERIFY(!before.contains("Learned"));         // a haystack without the feature: today's line
        QCOMPARE(Insight::why(why, "#fff", {"rerank"}), before);
        const QString s = Insight::why(why, "#fff", {"why.reasons"});
        QVERIFY(s.contains("(similarity 0.90). Learned from your Verdicts: brighter than &lt;you&gt; usually like. "
                           "Found through Deezer artist radio."));
        QVERIFY(!s.contains("usual styles"));         // the first reason only
        QVariantMap none = why;
        none["reasons"] = QVariantList{};
        QCOMPARE(Insight::why(none, "#fff", {"why.reasons"}), before);
        none.remove("reasons");
        QCOMPARE(Insight::why(none, "#fff", {"why.reasons"}), before);
    }

    void whyLineAsksAboutACalibrateCardOnlyWithTheFeature()
    {
        const QVariantMap why{{"sounds_like", QVariantMap{{"title", "Cradles"}, {"artist", "Sub Urban"}}},
                              {"similarity", 0.9}, {"source_label", "Deezer artist radio"}, {"exploration", false},
                              {"calibrate", QVariantMap{{"band", "90-97"}, {"percentile", 93.1}, {"chance", 0.52},
                                                        {"reason", "the re-ranker gives it <even> odds"}}}};
        const QString s = Insight::why(why, "#fff", {"queue.calibrate"});
        QVERIFY(s.startsWith(QString::fromUtf8("haystack isn’t sure about this one: the re-ranker gives it &lt;even&gt; "
                                               "odds. Your verdict teaches it the most. It sounds most like <b>")));
        QVERIFY(s.endsWith("(similarity 0.90). Found through Deezer artist radio."));
        // a haystack without the feature never sends the object; if one did, the line stays an offer's
        QVERIFY(Insight::why(why, "#fff").startsWith("Picked because it sounds like"));
        QVariantMap quiet = why;
        quiet["calibrate"] = QVariantMap{{"band", "50-75"}, {"percentile", 60.0}, {"chance", QVariant()}};
        QVERIFY(Insight::why(quiet, "#fff", {"queue.calibrate"})
                    .startsWith(QString::fromUtf8("haystack isn’t sure about this one. Your verdict teaches it the most.")));
        // nothing it sounds like: the other sentences follow on, one space apart
        QVariantMap lone = why;
        lone.remove("sounds_like");
        lone.remove("similarity");
        lone["reasons"] = QVariantList{"in your usual styles"};
        const QString ask = QString::fromUtf8("haystack isn’t sure about this one: the re-ranker gives it &lt;even&gt; "
                                              "odds. Your verdict teaches it the most.");
        QCOMPARE(Insight::why(lone, "#fff", {"queue.calibrate", "why.reasons"}),
                 ask + " Learned from your Verdicts: in your usual styles. Found through Deezer artist radio.");
        lone.remove("source_label");
        QCOMPARE(Insight::why(lone, "#fff", {"queue.calibrate", "why.reasons"}),
                 ask + " Learned from your Verdicts: in your usual styles.");
        QCOMPARE(Insight::why(lone, "#fff", {"queue.calibrate"}), ask);
    }

    void confidenceChipNeedsTheRerankFeatureAndAProbability()
    {
        const QVariantMap why{{"model", "rerank"}, {"confidence", 0.814}};
        QCOMPARE(Insight::confidence(why, {"why.band", "rerank"}), QString("Confidence 81%"));
        QCOMPARE(Insight::confidence(why, {"why.band"}), QString());
        QCOMPARE(Insight::confidence({{"model", "similarity"}, {"confidence", QVariant()}}, {"rerank"}), QString());
        QCOMPARE(Insight::confidence({}, {"rerank"}), QString());
        QCOMPARE(Insight::confidence({{"confidence", 1.2}}, {"rerank"}), QString());
        QCOMPARE(Insight::confidence({{"confidence", 0.0}}, {"rerank"}), QString("Confidence 0%"));
    }

    // ---- a Review queue ----
    void queueStartsAtTheFirstTrackWithoutAVerdict()
    {
        ReviewQueue q;
        q.setCards({card("a", rating(4)), card("b"), card("c")});
        QCOMPARE(q.currentId(), QString("b"));
        QCOMPARE(q.upNext().value("id").toString(), QString("c"));
        QCOMPARE(q.reviewed(), 1);
    }

    void nextSkipsReviewedTracksAndWraps()
    {
        ReviewQueue q;
        q.setCards({card("a"), card("b", rating(3)), card("c")});
        QVERIFY(q.next());
        QCOMPARE(q.currentId(), QString("c"));
        QVERIFY(q.next());                           // wraps to the unreviewed a
        QCOMPARE(q.currentId(), QString("a"));
    }

    void aVerdictStaysOnTheTrackAndTheQueueEnds()
    {
        ReviewQueue q;
        q.setCards({card("a"), card("b")});
        q.applyVerdict("a", rating(5));
        QCOMPARE(q.currentId(), QString("a"));       // a plain Rating keeps playing
        QVERIFY(q.next());
        q.applyVerdict("b", {{"kind", "pass"}});
        QVERIFY(!q.next());
        QVERIFY(!q.hasCurrent());                    // done: needle shows the next batch
        QVERIFY(q.back());
        QCOMPARE(q.currentId(), QString("b"));       // and Back can still change a verdict
    }

    void aFreshSnapshotKeepsThePlayingTrack()
    {
        ReviewQueue q;
        q.setCards({card("a"), card("b"), card("c")});
        q.next();
        q.setCards({card("x"), card("b"), card("c")});
        QCOMPARE(q.currentId(), QString("b"));
        // haystack leaves a rated like out of Rate your likes; it keeps playing until Next
        q.applyVerdict("b", rating(4));
        q.setCards({card("c"), card("d")});
        QCOMPARE(q.currentId(), QString("b"));
        QVERIFY(q.next());
        QCOMPARE(q.currentId(), QString("c"));
    }

    void aRatingKeepsTheTrackUpNext()
    {
        // a was skipped (Next, or Spotify can't play it); b gets a Rating and haystack leaves it out
        ReviewQueue likes;
        likes.setCards({card("a"), card("b"), card("c")});
        likes.next();
        likes.applyVerdict("b", rating(4));
        likes.setCards({card("a"), card("c")});
        QCOMPARE(likes.currentId(), QString("b"));
        QCOMPARE(likes.upNext().value("id").toString(), QString("c"));
        QVERIFY(likes.next());
        QCOMPARE(likes.currentId(), QString("c"));
        // Discoveries lists the tracks with a verdict last
        ReviewQueue disc;
        disc.setCards({card("a"), card("b"), card("c")});
        disc.next();
        disc.applyVerdict("b", rating(4));
        disc.setCards({card("a"), card("c"), card("b", rating(4))});
        QCOMPARE(disc.currentId(), QString("b"));
        QCOMPARE(disc.upNext().value("id").toString(), QString("c"));
        QVERIFY(disc.next());
        QCOMPARE(disc.currentId(), QString("c"));
    }

    void revisitingPlaysSongsWithAVerdictOnce()
    {
        ReviewQueue q;
        q.reset({card("a", rating(3)), card("b")}, true);
        QCOMPARE(q.currentId(), QString("a"));       // its old verdict doesn't count here
        QCOMPARE(q.reviewed(), 0);
        q.applyVerdict("a", rating(5));
        QVERIFY(q.currentReviewed());
        QVERIFY(q.next());
        QCOMPARE(q.currentId(), QString("b"));
        q.applyVerdict("b", {{"kind", "pass"}});
        QVERIFY(!q.next());
        QVERIFY(!q.hasCurrent());
        q.reset({card("a", rating(5)), card("b"), card("c")}, true, {"a", "b"});   // after a restart
        QCOMPARE(q.currentId(), QString("c"));
    }

    void aCardUpdateKeepsItsOwnFieldsAndALocalVerdict()
    {
        ReviewQueue q;
        QVariantMap a = card("a");
        a.insert("fromPlaylist", "EDM");
        a.insert("measure", "unknown");
        q.reset({a});
        q.applyVerdict("a", {{"kind", "rating"}, {"stars", 4}, {"given_at", "2026-10-16T08:00:00Z"}});
        QVERIFY(q.updateCard({{"id", "a"}, {"measure", "ok"}, {"readings", QVariantMap{{"tempo", 140}}}, {"verdict", QVariant()}}));
        QCOMPARE(q.current().value("measure").toString(), QString("ok"));
        QCOMPARE(q.current().value("fromPlaylist").toString(), QString("EDM"));
        QCOMPARE(q.current().value("verdict").toMap().value("stars").toInt(), 4);
        QVERIFY(!q.updateCard({{"id", "zzz"}}));
    }

    // ---- playlists ----
    void playlistProgressComesFromTheVerdicts()
    {
        PlaylistLibrary lib;
        lib.setState(playlistState());
        auto bass = lib.progress(pid("bass"));
        QCOMPARE(bass.songs, 3);
        QCOMPARE(bass.stars[3], 1);
        QCOMPARE(bass.passed, 1);
        QCOMPARE(bass.left(), 1);
        QCOMPARE(bass.average(), 4.0);
        QCOMPARE(lib.stage(pid("bass")), QString("progress"));
        QVERIFY(!lib.readable(pid("phonk")) && lib.needsEngine(pid("phonk")));
        QCOMPARE(lib.stage(pid("phonk")), QString("fresh"));
        const auto sel = lib.selection({pid("bass"), pid("dub"), pid("phonk")});
        QCOMPARE(sel.playlists, 2);                  // Phonk can't be picked yet
        QCOMPARE(sel.songs, 4);                      // c is on both and counts once
        QCOMPARE(sel.withVerdict, 2);
        QCOMPARE(sel.overlap, 1);
        QCOMPARE(lib.union_({pid("dub"), pid("bass")}), QStringList({"c", "d", "a", "b"}));
        // a verdict given here counts at once, and still after a snapshot that doesn't have it yet
        lib.applyVerdict("b", {{"kind", "rating"}, {"stars", 2}, {"given_at", "2026-10-16T08:00:00Z"}});
        QCOMPARE(lib.stage(pid("bass")), QString("done"));
        lib.setState(playlistState());
        QCOMPARE(lib.stage(pid("bass")), QString("done"));
    }

    void aPlaylistReviewKeepsItsOrder()
    {
        const QStringList songs{"a", "b", "c", "d", "e", "f", "g", "h"};
        QCOMPARE(PlaylistSession::order(songs, false, 7), songs);
        const auto one = PlaylistSession::order(songs, true, 7);
        QCOMPARE(PlaylistSession::order(songs, true, 7), one);
        QVERIFY(PlaylistSession::order(songs, true, 8) != one);
        QStringList less = one;
        less.removeAll("d");
        QCOMPARE(PlaylistSession::order(QStringList{"h", "g", "f", "e", "c", "b", "a"}, true, 7), less);

        PlaylistSession s;
        s.playlists = {pid("dub"), pid("bass")};
        s.tracks = {"c", "a", "x"};
        QHash<QString, QVariantMap> cards;
        QVariantMap c = card("c");
        c.insert("playlists", QStringList{pid("bass"), pid("dub"), pid("edm")});
        cards.insert("c", c);
        QVariantMap a = card("a");
        a.insert("playlists", QStringList{pid("bass")});
        cards.insert("a", a);
        const QHash<QString, QString> names{{pid("bass"), "Wide Mouth Bass"}, {pid("dub"), "All the Dubstep"}, {pid("edm"), "EDM"}};
        const auto out = s.cards(cards, names);
        QCOMPARE(out.size(), 2);                     // x has no card
        QCOMPARE(out[0].toMap().value("fromPlaylist").toString(), QString("All the Dubstep"));   // the review's order
        QCOMPARE(out[0].toMap().value("alsoOn").toStringList(), QStringList({"Wide Mouth Bass", "EDM"}));
        QCOMPARE(out[1].toMap().value("fromPlaylist").toString(), QString("Wide Mouth Bass"));
        QCOMPARE(out[1].toMap().value("reviewPlaylists").toInt(), 2);
        QCOMPARE(PlaylistSession::fromJson(s.toJson()).tracks, s.tracks);
    }

    void thePickerStartsAReviewAndBringsItBack()
    {
        QTemporaryDir dir;
        const QString cache = dir.path() + "/cache", state = dir.path() + "/state";
        QFile tok(dir.path() + "/token");
        QVERIFY(tok.open(QIODevice::WriteOnly));
        tok.write("t");
        tok.close();
        // haystack is out of reach: the picked playlists' cards come from the copies fetched before
        QDir().mkpath(cache + "/playlists");
        const auto save = [&](const QVariantMap &pl) {
            QFile f(cache + "/playlists/" + pl.value("id").toString() + ".json");
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(QJsonDocument::fromVariant(pl).toJson());
        };
        save(playlistCards("bass", {"a", "b", "c"}));
        save(playlistCards("dub", {"c", "d"}));
        HaystackClient haystack({"http://127.0.0.1:1"}, dir.path() + "/token", cache, state);
        Review r(&haystack, nullptr);
        Playlists p(&haystack, nullptr, state);
        connect(&p, &Playlists::sessionReady, &r, &Review::startPlaylistReview);
        r.applyState({{"queues", QVariantMap{{"discoveries", QVariantMap{{"items", QVariantList{card("z")}}}}}}});
        p.applyState({{"playlists", playlistState()}});
        QCOMPARE(p.total(), 3);
        QCOMPARE(p.stageCounts().value("progress").toInt(), 2);
        QCOMPARE(p.selectionInfo().value("line1").toString(), QString("Nothing picked yet"));
        QVERIFY(p.selectionInfo().value("canStart").toBool());       // Enter would review the one in focus
        p.toggle(2);                                 // Phonk: not read yet
        QVERIFY(p.notice().contains("Sign in to Spotify"));
        QCOMPARE(p.selectionInfo().value("count").toInt(), 0);
        p.pickAll();
        QCOMPARE(p.selectionInfo().value("count").toInt(), 2);
        QCOMPARE(p.selectionInfo().value("line1").toString(), QString::fromUtf8("2 playlists · 2 songs to review"));
        QCOMPARE(p.selectionInfo().value("line2").toString(),
                 QString::fromUtf8("2 already have a verdict · 1 is on more than one and plays once"));
        p.setShuffle(false);
        p.setFilter("done");
        QCOMPARE(p.shown(), 0);
        p.setFilter("all");
        p.setNameFilter("dub");
        QCOMPARE(p.shown(), 1);
        p.setNameFilter("");

        QSignalSpy ready(&p, &Playlists::sessionReady);
        p.start();
        QVERIFY(ready.wait(5000));
        QCOMPARE(ready.first().at(3).toBool(), true);
        QCOMPARE(r.queue(), QString("playlists"));
        QCOMPARE(r.current().value("id").toString(), QString("b"));    // a and c have a verdict: skipped
        QCOMPARE(r.current().value("fromPlaylist").toString(), QString("Wide Mouth Bass"));
        QCOMPARE(r.playlistsTotal(), 2);
        QCOMPARE(r.playlistsPosition(), 1);
        QVERIFY(p.hasSession() && !p.pickerOpen());
        r.rate(5, true);
        QCOMPARE(r.current().value("id").toString(), QString("d"));
        QCOMPARE(r.current().value("fromPlaylist").toString(), QString("All the Dubstep"));
        QCOMPARE(r.playlistsPosition(), 2);
        QCOMPARE(p.library().stage(pid("bass")), QString("done"));      // the picker counts it at once
        QCOMPARE(haystack.pendingVerdicts().last().toObject().value("queue").toString(), QString("playlists"));
        r.pass();
        QVERIFY(r.queueDone());

        // a restart: the review comes back, quietly, from what was saved
        Review r2(&haystack, nullptr);
        Playlists p2(&haystack, nullptr, state);
        connect(&p2, &Playlists::sessionReady, &r2, &Review::startPlaylistReview);
        QSignalSpy back(&p2, &Playlists::sessionReady);
        p2.restore();
        QCOMPARE(back.size(), 1);
        QCOMPARE(back.first().at(3).toBool(), false);
        QCOMPARE(r2.playlistsTotal(), 2);
        QCOMPARE(r2.playlistsLeft(), 0);             // the saved cards predate both verdicts; they count anyway
        QVERIFY(r2.queue() != "playlists");
        QVERIFY(!p2.shuffle());
    }

    void aSongOnTwoReleasesIsReviewedOnce()
    {
        QTemporaryDir dir;
        const QString cache = dir.path() + "/cache", state = dir.path() + "/state";
        QFile tok(dir.path() + "/token");
        QVERIFY(tok.open(QIODevice::WriteOnly));
        tok.write("t");
        tok.close();
        QDir().mkpath(cache + "/playlists");
        const auto save = [&](const QVariantMap &pl) {
            QFile f(cache + "/playlists/" + pl.value("id").toString() + ".json");
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(QJsonDocument::fromVariant(pl).toJson());
        };
        // b and b2 are one Song on two releases (the album and a best-of), d and d2 another; a has no ISRC
        const auto release = [](const char *id, const char *song, const char *playlist) {
            QVariantMap c = card(id);
            if (*song)
                c.insert("song", song);
            c.insert("playlists", QStringList{pid(playlist)});
            return c;
        };
        save({{"id", pid("bass")}, {"items", QVariantList{release("a", "", "bass"), release("b", "USAAA1900001", "bass")}}});
        save({{"id", pid("dub")}, {"items", QVariantList{release("b2", "USAAA1900001", "dub"),
                                                         release("d", "USAAA1900002", "dub"),
                                                         release("d2", "USAAA1900002", "dub")}}});
        const auto list = [](const char *id, const QStringList &tracks) {
            return QVariantMap{{"id", pid(id)}, {"name", id}, {"cover", ""}, {"mine", true}, {"tracks", tracks},
                               {"current", true}, {"reader", "server"}, {"snapshot_id", "s1"}};
        };
        // the one verdict is a Pass on d2, given to that release only
        const QVariantMap playlists{{"items", QVariantList{list("bass", {"a", "b"}), list("dub", {"b2", "d", "d2"})}},
                                    {"verdicts", QVariantMap{{"d2", QVariantMap{{"kind", "pass"}, {"given_at", "2026-10-01T00:00:00Z"}}}}}};
        HaystackClient haystack({"http://127.0.0.1:1"}, dir.path() + "/token", cache, state);
        Review r(&haystack, nullptr);
        Playlists p(&haystack, nullptr, state);
        connect(&p, &Playlists::sessionReady, &r, &Review::startPlaylistReview);
        r.applyState({{"queues", QVariantMap{{"likes", QVariantMap{{"items", QVariantList{release("b2", "USAAA1900001", "dub")}}}}}}});
        p.applyState({{"playlists", playlists}});
        p.setShuffle(false);
        p.pickAll();

        QSignalSpy ready(&p, &Playlists::sessionReady);
        p.start();
        QVERIFY(ready.wait(5000));
        QCOMPARE(r.playlistsTotal(), 2);             // a, and b for both releases of its song; d2's Pass counts for d
        QCOMPARE(r.current().value("id").toString(), QString("a"));
        r.rate(4, true);
        QCOMPARE(r.current().value("id").toString(), QString("b"));
        QCOMPARE(r.likesLeft(), 1);
        r.rate(5, true);
        QVERIFY(r.queueDone());
        QCOMPARE(r.likesLeft(), 0);                  // the liked best-of has its verdict too
        QCOMPARE(p.library().verdict("b2").value("stars").toInt(), 5);    // and the picker counts it at once
        QCOMPARE(haystack.pendingVerdicts().last().toObject().value("track").toString(), QString("b"));

        // played again with their verdicts, still once a song
        p.setSkipVerdicts(false);
        QSignalSpy again(&p, &Playlists::sessionReady);
        p.start();
        QVERIFY(again.wait(5000));
        QCOMPARE(r.playlistsTotal(), 3);
        QCOMPARE(r.current().value("id").toString(), QString("a"));
    }

    void aSavedReviewListingBothReleasesCountsARatingOnEither()
    {
        QTemporaryDir dir;
        const QString cache = dir.path() + "/cache", state = dir.path() + "/state";
        QDir().mkpath(cache + "/playlists");
        QDir().mkpath(state);
        const auto write = [](const QString &path, const QJsonObject &o) {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(QJsonDocument(o).toJson());
        };
        const auto release = [](const char *id, const char *song, const char *playlist) {
            QJsonObject c{{"id", id}, {"title", QString("Song ") + id}, {"playlists", QJsonArray{pid(playlist)}}};
            if (*song)
                c.insert("song", song);
            return c;
        };
        write(cache + "/playlists/" + pid("bass") + ".json",
              {{"id", pid("bass")}, {"items", QJsonArray{release("a", "", "bass"), release("b", "USAAA1900001", "bass")}}});
        write(cache + "/playlists/" + pid("dub") + ".json",
              {{"id", pid("dub")}, {"items", QJsonArray{release("b2", "USAAA1900001", "dub")}}});
        // an older needle saved this review: it listed b and b2, two releases of one song
        write(state + "/playlist-review.json",
              {{"session", QJsonObject{{"playlists", QJsonArray{pid("bass"), pid("dub")}}, {"skip_verdicts", true},
                                       {"shuffle", false}, {"seed", "1"}, {"tracks", QJsonArray{"a", "b", "b2"}},
                                       {"done", QJsonArray{}}, {"started_at", "2026-10-01T00:00:00Z"}}}});
        HaystackClient haystack({"http://127.0.0.1:1"}, dir.path() + "/no-token", cache, state);
        Review r(&haystack, nullptr);
        Playlists p(&haystack, nullptr, state);
        connect(&p, &Playlists::sessionReady, &r, &Review::startPlaylistReview);
        const auto list = [](const char *id, const QStringList &tracks) {
            return QVariantMap{{"id", pid(id)}, {"name", id}, {"cover", ""}, {"mine", true}, {"tracks", tracks},
                               {"current", true}, {"reader", "server"}, {"snapshot_id", "s1"}};
        };
        // since then b was rated on another computer: the snapshot has it on b only
        p.applyState({{"playlists", QVariantMap{
            {"items", QVariantList{list("bass", {"a", "b"}), list("dub", {"b2"})}},
            {"verdicts", QVariantMap{{"b", QVariantMap{{"kind", "rating"}, {"stars", 4},
                                                        {"given_at", "2026-10-02T00:00:00Z"}}}}}}}});
        p.restore();
        QCOMPARE(r.playlistsTotal(), 3);             // the saved review keeps its songs
        QCOMPARE(r.playlistsLeft(), 1);              // a: b2 counts b's rating
    }

    void aRatingInAnyQueueCountsForALikedReleaseOfTheSong()
    {
        // haystack's Rate your likes goes by the Song whichever queue the verdict came from, and so does needle
        QTemporaryDir dir;
        HaystackClient haystack({"http://127.0.0.1:1"}, dir.path() + "/no-token", dir.path() + "/cache",
                                dir.path() + "/state");
        Review r(&haystack, nullptr);
        QVariantMap found = card("x"), liked = card("x2");
        found.insert("song", "USAAA1900003");
        liked.insert("song", "USAAA1900003");
        r.applyState({{"queues", QVariantMap{
            {"discoveries", QVariantMap{{"items", QVariantList{found, card("y")}}}},
            {"likes", QVariantMap{{"items", QVariantList{liked, card("l1")}}}}}}});
        QCOMPARE(r.queue(), QString("discoveries"));
        QCOMPARE(r.likesLeft(), 2);
        r.rate(4, true);
        QCOMPARE(r.current().value("id").toString(), QString("y"));
        QCOMPARE(r.likesLeft(), 1);                  // x2, the liked release of x, has its verdict
        r.switchQueue();
        QCOMPARE(r.queue(), QString("likes"));
        QCOMPARE(r.current().value("id").toString(), QString("l1"));
    }

    void playlistLineSaysWhereASongComesFrom()
    {
        QVariantMap c{{"fromPlaylist", "All the Dubstep"}, {"reviewPlaylists", 3}, {"alsoOn", QStringList{"EDM"}},
                      {"in_library", true}};
        QCOMPARE(Insight::playlistLine(c, "#fff"),
                 QString::fromUtf8("From <b><font color=\"#fff\">All the Dubstep</font></b>, one of the 3 playlists in this "
                                   "review. It’s also on <b><font color=\"#fff\">EDM</font></b>. It’s in your music library already."));
        c.insert("reviewPlaylists", 1);
        c.insert("alsoOn", QStringList{"A", "B", "C", "D"});
        c.insert("liked", true);
        const QString s = Insight::playlistLine(c, "#fff");
        QVERIFY(s.contains("</b>. It’s also on <b><font color=\"#fff\">A</font></b>, <b><font color=\"#fff\">B</font></b> and 2 more."));
        QVERIFY(s.endsWith("Liked Songs."));
        QVERIFY(Insight::playlistLine({}, "#fff").isEmpty());
    }

    // ---- where haystack is ----
    void configReadsTheServersAndTheTokenFile()
    {
        // (no raw string here: moc can't read one with an odd number of quotes in it)
        const Config c = Config::parse("# where haystack is\n"
                                       "[haystack]\n"
                                       "servers = [\n"
                                       "  \"http://192.168.1.10:8517/\",   # at home\n"
                                       "  'http://10.20.0.10:8517',\n"
                                       "]\n"
                                       "token_file = \"~/.config/needle/haystack-token\"\n"
                                       "\n"
                                       "[other]\n"
                                       "colour = \"blue\"\n", "/etc/needle");
        QCOMPARE(c.servers, QStringList({"http://192.168.1.10:8517", "http://10.20.0.10:8517"}));
        QCOMPARE(c.tokenFile, QDir::homePath() + "/.config/needle/haystack-token");
        QCOMPARE(c.problems, QStringList{"unknown setting other.colour"});
        // a relative token file is in needle's config folder
        QCOMPARE(Config::parse("[haystack]\ntoken_file = 'keys/t'\n", "/etc/needle").tokenFile, QString("/etc/needle/keys/t"));
    }

    void configFallsBackToTheDefaults()
    {
        QTemporaryDir dir;
        const QProcessEnvironment none;
        Config c = Config::load(dir.path() + "/config.toml", dir.path(), none);       // no file
        QCOMPARE(c.servers, QStringList{"http://127.0.0.1:8517"});
        QCOMPARE(c.tokenFile, dir.path() + "/token");
        QVERIFY(c.problems.isEmpty());

        // a file that doesn't parse is ignored as a whole, and the warning names the line
        QFile f(dir.path() + "/config.toml");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("[haystack]\nservers = [\"http://a:1\"]\ntoken_file = \"x\n");
        f.close();
        c = Config::load(f.fileName(), dir.path(), none);
        QCOMPARE(c.servers, QStringList{"http://127.0.0.1:8517"});
        QCOMPARE(c.problems.size(), 1);
        QVERIFY(c.problems.first().startsWith(f.fileName() + ": line 3: "));

        // an address that isn't one is skipped, and said
        c = Config::parse("[haystack]\nservers = [\"ftp://a\", \"http://b:2\"]\n", dir.path());
        QCOMPARE(c.servers, QStringList{"http://b:2"});
        QCOMPARE(c.problems.size(), 1);
    }

    void theEnvironmentOverridesTheConfig()
    {
        QTemporaryDir dir;
        QFile f(dir.path() + "/config.toml");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("[haystack]\nservers = \"http://a:1\"\ntoken_file = \"/srv/token\"\n");
        f.close();
        QProcessEnvironment env;
        env.insert("NEEDLE_SERVER", "http://b:2, https://c.example/haystack/");
        env.insert("NEEDLE_TOKEN_FILE", "~/t");
        const Config c = Config::load(f.fileName(), dir.path(), env);
        QCOMPARE(c.servers, QStringList({"http://b:2", "https://c.example/haystack"}));
        QCOMPARE(c.tokenFile, QDir::homePath() + "/t");
        QVERIFY(c.problems.isEmpty());
        const Config file = Config::load(f.fileName(), dir.path(), QProcessEnvironment());
        QCOMPARE(file.servers, QStringList{"http://a:1"});
        QCOMPARE(file.tokenFile, QString("/srv/token"));

        HaystackClient h(file.servers, QDir::homePath() + "/.config/needle/token", dir.path() + "/cache",
                         dir.path() + "/state");
        QCOMPARE(h.servers(), QStringList{"http://a:1"});
        QCOMPARE(h.tokenFile(), QString("~/.config/needle/token"));      // as the notices show it
    }

    // ---- the outbox ----
    void outboxSurvivesARestartAndSettlesById()
    {
        QTemporaryDir dir;
        const QString path = dir.path() + "/state/outbox.json";
        {
            Outbox o(path);
            o.addVerdict({{"id", "v1"}, {"track", "t"}});
            o.addVerdict({{"id", "v2"}, {"track", "t"}});
            o.addPlay({{"id", "p1"}});
        }
        Outbox o(path);
        QCOMPARE(o.pendingVerdicts(), 2);
        QCOMPARE(o.size(), 3);
        o.settle({"v1", "p1"});
        QCOMPARE(Outbox(path).size(), 1);
        QCOMPARE(QFile(path).permissions() & (QFileDevice::ReadGroup | QFileDevice::ReadOther), QFileDevice::Permissions());
    }

    // ---- the engine's events ----
    void engineEventsUpdateTheState()
    {
        EngineClient e({}, {});
        QSignalSpy ended(&e, &EngineClient::ended);
        e.handleLine(R"({"event":"auth","state":"ok","user":"me"})");
        QCOMPARE(e.auth(), QString("ok"));
        e.load("4MbtaySL1453JwADvGryCL", true);
        e.handleLine(R"({"event":"state","state":"playing","id":"4MbtaySL1453JwADvGryCL","position_ms":72000,"duration_ms":179000})");
        QVERIFY(e.playing());
        QCOMPARE(e.durationMs(), 179000);
        QVERIFY(e.positionMs() >= 72000);
        e.handleLine(R"({"event":"state","state":"paused","id":"someOtherTrack0000000","position_ms":1})");
        QVERIFY(e.playing());                        // a late event about another track changes nothing
        e.handleLine(R"({"event":"spectrum","bands":[0,128,255],"peaks":[1,2,3]})");
        QCOMPARE(quint8(e.bands().at(2)), quint8(255));
        e.handleLine(R"({"event":"end","id":"4MbtaySL1453JwADvGryCL"})");
        QCOMPARE(ended.count(), 1);
        QSignalSpy read(&e, &EngineClient::playlistRead);
        e.handleLine(R"({"event":"playlist","id":"p","name":"OG Drift Phonk","snapshot_id":"s","tracks":[{"id":"t"}]})");
        QCOMPARE(read.count(), 1);
        QCOMPARE(read.first().at(1).toJsonObject().value("tracks").toArray().size(), 1);
    }

    // ---- a Review session ----
    void verdictKeysAdvanceAsDesigned()
    {
        QTemporaryDir dir;
        HaystackClient haystack({"http://127.0.0.1:1"}, dir.path() + "/no-token", dir.path() + "/cache",
                                dir.path() + "/state");
        Review r(&haystack, nullptr);
        const QVariantMap state{{"queues", QVariantMap{
            {"discoveries", QVariantMap{{"items", QVariantList{card("a"), card("b"), card("c")}},
                                        {"next_batch_at", "2026-10-12T00:05:51Z"}}},
            {"likes", QVariantMap{{"items", QVariantList{card("b"), card("l1")}}}}}}};
        r.applyState(state);
        QCOMPARE(r.current().value("id").toString(), QString("a"));
        QCOMPARE(r.discoveriesPosition(), 1);
        r.rate(4);                                   // stays
        QCOMPARE(r.current().value("id").toString(), QString("a"));
        QCOMPARE(r.discoveriesPosition(), 1);
        r.pass();                                    // advances
        QCOMPARE(r.current().value("id").toString(), QString("b"));
        QCOMPARE(r.discoveriesPosition(), 2);
        r.rate(5, true);                             // Shift+5: rates and moves on
        QCOMPARE(r.current().value("id").toString(), QString("c"));
        QCOMPARE(r.likesLeft(), 1);                  // b was in both queues
        r.dislike();
        QVERIFY(r.queueDone());
        QCOMPARE(haystack.pendingVerdicts().size(), 4);
        const auto last = haystack.pendingVerdicts().last().toObject();
        QCOMPARE(last.value("kind").toString(), QString("dislike"));
        QCOMPARE(last.value("queue").toString(), QString("discoveries"));
        QVERIFY(last.value("given_at").toString().endsWith("Z"));
        // a snapshot that doesn't know our verdicts yet still shows them
        r.applyState(state);
        QVERIFY(r.queueDone());
        r.switchQueue();
        QCOMPARE(r.current().value("id").toString(), QString("l1"));
    }

    // haystack's /v1 only grows (its docs/api.md): a snapshot from a haystack before 0.4.0 and one with the new
    // fields both review, and the features come from the snapshot.
    void oldAndNewSnapshotsBothReview()
    {
        QTemporaryDir dir;
        HaystackClient haystack({"http://127.0.0.1:1"}, dir.path() + "/no-token", dir.path() + "/cache",
                                dir.path() + "/state");
        Review r(&haystack, nullptr);
        const QVariantMap old{{"queues", QVariantMap{
            {"discoveries", QVariantMap{{"items", QVariantList{card("a"), card("b")}}}}}}};
        r.applyState(old);
        QCOMPARE(r.current().value("id").toString(), QString("a"));
        QVERIFY(r.features().isEmpty());
        QCOMPARE(Insight::confidence(r.current().value("why").toMap(), r.features()), QString());
        r.pass();
        QCOMPARE(r.current().value("id").toString(), QString("b"));

        QVariantMap fresh = card("c");
        fresh["why"] = QVariantMap{{"similarity", 0.8}, {"model", "rerank"}, {"confidence", 0.62},
                                   {"reasons", QVariantList{"in your usual styles"}},
                                   {"band", QVariantMap{{"kind", "rerank"}, {"percentile", 91}}}};
        const QVariantMap now{{"server", QVariantMap{{"version", "0.4.0"},
                                                     {"features", QVariantList{"why.band", "rerank", "why.reasons"}}}},
                              {"queues", QVariantMap{{"discoveries", QVariantMap{{"items", QVariantList{fresh}}}}}}};
        r.applyState(now);
        QCOMPARE(r.current().value("id").toString(), QString("c"));
        QCOMPARE(r.features(), QStringList({"why.band", "rerank", "why.reasons"}));
        const QVariantMap why = r.current().value("why").toMap();
        QCOMPARE(Insight::confidence(why, r.features()), QString("Confidence 62%"));
        QVERIFY(Insight::why(why, "#fff", r.features()).contains("Learned from your Verdicts: in your usual styles."));
        r.rate(4, true);
        QVERIFY(r.queueDone());
        QCOMPARE(haystack.pendingVerdicts().size(), 2);
    }

    // haystack 0.5.0's Calibrate queue (feature queue.calibrate): a tab and a stop on Tab only when it is listed
    void calibrateIsAQueueOnlyWhenTheServerListsIt()
    {
        QTemporaryDir dir;
        HaystackClient haystack({"http://127.0.0.1:1"}, dir.path() + "/no-token", dir.path() + "/cache",
                                dir.path() + "/state");
        Review r(&haystack, nullptr);
        const QVariantMap queues{{"discoveries", QVariantMap{{"items", QVariantList{card("a")}}}},
                                 {"calibrate", QVariantMap{{"items", QVariantList{card("k1"), card("k2")}}}},
                                 {"likes", QVariantMap{{"items", QVariantList{card("l1")}}}}};
        const auto server = [](const QVariantList &features) {
            return QVariantMap{{"version", "0.5.0"}, {"features", features}};
        };
        const QVariantMap without{{"server", server({"why.band", "rerank", "why.reasons"})}, {"queues", queues}};
        r.applyState(without);
        QVERIFY(!r.hasCalibrate());
        QCOMPARE(r.calibrateTotal(), 0);
        QCOMPARE(r.queue(), QString("discoveries"));
        r.setQueue("calibrate");                     // not offered: stays
        QCOMPARE(r.queue(), QString("discoveries"));
        QStringList round;
        for (int i = 0; i < 3; ++i) {
            r.switchQueue();
            round << r.queue();
        }
        QCOMPARE(round, QStringList({"likes", "playlists", "discoveries"}));

        const QVariantMap with{{"server", server({"why.band", "rerank", "why.reasons", "queue.calibrate"})},
                               {"queues", queues}};
        r.applyState(with);
        QVERIFY(r.hasCalibrate());
        QCOMPARE(r.calibrateTotal(), 2);
        QCOMPARE(r.calibrateLeft(), 2);
        round.clear();
        for (int i = 0; i < 4; ++i) {
            r.switchQueue();
            round << r.queue();
        }
        QCOMPARE(round, QStringList({"calibrate", "likes", "playlists", "discoveries"}));
        r.setQueue("calibrate");
        QCOMPARE(r.current().value("id").toString(), QString("k1"));
        QCOMPARE(r.calibratePosition(), 1);
        r.rate(2, true);
        QCOMPARE(r.current().value("id").toString(), QString("k2"));
        QCOMPARE(r.calibrateLeft(), 1);
        const auto v = haystack.pendingVerdicts().last().toObject();
        QCOMPARE(v.value("track").toString(), QString("k1"));
        QCOMPARE(v.value("queue").toString(), QString("calibrate"));
        QCOMPARE(v.value("stars").toInt(), 2);
        r.pass();
        QVERIFY(r.queueDone());
        // a snapshot that doesn't know these verdicts yet still shows them
        r.applyState(with);
        QCOMPARE(r.calibrateLeft(), 0);

        // haystack rolled back to before 0.5.0: back to Discoveries
        r.applyState(without);
        QVERIFY(!r.hasCalibrate());
        QCOMPARE(r.queue(), QString("discoveries"));
        QCOMPARE(r.current().value("id").toString(), QString("a"));
    }

    // a finished queue's notice offers the next queue with something left, Calibrate included
    void theDoneNoticeOffersTheNextQueueWithSomethingLeft()
    {
        QTemporaryDir dir;
        HaystackClient haystack({"http://127.0.0.1:1"}, dir.path() + "/no-token", dir.path() + "/cache",
                                dir.path() + "/state");
        Review r(&haystack, nullptr);
        const QVariantMap queues{{"discoveries", QVariantMap{{"items", QVariantList{card("a")}}}},
                                 {"calibrate", QVariantMap{{"items", QVariantList{card("k1"), card("k2")}}}},
                                 {"likes", QVariantMap{{"items", QVariantList{card("l1")}}}}};
        r.applyState({{"server", QVariantMap{{"features", QVariantList{"queue.calibrate"}}}}, {"queues", queues}});
        QCOMPARE(r.queue(), QString("discoveries"));
        QCOMPARE(r.tabNext(), QString("calibrate"));
        r.rate(4, true);
        QVERIFY(r.queueDone());
        QCOMPARE(r.doneNext(), QString("calibrate"));
        r.setQueue("likes");
        QCOMPARE(r.tabNext(), QString("playlists"));
        r.rate(5, true);
        QVERIFY(r.queueDone());
        QCOMPARE(r.doneNext(), QString("calibrate"));    // Discoveries are done; Calibrate still has two
        r.setQueue("calibrate");
        QCOMPARE(r.tabNext(), QString("likes"));
        QCOMPARE(r.doneNext(), QString());               // not done yet
        r.rate(3, true);
        r.pass();
        QVERIFY(r.queueDone());
        QCOMPARE(r.doneNext(), QString());               // nothing left anywhere

        // Rate your likes done with Discoveries left: back to Discoveries first (a fresh outbox: the verdicts above
        // would count as given)
        QTemporaryDir dir2;
        HaystackClient fresh({"http://127.0.0.1:1"}, dir2.path() + "/no-token", dir2.path() + "/cache",
                             dir2.path() + "/state");
        Review r2(&fresh, nullptr);
        r2.applyState({{"server", QVariantMap{{"features", QVariantList{"queue.calibrate"}}}}, {"queues", queues}});
        r2.setQueue("likes");
        r2.rate(5, true);
        QCOMPARE(r2.doneNext(), QString("discoveries"));
        // without the feature, Calibrate is never offered
        QTemporaryDir dir3;
        HaystackClient old({"http://127.0.0.1:1"}, dir3.path() + "/no-token", dir3.path() + "/cache",
                           dir3.path() + "/state");
        Review r3(&old, nullptr);
        r3.applyState({{"queues", QVariantMap{{"calibrate", queues.value("calibrate")},
                                              {"likes", queues.value("likes")}}}});
        r3.setQueue("likes");
        QCOMPARE(r3.current().value("id").toString(), QString("l1"));
        r3.rate(5, true);
        QVERIFY(r3.queueDone());
        QCOMPARE(r3.doneNext(), QString());
    }

    // with Discoveries reviewed, needle opens on Calibrate before Rate your likes
    void aNewSessionOpensOnCalibrateWhenDiscoveriesAreDone()
    {
        QTemporaryDir dir;
        HaystackClient haystack({"http://127.0.0.1:1"}, dir.path() + "/no-token", dir.path() + "/cache",
                                dir.path() + "/state");
        Review r(&haystack, nullptr);
        r.applyState({{"server", QVariantMap{{"features", QVariantList{"queue.calibrate"}}}},
                      {"queues", QVariantMap{
                          {"discoveries", QVariantMap{{"items", QVariantList{card("a", rating(4))}}}},
                          {"calibrate", QVariantMap{{"items", QVariantList{card("k1")}}}},
                          {"likes", QVariantMap{{"items", QVariantList{card("l1")}}}}}}});
        QCOMPARE(r.queue(), QString("calibrate"));
        QCOMPARE(r.current().value("id").toString(), QString("k1"));
    }
};

QTEST_MAIN(NeedleTests)
#include "needle_tests.moc"
