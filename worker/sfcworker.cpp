#include "sfcworker.h"

#include "sfc/split_encoder.h"
#include "sfc/split_decoder.h"
#include "sfc/directory.h"
#include "sfc/global_header.h"
#include "sfc/byte_utils.h"

#include <QFile>
#include <QFileInfo>
#include <span>
#include <cstdint>

SfcWorker::SfcWorker(QObject* parent) : QObject(parent) {}

void SfcWorker::runEncode(QByteArray content, sfc::EncodeParams params,
                           QString outputPath, uint32_t numSegments) {
    auto bytes = std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(content.constData()),
        static_cast<size_t>(content.size()));

    QStringList written;

    if (numSegments > 1) {
        auto result = sfc::encode_split(bytes, params, numSegments);
        if (!result) { emit error(QString::fromStdString(result.error().detail)); return; }

        const auto& segs = *result;
        int digits = (segs.size() >= 1000) ? 4 : (segs.size() >= 100) ? 3 : 3;
        for (size_t i = 0; i < segs.size(); ++i) {
            QString path = outputPath + QString(".%1.sfc").arg(
                static_cast<int>(i), digits, 10, QChar('0'));
            QFile f(path);
            if (!f.open(QIODevice::WriteOnly)) {
                emit error("Cannot write " + path); return;
            }
            f.write(reinterpret_cast<const char*>(segs[i].data()),
                    static_cast<qint64>(segs[i].size()));
            written.append(path);
            emit progress(static_cast<int>(100 * (i + 1) / segs.size()));
        }
    } else {
        auto result = sfc::encode(bytes, params);
        if (!result) { emit error(QString::fromStdString(result.error().detail)); return; }

        QString path = outputPath.endsWith(".sfc") ? outputPath : outputPath + ".sfc";
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) {
            emit error("Cannot write " + path); return;
        }
        f.write(reinterpret_cast<const char*>(result->data()),
                static_cast<qint64>(result->size()));
        written.append(path);
        emit progress(100);
    }

    emit encodeFinished(written);
}

void SfcWorker::runEncodeDir(QStringList inputPaths, QStringList relativePaths,
                              sfc::EncodeParams params, QString outputPath,
                              uint32_t numSegments) {
    std::vector<sfc::DirectoryInputFile> files;
    files.reserve(static_cast<size_t>(inputPaths.size()));

    for (int i = 0; i < inputPaths.size(); ++i) {
        QFile f(inputPaths[i]);
        if (!f.open(QIODevice::ReadOnly)) {
            emit error("Cannot read " + inputPaths[i]); return;
        }
        QByteArray raw = f.readAll();
        std::vector<uint8_t> content(
            reinterpret_cast<const uint8_t*>(raw.constData()),
            reinterpret_cast<const uint8_t*>(raw.constData()) + raw.size());

        files.push_back(sfc::DirectoryInputFile{
            .path      = relativePaths[i].toStdString(),
            .content   = std::move(content),
            .format_id = 0x0001,
        });
        emit progress(static_cast<int>(50 * (i + 1) / inputPaths.size()));
    }

    QStringList written;

    if (numSegments > 1) {
        auto result = sfc::encode_directory_split(std::move(files), params, numSegments);
        if (!result) { emit error(QString::fromStdString(result.error().detail)); return; }

        const auto& segs = *result;
        for (size_t i = 0; i < segs.size(); ++i) {
            QString path = outputPath + QString(".%1.sfc").arg(
                static_cast<int>(i), 3, 10, QChar('0'));
            QFile f(path);
            if (!f.open(QIODevice::WriteOnly)) {
                emit error("Cannot write " + path); return;
            }
            f.write(reinterpret_cast<const char*>(segs[i].data()),
                    static_cast<qint64>(segs[i].size()));
            written.append(path);
            emit progress(50 + static_cast<int>(50 * (i + 1) / segs.size()));
        }
    } else {
        auto result = sfc::encode_directory(std::move(files), params);
        if (!result) { emit error(QString::fromStdString(result.error().detail)); return; }

        QString path = outputPath.endsWith(".sfc") ? outputPath : outputPath + ".sfc";
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) {
            emit error("Cannot write " + path); return;
        }
        f.write(reinterpret_cast<const char*>(result->data()),
                static_cast<qint64>(result->size()));
        written.append(path);
        emit progress(100);
    }

    emit encodeFinished(written);
}

