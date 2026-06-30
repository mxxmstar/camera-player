#include "ui/main_window.h"
#include "ui/avtp_player_widget.h"
#include "ui/rtp_player_widget.h"
#include "ui/page_menu.h"

#include <QHBoxLayout>
#include <QFile>
#include <QApplication>
#include <QPushButton>
#include <QStackedWidget>
#include <QMenuBar>
#include <QLabel>
#include <QMainWindow>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupMenuBar();
    setupToolBar();
    setupCentralWidget();
    setupStyle();
    setupConnections();

    setWindowTitle(tr("Camera Player"));
    setWindowIcon(QIcon(":/res/icon/logo.ico"));
    resize(1024, 768);
}

void MainWindow::setupMenuBar()
{
    m_fileMenu = menuBar()->addMenu(tr("File"));
    m_pageMenu = new PageMenu(tr("Page"), this);
    menuBar()->addMenu(m_pageMenu);
    m_toolMenu = menuBar()->addMenu(tr("Tool"));
    m_helpMenu = menuBar()->addMenu(tr("Help"));

    m_rtpPlayerAction = m_pageMenu->addAction(tr("RTP Player"));
    m_avtpPlayerAction = m_pageMenu->addAction(tr("AVTP Player"));
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

void MainWindow::setupCentralWidget()
{
    m_stackedWidget = new QStackedWidget(this);
    auto* placeholder = new QLabel(tr("No page selected"), this);
    placeholder->setAlignment(Qt::AlignCenter);
    m_stackedWidget->addWidget(placeholder);
    m_rtpPlayerWidget = new RtpPlayerWidget(this);
    m_stackedWidget->addWidget(m_rtpPlayerWidget);
    m_avtpPlayerWidget = new AvtpPlayerWidget(this);
    m_stackedWidget->addWidget(m_avtpPlayerWidget);
    setCentralWidget(m_stackedWidget);
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

    connect(m_rtpPlayerAction, &QAction::triggered,
            this, [this]() { switchToPage(1); });
    connect(m_avtpPlayerAction, &QAction::triggered,
            this, [this]() { switchToPage(2); });

    connect(m_pageMenu, &PageMenu::detachPageRequested,
            this, &MainWindow::detachPage);
}

void MainWindow::switchToPage(int index)
{
    m_stackedWidget->setCurrentIndex(index);
}

void MainWindow::detachPage(QAction* action)
{
    if (action == m_rtpPlayerAction) {
        auto* window = new QMainWindow(this);
        window->setWindowTitle(tr("RTP Player - Detached"));
        window->setWindowIcon(QIcon(":/res/icon/logo.ico"));
        window->resize(800, 600);

        auto* widget = new RtpPlayerWidget(window);
        window->setCentralWidget(widget);
        window->setAttribute(Qt::WA_DeleteOnClose);
        window->show();
    } else if (action == m_avtpPlayerAction) {
        auto* window = new QMainWindow(this);
        window->setWindowTitle(tr("AVTP Player - Detached"));
        window->setWindowIcon(QIcon(":/res/icon/logo.ico"));
        window->resize(900, 600);

        auto* widget = new AvtpPlayerWidget(window);
        window->setCentralWidget(widget);
        window->setAttribute(Qt::WA_DeleteOnClose);
        window->show();
    }
}
