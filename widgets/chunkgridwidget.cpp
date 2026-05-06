#include "chunkgridwidget.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

ChunkGridWidget::ChunkGridWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void ChunkGridWidget::clear() {
    m_n = 0; m_m = 0;
    m_dataPresent.clear();
    m_recoveryPresent.clear();
    m_cells.clear();
    updateGeometry();
    update();
}

void ChunkGridWidget::setChunkInfo(uint32_t n, uint32_t m,
                                    const std::vector<bool>& dataPresent,
                                    const std::vector<bool>& recoveryPresent) {
    m_n = n; m_m = m;
    m_dataPresent     = dataPresent;
    m_recoveryPresent = recoveryPresent;

    // Fill to size if shorter than n/m
    m_dataPresent.resize(n, false);
    m_recoveryPresent.resize(m, false);

    rebuildLayout();
    updateGeometry();
    update();
}

void ChunkGridWidget::rebuildLayout() {
    m_cells.clear();
    if (m_n == 0 && m_m == 0) return;

    const int w     = width() > 0 ? width() : 300;
    const int step  = kCellSize + kGap;
    const int cols  = std::max(1, (w - kGap) / step);

    int x = kGap, y = kGap;
    auto place = [&](bool isRec, bool present, uint32_t idx) {
        m_cells.push_back({ QRect(x, y, kCellSize, kCellSize), isRec, present, idx });
        x += step;
        if ((static_cast<int>(m_cells.size())) % cols == 0) { x = kGap; y += step; }
    };

    for (uint32_t i = 0; i < m_n; ++i)
        place(false, m_dataPresent[i], i);

    // small gap between data and recovery chunks
    if (m_m > 0 && !m_cells.empty()) {
        if (x != kGap) { x = kGap; y += step; }  // force new row
        y += kGap * 2;
    }

    for (uint32_t i = 0; i < m_m; ++i)
        place(true, m_recoveryPresent[i], i);
}

QSize ChunkGridWidget::sizeHint() const {
    if (m_cells.empty()) return { 100, 36 };
    int maxY = 0;
    for (const auto& c : m_cells) maxY = std::max(maxY, c.rect.bottom());
    return { width(), maxY + kGap + 4 };
}

QColor ChunkGridWidget::cellColor(bool isRecovery, bool present) const {
    bool dark = palette().color(QPalette::Window).lightness() < 128;
    if (present) {
        if (isRecovery) return dark ? QColor(0x64, 0xB5, 0xF6) : QColor(0x21, 0x96, 0xF3);
        else            return dark ? QColor(0x81, 0xC7, 0x84) : QColor(0x4C, 0xAF, 0x50);
    } else {
        if (isRecovery) return dark ? QColor(0x75, 0x75, 0x75) : QColor(0x9E, 0x9E, 0x9E);
        else            return dark ? QColor(0xEF, 0x53, 0x50) : QColor(0xF4, 0x43, 0x36);
    }
}

void ChunkGridWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    for (const auto& c : m_cells) {
        p.fillRect(c.rect, cellColor(c.isRecovery, c.present));
    }
}

void ChunkGridWidget::mouseMoveEvent(QMouseEvent* ev) {
    for (const auto& c : m_cells) {
        if (c.rect.contains(ev->pos())) {
            QString label = c.isRecovery
                ? QString("Recovery chunk %1 — %2").arg(c.index).arg(c.present ? "present" : "missing")
                : QString("Data chunk %1 — %2").arg(c.index).arg(c.present ? "present" : "missing");
            QToolTip::showText(ev->globalPosition().toPoint(), label, this);
            return;
        }
    }
    QToolTip::hideText();
}

bool ChunkGridWidget::event(QEvent* e) {
    if (e->type() == QEvent::Resize) rebuildLayout();
    return QWidget::event(e);
}
