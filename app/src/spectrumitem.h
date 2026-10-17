#pragma once

#include <QByteArray>
#include <QColor>
#include <QElapsedTimer>
#include <QQuickPaintedItem>
#include <QVector>

// The live spectrum: 56 bars from the engine's frames, bars in `barColor` (stronger where louder), a peak
// tick per bar in `peakColor`. With reduced motion it shows a slowly settling average, redrawn at most once a
// second, and no peak ticks.
class SpectrumItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QByteArray bands READ bands WRITE setBands NOTIFY bandsChanged)
    Q_PROPERTY(QByteArray peaks READ peaks WRITE setPeaks NOTIFY peaksChanged)
    Q_PROPERTY(QColor barColor MEMBER m_barColor NOTIFY styleChanged)
    Q_PROPERTY(QColor peakColor MEMBER m_peakColor NOTIFY styleChanged)
    Q_PROPERTY(bool reducedMotion MEMBER m_reduced NOTIFY styleChanged)
    Q_PROPERTY(int count MEMBER m_count NOTIFY styleChanged)

public:
    explicit SpectrumItem(QQuickItem *parent = nullptr);
    void paint(QPainter *p) override;

    QByteArray bands() const { return m_bands; }
    QByteArray peaks() const { return m_peaks; }
    void setBands(const QByteArray &b);
    void setPeaks(const QByteArray &p);

signals:
    void bandsChanged();
    void peaksChanged();
    void styleChanged();

private:
    QByteArray m_bands, m_peaks;
    QVector<double> m_average;
    QElapsedTimer m_lastSlowPaint;
    QColor m_barColor, m_peakColor;
    bool m_reduced = false;
    int m_count = 56;
};
