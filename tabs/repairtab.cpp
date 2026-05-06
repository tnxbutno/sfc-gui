#include "repairtab.h"

#include "widgets/dropzone.h"
#include "widgets/chunkgridwidget.h"
#include "worker/sfcworker.h"

#include "sfc/global_header.h"

#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QThread>
#include <QUrl>
#include <QVBoxLayout>

#include <span>

RepairTab::RepairTab(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(10);

    root->addWidget(new QLabel("Input files (one file or multiple P2 segments)", this));
    m_dropZone = new DropZone(DropZone::Mode::MultiFile, this);
    root->addWidget(m_dropZone);

    auto* listRow = new QHBoxLayout;
    m_fileList  = new QListWidget(this);
    m_fileList->setMaximumHeight(100);
    listRow->addWidget(m_fileList, 1);
    auto* btnCol = new QVBoxLayout;
    m_removeBtn = new QPushButton("Remove", this);
    m_clearBtn  = new QPushButton("Clear",  this);
    btnCol->addWidget(m_removeBtn);
    btnCol->addWidget(m_clearBtn);
    btnCol->addStretch();
    listRow->addLayout(btnCol);
    root->addLayout(listRow);

    // Chunk availability
    auto* chunkBox    = new QGroupBox("Chunk availability", this);
    auto* chunkLayout = new QVBoxLayout(chunkBox);
    m_chunkGrid  = new ChunkGridWidget(this);
    m_chunkLabel = new QLabel(this);
    chunkLayout->addWidget(m_chunkGrid);
    chunkLayout->addWidget(m_chunkLabel);
    root->addWidget(chunkBox);

    // Recovery status (shown after repair attempt)
    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setVisible(false);
    root->addWidget(m_statusLabel);

    // Output
    auto* outBox    = new QGroupBox("Output", this);
    auto* outLayout = new QVBoxLayout(outBox);
    m_autoOutput   = new QRadioButton("Auto (next to input file)", this);
    m_chooseOutput = new QRadioButton("Choose folder:", this);
    m_autoOutput->setChecked(true);
    auto* outRow = new QHBoxLayout;
    m_outputEdit = new QLineEdit(this);
    m_outputBtn  = new QPushButton("Browse…", this);
    m_outputEdit->setEnabled(false);
    m_outputBtn->setEnabled(false);
    outRow->addWidget(m_outputEdit, 1);
    outRow->addWidget(m_outputBtn);
    outLayout->addWidget(m_autoOutput);
    outLayout->addWidget(m_chooseOutput);
    outLayout->addLayout(outRow);
    root->addWidget(outBox);

    m_repairBtn = new QPushButton("Repair", this);
    m_progress  = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setVisible(false);
    root->addWidget(m_repairBtn, 0, Qt::AlignRight);
    root->addWidget(m_progress);
    root->addStretch();

    m_thread = new QThread(this);
    m_worker = new SfcWorker;
    m_worker->moveToThread(m_thread);
    m_thread->start();

    connect(m_dropZone,    &DropZone::filesSelected,  this, &RepairTab::onFilesSelected);
    connect(m_removeBtn,   &QPushButton::clicked,     this, &RepairTab::onRemoveSelected);
    connect(m_clearBtn,    &QPushButton::clicked,     this, &RepairTab::onClearAll);
    connect(m_outputBtn,   &QPushButton::clicked,     this, &RepairTab::onOutputBrowse);
    connect(m_repairBtn,   &QPushButton::clicked,     this, &RepairTab::onRepair);
    connect(m_chooseOutput, &QRadioButton::toggled,   m_outputEdit, &QLineEdit::setEnabled);
    connect(m_chooseOutput, &QRadioButton::toggled,   m_outputBtn,  &QPushButton::setEnabled);

    connect(this,     &RepairTab::doRepair,          m_worker, &SfcWorker::runRepair);
    connect(m_worker, &SfcWorker::progress,          this,     &RepairTab::onProgress);
    connect(m_worker, &SfcWorker::repairFinished,    this,     &RepairTab::onRepairFinished);
    connect(m_worker, &SfcWorker::error,             this,     &RepairTab::onError);
}

RepairTab::~RepairTab() {
    m_thread->quit();
    m_thread->wait();
    delete m_worker;
}

void RepairTab::onFilesSelected(QStringList paths) {
    for (const auto& p : paths) {
        if (!m_filePaths.contains(p)) {
            m_filePaths.append(p);
            m_fileList->addItem(QFileInfo(p).fileName());
        }
    }
    m_statusLabel->setVisible(false);
    updateChunkGrid();
}

void RepairTab::onRemoveSelected() {
    const auto rows = m_fileList->selectedItems();
    for (auto* item : rows) {
        int row = m_fileList->row(item);
        m_filePaths.removeAt(row);
        delete m_fileList->takeItem(row);
    }
    updateChunkGrid();
}

void RepairTab::onClearAll() {
    m_filePaths.clear();
    m_fileList->clear();
    m_chunkGrid->clear();
    m_chunkLabel->clear();
    m_statusLabel->setVisible(false);
}

void RepairTab::onOutputBrowse() {
    QString dir = QFileDialog::getExistingDirectory(this, "Output folder");
    if (!dir.isEmpty()) m_outputEdit->setText(dir);
}

