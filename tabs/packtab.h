#pragma once

#include <QWidget>
#include <QThread>
#include "sfc/encoder.h"

class DropZone;
class MetadataEditor;
class QLineEdit;
class QSpinBox;
class QCheckBox;
class QRadioButton;
class QGroupBox;
class QPushButton;
class QProgressBar;
class SfcWorker;

class PackTab : public QWidget {
    Q_OBJECT
public:
    explicit PackTab(QWidget* parent = nullptr);
    ~PackTab() override;

signals:
    void statusMessage(QString msg);
    void doEncode(QByteArray content, sfc::EncodeParams params,
                  QString outputPath, uint32_t segments);
    void doEncodeDir(QStringList inputPaths, QStringList relativePaths,
                     sfc::EncodeParams params, QString outputPath,
                     uint32_t segments);

private slots:
    void onInputSelected(QStringList paths);
    void onOutputBrowse();
    void onPack();
    void onProgress(int pct);
    void onEncodeFinished(QStringList outputPaths);
    void onError(QString detail);

private:
    DropZone*       m_dropZone    = nullptr;
    QLineEdit*      m_outputEdit  = nullptr;
    QPushButton*    m_outputBtn   = nullptr;
    QCheckBox*      m_splitCheck  = nullptr;
    QSpinBox*       m_segmentsSpin = nullptr;
    MetadataEditor* m_meta        = nullptr;
    QPushButton*    m_advToggle   = nullptr;
    QWidget*        m_advContent  = nullptr;
    QRadioButton*   m_algoZstd   = nullptr;
    QRadioButton*   m_algoBrotli = nullptr;
    QRadioButton*   m_algoLz4   = nullptr;
    QRadioButton*   m_algoNone  = nullptr;
    QSpinBox*       m_recoverySpin = nullptr;
    QSpinBox*       m_chunkSpin  = nullptr;
    QPushButton*    m_packBtn    = nullptr;
    QProgressBar*   m_progress   = nullptr;

    QThread*    m_thread = nullptr;
    SfcWorker*  m_worker = nullptr;

    QString     m_inputPath;
    bool        m_inputIsDir = false;
};
