#include "prefs.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>

static const char *kPortal = "org.freedesktop.portal.Desktop";
static const char *kPath = "/org/freedesktop/portal/desktop";
static const char *kSettings = "org.freedesktop.portal.Settings";

Prefs::Prefs(bool watchDesktop, QObject *parent) : QObject(parent), m_settings("needle", "needle")
{
    if (!watchDesktop)
        return;
    QDBusInterface portal(kPortal, kPath, kSettings, QDBusConnection::sessionBus());
    QDBusReply<QDBusVariant> r = portal.call("ReadOne", "org.gnome.desktop.interface", "enable-animations");
    if (r.isValid())
        m_systemReduced = !r.value().variant().toBool();
    QDBusConnection::sessionBus().connect(kPortal, kPath, kSettings, "SettingChanged", this,
                                          SLOT(portalChanged(QString, QString, QDBusVariant)));
}

void Prefs::portalChanged(const QString &ns, const QString &key, const QDBusVariant &value)
{
    if (ns == "org.gnome.desktop.interface" && key == "enable-animations") {
        m_systemReduced = !value.variant().toBool();
        emit changed();
    }
}

bool Prefs::glow() const { return m_settings.value("glow", true).toBool(); }

void Prefs::setGlow(bool on)
{
    m_settings.setValue("glow", on);
    emit changed();
}

QString Prefs::motion() const { return m_settings.value("motion", "system").toString(); }

bool Prefs::reducedMotion() const
{
    const QString m = motion();
    return m == "reduce" || (m == "system" && m_systemReduced);
}

void Prefs::toggleMotion()
{
    const QString m = motion();
    m_settings.setValue("motion", m != "system" ? "system" : (m_systemReduced ? "full" : "reduce"));
    emit changed();
}
