#include "ui/avtp_player_widget.h"

#include "player/avtp_player.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>

AvtpPlayerWidget::AvtpPlayerWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    setupConnections();
}

AvtpPlayerWidget::~AvtpPlayerWidget()
{
    if (m_avtpPlayer) {
        m_avtpPlayer->Stop();
    }
}

void AvtpPlayerWidget::setupUi()
{
    auto* rootLayout = new QVBoxLayout(this);
    auto* controlsLayout = new QHBoxLayout();

    m_captureDeviceCombo = new QComboBox(this);
    m_captureDeviceCombo->setEditable(true);
    m_captureDeviceCombo->setMinimumWidth(360);
    m_captureDeviceCombo->setToolTip(
        tr("Npcap capture device. Use default if you are not sure."));

    m_sourceMacEdit =
        new QLineEdit(QStringLiteral("aa:87:26:53:bb:6c"), this);
    m_sourceMacEdit->setToolTip(
        tr("Only accept AVTP packets from this source MAC."));

    m_streamIdEdit = new QLineEdit(this);
    m_streamIdEdit->setPlaceholderText(
        tr("optional, e.g. 0x0000000000000000"));
    m_streamIdEdit->setToolTip(
        tr("Optional AVTP stream id filter. Leave empty for all streams."));

    m_refreshDevicesButton = new QPushButton(tr("Refresh devices"), this);
    m_startButton = new QPushButton(tr("Start listening"), this);
    m_stopButton = new QPushButton(tr("Stop"), this);
    m_stopButton->setEnabled(false);

    controlsLayout->addWidget(new QLabel(tr("Capture device:"), this));
    controlsLayout->addWidget(m_captureDeviceCombo, 1);
    controlsLayout->addWidget(new QLabel(tr("Source MAC:"), this));
    controlsLayout->addWidget(m_sourceMacEdit);
    controlsLayout->addWidget(new QLabel(tr("Stream ID:"), this));
    controlsLayout->addWidget(m_streamIdEdit);
    controlsLayout->addWidget(m_refreshDevicesButton);
    controlsLayout->addWidget(m_startButton);
    controlsLayout->addWidget(m_stopButton);

    m_statusLabel = new QLabel(tr("Stopped"), this);
    m_videoLabel = new QLabel(this);
    m_videoLabel->setAlignment(Qt::AlignCenter);
    m_videoLabel->setMinimumSize(640, 360);
    m_videoLabel->setText(tr("Start AVTP listening before sending the "
                             "camera stream command"));
    m_videoLabel->setStyleSheet(
        QStringLiteral("background-color: #111; color: #aaa;"));

    rootLayout->addLayout(controlsLayout);
    rootLayout->addWidget(m_statusLabel);
    rootLayout->addWidget(m_videoLabel, 1);

    m_avtpPlayer = new AvtpPlayer(this);
    refreshCaptureDevices();
    updateControlState();
}

void AvtpPlayerWidget::setupConnections()
{
    connect(m_refreshDevicesButton, &QPushButton::clicked,
            this, &AvtpPlayerWidget::refreshCaptureDevices);
    connect(m_startButton, &QPushButton::clicked,
            this, &AvtpPlayerWidget::startAvtpPlayer);
    connect(m_stopButton, &QPushButton::clicked,
            this, &AvtpPlayerWidget::stopAvtpPlayer);
    connect(m_avtpPlayer, &AvtpPlayer::FrameReady,
            this, &AvtpPlayerWidget::onVideoFrame);
    connect(m_avtpPlayer, &AvtpPlayer::StateChanged,
            this, [this](const QString& state) {
                m_statusLabel->setText(state);
            });
    connect(m_avtpPlayer, &AvtpPlayer::ErrorOccurred,
            this, [this](const QString& error) {
                m_statusLabel->setText(tr("Error: %1").arg(error));
            });
}

void AvtpPlayerWidget::refreshCaptureDevices()
{
    const QString currentText = m_captureDeviceCombo->currentText().trimmed();
    m_captureDeviceCombo->clear();
    m_captureDeviceCombo->addItem(QStringLiteral("default"),
                                  QStringLiteral("default"));

    const auto devices = AvtpPlayer::ListCaptureDevices();
    for (const auto& device : devices) {
        QString label = device.name;
        if (!device.description.trimmed().isEmpty()) {
            label += QStringLiteral("  —  ") + device.description;
        }
        m_captureDeviceCombo->addItem(label, device.name);
    }

    if (!currentText.isEmpty()) {
        const int index = m_captureDeviceCombo->findText(currentText);
        if (index >= 0) {
            m_captureDeviceCombo->setCurrentIndex(index);
        } else {
            m_captureDeviceCombo->setEditText(currentText);
        }
    }

    if (devices.empty()) {
        m_statusLabel->setText(
            tr("No capture devices found. Check Npcap runtime/admin "
               "privileges, or type a device name manually."));
    } else {
        m_statusLabel->setText(
            tr("Found %1 capture devices").arg(devices.size()));
    }
}

void AvtpPlayerWidget::updateControlState()
{
    const bool running = m_avtpPlayer && m_avtpPlayer->IsRunning();
    m_captureDeviceCombo->setEnabled(!running);
    m_sourceMacEdit->setEnabled(!running);
    m_streamIdEdit->setEnabled(!running);
    m_refreshDevicesButton->setEnabled(!running);
    m_startButton->setEnabled(!running);
    m_stopButton->setEnabled(running);
}

void AvtpPlayerWidget::startAvtpPlayer()
{
    const QString typedDevice =
        m_captureDeviceCombo->currentText().trimmed();
    QString device = typedDevice;
    const int deviceIndex = m_captureDeviceCombo->currentIndex();
    if (deviceIndex >= 0 &&
        typedDevice == m_captureDeviceCombo->itemText(deviceIndex)) {
        const QString selectedData =
            m_captureDeviceCombo->currentData().toString().trimmed();
        if (!selectedData.isEmpty()) {
            device = selectedData;
        }
    }

    if (m_avtpPlayer->Start(
            device,
            m_sourceMacEdit->text().trimmed(),
            m_streamIdEdit->text().trimmed())) {
        updateControlState();
    }
}

void AvtpPlayerWidget::stopAvtpPlayer()
{
    m_avtpPlayer->Stop();
    updateControlState();
}

void AvtpPlayerWidget::onVideoFrame(const QImage& image)
{
    m_lastFrame = image;
    updateVideoPixmap();
    m_statusLabel->setText(
        tr("Playing %1x%2").arg(image.width()).arg(image.height()));
}

void AvtpPlayerWidget::updateVideoPixmap()
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

void AvtpPlayerWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateVideoPixmap();
}
