#include "metadataeditor.h"

#include <QCoreApplication>
#include <QFormLayout>
#include <QLineEdit>

MetadataEditor::MetadataEditor(QWidget* parent)
    : QGroupBox("Metadata", parent) {
    auto* form = new QFormLayout(this);
    form->setLabelAlignment(Qt::AlignRight);

    m_author      = new QLineEdit(this);
    m_description = new QLineEdit(this);
    m_location    = new QLineEdit(this);
    m_software    = new QLineEdit(this);
    m_comment     = new QLineEdit(this);

    form->addRow("Author:",      m_author);
    form->addRow("Description:", m_description);
    form->addRow("Location:",    m_location);
    form->addRow("Software:",    m_software);
    form->addRow("Comment:",     m_comment);

    resetToDefaults();
}

sfc::FileMetadata MetadataEditor::metadata() const {
    return sfc::FileMetadata{
        .author      = m_author->text().toStdString(),
        .description = m_description->text().toStdString(),
        .location    = m_location->text().toStdString(),
        .software    = m_software->text().toStdString(),
        .comment     = m_comment->text().toStdString(),
    };
}

void MetadataEditor::setMetadata(const sfc::FileMetadata& m) {
    m_author->setText(QString::fromStdString(m.author));
    m_description->setText(QString::fromStdString(m.description));
    m_location->setText(QString::fromStdString(m.location));
    m_software->setText(QString::fromStdString(m.software));
    m_comment->setText(QString::fromStdString(m.comment));
}

void MetadataEditor::resetToDefaults() {
    m_author->clear();
    m_description->clear();
    m_location->clear();
    m_software->setText(
        QCoreApplication::applicationName() + " " +
        QCoreApplication::applicationVersion());
    m_comment->clear();
}
