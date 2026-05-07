#include "unpacktab.h"

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

UnpackTab::UnpackTab(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(10);

    root->addWidget(new QLabel("Input files (one file or multiple split-transport segments)", this));
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
    auto* chunkBox = new QGroupBox("Chunk availability", this);
    auto* chunkLayout = new QVBoxLayout(chunkBox);
    m_chunkGrid  = new ChunkGridWidget(this);
    m_chunkLabel = new QLabel(this);
    chunkLayout->addWidget(m_chunkGrid);
    chunkLayout->addWidget(m_chunkLabel);
    root->addWidget(chunkBox);

    // Output
    auto* outBox    = new QGroupBox("Output", this);
    auto* outLayout = new QVBoxLayout(outBox);
    m_autoOutput    = new QRadioButton("Auto (next to input file)", this);
    m_chooseOutput  = new QRadioButton("Choose folder:", this);
    m_autoOutput->setChecked(true);
    auto* outRow = new QHBoxLayout;
    m_outputEdit = new QLineEdit(this);
    m_outputBtn  = new QPushButton("Browse\u2026", this);
    m_outputEdit->setEnabled(false);
    m_outputBtn->setEnabled(false);
    outRow->addWidget(m_outputEdit, 1);
    outRow->addWidget(m_outputBtn);
    outLayout->addWidget(m_autoOutput);
    outLayout->addWidget(m_chooseOutput);
    outLayout->addLayout(outRow);
    root->addWidget(outBox);

    m_unpackBtn = new QPushButton("Unpack", this);
    m_progress  = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setVisible(false);
    root->addWidget(m_unpackBtn, 0, Qt::AlignRight);
    root->addWidget(m_progress);
    root->addStretch();

    m_thread = new QThread(this);
    m_worker = new SfcWorker;
    m_worker->moveToThread(m_thread);
    m_thread->start();

    connect(m_dropZone,    &DropZone::filesSelected, this, &UnpackTab::onFilesSelected);
    connect(m_removeBtn,   &QPushButton::clicked,    this, &UnpackTab::onRemoveSelected);
    connect(m_clearBtn,    &QPushButton::clicked,    this, &UnpackTab::onClearAll);
    connect(m_outputBtn,   &QPushButton::clicked,    this, &UnpackTab::onOutputBrowse);
    connect(m_unpackBtn,   &QPushButton::clicked,    this, &UnpackTab::onUnpack);
    connect(m_chooseOutput, &QRadioButton::toggled,  m_outputEdit, &QLineEdit::setEnabled);
    connect(m_chooseOutput, &QRadioButton::toggled,  m_outputBtn,  &QPushButton::setEnabled);

    connect(this,     &UnpackTab::doDecode,         m_worker, &SfcWorker::runDecode);
    connect(m_worker, &SfcWorker::progress,         this,     &UnpackTab::onProgress);
    connect(m_worker, &SfcWorker::decodeFinished, this, &UnpackTab::onDecodeFinished);
    connect(m_worker, &SfcWorker::error,            this,     &UnpackTab::onError);
}

UnpackTab::~UnpackTab() {
    m_thread->quit();
    m_thread->wait();
    delete m_worker;
}

void UnpackTab::onFilesSelected(QStringList paths) {
    for (const auto& p : paths) {
        if (!m_filePaths.contains(p)) {
            m_filePaths.append(p);
            m_fileList->addItem(QFileInfo(p).fileName());
        }
    }
    updateChunkGrid();
}

void UnpackTab::onRemoveSelected() {
    auto rows = m_fileList->selectedItems();
    for (auto* item : rows) {
        int row = m_fileList->row(item);
        m_filePaths.removeAt(row);
        delete m_fileList->takeItem(row);
    }
    updateChunkGrid();
}

void UnpackTab::onClearAll() {
    m_filePaths.clear();
    m_fileList->clear();
    m_chunkGrid->clear();
    m_chunkLabel->clear();
}

void UnpackTab::onOutputBrowse() {
    QString dir = QFileDialog::getExistingDirectory(this, "Output folder");
    if (!dir.isEmpty()) m_outputEdit->setText(dir);
}

void UnpackTab::updateChunkGrid() {
    if (m_filePaths.isEmpty()) { m_chunkGrid->clear(); m_chunkLabel->clear(); return; }

    // Parse the first file's header to get N/M geometry
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
    std::vector<bool> dataPresent(n, true);   // assume all chunks in supplied files are present
    std::vector<bool> recPresent(m, true);

    m_chunkGrid->setChunkInfo(n, m, dataPresent, recPresent);

    uint32_t dataOk = static_cast<uint32_t>(std::count(dataPresent.begin(), dataPresent.end(), true));
    uint32_t recOk  = static_cast<uint32_t>(std::count(recPresent.begin(), recPresent.end(), true));
    m_chunkLabel->setText(
        QString("N=%1  M=%2  Data: %3/%4  Recovery: %5/%6")
            .arg(n).arg(m).arg(dataOk).arg(n).arg(recOk).arg(m));
}

void UnpackTab::onUnpack() {
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

    m_unpackBtn->setEnabled(false);
    m_progress->setValue(0);
    m_progress->setVisible(true);
    emit doDecode(fileBytes);
}

void UnpackTab::onProgress(int pct) { m_progress->setValue(pct); }

void UnpackTab::onDecodeFinished(sfc::ReassemblyResult result, QString innerFilename) {
    m_unpackBtn->setEnabled(true);
    m_progress->setVisible(false);

    QString outDir = m_chooseOutput->isChecked()
        ? m_outputEdit->text()
        : QFileInfo(m_filePaths[0]).absolutePath();

    if (outDir.isEmpty()) outDir = QFileInfo(m_filePaths[0]).absolutePath();

    QString fname = innerFilename.isEmpty() ? "output.bin" : innerFilename;
    QString outPath = outDir + "/" + fname;

    QFile out(outPath);
    if (!out.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Error", "Cannot write: " + outPath); return;
    }
    out.write(reinterpret_cast<const char*>(result.content.data()),
              static_cast<qint64>(result.content.size()));

    emit statusMessage("Unpacked \u2192 " + outPath);
    auto* box = new QMessageBox(this);
    box->setWindowTitle("Unpacked");
    box->setText("Saved: " + outPath);
    auto* openBtn = box->addButton("Open folder", QMessageBox::ActionRole);
    box->addButton(QMessageBox::Ok);
    box->exec();
    if (box->clickedButton() == openBtn)
        QDesktopServices::openUrl(QUrl::fromLocalFile(outDir));
}

void UnpackTab::onError(QString detail) {
    m_unpackBtn->setEnabled(true);
    m_progress->setVisible(false);
    QMessageBox::critical(this, "Unpack failed", detail);
    emit statusMessage("Error: " + detail);
}
