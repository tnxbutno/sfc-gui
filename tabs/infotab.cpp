#include "infotab.h"

#include "widgets/dropzone.h"
#include "widgets/chunkgridwidget.h"

#include "sfc/global_header.h"
#include "sfc/trailer.h"
#include "sfc/tlv.h"

#include <QDateTime>
#include <QTimeZone>
#include <QFile>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <span>

static QString fmtUuid(const sfc::FileUuid& u) {
    const auto& b = u.bytes;
    return QString("%1%2%3%4-%5%6-%7%8-%9%10-%11%12%13%14%15%16")
        .arg(b[0],2,16,QChar('0')).arg(b[1],2,16,QChar('0'))
        .arg(b[2],2,16,QChar('0')).arg(b[3],2,16,QChar('0'))
        .arg(b[4],2,16,QChar('0')).arg(b[5],2,16,QChar('0'))
        .arg(b[6],2,16,QChar('0')).arg(b[7],2,16,QChar('0'))
        .arg(b[8],2,16,QChar('0')).arg(b[9],2,16,QChar('0'))
        .arg(b[10],2,16,QChar('0')).arg(b[11],2,16,QChar('0'))
        .arg(b[12],2,16,QChar('0')).arg(b[13],2,16,QChar('0'))
        .arg(b[14],2,16,QChar('0')).arg(b[15],2,16,QChar('0'));
}

static QString compressionName(uint8_t algo) {
    switch (algo) {
    case 0x01: return "zstd";
    case 0x02: return "brotli";
    case 0x03: return "lz4";
    default:   return "identity (none)";
    }
}

static QString formatIdName(uint16_t id) {
    switch (id) {
    case 0x0000: return "Unknown";
    case 0x0001: return "ArbitraryBinary";
    case 0x0010: return "PlainText";
    case 0x0011: return "LineOriented";
    case 0x0020: return "JpegBaseline";
    case 0x0021: return "JpegProgressive";
    case 0x0022: return "Jpeg2000";
    case 0x0023: return "JpegXl";
    case 0x0024: return "PngNonInterlaced";
    case 0x0025: return "PngAdam7";
    case 0x0026: return "WebP";
    case 0x0030: return "FragmentedMp4";
    case 0x0031: return "MatroskaWebm";
    case 0x0040: return "Zip";
    case 0x0041: return "Gzip";
    case 0x0042: return "ZstdData";
    case 0x0043: return "TarZstd";
    case 0x0050: return "SfcDirectory";
    case 0x00FF: return "NestedSfc";
    case 0x0100: return "Pdf";
    case 0x0101: return "EPub";
    default:     return "Unknown";
    }
}

InfoTab::InfoTab(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(10);

    root->addWidget(new QLabel("Input", this));
    m_dropZone = new DropZone(DropZone::Mode::SingleFile, this);
    root->addWidget(m_dropZone);

    m_loadBtn = new QPushButton("Load", this);
    root->addWidget(m_loadBtn, 0, Qt::AlignRight);

    // Scrollable info area
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    m_infoFrame  = new QFrame(this);
    auto* form   = new QFormLayout(m_infoFrame);
    form->setLabelAlignment(Qt::AlignRight);
    form->setHorizontalSpacing(12);

    auto addRow = [&](const QString& label, QLabel*& field) {
        field = new QLabel(m_infoFrame);
        field->setTextInteractionFlags(Qt::TextSelectableByMouse);
        form->addRow(label, field);
    };

    form->addRow(new QLabel("<b>File metadata</b>", m_infoFrame));
    addRow("UUID:",        m_uuid);
    addRow("Format:",      m_format);
    addRow("Filename:",    m_filename);
    addRow("Timestamp:",   m_timestamp);
    addRow("Inner size:",  m_innerSize);

    auto* sep1 = new QFrame(m_infoFrame);
    sep1->setFrameShape(QFrame::HLine);
    form->addRow(sep1);
    form->addRow(new QLabel("<b>Encoding</b>", m_infoFrame));
    addRow("Chunks:",      m_chunks);
    addRow("Compression:", m_compression);
    addRow("Profile:",     m_profile);

    m_metaSep = new QFrame(m_infoFrame);
    m_metaSep->setFrameShape(QFrame::HLine);
    form->addRow(m_metaSep);
    form->addRow(new QLabel("<b>User metadata</b>", m_infoFrame));
    addRow("Author:",      m_author);
    addRow("Description:", m_description);
    addRow("Location:",    m_location);
    addRow("Software:",    m_software);
    addRow("Comment:",     m_comment);

    auto* sep2 = new QFrame(m_infoFrame);
    sep2->setFrameShape(QFrame::HLine);
    form->addRow(sep2);
    form->addRow(new QLabel("<b>Chunk availability</b>", m_infoFrame));
    m_chunkGrid = new ChunkGridWidget(m_infoFrame);
    form->addRow(m_chunkGrid);

    scroll->setWidget(m_infoFrame);
    root->addWidget(scroll, 1);

    clearInfo();

    connect(m_dropZone, &DropZone::filesSelected, this, &InfoTab::onFileSelected);
    connect(m_loadBtn,  &QPushButton::clicked,    this, &InfoTab::onLoad);
}

