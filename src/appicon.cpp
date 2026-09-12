#include "appicon.h"

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QPixmap>

QIcon tailscaleLogoIcon()
{
    // This asset lives in a static library; referencing its generated init
    // symbol ensures the linker retains and registers the resource object.
    static const bool resourceInitialized = [] {
        Q_INIT_RESOURCE(tailswitch_assets);
        return true;
    }();
    Q_UNUSED(resourceInitialized);

    const QIcon source(QStringLiteral(":/tailswitch/assets/tailscale.svg"));
    const QColor foreground = QApplication::palette().color(QPalette::WindowText);
    QIcon result;
    for (const int size : {22, 32, 48, 64, 128}) {
        QPixmap pixmap = source.pixmap(size, size);
        if (pixmap.isNull()) {
            return {};
        }
        // SourceIn preserves the logo's solid/translucent alpha pattern while
        // adapting its fixed dark SVG fill to the active desktop palette.
        QPainter painter(&pixmap);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(pixmap.rect(), foreground);
        painter.end();
        result.addPixmap(pixmap);
    }
    return result;
}
