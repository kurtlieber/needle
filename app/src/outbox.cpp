#include "outbox.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

Outbox::Outbox(const QString &path) : m_path(path)
{
    QFile f(path);
    if (f.open(QIODevice::ReadOnly)) {
        const auto o = QJsonDocument::fromJson(f.readAll()).object();
        m_verdicts = o.value("verdicts").toArray();
        m_plays = o.value("plays").toArray();
        m_swaps = o.value("swaps").toArray();
    }
}

void Outbox::addVerdict(const QJsonObject &v)
{
    m_verdicts.append(v);
    save();
}

void Outbox::addPlay(const QJsonObject &p)
{
    m_plays.append(p);
    save();
}

void Outbox::addSwap(const QJsonObject &s)
{
    m_swaps.append(s);
    save();
}

void Outbox::settle(const QStringList &ids)
{
    if (ids.isEmpty())
        return;
    auto keep = [&](const QJsonArray &a) {
        QJsonArray out;
        for (const auto &v : a)
            if (!ids.contains(v.toObject().value("id").toString()))
                out.append(v);
        return out;
    };
    m_verdicts = keep(m_verdicts);
    m_plays = keep(m_plays);
    m_swaps = keep(m_swaps);
    save();
}

void Outbox::save()
{
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile f(m_path);
    if (!f.open(QIODevice::WriteOnly)) {
        m_error = f.errorString();
        return;
    }
    f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    f.write(QJsonDocument(QJsonObject{{"verdicts", m_verdicts}, {"plays", m_plays}, {"swaps", m_swaps}}).toJson(QJsonDocument::Compact));
    m_error = f.commit() ? QString() : f.errorString();
}
