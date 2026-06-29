#pragma once

#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QAction>
#include <QComboBox>
#include <QMenu>
#include <QLabel>

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private:
    void setupMenuBar();
    void setupToolBar();
    void setupStyle();
    void setupConnections();

    QMenu* m_fileMenu;
    QMenu* m_pageMenu;
    QMenu* m_toolMenu;
    QMenu* m_helpMenu;

    QAction* m_backAction;
    QAction* m_forwardAction;
    QComboBox* m_langCombo;
    QWidget* m_navWidget;
};
