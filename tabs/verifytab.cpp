#include "verifytab.h"

#include "widgets/dropzone.h"
#include "worker/sfcworker.h"

#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>

VerifyTab::VerifyTab(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(10);

    root->addWidget(new QLabel("Input files", this));
    m_dropZone = new DropZone(DropZone::Mode::MultiFile, this);
    root->addWidget(m_dropZone);

    auto* btnRow = new QHBoxLayout;
    m_clearBtn  = new QPushButton("Clear",      this);
    m_verifyBtn = new QPushButton("Verify All", this);
    btnRow->addWidget(m_clearBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_verifyBtn);
    root->addLayout(btnRow);

    root->addWidget(new QLabel("Results", this));
    m_resultList = new QListWidget(this);
    m_resultList->setAlternatingRowColors(true);
    root->addWidget(m_resultList, 1);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setVisible(false);
    root->addWidget(m_progress);

    m_thread = new QThread(this);
    m_worker = new SfcWorker;
    m_worker->moveToThread(m_thread);
    m_thread->start();

    connect(m_dropZone,  &DropZone::filesSelected,  this,     &VerifyTab::onFilesSelected);
    connect(m_clearBtn,  &QPushButton::clicked,      this,     &VerifyTab::onClear);
    connect(m_verifyBtn, &QPushButton::clicked,      this,     &VerifyTab::onVerifyAll);

    connect(this,     &VerifyTab::doVerify,          m_worker, &SfcWorker::runVerify);
    connect(m_worker, &SfcWorker::progress,          this,     &VerifyTab::onProgress);
    connect(m_worker, &SfcWorker::verifyFinished,    this,     &VerifyTab::onVerifyFinished);
    connect(m_worker, &SfcWorker::error,             this,     &VerifyTab::onError);
}

VerifyTab::~VerifyTab() {
    m_thread->quit();
    m_thread->wait();
    delete m_worker;
}

void VerifyTab::onFilesSelected(QStringList paths) {
    for (const auto& p : paths)
        if (!m_filePaths.contains(p)) m_filePaths.append(p);
}

void VerifyTab::onClear() {
    m_filePaths.clear();
    m_resultList->clear();
    m_dropZone->clear();
    m_progress->setVisible(false);
}

void VerifyTab::onVerifyAll() {
    if (m_filePaths.isEmpty()) {
        QMessageBox::warning(this, "No files", "Add .sfc files to verify.");
        return;
    }
    m_resultList->clear();
    m_pendingCount = m_filePaths.size();
    m_okCount      = 0;
    m_verifyBtn->setEnabled(false);
    m_progress->setValue(0);
    m_progress->setVisible(true);

    // Fire verification tasks sequentially by chaining through the worker
    // Each call is non-blocking on the UI thread (runs in m_thread).
    for (const auto& path : m_filePaths) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            onError(path + ": cannot read file");
            continue;
        }
        emit doVerify(path, f.readAll());
    }
}

void VerifyTab::onProgress(int pct) { m_progress->setValue(pct); }

void VerifyTab::onVerifyFinished(QString filePath, sfc::ReassemblyResult result) {
    --m_pendingCount;

    const bool ok = (result.status == sfc::ReassemblyStatus::FullyVerified ||
                     result.status == sfc::ReassemblyStatus::ContentVerified);
    if (ok) ++m_okCount;

    QString statusStr;
    switch (result.status) {
    case sfc::ReassemblyStatus::FullyVerified:   statusStr = "\u2713 FullyVerified";   break;
    case sfc::ReassemblyStatus::ContentVerified: statusStr = "\u2713 ContentVerified"; break;
    default:                                     statusStr = "\u2717 Partial/Failed";  break;
    }

    QString missing;
    if (!result.missing_chunks.empty())
        missing = QString("  missing chunks: %1").arg(result.missing_chunks.size());

    auto* item = new QListWidgetItem(
        QFileInfo(filePath).fileName() + "   " + statusStr + missing);
    item->setForeground(ok ? Qt::darkGreen : Qt::red);
    m_resultList->addItem(item);

    if (m_pendingCount == 0) {
        m_verifyBtn->setEnabled(true);
        m_progress->setVisible(false);
        const int total = m_filePaths.size();
        emit statusMessage(
            QString("Verify: %1/%2 OK").arg(m_okCount).arg(total));
    }
}

void VerifyTab::onError(QString detail) {
    --m_pendingCount;
    auto* item = new QListWidgetItem("\u2717 " + detail);
    item->setForeground(Qt::red);
    m_resultList->addItem(item);

    if (m_pendingCount == 0) {
        m_verifyBtn->setEnabled(true);
        m_progress->setVisible(false);
    }
    emit statusMessage("Error: " + detail);
}
