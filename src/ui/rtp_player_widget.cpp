#include "ui/rtp_player_widget.h"

#include "media/media_frame.hpp"
#include "player/rtp_player.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QMetaObject>
#include <QPixmap>
#include <QResizeEvent>

#include <string>
#include <utility>

RtpPlayerWidget::RtpPlayerWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    setupConnections();
}

RtpPlayerWidget::~RtpPlayerWidget()
{
    if (m_rtpPlayer) {
        m_rtpPlayer->Stop();
    }
}

void RtpPlayerWidget::setupUi()
{
    auto* rootLayout = new QVBoxLayout(this);
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

    m_rtpPlayer = std::make_unique<RtpPlayer>();
}

void RtpPlayerWidget::setupConnections()
{
    connect(m_startButton, &QPushButton::clicked,
            this, &RtpPlayerWidget::startRtpPlayer);
    connect(m_stopButton, &QPushButton::clicked,
            this, &RtpPlayerWidget::stopRtpPlayer);
    m_rtpPlayer->SetFrameCallback(
        [this](std::shared_ptr<const MediaFrame> frame) {
            QMetaObject::invokeMethod(
                this,
                [this, frame = std::move(frame)]() {
                    onVideoFrame(frame);
                },
                Qt::QueuedConnection);
        });
    m_rtpPlayer->SetStateCallback([this](const std::string& state) {
        QMetaObject::invokeMethod(
            this,
            [this, state]() {
                m_statusLabel->setText(QString::fromStdString(state));
            },
            Qt::QueuedConnection);
    });
    m_rtpPlayer->SetErrorCallback([this](const std::string& error) {
        QMetaObject::invokeMethod(
            this,
            [this, error]() {
                m_statusLabel->setText(
                    tr("Error: %1").arg(QString::fromStdString(error)));
            },
            Qt::QueuedConnection);
    });
}

void RtpPlayerWidget::startRtpPlayer()
{
    if (m_rtpPlayer->Start(
            m_localAddressEdit->text().trimmed().toStdString(),
            static_cast<uint16_t>(m_portSpin->value()),
            m_cameraAddressEdit->text().trimmed().toStdString())) {
        m_startButton->setEnabled(false);
        m_stopButton->setEnabled(true);
        m_localAddressEdit->setEnabled(false);
        m_portSpin->setEnabled(false);
        m_cameraAddressEdit->setEnabled(false);
    }
}

void RtpPlayerWidget::stopRtpPlayer()
{
    m_rtpPlayer->Stop();
    m_startButton->setEnabled(true);
    m_stopButton->setEnabled(false);
    m_localAddressEdit->setEnabled(true);
    m_portSpin->setEnabled(true);
    m_cameraAddressEdit->setEnabled(true);
}

void RtpPlayerWidget::onVideoFrame(std::shared_ptr<const MediaFrame> frame)
{
    if (!frame || !frame->buffer ||
        frame->pixel_format != PixelFormat::kRGB24 ||
        frame->width <= 0 || frame->height <= 0) {
        return;
    }

    const int stride =
        frame->stride[0] > 0 ? frame->stride[0] : frame->width * 3;
    QImage image(
        frame->buffer->Data(),
        frame->width,
        frame->height,
        stride,
        QImage::Format_RGB888);
    if (image.isNull()) {
        return;
    }

    m_lastFrame = image.copy();
    updateVideoPixmap();
    m_statusLabel->setText(
        tr("Playing %1x%2").arg(frame->width).arg(frame->height));
}

void RtpPlayerWidget::updateVideoPixmap()
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

void RtpPlayerWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateVideoPixmap();
}
