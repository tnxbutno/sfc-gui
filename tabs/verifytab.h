#pragma once

#include <QWidget>
#include <QThread>
#include "sfc/decoder.h"

class DropZone;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QProgressBar;
class SfcWorker;

class VerifyTab : public QWidget {
    Q_OBJECT
public:
    explicit VerifyTab(QWidget* parent = nullptr);
    ~VerifyTab() override;

signals:
    void statusMessage(QString msg);
    void doVerify(QString filePath, QByteArray fileBytes);

private slots:
    void onFilesSelected(QStringList paths);
    void onClear();
    void onVerifyAll();
    void onProgress(int pct);
    void onVerifyFinished(QString filePath, sfc::ReassemblyResult result);
    void onError(QString detail);

private:
    DropZone*     m_dropZone   = nullptr;
    QListWidget*  m_resultList = nullptr;
    QPushButton*  m_clearBtn   = nullptr;
    QPushButton*  m_verifyBtn  = nullptr;
    QProgressBar* m_progress   = nullptr;

    QStringList m_filePaths;
    int         m_pendingCount = 0;
    int         m_okCount      = 0;

    QThread*   m_thread = nullptr;
    SfcWorker* m_worker = nullptr;
};
