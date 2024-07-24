#ifndef __SYLAR_IOMANAGER_H__
#define __SYLAR_IOMANAGER_H__

#include "scheduler.h"
#include "timer.h"

class IOManager : public Scheduler {
public:
    typedef std::shared_ptr<IOManager> ptr;
    typedef RWMutex RWMutexType;

    enum Event {
        NONE = 0x0,
        READ = 0x1,
        WRITE = 0x4
    };

private:
    struct FdContext {
        typedef Mutex MutexType;

        struct EventContext {
            Scheduler* scheduler = nullptr;
            Fiber::Ptr fiber;
            std::function<void()> cb;
        };

        EventContext& getEventContext(Event event);

        void resetEventContext(EventContext& ctx);

        void triggerEvent(Event event);

        EventContext read;
        EventContext write;
        int fd = 0;
        Event events = NONE;
        MutexType mutex;
    };

    int m_epfd = 0;
    int m_tickleFds[2]; // fd[0] read, fd[1] write
    std::atomic<size_t> m_pendingEventCount = {0};
    RWMutexType m_mutex;
    std::vector<FdContext*> m_fdContexts;

}

#endif
