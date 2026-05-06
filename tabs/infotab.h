#pragma once

#include <QWidget>

class DropZone;
class ChunkGridWidget;
class QLabel;
class QPushButton;
class QFrame;

class InfoTab : public QWidget {
    Q_OBJECT
public:
    explicit InfoTab(QWidget* parent = nullptr);

signals:
    void statusMessage(QString msg);

private slots:
    void onFileSelected(QStringList paths);
    void onLoad();

private:
    void showInfo(const QString& path);
    void clearInfo();

    DropZone*       m_dropZone   = nullptr;
    QPushButton*    m_loadBtn    = nullptr;
    QFrame*         m_infoFrame  = nullptr;
    ChunkGridWidget* m_chunkGrid = nullptr;

    QLabel* m_uuid        = nullptr;
    QLabel* m_format      = nullptr;
    QLabel* m_filename    = nullptr;
    QLabel* m_timestamp   = nullptr;
    QLabel* m_innerSize   = nullptr;
    QLabel* m_chunks      = nullptr;
    QLabel* m_compression = nullptr;
    QLabel* m_profile     = nullptr;
    QLabel* m_author      = nullptr;
    QLabel* m_description = nullptr;
    QLabel* m_location    = nullptr;
    QLabel* m_software    = nullptr;
    QLabel* m_comment     = nullptr;
    QFrame* m_metaSep     = nullptr;

    QString m_currentPath;
};
