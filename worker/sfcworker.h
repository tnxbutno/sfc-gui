#pragma once

#include <QObject>
#include <QByteArray>
#include <QStringList>
#include <QString>
#include <vector>

#include "sfc/encoder.h"
#include "sfc/decoder.h"

class SfcWorker : public QObject {
    Q_OBJECT
public:
    explicit SfcWorker(QObject* parent = nullptr);

signals:
    void progress(int percent);
    void encodeFinished(QStringList outputPaths);
    void decodeFinished(sfc::ReassemblyResult result, QString innerFilename);
    void repairFinished(sfc::ReassemblyResult result, QString innerFilename,
                        quint64 fullSize, quint32 n, quint32 m);
    void verifyFinished(QString filePath, sfc::ReassemblyResult result);
    void error(QString detail);

public slots:
    void runEncode(QByteArray content, sfc::EncodeParams params,
                   QString outputPath, uint32_t numSegments);
    void runEncodeDir(QStringList inputPaths, QStringList relativePaths,
                      sfc::EncodeParams params, QString outputPath,
                      uint32_t numSegments);
    void runDecode(QList<QByteArray> fileBytes);
    void runRepair(QList<QByteArray> fileBytes);
    void runVerify(QString filePath, QByteArray fileBytes);
};
