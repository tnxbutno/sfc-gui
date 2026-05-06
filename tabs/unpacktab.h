#pragma once

#include <QWidget>
#include <QThread>
#include "sfc/decoder.h"

class DropZone;
class ChunkGridWidget;
class QListWidget;
class QLabel;
class QRadioButton;
class QLineEdit;
class QPushButton;
class QProgressBar;
class SfcWorker;

class UnpackTab : public QWidget {
    Q_OBJECT
public:
    explicit UnpackTab(QWidget* parent = nullptr);
    ~UnpackTab() override;

signals:
    void statusMessage(QString msg);
    void doDecode(QList<QByteArray> fileBytes);

private slots:
    void onFilesSelected(QStringList paths);
    void onRemoveSelected();
    void onClearAll();
    void onOutputBrowse();
    void onUnpack();
    void onProgress(int pct);
    void onDecodeFinished(sfc::ReassemblyResult result, QString innerFilename);
    void onError(QString detail);

private:
    void updateChunkGrid();

    DropZone*       m_dropZone    = nullptr;
    QListWidget*    m_fileList    = nullptr;
    QPushButton*    m_removeBtn   = nullptr;
    QPushButton*    m_clearBtn    = nullptr;
    ChunkGridWidget* m_chunkGrid  = nullptr;
    QLabel*         m_chunkLabel  = nullptr;
    QRadioButton*   m_autoOutput  = nullptr;
    QRadioButton*   m_chooseOutput = nullptr;
    QLineEdit*      m_outputEdit  = nullptr;
    QPushButton*    m_outputBtn   = nullptr;
    QPushButton*    m_unpackBtn   = nullptr;
    QProgressBar*   m_progress    = nullptr;

    QStringList m_filePaths;
    QThread*    m_thread = nullptr;
    SfcWorker*  m_worker = nullptr;
};
