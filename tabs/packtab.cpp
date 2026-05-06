#include "packtab.h"

#include "widgets/dropzone.h"
#include "widgets/metadataeditor.h"
#include "worker/sfcworker.h"

#include "sfc/encoder.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QThread>
#include <QUrl>
#include <QVBoxLayout>

#include <QUuid>

#include <chrono>
#include <filesystem>
#include <cstdint>

static sfc::FileUuid randomUuid() {
    sfc::FileUuid uuid;
    const QByteArray ba = QUuid::createUuid().toRfc4122();
    std::copy(ba.begin(), ba.end(), uuid.bytes.begin());
    return uuid;
}

PackTab::PackTab(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(10);

    // Input drop zone
    m_dropZone = new DropZone(DropZone::Mode::FileOrDirectory, this);
    root->addWidget(new QLabel("Input", this));
    root->addWidget(m_dropZone);

    // Output row
    auto* outRow = new QHBoxLayout;
    m_outputEdit = new QLineEdit(this);
    m_outputEdit->setPlaceholderText("Output path (default: <input>.sfc)");
    m_outputBtn  = new QPushButton("Browse\u2026", this);
    outRow->addWidget(new QLabel("Output:", this));
    outRow->addWidget(m_outputEdit, 1);
    outRow->addWidget(m_outputBtn);
    root->addLayout(outRow);

    // Split option
    auto* splitRow = new QHBoxLayout;
    m_splitCheck   = new QCheckBox("Split into segments", this);
    m_segmentsSpin = new QSpinBox(this);
    m_segmentsSpin->setRange(2, 9999);
    m_segmentsSpin->setValue(4);
    m_segmentsSpin->setEnabled(false);
    m_segmentsSpin->setPrefix("Count: ");
    splitRow->addWidget(m_splitCheck);
    splitRow->addWidget(m_segmentsSpin);
    splitRow->addStretch();
    root->addLayout(splitRow);

    // Metadata
    m_meta = new MetadataEditor(this);
    root->addWidget(m_meta);

    // Advanced toggle button
    m_advToggle = new QPushButton("\u25b6  Advanced", this);
    m_advToggle->setFlat(true);
    m_advToggle->setCheckable(true);
    m_advToggle->setCursor(Qt::PointingHandCursor);
    root->addWidget(m_advToggle, 0, Qt::AlignLeft);

    // Advanced content (hidden by default)
    m_advContent = new QWidget(this);
    m_advContent->setVisible(false);
    auto* advForm = new QFormLayout(m_advContent);
    advForm->setContentsMargins(16, 4, 0, 4);

    auto* algoRow = new QHBoxLayout;
    m_algoZstd   = new QRadioButton("zstd",   m_advContent);  m_algoZstd->setChecked(true);
    m_algoBrotli = new QRadioButton("brotli", m_advContent);
    m_algoLz4    = new QRadioButton("lz4",    m_advContent);
    m_algoNone   = new QRadioButton("none",   m_advContent);
    algoRow->addWidget(m_algoZstd);
    algoRow->addWidget(m_algoBrotli);
    algoRow->addWidget(m_algoLz4);
    algoRow->addWidget(m_algoNone);
    algoRow->addStretch();
    advForm->addRow("Compression:", algoRow);

    m_recoverySpin = new QSpinBox(m_advContent);
    m_recoverySpin->setRange(0, 255);
    m_recoverySpin->setValue(0);
    m_recoverySpin->setToolTip("Chunks that can be lost and still reconstruct");
    advForm->addRow("Recovery chunks M:", m_recoverySpin);

    m_chunkSpin = new QSpinBox(m_advContent);
    m_chunkSpin->setRange(512, 1 << 20);
    m_chunkSpin->setSingleStep(512);
    m_chunkSpin->setValue(65536);
    m_chunkSpin->setToolTip("Must be even, in bytes");
    advForm->addRow("Chunk size S:", m_chunkSpin);

    root->addWidget(m_advContent);

    // Pack button + progress
    m_packBtn  = new QPushButton("Pack", this);
    m_progress = new QProgressBar(this);
    m_progress->setVisible(false);
    m_progress->setRange(0, 100);
    root->addWidget(m_packBtn, 0, Qt::AlignRight);
    root->addWidget(m_progress);
    root->addStretch();

    // Worker thread
    m_thread = new QThread(this);
    m_worker = new SfcWorker;
    m_worker->moveToThread(m_thread);
    m_thread->start();

    connect(m_splitCheck, &QCheckBox::toggled, m_segmentsSpin, &QSpinBox::setEnabled);
    connect(m_outputBtn,  &QPushButton::clicked, this, &PackTab::onOutputBrowse);
    connect(m_packBtn,    &QPushButton::clicked, this, &PackTab::onPack);
    connect(m_dropZone,   &DropZone::filesSelected, this, &PackTab::onInputSelected);
    connect(m_advToggle,  &QPushButton::toggled, this, [this](bool checked) {
        m_advContent->setVisible(checked);
        m_advToggle->setText(checked ? "\u25bc  Advanced" : "\u25b6  Advanced");
    });

    connect(this,     &PackTab::doEncode,    m_worker, &SfcWorker::runEncode);
    connect(this,     &PackTab::doEncodeDir, m_worker, &SfcWorker::runEncodeDir);
    connect(m_worker, &SfcWorker::progress,  this,     &PackTab::onProgress);
    connect(m_worker, &SfcWorker::encodeFinished, this, &PackTab::onEncodeFinished);
    connect(m_worker, &SfcWorker::error,     this,     &PackTab::onError);

}

