#pragma once

#include <QMainWindow>
#include <QMenu>
#include <QAction>
#include <QComboBox>

class QStackedWidget;
class PageMenu;
class RtpPlayerWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private:
    void setupMenuBar();
    void setupToolBar();
    void setupCentralWidget();
    void setupStyle();
    void setupConnections();
    void switchToPage(int index);
    void detachPage(QAction* action);

    QMenu* m_fileMenu;
    PageMenu* m_pageMenu;
    QMenu* m_toolMenu;
    QMenu* m_helpMenu;

    QAction* m_backAction;
    QAction* m_forwardAction;
    QComboBox* m_langCombo;
    QWidget* m_navWidget;

    QStackedWidget* m_stackedWidget;
    RtpPlayerWidget* m_rtpPlayerWidget;
    QAction* m_rtpPlayerAction;
};
