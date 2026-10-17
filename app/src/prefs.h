#pragma once

#include <QDBusVariant>
#include <QObject>
#include <QSettings>
#include <QVariant>

// needle's settings (~/.config/needle/needle.conf) and the desktop's motion preference.
// Reduced motion follows the desktop (org.gnome.desktop.interface enable-animations, read through the
// settings portal and followed live) until the M key overrides it.
class Prefs : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool glow READ glow WRITE setGlow NOTIFY changed)
    Q_PROPERTY(bool reducedMotion READ reducedMotion NOTIFY changed)
    Q_PROPERTY(QString motion READ motion NOTIFY changed)   // system | reduce | full

public:
    explicit Prefs(bool watchDesktop = true, QObject *parent = nullptr);

    bool glow() const;
    void setGlow(bool on);
    bool reducedMotion() const;
    QString motion() const;
    bool systemReduced() const { return m_systemReduced; }

signals:
    void changed();

public slots:
    void toggleGlow() { setGlow(!glow()); }
    // system -> the opposite of what the system says -> back to following the system
    void toggleMotion();

private slots:
    void portalChanged(const QString &ns, const QString &key, const QDBusVariant &value);

private:
    QSettings m_settings;
    bool m_systemReduced = false;
};
