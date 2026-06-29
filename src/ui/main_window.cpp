#include "ui/main_window.h"
#include <QHBoxLayout>
#include <QFile>
#include <QDir>
#include <QApplication>
#include <QPushButton>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupMenuBar();
    setupToolBar();
    setupStyle();
    setupConnections();

    setWindowTitle(tr("Camera Player"));
    setWindowIcon(QIcon(":/res/icon/logo.ico"));
    resize(1024, 768);
}

void MainWindow::setupMenuBar()
{
    m_fileMenu = menuBar()->addMenu(tr("File"));
    m_pageMenu = menuBar()->addMenu(tr("Page"));
    m_toolMenu = menuBar()->addMenu(tr("Tool"));
    m_helpMenu = menuBar()->addMenu(tr("Help"));
}

void MainWindow::setupToolBar()
{
    m_navWidget = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(m_navWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);

    m_backAction = new QAction(tr("Back"), this);
    m_forwardAction = new QAction(tr("Forward"), this);

    QPushButton* backButton = new QPushButton("\u2190");
    QPushButton* forwardButton = new QPushButton("\u2192");
    backButton->setObjectName("navButton");
    forwardButton->setObjectName("navButton");

    m_langCombo = new QComboBox();
    m_langCombo->addItem("中文");
    m_langCombo->addItem("English");
    m_langCombo->setObjectName("langCombo");

    layout->addWidget(backButton);
    layout->addWidget(forwardButton);
    layout->addWidget(m_langCombo);
    layout->addStretch();

    menuBar()->setCornerWidget(m_navWidget, Qt::TopRightCorner);
}

void MainWindow::setupStyle()
{
    QString qssPath = ":/style.qss";
    QFile file(qssPath);
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        QString styleSheet = QString::fromUtf8(file.readAll());
        qApp->setStyleSheet(styleSheet);
        file.close();
    }
}

void MainWindow::setupConnections()
{
    connect(m_langCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [](int index) {
        if (index == 0) {
            // Chinese
        } else {
            // English
        }
    });
}
