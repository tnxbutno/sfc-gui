#pragma once

#include <QGroupBox>
#include "sfc/types.h"

class QLineEdit;

class MetadataEditor : public QGroupBox {
    Q_OBJECT
public:
    explicit MetadataEditor(QWidget* parent = nullptr);

    sfc::FileMetadata metadata() const;
    void setMetadata(const sfc::FileMetadata& m);
    void resetToDefaults();

private:
    QLineEdit* m_author      = nullptr;
    QLineEdit* m_description = nullptr;
    QLineEdit* m_location    = nullptr;
    QLineEdit* m_software    = nullptr;
    QLineEdit* m_comment     = nullptr;
};
