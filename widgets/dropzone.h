#pragma once

#include <QWidget>
#include <QStringList>

class QLabel;
class QPushButton;

class DropZone : public QWidget {
    Q_OBJECT
public:
    enum class Mode { SingleFile, MultiFile, FileOrDirectory };

    explicit DropZone(Mode mode, QWidget* parent = nullptr);

    QStringList files() const { return m_files; }
    void clear();

signals:
    void filesSelected(QStringList paths);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onBrowse();

private:
    void addFiles(const QStringList& paths);
    void updateLabel();
    void setHighlighted(bool on);

    Mode        m_mode;
    QStringList m_files;
    QLabel*     m_label  = nullptr;
    QPushButton* m_browse = nullptr;
    bool        m_highlighted = false;
};
