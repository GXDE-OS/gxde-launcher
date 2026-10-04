#ifndef DOCKGEOMETRY_H
#define DOCKGEOMETRY_H

#include <QRect>

class QScreen;

namespace DockGeometry {

QRect toTargetScreen(const QRect &rawGeometry, QScreen *targetScreen,
                     int dockPosition, bool geometryIsForTargetScreen);

// Reads the dock geometry directly from the running dock service.  The legacy
// DBusDock proxy (frontendRect()) targets an interface that is not exposed by
// the current dock, so it always returns an empty rect; this provides a
// reliable fallback using the dock's real "geometry" property.
QRect currentDockGeometry();

} // namespace DockGeometry

#endif // DOCKGEOMETRY_H
