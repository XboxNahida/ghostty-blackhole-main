// wayland_surface_export.h — Export a wl_surface handle for Portal ScreenCast
#pragma once

#include <QString>
#include <QObject>
#include <memory>

struct wl_display;
struct wl_surface;
struct wl_registry;
struct zxdg_exporter_v2;
struct zxdg_exported_v2;

// Exports a wl_surface via xdg-foreign-unstable-v2 and returns a
// Wayland handle string suitable for Portal ScreenCast Start's parent_window.
// Usage: create, pass the GLFW-created wl_surface, call exportSurface().
class WaylandSurfaceExport : public QObject
{
    Q_OBJECT
public:
    explicit WaylandSurfaceExport(QObject *parent = nullptr);
    ~WaylandSurfaceExport() override;

    // Initialize with a wl_display and wl_surface (from GLFW).
    // Returns true if xdg-foreign-unstable-v2 is available.
    bool init(struct wl_display *display, struct wl_surface *surface);

    // Export the surface. Returns the handle string on success, empty on failure.
    // Signals exported() or exportFailed() when done.
    void exportSurface();

    // Get the exported handle (valid after exported() signal).
    QString handle() const { return m_handle; }

    // Protocol callback boundary, kept small so it can be verified without a
    // compositor in unit tests.
    static QString portalParentIdentifier(const char *exportedHandle);

signals:
    void exported(const QString &handle);
    void exportFailed(const QString &reason);

private:
    struct wl_display *m_display = nullptr;
    struct wl_surface *m_surface = nullptr;
    struct wl_registry *m_registry = nullptr;
    struct zxdg_exporter_v2 *m_exporter = nullptr;
    struct zxdg_exported_v2 *m_exported = nullptr;
    QString m_handle;
};
