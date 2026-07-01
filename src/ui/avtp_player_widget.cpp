#include "ui/avtp_player_widget.h"

#include "media/media_frame.hpp"
#include "player/avtp_player.h"
#ifdef ENABLE_PCAP
#include "puller/pcap_puller.hpp"
#endif

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>

#include <cstdint>
#include <string>
#include <thread>
#include <utility>

namespace {
#ifdef ENABLE_PCAP
QString FormatMacAddress(const uint8_t* mac)
{
    return QStringLiteral("%1:%2:%3:%4:%5:%6")
        .arg(static_cast<unsigned int>(mac[0]), 2, 16, QChar('0'))
        .arg(static_cast<unsigned int>(mac[1]), 2, 16, QChar('0'))
        .arg(static_cast<unsigned int>(mac[2]), 2, 16, QChar('0'))
        .arg(static_cast<unsigned int>(mac[3]), 2, 16, QChar('0'))
        .arg(static_cast<unsigned int>(mac[4]), 2, 16, QChar('0'))
        .arg(static_cast<unsigned int>(mac[5]), 2, 16, QChar('0'));
}
#endif
} // namespace

AvtpPlayerWidget::AvtpPlayerWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    setupConnections();
}

AvtpPlayerWidget::~AvtpPlayerWidget()
{
    if (m_avtpPlayer)
    {
        m_avtpPlayer->Stop();
    }
}

void AvtpPlayerWidget::setupUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    auto *controlsLayout = new QHBoxLayout();

    m_captureDeviceCombo = new QComboBox(this);
    m_captureDeviceCombo->setEditable(true);
    m_captureDeviceCombo->setMinimumWidth(360);
    m_captureDeviceCombo->setToolTip(
        tr("Npcap capture device. Use default if you are not sure."));

    m_sourceMacEdit = new QLineEdit(this);
    m_sourceMacEdit->setPlaceholderText(tr("e.g. aa:87:26:53:bb:6c"));
    m_sourceMacEdit->setToolTip(tr("Only accept AVTP packets from this source MAC."));

    m_streamIdEdit = new QLineEdit(this);
    m_streamIdEdit->setPlaceholderText(tr("optional, e.g. 0x0000000000000000"));
    m_streamIdEdit->setToolTip(tr("Optional AVTP stream id filter. Leave empty for all streams."));

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

    m_avtpPlayer = std::make_unique<AvtpPlayer>();
    refreshCaptureDevices();
    updateControlState();
}

void AvtpPlayerWidget::setupConnections()
{
    connect(m_refreshDevicesButton, &QPushButton::clicked, this, &AvtpPlayerWidget::refreshCaptureDevices);
    connect(m_captureDeviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index)
    {
        Q_UNUSED(index);
        captureBroadcastMac();
    });
    connect(m_startButton, &QPushButton::clicked, this, &AvtpPlayerWidget::startAvtpPlayer);
    connect(m_stopButton, &QPushButton::clicked, this, &AvtpPlayerWidget::stopAvtpPlayer);
    m_avtpPlayer->SetFrameCallback(
        [this](std::shared_ptr<const MediaFrame> frame)
        {
            QMetaObject::invokeMethod(
                this,
                [this, frame = std::move(frame)]()
                {
                    onVideoFrame(frame);
                },
                Qt::QueuedConnection);
        });
    m_avtpPlayer->SetStateCallback([this](const std::string &state)
                                   { QMetaObject::invokeMethod(
                                         this,
                                         [this, state]()
                                         {
                                             m_statusLabel->setText(QString::fromStdString(state));
                                         },
                                         Qt::QueuedConnection); });
    m_avtpPlayer->SetErrorCallback([this](const std::string &error)
                                   { QMetaObject::invokeMethod(
                                         this,
                                         [this, error]()
                                         {
                                             m_statusLabel->setText(
                                                 tr("Error: %1").arg(QString::fromStdString(error)));
                                         },
                                         Qt::QueuedConnection); });
}

void AvtpPlayerWidget::refreshCaptureDevices()
{
    const QString currentText = m_captureDeviceCombo->currentText().trimmed();
    m_captureDeviceCombo->clear();
    m_captureDeviceCombo->addItem(QStringLiteral("default"), QStringLiteral("default"));

    const auto devices = AvtpPlayer::ListCaptureDevices();
    for (const auto &device : devices)
    {
        const QString name = QString::fromStdString(device.name);
        const QString description =
            QString::fromStdString(device.description).trimmed();
        QString label = name;
        if (!description.isEmpty())
        {
            label += QStringLiteral("  —  ") + description;
        }
        m_captureDeviceCombo->addItem(label, name);
    }

    if (!currentText.isEmpty())
    {
        const int index = m_captureDeviceCombo->findText(currentText);
        if (index >= 0)
        {
            m_captureDeviceCombo->setCurrentIndex(index);
        }
        else
        {
            m_captureDeviceCombo->setEditText(currentText);
        }
    }

    if (devices.empty())
    {
        m_statusLabel->setText(
            tr("No capture devices found. Check Npcap runtime/admin "
               "privileges, or type a device name manually."));
    }
    else
    {
        m_statusLabel->setText(
            tr("Found %1 capture devices")
                .arg(static_cast<int>(devices.size())));
    }
}

