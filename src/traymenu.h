#pragma once

class KStatusNotifierItem;
class QMenu;

// Export a menu-only StatusNotifierItem so Plasma renders the menu for both
// primary and secondary clicks. The item takes ownership of the heap menu.
void configureTrayMenu(KStatusNotifierItem &tray, QMenu &menu);
