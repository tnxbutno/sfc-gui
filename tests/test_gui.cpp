#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QTest>

#include "worker/sfcworker.h"
#include "widgets/metadataeditor.h"
#include "widgets/chunkgridwidget.h"

#include "sfc/types.h"

// ── Helpers ──────────────────────────────────────────────────────────────────

static sfc::EncodeParams makeParams(const std::string& filename = "test.bin",
                                    uint32_t m = 0) {
    sfc::FileUuid uuid;
    uuid.bytes.fill(0x42);
    return sfc::EncodeParams{
        .m         = m,
        .s         = 512,
        .algo      = sfc::CompressionAlgo::Zstd,
        .uuid      = uuid,
        .timestamp = 1000000000,
        .format_id = 0x0001,
        .filename  = filename,
        .metadata  = {},
    };
}

// Encode content to a temp file; returns path. Fails the test on error.
static QString encodeToFile(const QByteArray& content,
                             const sfc::EncodeParams& params,
                             const QString& outPath,
                             uint32_t segments = 1) {
    SfcWorker worker;
    QString    lastError;
    QStringList written;

    QObject::connect(&worker, &SfcWorker::error,
                     [&](QString e) { lastError = e; });
    QObject::connect(&worker, &SfcWorker::encodeFinished,
                     [&](QStringList paths) { written = paths; });

    worker.runEncode(content, params, outPath, segments);

    if (!lastError.isEmpty()) qWarning() << lastError;
    return written.isEmpty() ? QString{} : written[0];
}

// ── SfcWorker tests ──────────────────────────────────────────────────────────

class TestSfcWorker : public QObject {
    Q_OBJECT

private slots:
    void singleFileRoundTrip();
    void singleFileRoundTrip_LargeContent();
    void metadataRoundTrip();
    void splitSegmentsRoundTrip();
    void verifyGoodFile();
    void verifyBadFile();
    void encodeError_OddChunkSize();
    void decodeError_Truncated();
};

void TestSfcWorker::singleFileRoundTrip() {
    QTemporaryDir dir; QVERIFY(dir.isValid());
    const QByteArray content("Hello, SFC! Round-trip test.");
    auto params = makeParams();

    SfcWorker worker;
    QString error;
    QStringList encoded;
    QObject::connect(&worker, &SfcWorker::error,         [&](QString e)      { error = e; });
    QObject::connect(&worker, &SfcWorker::encodeFinished,[&](QStringList p)  { encoded = p; });
    worker.runEncode(content, params, dir.filePath("out"), 1);

    QVERIFY2(error.isEmpty(), error.toUtf8());
    QCOMPARE(encoded.size(), 1);
    QVERIFY(QFile::exists(encoded[0]));

    QFile f(encoded[0]); QVERIFY(f.open(QIODevice::ReadOnly));
    sfc::ReassemblyResult result;
    QString innerName;
    bool got = false;
    QObject::connect(&worker, &SfcWorker::decodeFinished,
                     [&](sfc::ReassemblyResult r, QString n) {
                         result = std::move(r); innerName = n; got = true;
                     });
    worker.runDecode({f.readAll()});

    QVERIFY2(error.isEmpty(), error.toUtf8());
    QVERIFY(got);
    QCOMPARE(result.status, sfc::ReassemblyStatus::FullyVerified);
    QByteArray decoded(reinterpret_cast<const char*>(result.content.data()),
                       static_cast<int>(result.content.size()));
    QCOMPARE(decoded, content);
    QCOMPARE(innerName, QString("test.bin"));
}