void SfcWorker::runDecode(QList<QByteArray> fileBytesList) {
    std::vector<std::vector<uint8_t>> files;
    files.reserve(static_cast<size_t>(fileBytesList.size()));
    for (const auto& ba : fileBytesList) {
        files.emplace_back(
            reinterpret_cast<const uint8_t*>(ba.constData()),
            reinterpret_cast<const uint8_t*>(ba.constData()) + ba.size());
    }

    sfc::Result<sfc::ReassemblyResult> result;
    if (files.size() == 1) {
        result = sfc::decode(std::span<const uint8_t>(files[0]));
    } else {
        auto multi = sfc::decode_split(std::span<const std::vector<uint8_t>>(files));
        if (multi) result = std::move(*multi);
        else result = std::unexpected(multi.error());
    }

    if (!result) { emit error(QString::fromStdString(result.error().detail)); return; }

    // Extract inner filename from header for the caller.
    // parse_global_header expects the header region bytes (after 8-byte preamble),
    // matching the layout used by parse_header_from_file in cli/utils.h.
    QString innerFilename;
    {
        auto span2 = std::span<const uint8_t>(files[0]);
        if (span2.size() >= 12) {
            const uint32_t h = sfc::read_u32_le(
                std::span<const uint8_t, 4>{span2.data() + 8, 4});
            if (span2.size() >= static_cast<size_t>(8 + h + 4)) {
                if (auto hdr = sfc::parse_global_header(
                        span2.subspan(8, static_cast<size_t>(h + 4)))) {
                    const auto& arr = hdr->inner_filename;
                    auto end = std::find(arr.begin(), arr.end(), uint8_t{0});
                    innerFilename = QString::fromUtf8(
                        reinterpret_cast<const char*>(arr.data()),
                        static_cast<int>(end - arr.begin()));
                }
            }
        }
    }

    emit progress(100);
    emit decodeFinished(std::move(*result), innerFilename);
}

void SfcWorker::runRepair(QList<QByteArray> fileBytesList) {
    std::vector<std::vector<uint8_t>> files;
    files.reserve(static_cast<size_t>(fileBytesList.size()));
    for (const auto& ba : fileBytesList) {
        files.emplace_back(
            reinterpret_cast<const uint8_t*>(ba.constData()),
            reinterpret_cast<const uint8_t*>(ba.constData()) + ba.size());
    }

    auto multi = sfc::decode_multi(std::span<const std::vector<uint8_t>>{files});
    if (!multi) { emit error(QString::fromStdString(multi.error().detail)); return; }
    if (multi->empty()) { emit error("No decodable SFC data found"); return; }

    auto& entry = (*multi)[0];

    QString innerFilename;
    quint64 fullSize = 0;
    quint32 n = 0, m = 0;
    {
        auto span = std::span<const uint8_t>(files[0]);
        if (span.size() >= 12) {
            const uint32_t h = sfc::read_u32_le(
                std::span<const uint8_t, 4>{span.data() + 8, 4});
            if (span.size() >= static_cast<size_t>(8 + h + 4)) {
                if (auto hdr = sfc::parse_global_header(
                        span.subspan(8, static_cast<size_t>(h + 4)))) {
                    const auto& arr = hdr->inner_filename;
                    auto end = std::find(arr.begin(), arr.end(), uint8_t{0});
                    innerFilename = QString::fromUtf8(
                        reinterpret_cast<const char*>(arr.data()),
                        static_cast<int>(end - arr.begin()));
                    fullSize = static_cast<quint64>(hdr->inner_file_size);
                    n = static_cast<quint32>(hdr->n);
                    m = static_cast<quint32>(hdr->m);
                }
            }
        }
    }

    emit progress(100);
    emit repairFinished(std::move(entry.result), innerFilename, fullSize, n, m);
}

void SfcWorker::runVerify(QString filePath, QByteArray fileBytes) {
    auto bytes = std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(fileBytes.constData()),
        static_cast<size_t>(fileBytes.size()));

    auto result = sfc::decode(bytes);
    if (!result) { emit error(filePath + ": " + QString::fromStdString(result.error().detail)); return; }
    emit progress(100);
    emit verifyFinished(filePath, std::move(*result));
}
