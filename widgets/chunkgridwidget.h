#pragma once

#include <QWidget>
#include <vector>
#include <cstdint>

class ChunkGridWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChunkGridWidget(QWidget* parent = nullptr);

    void setChunkInfo(uint32_t n, uint32_t m,
                      const std::vector<bool>& dataPresent,
                      const std::vector<bool>& recoveryPresent);
    void clear();

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    bool event(QEvent*) override;

private:
    struct ChunkCell {
        QRect   rect;
        bool    isRecovery;
        bool    present;
        uint32_t index;
    };

    void rebuildLayout();
    QColor cellColor(bool isRecovery, bool present) const;

    uint32_t            m_n = 0;
    uint32_t            m_m = 0;
    std::vector<bool>   m_dataPresent;
    std::vector<bool>   m_recoveryPresent;
    std::vector<ChunkCell> m_cells;

    static constexpr int kCellSize = 14;
    static constexpr int kGap      = 2;
};
