#pragma once

#include <QSet>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// One Review queue (Discoveries, Rate your likes or a Playlist review): the track cards in order, which one is
// current, and the way through them. Pure: no network, no playback.
//
// Next goes to the next track without a verdict (wrapping round); a verdict marks a card reviewed and stays on
// it; Back returns to the tracks visited before, reviewed or not, so a verdict can be changed. A queue that
// revisits (a Playlist review with "Skip songs with a verdict" off) plays songs that already have a verdict too:
// there a song counts as reviewed once it gets a verdict in this queue.
class ReviewQueue {
public:
    // Replace the cards (a fresh /v1/state). The current track stays current if it is still in the queue, or if
    // it has its verdict now (haystack leaves rated tracks out of Rate your likes) until Next.
    void setCards(const QVariantList &cards);
    // Start over with these cards: no history, the first one not reviewed is current.
    void reset(const QVariantList &cards, bool revisit = false, const QStringList &done = {});
    void applyVerdict(const QString &id, const QVariantMap &verdict);
    // Newer details for a card already here (its Readings, say); the fields it doesn't carry are kept.
    bool updateCard(const QVariantMap &card);

    int size() const { return m_cards.size(); }
    int reviewed() const;
    int unreviewed() const { return size() - reviewed(); }
    bool hasCurrent() const { return m_current >= 0; }
    bool currentReviewed() const { return m_current >= 0 && isReviewed(m_current); }
    bool revisits() const { return m_revisit; }
    QVariantMap current() const { return m_current >= 0 ? m_cards[m_current].toMap() : QVariantMap{}; }
    QString currentId() const { return current().value("id").toString(); }
    // The track Next would go to.
    QVariantMap upNext() const;
    QVariantList lookahead(int n) const;
    QVariantMap card(const QString &id) const;

    // -> false when no other track without a verdict is left (the current one is then left as it is)
    bool next();
    bool back();
    bool canGoBack() const { return !m_history.isEmpty(); }
    // Start (or restart) at the first track without a verdict; none -> no current track.
    void restart();

private:
    int indexOf(const QString &id) const;
    int nextUnreviewed(int from) const;
    bool isReviewed(int i) const;

    QVariantList m_cards;
    int m_current = -1;
    QStringList m_history;
    bool m_revisit = false;
    QSet<QString> m_done;
};
