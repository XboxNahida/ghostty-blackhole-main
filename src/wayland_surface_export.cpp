// wayland_surface_export.cpp — Export a wl_surface handle for Portal ScreenCast
#include "wayland_surface_export.h"

#include <wayland-client.h>
#include "xdg-foreign-unstable-v2.h"

#include <QDebug>
#include <QVariant>
#include <cstring>

WaylandSurfaceExport::WaylandSurfaceExport(QObject *parent)
    : QObject(parent)
{
}

WaylandSurfaceExport::~WaylandSurfaceExport()
{
    if (m_exported) zxdg_exported_v2_destroy(m_exported);
    if (m_exporter) zxdg_exporter_v2_destroy(m_exporter);
    if (m_registry) wl_registry_destroy(m_registry);
}

namespace {
void registryGlobal(void *data, wl_registry *registry, uint32_t name,
                    const char *interface, uint32_t version)
{
    auto *self = static_cast<WaylandSurfaceExport *>(data);
    if (strcmp(interface, zxdg_exporter_v2_interface.name) == 0) {
        const uint32_t bindVersion = qMin(version, uint32_t(1));
        auto *exporter = static_cast<zxdg_exporter_v2 *>(
            wl_registry_bind(registry, name, &zxdg_exporter_v2_interface, bindVersion));
        self->setProperty("xdgExporter", QVariant::fromValue<qulonglong>(
            reinterpret_cast<qulonglong>(exporter)));
    }
}
void registryRemove(void *, wl_registry *, uint32_t) {}
const wl_registry_listener registryListener{registryGlobal, registryRemove};

void exportedHandle(void *data, zxdg_exported_v2 *, const char *handle)
{
    auto *self = static_cast<WaylandSurfaceExport *>(data);
    const QString parent = WaylandSurfaceExport::portalParentIdentifier(handle);
    if (parent.isEmpty()) emit self->exportFailed(QStringLiteral("Compositor returned an empty xdg-foreign handle"));
    else {
        self->setProperty("portalParent", parent);
        emit self->exported(parent);
    }
}
const zxdg_exported_v2_listener exportedListener{exportedHandle};
}

QString WaylandSurfaceExport::portalParentIdentifier(const char *exportedHandle)
{
    if (!exportedHandle || !*exportedHandle) return {};
    return QStringLiteral("wayland:%1").arg(QString::fromUtf8(exportedHandle));
}

bool WaylandSurfaceExport::init(struct wl_display *display, struct wl_surface *surface)
{
    if (!display || !surface) return false;
    m_display = display;
    m_surface = surface;
    m_registry = wl_display_get_registry(display);
    if (!m_registry) return false;
    wl_registry_add_listener(m_registry, &registryListener, this);
    if (wl_display_roundtrip(display) < 0) return false;
    m_exporter = reinterpret_cast<zxdg_exporter_v2 *>(
        property("xdgExporter").toULongLong());
    setProperty("xdgExporter", {});
    return m_exporter != nullptr;
}

void WaylandSurfaceExport::exportSurface()
{
    if (!m_display || !m_surface) {
        emit exportFailed(QStringLiteral("Wayland display or surface not initialized"));
        return;
    }

    m_exported = zxdg_exporter_v2_export_toplevel(m_exporter, m_surface);
    if (!m_exported) { emit exportFailed(QStringLiteral("xdg-foreign export_toplevel failed")); return; }
    zxdg_exported_v2_add_listener(m_exported, &exportedListener, this);
    if (wl_display_roundtrip(m_display) < 0) {
        emit exportFailed(QStringLiteral("Wayland roundtrip failed while exporting parent"));
        return;
    }
    m_handle = property("portalParent").toString();
    setProperty("portalParent", {});
}
