#include <QWindow>
#include <QGuiApplication>
#include <QHash>
#include <qpa/qplatformnativeinterface.h>
#include <wayland-client.h>

#include "layershell_styler.h"
#include "protocols/blur-client-protocol.h"
#include "protocols/dde-shell-client-protocol.h"

namespace Wayland {
namespace LayerShellStyler {

struct BindContext {
    dde_shell *ddeShell = nullptr;
    org_kde_kwin_blur_manager *blurManager = nullptr;
};

struct SurfaceState {
    wl_display *display = nullptr;
    dde_shell *ddeShell = nullptr;
    dde_shell_surface *shellSurface = nullptr;
    org_kde_kwin_blur_manager *blurManager = nullptr;
    org_kde_kwin_blur *blur = nullptr;
};

static QHash<QWindow *, SurfaceState> surfaceStates;

QRegion roundedRegion(const QRect &rect, int radius) {
    if (radius <= 0 || rect.isEmpty()) {
        return QRegion(rect);
    }

    QRegion region(rect.adjusted(radius, 0, -radius, 0));
    region += QRegion(rect.adjusted(0, radius, 0, -radius));

    const int diameter = radius * 2;
    region += QRegion(rect.x(), rect.y(), diameter, diameter, QRegion::Ellipse)
        & QRect(rect.x(), rect.y(), radius, radius);
    region += QRegion(rect.right() - diameter + 1, rect.y(), diameter,
                      diameter, QRegion::Ellipse)
        & QRect(rect.right() - radius + 1, rect.y(), radius, radius);
    region += QRegion(rect.x(), rect.bottom() - diameter + 1, diameter,
                      diameter, QRegion::Ellipse)
        & QRect(rect.x(), rect.bottom() - radius + 1, radius, radius);
    region += QRegion(rect.right() - diameter + 1,
                      rect.bottom() - diameter + 1, diameter, diameter,
                      QRegion::Ellipse)
        & QRect(rect.right() - radius + 1, rect.bottom() - radius + 1,
                radius, radius);
    return region;
}

static void registry_global(void *data, wl_registry *registry,
        uint32_t name, const char *interface, uint32_t version) {
    auto *ctx = static_cast<BindContext *>(data);
    if (strcmp(interface, dde_shell_interface.name) == 0) {
        ctx->ddeShell = static_cast<dde_shell *>(
            wl_registry_bind(registry, name, &dde_shell_interface, 2));
    } else if (strcmp(interface, org_kde_kwin_blur_manager_interface.name) == 0) {
        ctx->blurManager = static_cast<org_kde_kwin_blur_manager *>(
            wl_registry_bind(registry, name, &org_kde_kwin_blur_manager_interface, 1));
    }
}

static void registry_global_remove(void *, wl_registry *, uint32_t) {}

static const wl_registry_listener registry_listener = {
    registry_global,
    registry_global_remove,
};

void clear(QWindow *window) {
    const auto it = surfaceStates.find(window);
    if (it == surfaceStates.end()) {
        return;
    }

    const SurfaceState state = it.value();
    surfaceStates.erase(it);

    if (state.blur) {
        org_kde_kwin_blur_release(state.blur);
    }
    if (state.shellSurface) {
        dde_shell_surface_destroy(state.shellSurface);
    }
    if (state.blurManager) {
        org_kde_kwin_blur_manager_destroy(state.blurManager);
    }
    if (state.ddeShell) {
        dde_shell_destroy(state.ddeShell);
    }
    if (state.display) {
        wl_display_flush(state.display);
    }
}

void apply(QWindow *window, int radius, const QRegion &blurRegion) {
    if (!window || radius < 0) {
        return;
    }

    auto *native = QGuiApplication::platformNativeInterface();
    if (!native) {
        return;
    }

    auto *display = static_cast<wl_display *>(
        native->nativeResourceForWindow("display", nullptr));
    auto *surface = static_cast<wl_surface *>(
        native->nativeResourceForWindow("surface", window));
    auto *compositor = static_cast<wl_compositor *>(
        native->nativeResourceForIntegration("compositor"));

    if (!display || !surface) {
        return;
    }

    // A QWindow survives layer-shell surface recreation. Release every
    // protocol object tied to its previous wl_surface before binding anew.
    clear(window);

    wl_registry *registry = wl_display_get_registry(display);
    if (!registry) {
        return;
    }
    BindContext ctx;
    wl_registry_add_listener(registry, &registry_listener, &ctx);
    wl_display_roundtrip(display);
    wl_registry_destroy(registry);

    SurfaceState state;
    state.display = display;
    state.ddeShell = ctx.ddeShell;
    state.blurManager = ctx.blurManager;

    if (radius > 0 && state.ddeShell) {
        state.shellSurface = dde_shell_get_shell_surface(state.ddeShell, surface);
        if (state.shellSurface) {
            float vals[2] = { static_cast<float>(radius), static_cast<float>(radius) };
            wl_array dataArr;
            wl_array_init(&dataArr);
            float *arr_data = static_cast<float *>(
                wl_array_add(&dataArr, sizeof(float) * 2));
            arr_data[0] = vals[0];
            arr_data[1] = vals[1];
            dde_shell_surface_set_property(
                state.shellSurface,
                DDE_SHELL_PROPERTY_WINDOWRADIUS,
                &dataArr);
            wl_array_release(&dataArr);

            wl_array emptyArr;
            wl_array_init(&emptyArr);
            dde_shell_surface_set_property(
                state.shellSurface,
                DDE_SHELL_PROPERTY_NOTITLEBAR,
                &emptyArr);
            wl_array_release(&emptyArr);
        }
    }

    if (state.blurManager) {
        state.blur = org_kde_kwin_blur_manager_create(state.blurManager, surface);
        if (state.blur) {
            wl_region *region = nullptr;
            if (!blurRegion.isEmpty() && compositor) {
                region = wl_compositor_create_region(compositor);
                for (const QRect &rect : blurRegion) {
                    wl_region_add(region, rect.x(), rect.y(), rect.width(), rect.height());
                }
            }
            org_kde_kwin_blur_set_region(state.blur, region);
            if (region) {
                wl_region_destroy(region);
            }
            org_kde_kwin_blur_set_strength(state.blur, 300);
            org_kde_kwin_blur_commit(state.blur);
        }
    }

    surfaceStates.insert(window, state);
    wl_display_flush(display);
}

}  // namespace LayerShellStyler
}  // namespace Wayland
