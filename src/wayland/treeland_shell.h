#ifndef TREELAND_SHELL_H
#define TREELAND_SHELL_H

class QWindow;

namespace Wayland {
namespace TreelandDdeShell {

// 绑定 treeland_dde_shell_manager_v1 全局（只需在启动时调用一次）。
// 若合成器不支持该协议（例如运行在其它 Wayland 合成器或 X11 下），
// 后续 setAutoPlacement 会是空操作，不影响原有逻辑。
void init();

// 是否拿到了合成器提供的 treeland_dde_shell_manager_v1 全局。
bool available();

// 让菜单 surface 由合成器按其“全局光标位置”摆放（专为右键上下文菜单设计）。
// 合成器会把 surface 左上角放到光标处，并自动校正到输出可见区域内，
// 因此无需客户端自己计算全局坐标、也无需手动翻转。
//   yOffset: 相对光标额外向下的偏移（0 表示左上角对齐光标）。
void setAutoPlacement(QWindow *window, int yOffset = 0);

}  // namespace TreelandDdeShell
}  // namespace Wayland

#endif  // TREELAND_SHELL_H