void RepairTab::updateChunkGrid() {
    if (m_filePaths.isEmpty()) { m_chunkGrid->clear(); m_chunkLabel->clear(); return; }

    QFile f(m_filePaths[0]);
    if (!f.open(QIODevice::ReadOnly)) return;
    QByteArray raw = f.readAll();
    auto span = std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(raw.constData()),
        static_cast<size_t>(raw.size()));
    auto hdr = sfc::parse_global_header(span);
    if (!hdr) return;

    const uint32_t n = hdr->n;
    const uint32_t m = hdr->m;
    m_chunkGrid->setChunkInfo(n, m,
        std::vector<bool>(n, true),
        std::vector<bool>(m, true));
    m_chunkLabel->setText(
        QString("N=%1  M=%2  (availability known after repair)").arg(n).arg(m));
}

void RepairTab::onRepair() {
    if (m_filePaths.isEmpty()) {
        QMessageBox::warning(this, "No input", "Add at least one .sfc file.");
        return;
    }

    QList<QByteArray> fileBytes;
    for (const auto& path : m_filePaths) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, "Error", "Cannot read: " + path); return;
        }
        fileBytes.append(f.readAll());
    }

    m_repairBtn->setEnabled(false);
    m_progress->setValue(0);
    m_progress->setVisible(true);
    m_statusLabel->setVisible(false);
    emit doRepair(fileBytes);
}

void RepairTab::onProgress(int pct) { m_progress->setValue(pct); }

void RepairTab::onRepairFinished(sfc::ReassemblyResult result, QString innerFilename,
                                  quint64 fullSize, quint32 n, quint32 m) {
    m_repairBtn->setEnabled(true);
    m_progress->setVisible(false);

    const bool partial = (result.status == sfc::ReassemblyStatus::Partial);

    // Update chunk grid with actual missing chunk data.
    if (n > 0) {
        std::vector<bool> dataPresent(n, true);
        std::vector<bool> recPresent(m, true);
        for (uint32_t idx : result.missing_chunks) {
            if (idx < n)      dataPresent[idx]     = false;
            else if (idx < n + m) recPresent[idx - n] = false;
        }
        m_chunkGrid->setChunkInfo(n, m, dataPresent, recPresent);

        const auto dataOk = static_cast<uint32_t>(
            std::count(dataPresent.begin(), dataPresent.end(), true));
        const auto recOk = static_cast<uint32_t>(
            std::count(recPresent.begin(), recPresent.end(), true));
        m_chunkLabel->setText(
            QString("N=%1  M=%2  Data: %3/%4  Recovery: %5/%6")
                .arg(n).arg(m).arg(dataOk).arg(n).arg(recOk).arg(m));
    }

    // Build status text.
    if (partial) {
        QString missing;
        for (uint32_t idx : result.missing_chunks)
            missing += QString::number(idx) + " ";
        m_statusLabel->setText(
            QString("<b>Partial recovery</b> — %1 of %2 bytes recovered.<br>"
                    "Missing chunks: %3")
                .arg(result.content.size())
                .arg(fullSize)
                .arg(missing.trimmed()));
        m_statusLabel->setStyleSheet("color: #b85c00;");
    } else {
        m_statusLabel->setText("<b>Full recovery</b> — all data recovered.");
        m_statusLabel->setStyleSheet("color: #2a7a2a;");
    }
    m_statusLabel->setVisible(true);

    // Write output.
    QString outDir = m_chooseOutput->isChecked()
        ? m_outputEdit->text()
        : QFileInfo(m_filePaths[0]).absolutePath();
    if (outDir.isEmpty()) outDir = QFileInfo(m_filePaths[0]).absolutePath();

    QString fname = innerFilename.isEmpty() ? "repaired.bin" : innerFilename;
    if (partial) fname = "partial_" + fname;
    QString outPath = outDir + "/" + fname;

    QFile out(outPath);
    if (!out.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Error", "Cannot write: " + outPath); return;
    }
    out.write(reinterpret_cast<const char*>(result.content.data()),
              static_cast<qint64>(result.content.size()));

    const QString statusMsg = partial
        ? QString("Partial repair → %1 (%2/%3 bytes)")
              .arg(outPath).arg(result.content.size()).arg(fullSize)
        : "Repaired → " + outPath;
    emit statusMessage(statusMsg);

    auto* box = new QMessageBox(this);
    box->setWindowTitle(partial ? "Partial recovery" : "Repaired");
    box->setIcon(partial ? QMessageBox::Warning : QMessageBox::Information);
    box->setText(partial
        ? QString("Partially recovered %1 of %2 bytes.\nSaved: %3")
              .arg(result.content.size()).arg(fullSize).arg(outPath)
        : "Fully recovered.\nSaved: " + outPath);
    auto* openBtn = box->addButton("Open folder", QMessageBox::ActionRole);
    box->addButton(QMessageBox::Ok);
    box->exec();
    if (box->clickedButton() == openBtn)
        QDesktopServices::openUrl(QUrl::fromLocalFile(outDir));
}

void RepairTab::onError(QString detail) {
    m_repairBtn->setEnabled(true);
    m_progress->setVisible(false);
    QMessageBox::critical(this, "Repair failed", detail);
    emit statusMessage("Error: " + detail);
}
