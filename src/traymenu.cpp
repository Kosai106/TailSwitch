#include "traymenu.h"

#include <KStatusNotifierItem>
#include <QMenu>

void configureTrayMenu(KStatusNotifierItem &tray, QMenu &menu)
{
    tray.setStandardActionsEnabled(false);
    tray.setContextMenu(&menu);
    // Plasma reads ItemIsMenu over D-Bus and opens the exported menu itself.
    // Never call QMenu::popup here: a tray-only Wayland process has no focused
    // parent surface/input serial with which to create a grabbing popup.
    tray.setIsMenu(true);
}
