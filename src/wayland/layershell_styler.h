#ifndef LAYERSHELL_STYLER_H
#define LAYERSHELL_STYLER_H

#include <QRegion>

class QWindow;

namespace Wayland {
namespace LayerShellStyler {

void apply(QWindow *window, int radius, const QRegion &blurRegion = QRegion());
void clear(QWindow *window);

QRegion roundedRegion(const QRect &rect, int radius);

}  // namespace LayerShellStyler
}  // namespace Wayland

#endif  // LAYERSHELL_STYLER_H
