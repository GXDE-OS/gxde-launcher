#ifndef LAYERSHELL_STYLER_H
#define LAYERSHELL_STYLER_H

class QWindow;

namespace Wayland {
namespace LayerShellStyler {

void apply(QWindow *window, int radius, bool enableBlur);

}  // namespace LayerShellStyler
}  // namespace Wayland

#endif  // LAYERSHELL_STYLER_H
