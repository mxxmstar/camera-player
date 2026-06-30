#include "ui/page_menu.h"
#include <QMouseEvent>

PageMenu::PageMenu(const QString& title, QWidget* parent)
    : QMenu(title, parent)
{
}

void PageMenu::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        QAction* action = actionAt(event->pos());
        if (action) {
            QMenu contextMenu;
            QAction* detachAction =
                contextMenu.addAction(QStringLiteral("独立页面"));
            QAction* selected = contextMenu.exec(event->globalPos());
            if (selected == detachAction) {
                emit detachPageRequested(action);
            }
            return;
        }
    }
    QMenu::mousePressEvent(event);
}
