#pragma once

class QMenu;
class QSystemTrayIcon;

// Keep the platform-provided context menu, and also open it on primary click.
void configureTrayMenu(QSystemTrayIcon &tray, QMenu &menu);
