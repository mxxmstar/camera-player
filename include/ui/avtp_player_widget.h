#pragma once

#include <QImage>
#include <QWidget>

#include <memory>

class AvtpPlayer;
class MediaFrame;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QResizeEvent;

class AvtpPlayerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AvtpPlayerWidget(QWidget* parent = nullptr);
    ~AvtpPlayerWidget() override;

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupUi();
    void setupConnections();
    void refreshCaptureDevices();
    void captureBroadcastMac();
    void updateControlState();
    void updateVideoPixmap();
    void startAvtpPlayer();
    void stopAvtpPlayer();
    void onVideoFrame(std::shared_ptr<const MediaFrame> frame);

    QComboBox* m_captureDeviceCombo;
    QLineEdit* m_sourceMacEdit;
    QLineEdit* m_streamIdEdit;
    QPushButton* m_refreshDevicesButton;
    QPushButton* m_startButton;
    QPushButton* m_stopButton;
    QLabel* m_statusLabel;
    QLabel* m_videoLabel;
    QImage m_lastFrame;
    std::unique_ptr<AvtpPlayer> m_avtpPlayer;
};