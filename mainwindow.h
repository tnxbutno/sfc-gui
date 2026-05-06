#pragma once

#include <QMainWindow>

class QTabWidget;
class PackTab;
class UnpackTab;
class InfoTab;
class VerifyTab;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    QTabWidget* m_tabs     = nullptr;
    PackTab*    m_packTab  = nullptr;
    UnpackTab*  m_unpackTab = nullptr;
    InfoTab*    m_infoTab  = nullptr;
    VerifyTab*  m_verifyTab = nullptr;
};