void InfoTab::onFileSelected(QStringList paths) {
    if (!paths.isEmpty()) m_currentPath = paths[0];
}

void InfoTab::onLoad() {
    if (m_currentPath.isEmpty()) {
        QMessageBox::warning(this, "No file", "Select an .sfc file first.");
        return;
    }
    showInfo(m_currentPath);
}

void InfoTab::clearInfo() {
    for (auto* lbl : { m_uuid, m_format, m_filename, m_timestamp, m_innerSize,
                       m_chunks, m_compression, m_profile,
                       m_author, m_description, m_location, m_software, m_comment }) {
        if (lbl) lbl->clear();
    }
    if (m_chunkGrid) m_chunkGrid->clear();
}

void InfoTab::showInfo(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "Error", "Cannot read: " + path); return;
    }
    QByteArray raw = f.readAll();
    auto span = std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(raw.constData()),
        static_cast<size_t>(raw.size()));

    auto hdr = sfc::parse_global_header(span);
    if (!hdr) {
        QMessageBox::critical(this, "Parse error",
            QString::fromStdString(hdr.error().detail));
        return;
    }

    m_uuid->setText(fmtUuid(hdr->uuid));
    m_format->setText(QString("%1 (0x%2)")
        .arg(formatIdName(hdr->inner_format_id))
        .arg(hdr->inner_format_id, 4, 16, QChar('0')));
    m_filename->setText(
        hdr->inner_filename.empty()
            ? "(none)"
            : QString::fromStdString(std::string(
                  hdr->inner_filename.begin(), hdr->inner_filename.end())));

    // Trailer timestamp
    uint64_t ts = 0;
    if (static_cast<size_t>(raw.size()) >= 64) {
        auto tr = sfc::parse_trailer(
            std::span<const uint8_t, 64>{
                reinterpret_cast<const uint8_t*>(raw.constData()) + raw.size() - 64, 64});
        if (tr) ts = tr->timestamp;
    }
    m_timestamp->setText(ts == 0 ? "(none)"
        : QDateTime::fromSecsSinceEpoch(static_cast<qint64>(ts), QTimeZone::utc())
              .toString("yyyy-MM-dd HH:mm:ss UTC"));

    m_innerSize->setText(QString("%L1 bytes").arg(hdr->inner_file_size));
    m_chunks->setText(QString("N=%1  M=%2  S=%3").arg(hdr->n).arg(hdr->m).arg(hdr->s));
    m_compression->setText(compressionName(hdr->compression_algo));

    const bool isSplit = (hdr->flags & 0x0001) != 0;  // SPLIT_TRANSPORT bit 0
    const bool isDir   = (hdr->flags & 0x0100) != 0;  // directory profile bit 8
    m_profile->setText(isSplit ? "Split transport" : isDir ? "Directory" : "Regular");

    // Metadata TLV
    auto getMeta = [&](uint16_t tag) -> QString {
        for (const auto& field : hdr->tlv_fields)
            if (field.tag == tag)
                return QString::fromUtf8(
                    reinterpret_cast<const char*>(field.value.data()),
                    static_cast<int>(field.value.size()));
        return {};
    };
    m_author->setText(getMeta(sfc::TlvTag::kAuthor));
    m_description->setText(getMeta(sfc::TlvTag::kDescription));
    m_location->setText(getMeta(sfc::TlvTag::kLocation));
    m_software->setText(getMeta(sfc::TlvTag::kSoftware));
    m_comment->setText(getMeta(sfc::TlvTag::kComment));

    // Chunk grid — all present (we only have header info here)
    std::vector<bool> dataPresent(hdr->n, true);
    std::vector<bool> recPresent(hdr->m, true);
    m_chunkGrid->setChunkInfo(hdr->n, hdr->m, dataPresent, recPresent);

    emit statusMessage("Loaded: " + path);
}
