#pragma once

#include <QtGlobal>

class GnomeIdleBackend
{
public:
    virtual ~GnomeIdleBackend() = default;
    virtual quint32 addIdleWatch(quint64 intervalMs) = 0;
    virtual quint32 addActiveWatch() = 0;
    virtual void removeWatch(quint32 id) = 0;
    virtual bool screenSaverActive(bool *ok) = 0;
    virtual bool idleServiceAvailable() const = 0;
    virtual bool screenSaverAvailable() const = 0;
    virtual void refresh() = 0;
};
