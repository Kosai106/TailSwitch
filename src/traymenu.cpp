#include "traymenu.h"

#include <QCursor>
#include <QMenu>
#include <QSystemTrayIcon>

void configureTrayMenu(QSystemTrayIcon &tray, QMenu &menu)
{
    tray.setContextMenu(&menu);
    QObject::connect(&tray, &QSystemTrayIcon::activated, &menu,
                     [&menu](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger && !menu.isVisible()) {
            // Use non-blocking popup, not exec(): clipboard replies and status
            // updates must keep flowing while the menu is open.
            // Final placement on Wayland remains compositor-controlled.
            menu.popup(QCursor::pos());
        }
    });
}
