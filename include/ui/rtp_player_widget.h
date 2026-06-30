#pragma once

#include <QWidget>
#include <QImage>

class QLineEdit;
class QSpinBox;
class QPushButton;
class QLabel;
class RtpPlayer;
class QResizeEvent;

class RtpPlayerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RtpPlayerWidget(QWidget* parent = nullptr);
    ~RtpPlayerWidget() override;

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupUi();
    void setupConnections();
    void updateVideoPixmap();
    void startRtpPlayer();
    void stopRtpPlayer();
    void onVideoFrame(const QImage& image);

    QLineEdit* m_localAddressEdit;
    QSpinBox* m_portSpin;
    QLineEdit* m_cameraAddressEdit;
    QPushButton* m_startButton;
    QPushButton* m_stopButton;
    QLabel* m_statusLabel;
    QLabel* m_videoLabel;
    QImage m_lastFrame;
    RtpPlayer* m_rtpPlayer;
};