void TestSfcWorker::singleFileRoundTrip_LargeContent() {
    QTemporaryDir dir; QVERIFY(dir.isValid());
    QByteArray content(128 * 1024, 'x');  // 128 KB — spans multiple chunks
    for (int i = 0; i < content.size(); ++i)
        content[i] = static_cast<char>(i & 0xFF);

    SfcWorker worker;
    QString error;
    QObject::connect(&worker, &SfcWorker::error, [&](QString e) { error = e; });

    QStringList encoded;
    QObject::connect(&worker, &SfcWorker::encodeFinished, [&](QStringList p) { encoded = p; });
    worker.runEncode(content, makeParams(), dir.filePath("big"), 1);
    QVERIFY2(error.isEmpty(), error.toUtf8());

    QFile f(encoded[0]); QVERIFY(f.open(QIODevice::ReadOnly));
    sfc::ReassemblyResult result;
    bool got = false;
    QObject::connect(&worker, &SfcWorker::decodeFinished,
                     [&](sfc::ReassemblyResult r, QString) { result = std::move(r); got = true; });
    worker.runDecode({f.readAll()});

    QVERIFY2(error.isEmpty(), error.toUtf8());
    QVERIFY(got);
    QByteArray decoded(reinterpret_cast<const char*>(result.content.data()),
                       static_cast<int>(result.content.size()));
    QCOMPARE(decoded, content);
}

void TestSfcWorker::metadataRoundTrip() {
    QTemporaryDir dir; QVERIFY(dir.isValid());
    const QByteArray content("metadata test payload");

    auto params = makeParams();
    params.metadata = sfc::FileMetadata{
        .author      = "Alice",
        .description = "Test description",
        .location    = "77.8S 166.7E",
        .software    = "sfc-gui-test",
        .comment     = "automated test",
    };

    SfcWorker worker;
    QString error;
    QStringList encoded;
    QObject::connect(&worker, &SfcWorker::error,          [&](QString e)     { error = e; });
    QObject::connect(&worker, &SfcWorker::encodeFinished, [&](QStringList p) { encoded = p; });
    worker.runEncode(content, params, dir.filePath("meta"), 1);
    QVERIFY2(error.isEmpty(), error.toUtf8());

    QFile f(encoded[0]); QVERIFY(f.open(QIODevice::ReadOnly));
    sfc::ReassemblyResult result;
    bool got = false;
    QObject::connect(&worker, &SfcWorker::decodeFinished,
                     [&](sfc::ReassemblyResult r, QString) { result = std::move(r); got = true; });
    worker.runDecode({f.readAll()});

    QVERIFY2(error.isEmpty(), error.toUtf8());
    QVERIFY(got);
    QCOMPARE(result.metadata.author,      std::string("Alice"));
    QCOMPARE(result.metadata.description, std::string("Test description"));
    QCOMPARE(result.metadata.location,    std::string("77.8S 166.7E"));
    QCOMPARE(result.metadata.software,    std::string("sfc-gui-test"));
    QCOMPARE(result.metadata.comment,     std::string("automated test"));
}

void TestSfcWorker::splitSegmentsRoundTrip() {
    QTemporaryDir dir; QVERIFY(dir.isValid());
    QByteArray content(32 * 1024, 0);
    for (int i = 0; i < content.size(); ++i) content[i] = static_cast<char>(i);

    auto params = makeParams("split.bin", /*m=*/1);

    SfcWorker worker;
    QString error;
    QStringList encoded;
    QObject::connect(&worker, &SfcWorker::error,          [&](QString e)     { error = e; });
    QObject::connect(&worker, &SfcWorker::encodeFinished, [&](QStringList p) { encoded = p; });
    worker.runEncode(content, params, dir.filePath("seg"), 3);

    QVERIFY2(error.isEmpty(), error.toUtf8());
    QCOMPARE(encoded.size(), 3);

    QList<QByteArray> segBytes;
    for (const auto& path : encoded) {
        QFile f(path); QVERIFY(f.open(QIODevice::ReadOnly));
        segBytes.append(f.readAll());
    }

    sfc::ReassemblyResult result;
    bool got = false;
    QObject::connect(&worker, &SfcWorker::decodeFinished,
                     [&](sfc::ReassemblyResult r, QString) { result = std::move(r); got = true; });
    worker.runDecode(segBytes);

    QVERIFY2(error.isEmpty(), error.toUtf8());
    QVERIFY(got);
    QByteArray decoded(reinterpret_cast<const char*>(result.content.data()),
                       static_cast<int>(result.content.size()));
    QCOMPARE(decoded, content);
}