void AvtpPlayerWidget::captureBroadcastMac()
{
    QString deviceName = m_captureDeviceCombo->currentData().toString();
    if (deviceName.isEmpty() || deviceName == "default")
    {
        deviceName = m_captureDeviceCombo->currentText().trimmed();
    }

    if (deviceName.isEmpty())
    {
        m_statusLabel->setText(tr("Please select a capture device first"));
        return;
    }

#ifndef ENABLE_PCAP
    m_statusLabel->setText(tr("Npcap support is disabled in this build"));
    m_startButton->setEnabled(true);
#else
    m_statusLabel->setText(tr("Capturing broadcast MAC..."));
    m_startButton->setEnabled(false);

    std::thread([this, deviceName]()
    {
        PcapPuller puller;
        puller.SetStripEthernetHeader(false);
        puller.SetBpfFilter("ether multicast");
        puller.SetReadTimeoutMs(100);
        puller.SetMaxQueueSize(32);

        std::string openError;
        puller.SetEventCallback([&openError](const std::string& error)
        {
            if (openError.empty())
            {
                openError = error;
            }
        });

        if (!puller.Open(deviceName.toStdString()))
        {
            const QString errorText = openError.empty()
                ? tr("Failed to open device. Check Npcap runtime/admin privileges.")
                : tr("Failed to open device: %1").arg(QString::fromStdString(openError));
            QMetaObject::invokeMethod(this, [this, errorText]()
            {
                m_statusLabel->setText(errorText);
                m_startButton->setEnabled(true);
            }, Qt::QueuedConnection);
            return;
        }

        QString capturedMac;
        for (int i = 0; i < 50; ++i)
        {
            std::shared_ptr<MediaPacket> packet;
            if (!puller.ReadPacket(packet))
            {
                break;
            }

            if (packet && packet->buffer && packet->buffer->Size() >= 14)
            {
                capturedMac = FormatMacAddress(packet->buffer->Data() + 6);
                break;
            }
        }

        puller.Close();

        QMetaObject::invokeMethod(this, [this, capturedMac]()
        {
            if (!capturedMac.isEmpty())
            {
                m_sourceMacEdit->setText(capturedMac);
                m_statusLabel->setText(tr("Captured MAC: %1").arg(capturedMac));
            }
            else
            {
                m_statusLabel->setText(tr("No broadcast MAC captured, please try again"));
            }
            m_startButton->setEnabled(true);
        }, Qt::QueuedConnection);
    }).detach();
#endif
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
    const QString typedDevice = m_captureDeviceCombo->currentText().trimmed();
    QString device = typedDevice;
    const int deviceIndex = m_captureDeviceCombo->currentIndex();
    if (deviceIndex >= 0 &&
        typedDevice == m_captureDeviceCombo->itemText(deviceIndex))
    {
        const QString selectedData =
            m_captureDeviceCombo->currentData().toString().trimmed();
        if (!selectedData.isEmpty())
        {
            device = selectedData;
        }
    }

    if (m_avtpPlayer->Start(
            device.toStdString(),
            m_sourceMacEdit->text().trimmed().toStdString(),
            m_streamIdEdit->text().trimmed().toStdString()))
    {
        updateControlState();
    }
}

void AvtpPlayerWidget::stopAvtpPlayer()
{
    m_avtpPlayer->Stop();
    updateControlState();
}

void AvtpPlayerWidget::onVideoFrame(std::shared_ptr<const MediaFrame> frame)
{
    if (!frame || !frame->buffer ||
        frame->pixel_format != PixelFormat::kRGB24 ||
        frame->width <= 0 || frame->height <= 0)
    {
        return;
    }

    const int stride = frame->stride[0] > 0 ? frame->stride[0] : frame->width * 3;
    QImage image(
        frame->buffer->Data(),
        frame->width,
        frame->height,
        stride,
        QImage::Format_RGB888);
    if (image.isNull())
    {
        return;
    }

    m_lastFrame = image.copy();
    updateVideoPixmap();
    m_statusLabel->setText(tr("Playing %1x%2").arg(frame->width).arg(frame->height));
}

void AvtpPlayerWidget::updateVideoPixmap()
{
    if (m_lastFrame.isNull())
    {
        return;
    }
    m_videoLabel->setPixmap(
        QPixmap::fromImage(m_lastFrame).scaled(m_videoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void AvtpPlayerWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateVideoPixmap();
}
