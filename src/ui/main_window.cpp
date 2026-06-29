#include "ui/main_window.h"
#include "player/rtp_player.h"

#include <QHBoxLayout>
#include <QFile>
#include <QApplication>
#include <QPushButton>
#include <QPixmap>
#include <QResizeEvent>
#include <QVBoxLayout>

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

MainWindow::~MainWindow()
{
    if (m_rtpPlayer) {
        m_rtpPlayer->Stop();
    }
}

void MainWindow::setupCentralWidget()
{
    auto* centralWidget = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(centralWidget);
    auto* controlsLayout = new QHBoxLayout();

    m_localAddressEdit = new QLineEdit(QStringLiteral("0.0.0.0"), this);
    m_localAddressEdit->setToolTip(
        tr("Local address to bind. Use 0.0.0.0 for all interfaces."));

    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(60000);

    m_cameraAddressEdit =
        new QLineEdit(QStringLiteral("192.168.66.166"), this);
    m_cameraAddressEdit->setToolTip(
        tr("Only accept RTP packets from this camera address."));

    m_startButton = new QPushButton(tr("Start listening"), this);
    m_stopButton = new QPushButton(tr("Stop"), this);
    m_stopButton->setEnabled(false);

    controlsLayout->addWidget(new QLabel(tr("Local IP:"), this));
    controlsLayout->addWidget(m_localAddressEdit);
    controlsLayout->addWidget(new QLabel(tr("RTP port:"), this));
    controlsLayout->addWidget(m_portSpin);
    controlsLayout->addWidget(new QLabel(tr("Camera IP:"), this));
    controlsLayout->addWidget(m_cameraAddressEdit);
    controlsLayout->addWidget(m_startButton);
    controlsLayout->addWidget(m_stopButton);

    m_statusLabel = new QLabel(tr("Stopped"), this);
    m_videoLabel = new QLabel(this);
    m_videoLabel->setAlignment(Qt::AlignCenter);
    m_videoLabel->setMinimumSize(640, 360);
    m_videoLabel->setText(tr("Start the RTP listener before sending the "
                             "camera stream command"));
    m_videoLabel->setStyleSheet(
        QStringLiteral("background-color: #111; color: #aaa;"));

    rootLayout->addLayout(controlsLayout);
    rootLayout->addWidget(m_statusLabel);
    rootLayout->addWidget(m_videoLabel, 1);
    setCentralWidget(centralWidget);

    m_rtpPlayer = new RtpPlayer(this);
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

    connect(m_startButton, &QPushButton::clicked,
            this, &MainWindow::startRtpPlayer);
    connect(m_stopButton, &QPushButton::clicked,
            this, &MainWindow::stopRtpPlayer);
    connect(m_rtpPlayer, &RtpPlayer::FrameReady,
            this, &MainWindow::onVideoFrame);
    connect(m_rtpPlayer, &RtpPlayer::StateChanged,
            this, [this](const QString& state) {
                m_statusLabel->setText(state);
            });
    connect(m_rtpPlayer, &RtpPlayer::ErrorOccurred,
            this, [this](const QString& error) {
                m_statusLabel->setText(tr("Error: %1").arg(error));
            });
}

void MainWindow::startRtpPlayer()
{
    if (m_rtpPlayer->Start(
            m_localAddressEdit->text().trimmed(),
            static_cast<uint16_t>(m_portSpin->value()),
            m_cameraAddressEdit->text().trimmed())) {
        m_startButton->setEnabled(false);
        m_stopButton->setEnabled(true);
        m_localAddressEdit->setEnabled(false);
        m_portSpin->setEnabled(false);
        m_cameraAddressEdit->setEnabled(false);
    }
}

void MainWindow::stopRtpPlayer()
{
    m_rtpPlayer->Stop();
    m_startButton->setEnabled(true);
    m_stopButton->setEnabled(false);
    m_localAddressEdit->setEnabled(true);
    m_portSpin->setEnabled(true);
    m_cameraAddressEdit->setEnabled(true);
}

void MainWindow::onVideoFrame(const QImage& image)
{
    m_lastFrame = image;
    updateVideoPixmap();
    m_statusLabel->setText(
        tr("Playing %1x%2").arg(image.width()).arg(image.height()));
}

void MainWindow::updateVideoPixmap()
{
    if (m_lastFrame.isNull()) {
        return;
    }
    m_videoLabel->setPixmap(
        QPixmap::fromImage(m_lastFrame).scaled(
            m_videoLabel->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation));
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    updateVideoPixmap();
}
