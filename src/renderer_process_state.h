#pragma once

class RendererProcessState {
public:
    bool processStarted() { const bool first = m_running++ == 0; return first; }
    bool processStopped() { if (m_running > 0) --m_running; return m_running == 0; }
    int runningCount() const { return m_running; }
    void reset() { m_running = 0; }
private:
    int m_running = 0;
};