void TestSfcWorker::verifyGoodFile() {
    QTemporaryDir dir; QVERIFY(dir.isValid());
    const QByteArray content("verify me");

    SfcWorker worker;
    QString error;
    QStringList encoded;
    QObject::connect(&worker, &SfcWorker::error,          [&](QString e)     { error = e; });
    QObject::connect(&worker, &SfcWorker::encodeFinished, [&](QStringList p) { encoded = p; });
    worker.runEncode(content, makeParams(), dir.filePath("v"), 1);
    QVERIFY2(error.isEmpty(), error.toUtf8());

    QFile f(encoded[0]); QVERIFY(f.open(QIODevice::ReadOnly));
    QByteArray fileBytes = f.readAll();

    sfc::ReassemblyResult result;
    bool got = false;
    QObject::connect(&worker, &SfcWorker::verifyFinished,
                     [&](QString, sfc::ReassemblyResult r) { result = std::move(r); got = true; });
    worker.runVerify(encoded[0], fileBytes);

    QVERIFY2(error.isEmpty(), error.toUtf8());
    QVERIFY(got);
    QVERIFY(result.status == sfc::ReassemblyStatus::FullyVerified ||
            result.status == sfc::ReassemblyStatus::ContentVerified);
}

void TestSfcWorker::verifyBadFile() {
    SfcWorker worker;
    QString error;
    bool verified = false;
    QObject::connect(&worker, &SfcWorker::error,         [&](QString e) { error = e; });
    QObject::connect(&worker, &SfcWorker::verifyFinished,[&](QString, sfc::ReassemblyResult) { verified = true; });

    QByteArray garbage(256, 0xFF);
    worker.runVerify("bad.sfc", garbage);

    QVERIFY(!error.isEmpty());
    QVERIFY(!verified);
}

void TestSfcWorker::encodeError_OddChunkSize() {
    QTemporaryDir dir; QVERIFY(dir.isValid());
    auto params = makeParams();
    params.s = 513;  // odd chunk size — must be rejected

    SfcWorker worker;
    QString error;
    bool encoded = false;
    QObject::connect(&worker, &SfcWorker::error,          [&](QString e) { error = e; });
    QObject::connect(&worker, &SfcWorker::encodeFinished, [&](QStringList) { encoded = true; });
    worker.runEncode(QByteArray("data"), params, dir.filePath("x"), 1);

    QVERIFY(!encoded);
    QVERIFY(!error.isEmpty());
}

void TestSfcWorker::decodeError_Truncated() {
    SfcWorker worker;
    QString error;
    bool decoded = false;
    QObject::connect(&worker, &SfcWorker::error,         [&](QString e) { error = e; });
    QObject::connect(&worker, &SfcWorker::decodeFinished,[&](sfc::ReassemblyResult, QString) { decoded = true; });

    worker.runDecode({QByteArray("too short")});

    QVERIFY(!decoded);
    QVERIFY(!error.isEmpty());
}

// ── MetadataEditor tests ─────────────────────────────────────────────────────

class TestMetadataEditor : public QObject {
    Q_OBJECT

private slots:
    void roundTrip_AllFields();
    void roundTrip_EmptyFields();
    void defaultSoftware();
    void resetToDefaults_ClearsUserFields();
};

void TestMetadataEditor::roundTrip_AllFields() {
    MetadataEditor editor;
    sfc::FileMetadata in{
        .author      = "Bob",
        .description = "Some desc",
        .location    = "51.5N 0.1W",
        .software    = "my-app 2.0",
        .comment     = "test comment",
    };
    editor.setMetadata(in);
    auto out = editor.metadata();

    QCOMPARE(out.author,      in.author);
    QCOMPARE(out.description, in.description);
    QCOMPARE(out.location,    in.location);
    QCOMPARE(out.software,    in.software);
    QCOMPARE(out.comment,     in.comment);
}

void TestMetadataEditor::roundTrip_EmptyFields() {
    MetadataEditor editor;
    editor.setMetadata({});
    auto out = editor.metadata();

    QVERIFY(out.author.empty());
    QVERIFY(out.description.empty());
    QVERIFY(out.location.empty());
    QVERIFY(out.comment.empty());
}

void TestMetadataEditor::defaultSoftware() {
    MetadataEditor editor;
    // Software field is pre-filled with app name + version by default.
    QVERIFY(!editor.metadata().software.empty());
}

