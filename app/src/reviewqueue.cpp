#include "reviewqueue.h"

int ReviewQueue::indexOf(const QString &id) const
{
    for (int i = 0; i < m_cards.size(); ++i)
        if (m_cards[i].toMap().value("id").toString() == id)
            return i;
    return -1;
}

bool ReviewQueue::isReviewed(int i) const
{
    if (m_revisit)
        return m_done.contains(m_cards[i].toMap().value("id").toString());
    const auto v = m_cards[i].toMap().value("verdict");
    return v.isValid() && !v.isNull() && !v.toMap().isEmpty();
}

int ReviewQueue::reviewed() const
{
    int n = 0;
    for (int i = 0; i < m_cards.size(); ++i)
        n += isReviewed(i);
    return n;
}

int ReviewQueue::nextUnreviewed(int from) const
{
    const int n = m_cards.size();
    for (int k = 1; k <= n; ++k) {
        const int i = ((from < 0 ? -1 : from) + k) % n;
        if (i != from && !isReviewed(i))
            return i;
    }
    return -1;
}

void ReviewQueue::setCards(const QVariantList &cards)
{
    const QString keep = currentId();
    const QVariantMap keepCard = current();
    const QString after = upNext().value("id").toString();
    m_cards = cards;
    m_current = keep.isEmpty() ? -1 : indexOf(keep);
    // haystack drops a rated track from Rate your likes, but it may still be playing: keep it until moving on
    if (!keep.isEmpty() && m_current < 0 && !keepCard.value("verdict").toMap().isEmpty()) {
        m_cards.prepend(keepCard);
        m_current = 0;
    }
    // haystack moves a rated track too (to the end of Discoveries): put it back just before the track that was up
    // next, or Next goes from wherever it landed (a skipped track, or round to the top of the queue)
    const int at = indexOf(after);
    if (m_current >= 0 && isReviewed(m_current) && at >= 0 && !isReviewed(at)) {
        const auto c = m_cards.takeAt(m_current);
        m_current = m_current < at ? at - 1 : at;
        m_cards.insert(m_current, c);
    }
    QStringList history;
    for (const auto &id : std::as_const(m_history))
        if (indexOf(id) >= 0)
            history << id;
    m_history = history;
    if (m_current < 0 && keep.isEmpty())
        restart();
    else if (m_current < 0)
        m_current = nextUnreviewed(-1);
}

void ReviewQueue::reset(const QVariantList &cards, bool revisit, const QStringList &done)
{
    m_cards = cards;
    m_history.clear();
    m_revisit = revisit;
    m_done = QSet<QString>(done.cbegin(), done.cend());
    restart();
}

bool ReviewQueue::updateCard(const QVariantMap &card)
{
    const int i = indexOf(card.value("id").toString());
    if (i < 0)
        return false;
    auto c = m_cards[i].toMap();
    const auto mine = c.value("verdict").toMap();
    for (auto it = card.cbegin(); it != card.cend(); ++it)
        c.insert(it.key(), it.value());
    // a verdict given here that haystack doesn't have yet stays
    if (!mine.isEmpty() && mine.value("given_at").toString() > c.value("verdict").toMap().value("given_at").toString())
        c.insert("verdict", mine);
    m_cards[i] = c;
    return true;
}

void ReviewQueue::restart()
{
    m_current = m_cards.isEmpty() ? -1 : nextUnreviewed(-1);
}

void ReviewQueue::applyVerdict(const QString &id, const QVariantMap &verdict)
{
    const int i = indexOf(id);
    if (i < 0)
        return;
    auto c = m_cards[i].toMap();
    c.insert("verdict", verdict);
    m_cards[i] = c;
    if (m_revisit)
        m_done.insert(id);
}

QVariantMap ReviewQueue::upNext() const
{
    const int i = m_cards.isEmpty() ? -1 : nextUnreviewed(m_current);
    return i >= 0 ? m_cards[i].toMap() : QVariantMap{};
}

QVariantList ReviewQueue::lookahead(int n) const
{
    QVariantList out;
    int i = m_current;
    QList<int> seen;
    while (out.size() < n) {
        i = m_cards.isEmpty() ? -1 : nextUnreviewed(i);
        if (i < 0 || seen.contains(i))
            break;
        seen << i;
        out << m_cards[i];
    }
    return out;
}

QVariantMap ReviewQueue::card(const QString &id) const
{
    const int i = indexOf(id);
    return i >= 0 ? m_cards[i].toMap() : QVariantMap{};
}

QStringList ReviewQueue::songIds(const QString &song) const
{
    QStringList out;
    if (song.isEmpty())
        return out;
    for (const auto &v : m_cards)
        if (v.toMap().value("song").toString() == song)
            out << v.toMap().value("id").toString();
    return out;
}

bool ReviewQueue::next()
{
    const int i = m_cards.isEmpty() ? -1 : nextUnreviewed(m_current);
    if (i < 0) {
        // nothing else waits; if the current one has its verdict now, the queue is done
        if (m_current >= 0 && isReviewed(m_current)) {
            m_history << currentId();
            m_current = -1;
        }
        return false;
    }
    if (m_current >= 0)
        m_history << currentId();
    m_current = i;
    return true;
}

bool ReviewQueue::back()
{
    while (!m_history.isEmpty()) {
        const int i = indexOf(m_history.takeLast());
        if (i >= 0 && i != m_current) {
            m_current = i;
            return true;
        }
    }
    return false;
}
