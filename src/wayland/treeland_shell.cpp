#include "treeland_shell.h"

#include <wayland-client.h>

#include <QGuiApplication>
#include <qpa/qplatformnativeinterface.h>

#include "protocols/treeland-dde-shell-v1-client-protocol.h"

namespace Wayland {
namespace TreelandDdeShell {

namespace {

struct Context {
    wl_display *display = nullptr;
    treeland_dde_shell_manager_v1 *manager = nullptr;
};

Context g_ctx;

void registry_global(void *data, wl_registry *registry, uint32_t name,
                     const char *interface, uint32_t version) {
    auto *ctx = static_cast<Context *>(data);
    if (strcmp(interface, treeland_dde_shell_manager_v1_interface.name) == 0) {
        const uint32_t v = version > treeland_dde_shell_manager_v1_interface.version
                                ? treeland_dde_shell_manager_v1_interface.version
                                : version;
        ctx->manager = static_cast<treeland_dde_shell_manager_v1 *>(
            wl_registry_bind(registry, name, &treeland_dde_shell_manager_v1_interface, v));
    }
}

void registry_global_remove(void *, wl_registry *, uint32_t) {}

const wl_registry_listener registry_listener = {registry_global, registry_global_remove};

wl_surface *surfaceFromWindow(QWindow *window) {
    if (!window)
        return nullptr;
    auto *native = QGuiApplication::platformNativeInterface();
    if (!native)
        return nullptr;
    return static_cast<wl_surface *>(
        native->nativeResourceForWindow("surface", window));
}

}  // namespace

void init() {
    auto *native = QGuiApplication::platformNativeInterface();
    if (!native)
        return;
    wl_display *display =
        static_cast<wl_display *>(native->nativeResourceForWindow("display", nullptr));
    if (!display)
        return;

    g_ctx.display = display;
    wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, &g_ctx);
    wl_display_roundtrip(display);
    wl_registry_destroy(registry);
    // manager 可能为 nullptr（合成器不支持），后续调用安全空转。
}

bool available() {
    return g_ctx.manager != nullptr;
}

void setAutoPlacement(QWindow *window, int yOffset) {
    if (!g_ctx.manager)
        return;
    wl_surface *surface = surfaceFromWindow(window);
    if (!surface)
        return;

    treeland_dde_shell_surface_v1 *shell =
        treeland_dde_shell_manager_v1_get_shell_surface(g_ctx.manager, surface);
    if (!shell)
        return;

    // 让合成器把该 surface 摆到全局光标处，并置于普通窗口之上。
    treeland_dde_shell_surface_v1_set_role(shell,
                                           TREELAND_DDE_SHELL_SURFACE_V1_ROLE_OVERLAY);
    treeland_dde_shell_surface_v1_set_auto_placement(shell,
                                                     static_cast<uint32_t>(yOffset));

    if (g_ctx.display)
        wl_display_flush(g_ctx.display);
}

}  // namespace TreelandDdeShell
}  // namespace Wayland
