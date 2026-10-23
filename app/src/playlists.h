#pragma once

#include <QAbstractListModel>
#include <QObject>
#include <QSet>
#include <QTimer>

#include "playlistlibrary.h"

class HaystackClient;
class EngineClient;
class Playlists;

// The cover wall's tiles: the playlists shown (after the filter, the name filter and the sort), one row each.
class PlaylistModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role {
        IdRole = Qt::UserRole + 1, NameRole, CoverRole, MineRole, OwnerRole, SongsRole, ReviewedRole, RatedRole,
        LeftRole, StarsRole, PassedRole, DislikedRole, AverageRole, StageRole, ReadableRole, MetaRole, SelectedRole,
        ArtistsRole, ReadingRole
    };
    explicit PlaylistModel(Playlists *owner) : QAbstractListModel(nullptr), m_owner(owner) {}
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void reset(const QStringList &ids);
    void refresh();
    QString idAt(int row) const { return m_ids.value(row); }
    int rowOf(const QString &id) const { return m_ids.indexOf(id); }

private:
    Playlists *m_owner;
    QStringList m_ids;
    mutable QHash<QString, QVariantMap> m_tiles;   // cleared on every change
};

// The Playlist review: the picker (option A, the cover wall) and the review it starts. You pick playlists,
// sees how far each is reviewed and what the pick adds up to, and starts; needle fetches their cards from haystack
// and hands Review one queue of their songs, each once. It also has needle's engine read the playlists Spotify
// won't list for haystack (ADR 0008), one at a time, and sends their songs on.
class Playlists : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *model READ model CONSTANT)
    Q_PROPERTY(int total READ total NOTIFY changed)
    Q_PROPERTY(QVariantMap stageCounts READ stageCounts NOTIFY changed)       // all fresh progress done
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY viewChanged)  // all fresh progress done
    Q_PROPERTY(QString sort READ sort WRITE setSort NOTIFY viewChanged)        // spotify left name
    Q_PROPERTY(QString sortLabel READ sortLabel NOTIFY viewChanged)
    Q_PROPERTY(QString nameFilter READ nameFilter WRITE setNameFilter NOTIFY viewChanged)
    Q_PROPERTY(int shown READ shown NOTIFY viewChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentChanged)
    Q_PROPERTY(QVariantMap current READ current NOTIFY currentChanged)
    Q_PROPERTY(QVariantMap selection READ selectionInfo NOTIFY selectionChanged)
    Q_PROPERTY(bool skipVerdicts READ skipVerdicts WRITE setSkipVerdicts NOTIFY selectionChanged)
    Q_PROPERTY(bool shuffle READ shuffle WRITE setShuffle NOTIFY selectionChanged)
    Q_PROPERTY(bool pickerOpen READ pickerOpen NOTIFY pickerChanged)
    Q_PROPERTY(bool starting READ starting NOTIFY pickerChanged)
    Q_PROPERTY(QString notice READ notice NOTIFY pickerChanged)
    Q_PROPERTY(bool hasSession READ hasSession NOTIFY sessionChanged)
    Q_PROPERTY(QVariantList sessionPlaylists READ sessionPlaylists NOTIFY sessionChanged)
    Q_PROPERTY(QString readingText READ readingText NOTIFY changed)

public:
    Playlists(HaystackClient *haystack, EngineClient *engine, const QString &stateDir, QObject *parent = nullptr);
    ~Playlists() override;

    QAbstractItemModel *model() { return &m_model; }
    int total() const { return m_library.size(); }
    QVariantMap stageCounts() const;
    QString filter() const { return m_filter; }
    void setFilter(const QString &f);
    QString sort() const { return m_sort; }
    void setSort(const QString &s);
    QString sortLabel() const;
    QString nameFilter() const { return m_nameFilter; }
    void setNameFilter(const QString &f);
    int shown() const { return m_model.rowCount(); }
    int currentIndex() const { return m_current; }
    void setCurrentIndex(int i);
    QVariantMap current() const;
    QVariantMap selectionInfo() const;
    bool skipVerdicts() const { return m_skip; }
    void setSkipVerdicts(bool on);
    bool shuffle() const { return m_shuffle; }
    void setShuffle(bool on);
    bool pickerOpen() const { return m_pickerOpen; }
    bool starting() const { return m_starting; }
    QString notice() const { return m_notice; }
    bool hasSession() const { return !m_session.isEmpty(); }
    QVariantList sessionPlaylists() const;
    QString readingText() const;
    const PlaylistSession &session() const { return m_session; }
    const PlaylistLibrary &library() const { return m_library; }

    // One tile's details (for the model and the focus card).
    QVariantMap tile(const QString &id) const;
    bool isSelected(const QString &id) const { return m_selected.contains(id); }

    // For tests and the clock: the snapshot handling without the network.
    void applyState(const QVariantMap &state);
    // Called once Review listens: hands it the saved Playlist review, if there is one.
    void restore();
    // haystack's answers to a start (public for the tests)
    void playlistArrived(const QString &id, const QVariantMap &playlist);
    void playlistFailed(const QString &id, int status);

public slots:
    void toggle(int row);
    void toggleCurrent() { toggle(m_current); }
    // Pick every playlist shown, or, when all of them are picked already, none of them.
    void pickAll();
    void clearSelection();
    void move(int delta);
    // The picked playlists, or with none picked the one in focus.
    void start();
    void openPicker();
    void closePicker();
    void cycleFilter();
    void cycleSort();
    void toggleSkipVerdicts() { setSkipVerdicts(!m_skip); }
    void toggleShuffle() { setShuffle(!m_shuffle); }

signals:
    void changed();
    void viewChanged();
    void currentChanged();
    void selectionChanged();
    void pickerChanged();
    void sessionChanged();
    // Review takes over: the cards in play order; revisit when songs with a verdict play too (and done lists the
    // ones given a verdict in this review so far); activate to switch to the review and play.
    void sessionReady(const QVariantList &cards, bool revisit, const QStringList &done, bool activate);

private:
    void rebuildModel(bool keepCurrent = true);
    QStringList ordered(const QStringList &ids) const;
    void verdictAdded(const QJsonObject &v);
    void swapAdded(const QJsonObject &s);
    void finishStart();
    QVariantList sessionCards() const;
    void readNext();
    void save() const;
    void load();
    QString name(const QString &id) const;

    HaystackClient *m_haystack;
    EngineClient *m_engine;
    QString m_file;
    PlaylistModel m_model;
    PlaylistLibrary m_library;
    QString m_filter = "all", m_sort = "spotify", m_nameFilter;
    int m_current = 0;
    QStringList m_selected;
    bool m_skip = true, m_shuffle = true;
    bool m_pickerOpen = false, m_starting = false;
    QString m_notice;
    PlaylistSession m_session;
    QHash<QString, QVariantMap> m_cards;       // the review's cards by track id
    // a start in progress: the playlists still being fetched, and the ones that arrived
    QStringList m_waiting, m_starts;
    QHash<QString, QVariantMap> m_fetched;
    // engine reads: the playlist being read now, and the versions already tried this run
    QString m_reading;
    QSet<QString> m_tried;
    QTimer m_readWatchdog;
};
