#pragma once

#include <QMenu>

class QMouseEvent;

class PageMenu : public QMenu
{
    Q_OBJECT
public:
    explicit PageMenu(const QString& title, QWidget* parent = nullptr);

signals:
    void detachPageRequested(QAction* action);

protected:
    void mousePressEvent(QMouseEvent* event) override;
};
