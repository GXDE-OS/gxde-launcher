#include "dockgeometry.h"

#include <QGuiApplication>
#include <QScreen>

#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>

namespace {

constexpr int DockTop = 0;
constexpr int DockRight = 1;
constexpr int DockBottom = 2;
constexpr int DockLeft = 3;

QRect fromRawGeometry(const QRect &rawGeometry, QScreen *screen)
{
    if (!screen)
        return rawGeometry;

    const qreal ratio = screen->devicePixelRatio();
    const QPoint origin = screen->geometry().topLeft();
    const QPoint topLeft(
        origin.x() + qRound((rawGeometry.x() - origin.x()) / ratio),
        origin.y() + qRound((rawGeometry.y() - origin.y()) / ratio));
    const QSize size(qRound(rawGeometry.width() / ratio),
                     qRound(rawGeometry.height() / ratio));
    return QRect(topLeft, size);
}

} // namespace

namespace DockGeometry {

QRect toTargetScreen(const QRect &rawGeometry, QScreen *targetScreen,
                     int dockPosition, bool geometryIsForTargetScreen)
{
    if (!rawGeometry.isValid() || !targetScreen)
        return rawGeometry;

    // ShowOnScreen supplies geometry for targetScreen.  The legacy Dock
    // FrontendWindowRect property, however, always describes the primary
    // screen and must be translated before it is used on another output.
    QScreen *sourceScreen = geometryIsForTargetScreen
        ? targetScreen
        : QGuiApplication::primaryScreen();
    if (!sourceScreen)
        sourceScreen = targetScreen;

    QRect logical = fromRawGeometry(rawGeometry, sourceScreen);
    if (geometryIsForTargetScreen)
        return logical;

    const QRect sourceRect = sourceScreen->geometry();
    const QRect targetRect = targetScreen->geometry();
    QSize size = logical.size();

    size.setWidth(qMin(size.width(), targetRect.width()));
    size.setHeight(qMin(size.height(), targetRect.height()));

    // Efficient/classic docks span an entire edge. Preserve that property
    // when the target output has a different size.
    if ((dockPosition == DockTop || dockPosition == DockBottom) &&
        size.width() >= sourceRect.width() - 1) {
        size.setWidth(targetRect.width());
    } else if ((dockPosition == DockLeft || dockPosition == DockRight) &&
               size.height() >= sourceRect.height() - 1) {
        size.setHeight(targetRect.height());
    }

    QPoint topLeft;
    switch (dockPosition) {
    case DockTop:
        topLeft = QPoint(targetRect.left() + (targetRect.width() - size.width()) / 2,
                         targetRect.top());
        break;
    case DockRight:
        topLeft = QPoint(targetRect.right() - size.width() + 1,
                         targetRect.top() + (targetRect.height() - size.height()) / 2);
        break;
    case DockBottom:
        topLeft = QPoint(targetRect.left() + (targetRect.width() - size.width()) / 2,
                         targetRect.bottom() - size.height() + 1);
        break;
    case DockLeft:
        topLeft = QPoint(targetRect.left(),
                         targetRect.top() + (targetRect.height() - size.height()) / 2);
        break;
    default:
        return logical;
    }

    return QRect(topLeft, size);
}

QRect currentDockGeometry()
{
    // The dock may be registered under different service/path/interface names
    // depending on the desktop environment.  Try the known ones and read the
    // "geometry" property (a (iiii) struct QRect understands natively).
    struct {
        const char *service;
        const char *path;
        const char *interface;
    } const candidates[] = {
        {"com.deepin.dde.Dock", "/com/deepin/dde/Dock", "com.deepin.dde.Dock"},
        {"top.gxde.daemon.dock", "/top/gxde/daemon/dock", "top.gxde.daemon.dock"},
    };

    for (const auto &c : candidates) {
        QDBusInterface dock(c.service, c.path, "org.freedesktop.DBus.Properties",
                            QDBusConnection::sessionBus());
        if (!dock.isValid())
            continue;

        for (const char *prop : {"geometry", "FrontendWindowRect"}) {
            QDBusReply<QDBusVariant> reply = dock.call("Get", c.interface, prop);
            if (reply.isValid())
                return qdbus_cast<QRect>(reply.value().variant());
        }
    }

    return QRect();
}

} // namespace DockGeometry