void TestMetadataEditor::resetToDefaults_ClearsUserFields() {
    MetadataEditor editor;
    editor.setMetadata({ .author = "Alice", .comment = "hi" });
    editor.resetToDefaults();
    auto out = editor.metadata();

    QVERIFY(out.author.empty());
    QVERIFY(out.comment.empty());
    QVERIFY(!out.software.empty());  // software is re-filled by resetToDefaults
}

// ── ChunkGridWidget tests ────────────────────────────────────────────────────

class TestChunkGridWidget : public QObject {
    Q_OBJECT

private slots:
    void defaultIsEmpty();
    void setChunkInfo_AllPresent();
    void setChunkInfo_NoneClear();
    void setChunkInfo_MixedPresence();
    void setChunkInfo_NoRecovery();
    void setChunkInfo_LargeGrid();
    void clear_AfterSet();
    void sizeHint_Reasonable();
    void setChunkInfo_ZeroNM();
};

void TestChunkGridWidget::defaultIsEmpty() {
    ChunkGridWidget w;
    QVERIFY(w.sizeHint().isValid());
}

void TestChunkGridWidget::setChunkInfo_AllPresent() {
    ChunkGridWidget w;
    w.resize(300, 200);
    std::vector<bool> data(10, true), rec(2, true);
    w.setChunkInfo(10, 2, data, rec);
    QVERIFY(w.sizeHint().height() > 0);
}

void TestChunkGridWidget::setChunkInfo_NoneClear() {
    ChunkGridWidget w;
    w.resize(300, 200);
    std::vector<bool> data(8, false), rec(3, false);
    w.setChunkInfo(8, 3, data, rec);
    QVERIFY(w.sizeHint().height() > 0);
}

void TestChunkGridWidget::setChunkInfo_MixedPresence() {
    ChunkGridWidget w;
    w.resize(300, 200);
    std::vector<bool> data = {true, false, true, true, false, false, true, true};
    std::vector<bool> rec  = {true, false};
    w.setChunkInfo(8, 2, data, rec);
    QVERIFY(w.sizeHint().height() > 0);
}

void TestChunkGridWidget::setChunkInfo_NoRecovery() {
    ChunkGridWidget w;
    w.resize(300, 200);
    std::vector<bool> data(5, true);
    w.setChunkInfo(5, 0, data, {});
    QVERIFY(w.sizeHint().height() > 0);
}

void TestChunkGridWidget::setChunkInfo_LargeGrid() {
    ChunkGridWidget w;
    w.resize(300, 200);
    std::vector<bool> data(200, true), rec(50, false);
    w.setChunkInfo(200, 50, data, rec);
    QVERIFY(w.sizeHint().height() > 0);
}

void TestChunkGridWidget::clear_AfterSet() {
    ChunkGridWidget w;
    w.resize(300, 200);
    std::vector<bool> data(10, true);
    w.setChunkInfo(10, 0, data, {});
    w.clear();
    // After clear, sizeHint should still be valid (not crash)
    QVERIFY(w.sizeHint().isValid());
}

void TestChunkGridWidget::sizeHint_Reasonable() {
    ChunkGridWidget w;
    w.resize(300, 200);
    std::vector<bool> data(16, true), rec(2, true);
    w.setChunkInfo(16, 2, data, rec);
    // Should be taller than zero, not absurdly tall
    int h = w.sizeHint().height();
    QVERIFY(h > 0);
    QVERIFY(h < 2000);
}

void TestChunkGridWidget::setChunkInfo_ZeroNM() {
    ChunkGridWidget w;
    w.resize(300, 200);
    // No chunks at all — should not crash
    w.setChunkInfo(0, 0, {}, {});
    QVERIFY(w.sizeHint().isValid());
}

// ── main ─────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("sfc-gui");
    app.setApplicationVersion("1.0");

    int status = 0;
    status |= QTest::qExec(new TestSfcWorker,       argc, argv);
    status |= QTest::qExec(new TestMetadataEditor,  argc, argv);
    status |= QTest::qExec(new TestChunkGridWidget, argc, argv);
    return status;
}

#include "test_gui.moc"
