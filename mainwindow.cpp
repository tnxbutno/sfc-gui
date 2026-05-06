#include "mainwindow.h"

#include "tabs/packtab.h"
#include "tabs/unpacktab.h"
#include "tabs/infotab.h"
#include "tabs/verifytab.h"
#include "tabs/repairtab.h"

#include <QStatusBar>
#include <QTabWidget>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("SFC");
    resize(640, 700);

    m_tabs      = new QTabWidget(this);
    m_packTab   = new PackTab(this);
    m_unpackTab = new UnpackTab(this);
    m_infoTab   = new InfoTab(this);
    m_verifyTab = new VerifyTab(this);
    m_repairTab = new RepairTab(this);

    m_tabs->addTab(m_packTab,   "Pack");
    m_tabs->addTab(m_unpackTab, "Unpack");
    m_tabs->addTab(m_repairTab, "Repair");
    m_tabs->addTab(m_infoTab,   "Info");
    m_tabs->addTab(m_verifyTab, "Verify");

    setCentralWidget(m_tabs);
    statusBar()->showMessage("Ready");

    auto showStatus = [this](const QString& msg) { statusBar()->showMessage(msg); };
    connect(m_packTab,   &PackTab::statusMessage,   this, showStatus);
    connect(m_unpackTab, &UnpackTab::statusMessage, this, showStatus);
    connect(m_repairTab, &RepairTab::statusMessage, this, showStatus);
    connect(m_infoTab,   &InfoTab::statusMessage,   this, showStatus);
    connect(m_verifyTab, &VerifyTab::statusMessage, this, showStatus);
}
