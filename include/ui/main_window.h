#pragma once

#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QAction>
#include <QComboBox>
#include <QImage>
#include <QLineEdit>
#include <QMenu>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

class QResizeEvent;
class RtpPlayer;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void setupMenuBar();
    void setupToolBar();
    void setupCentralWidget();
    void setupStyle();
    void setupConnections();
    void updateVideoPixmap();
    void startRtpPlayer();
    void stopRtpPlayer();
    void onVideoFrame(const QImage& image);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    QMenu* m_fileMenu;
    QMenu* m_pageMenu;
    QMenu* m_toolMenu;
    QMenu* m_helpMenu;

    QAction* m_backAction;
    QAction* m_forwardAction;
    QComboBox* m_langCombo;
    QWidget* m_navWidget;

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
