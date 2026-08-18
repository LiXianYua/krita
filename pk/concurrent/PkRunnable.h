#pragma once

// 替代 <QRunnable>。前置声明存在（KisRunnableStrokeJobData.h），
// 必须是真正的 class，不能是 using 别名。
class PkRunnable {
public:
    virtual ~PkRunnable() = default;
    virtual void run() = 0;

    void setAutoDelete(bool autoDelete) { m_autoDelete = autoDelete; }
    bool autoDelete() const { return m_autoDelete; }

private:
    bool m_autoDelete = true;
};