PackTab::~PackTab() {
    m_thread->quit();
    m_thread->wait();
    delete m_worker;
}

void PackTab::onInputSelected(QStringList paths) {
    if (paths.isEmpty()) return;
    m_inputPath  = paths[0];
    m_inputIsDir = QFileInfo(m_inputPath).isDir();

    if (m_outputEdit->text().isEmpty()) {
        QString base = m_inputPath;
        if (base.endsWith('/') || base.endsWith('\\'))
            base.chop(1);
        m_outputEdit->setText(base + ".sfc");
    }
}

void PackTab::onOutputBrowse() {
    QString path = QFileDialog::getSaveFileName(this, "Save as", {}, "SFC (*.sfc)");
    if (!path.isEmpty()) m_outputEdit->setText(path);
}

void PackTab::onPack() {
    if (m_inputPath.isEmpty()) {
        QMessageBox::warning(this, "No input", "Please select a file or folder to pack.");
        return;
    }
    QString outputPath = m_outputEdit->text().trimmed();
    if (outputPath.isEmpty()) outputPath = m_inputPath + ".sfc";

    sfc::CompressionAlgo algo = sfc::CompressionAlgo::Zstd;
    if (m_algoBrotli->isChecked()) algo = sfc::CompressionAlgo::Brotli;
    else if (m_algoLz4->isChecked()) algo = sfc::CompressionAlgo::Lz4Frame;
    else if (m_algoNone->isChecked()) algo = sfc::CompressionAlgo::Identity;

    const uint64_t ts = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    sfc::EncodeParams params{
        .m         = static_cast<uint32_t>(m_recoverySpin->value()),
        .s         = static_cast<uint32_t>(m_chunkSpin->value()),
        .algo      = algo,
        .uuid      = randomUuid(),
        .timestamp = ts,
        .format_id = 0x0001,
        .filename  = QFileInfo(m_inputPath).fileName().toStdString(),
        .metadata  = m_meta->metadata(),
    };

    const uint32_t segs = m_splitCheck->isChecked()
        ? static_cast<uint32_t>(m_segmentsSpin->value()) : 1;

    m_packBtn->setEnabled(false);
    m_progress->setValue(0);
    m_progress->setVisible(true);

    if (m_inputIsDir) {
        QStringList inputPaths, relativePaths;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                m_inputPath.toStdString())) {
            if (!entry.is_regular_file()) continue;
            inputPaths  << QString::fromStdString(entry.path().string());
            relativePaths << QString::fromStdString(
                std::filesystem::relative(entry.path(), m_inputPath.toStdString()).generic_string());
        }
        emit doEncodeDir(inputPaths, relativePaths, params, outputPath, segs);
    } else {
        QFile f(m_inputPath);
        if (!f.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, "Error", "Cannot read: " + m_inputPath);
            m_packBtn->setEnabled(true);
            m_progress->setVisible(false);
            return;
        }
        emit doEncode(f.readAll(), params, outputPath, segs);
    }
}

void PackTab::onProgress(int pct) { m_progress->setValue(pct); }

void PackTab::onEncodeFinished(QStringList outputPaths) {
    m_packBtn->setEnabled(true);
    m_progress->setVisible(false);
    if (outputPaths.isEmpty()) return;
    emit statusMessage("Packed \u2192 " + outputPaths[0]);

    auto* box = new QMessageBox(this);
    box->setWindowTitle("Packed successfully");
    box->setText(outputPaths.size() == 1
        ? "Saved: " + outputPaths[0]
        : QString("Saved %1 segments").arg(outputPaths.size()));
    auto* openBtn = box->addButton("Open folder", QMessageBox::ActionRole);
    box->addButton(QMessageBox::Ok);
    box->exec();
    if (box->clickedButton() == openBtn) {
        QDesktopServices::openUrl(
            QUrl::fromLocalFile(QFileInfo(outputPaths[0]).absolutePath()));
    }
}

void PackTab::onError(QString detail) {
    m_packBtn->setEnabled(true);
    m_progress->setVisible(false);
    QMessageBox::critical(this, "Pack failed", detail);
    emit statusMessage("Error: " + detail);
}
