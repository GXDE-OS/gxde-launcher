#ifndef DOCKGEOMETRY_H
#define DOCKGEOMETRY_H

#include <QRect>

class QScreen;

namespace DockGeometry {

QRect toTargetScreen(const QRect &rawGeometry, QScreen *targetScreen,
                     int dockPosition, bool geometryIsForTargetScreen);

} // namespace DockGeometry

#endif // DOCKGEOMETRY_H
