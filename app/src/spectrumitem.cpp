#include "spectrumitem.h"

#include <QPainter>
#include <QPainterPath>

SpectrumItem::SpectrumItem(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &SpectrumItem::styleChanged, this, [this] { update(); });
    connect(this, &QQuickPaintedItem::textureSizeChanged, this, [this] { update(); });
    m_lastSlowPaint.start();
}

void SpectrumItem::setBands(const QByteArray &b)
{
    m_bands = b;
    if (m_average.size() != b.size())
        m_average = QVector<double>(b.size(), 0.0);
    for (int i = 0; i < b.size(); ++i)
        m_average[i] = 0.97 * m_average[i] + 0.03 * quint8(b[i]);
    emit bandsChanged();
    if (!m_reduced || m_lastSlowPaint.elapsed() >= 1000) {
        m_lastSlowPaint.restart();
        update();
    }
}

void SpectrumItem::setPeaks(const QByteArray &p)
{
    m_peaks = p;
    emit peaksChanged();
}

void SpectrumItem::paint(QPainter *p)
{
    const int n = m_count;
    const double gap = 2.0, w = width(), h = height();
    const double bw = (w - gap * (n - 1)) / n;
    p->setPen(Qt::NoPen);
    for (int i = 0; i < n; ++i) {
        double v = 0;
        if (m_reduced && i < m_average.size())
            v = m_average[i] / 255.0;
        else if (i < m_bands.size())
            v = quint8(m_bands[i]) / 255.0;
        const double x = i * (bw + gap);
        const double bh = std::max(3.0, v * (h - 8));
        QColor c = m_barColor;
        c.setAlphaF(0.35 + 0.5 * v);
        QPainterPath bar;
        bar.addRoundedRect(QRectF(x, h - bh, bw, bh), 1.5, 1.5);
        p->fillPath(bar, c);
        if (!m_reduced && i < m_peaks.size()) {
            const double pk = quint8(m_peaks[i]) / 255.0;
            const double top = std::min(h - 2, std::max(bh + 4, pk * (h - 4)));
            QPainterPath tick;
            tick.addRoundedRect(QRectF(x, h - top, bw, 2), 1, 1);
            p->fillPath(tick, m_peakColor);
        }
    }
}
