#include "dropzone.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

DropZone::DropZone(Mode mode, QWidget* parent)
    : QWidget(parent), m_mode(mode) {
    setAcceptDrops(true);
    setMinimumHeight(72);

    m_label = new QLabel(this);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setWordWrap(true);

    m_browse = new QPushButton("Browse\u2026", this);
    m_browse->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(m_browse, &QPushButton::clicked, this, &DropZone::onBrowse);

    auto* vbox = new QVBoxLayout(this);
    vbox->addWidget(m_label);
    vbox->addWidget(m_browse, 0, Qt::AlignCenter);
    vbox->setContentsMargins(8, 8, 8, 8);

    updateLabel();
}

void DropZone::clear() {
    m_files.clear();
    updateLabel();
}

void DropZone::onBrowse() {
    QStringList paths;
    if (m_mode == Mode::SingleFile) {
        QString path = QFileDialog::getOpenFileName(this, "Select file", {},
                                                    "SFC files (*.sfc);;All files (*)");
        if (!path.isEmpty()) paths << path;
    } else if (m_mode == Mode::MultiFile) {
        paths = QFileDialog::getOpenFileNames(this, "Select files", {},
                                              "SFC files (*.sfc);;All files (*)");
    } else {
        // FileOrDirectory — native dialog; user can select a folder or a file.
        // We try directory first; if cancelled we stop (do not cascade to file dialog).
        QFileDialog dialog(this, "Select file or folder");
        dialog.setFileMode(QFileDialog::Directory);
        dialog.setOption(QFileDialog::ShowDirsOnly, false);  // also show files
        if (dialog.exec() == QDialog::Accepted)
            paths = dialog.selectedFiles();
    }
    if (!paths.isEmpty()) addFiles(paths);
}

void DropZone::addFiles(const QStringList& paths) {
    if (m_mode == Mode::SingleFile || m_mode == Mode::FileOrDirectory) {
        m_files = paths.mid(0, 1);
    } else {
        for (const auto& p : paths)
            if (!m_files.contains(p)) m_files.append(p);
    }
    updateLabel();
    emit filesSelected(m_files);
}

void DropZone::updateLabel() {
    if (m_files.isEmpty()) {
        switch (m_mode) {
        case Mode::SingleFile:
            m_label->setText("Drag & drop a .sfc file here, or click Browse"); break;
        case Mode::MultiFile:
            m_label->setText("Drag & drop .sfc files here, or click Browse"); break;
        case Mode::FileOrDirectory:
            m_label->setText("Drag & drop a file or folder here, or click Browse"); break;
        }
    } else if (m_files.size() == 1) {
        m_label->setText(QFileInfo(m_files[0]).fileName());
    } else {
        m_label->setText(QString("%1 files selected").arg(m_files.size()));
    }
}

void DropZone::setHighlighted(bool on) {
    m_highlighted = on;
    update();
}

void DropZone::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QColor border = m_highlighted
        ? palette().color(QPalette::Highlight)
        : palette().color(QPalette::Mid);
    QColor bg = m_highlighted
        ? palette().color(QPalette::Highlight).lighter(170)
        : palette().color(QPalette::AlternateBase);

    p.fillRect(rect(), bg);

    QPen pen(border, 2, Qt::DashLine);
    p.setPen(pen);
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 6, 6);
}

void DropZone::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        setHighlighted(true);
        m_label->setText("Release to add");
    }
}

void DropZone::dragLeaveEvent(QDragLeaveEvent*) {
    setHighlighted(false);
    updateLabel();
}

void DropZone::dropEvent(QDropEvent* event) {
    setHighlighted(false);
    QStringList paths;
    for (const auto& url : event->mimeData()->urls())
        paths << url.toLocalFile();
    if (!paths.isEmpty()) addFiles(paths);
}
