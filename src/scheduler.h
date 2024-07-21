#ifndef SCHDULER_H_
#define SCHDULER_H_

#include "fiber.h"

#include <memory>
#include <mutex>
#include <list>

/*
* @brief Fiber scheduler

* @details a M-N scheduler, M is the number of fibers, N is the number of threads
* @details includes a thread pool 
*/
class Scheduler {
public:
    typedef std::shared_ptr<Scheduler> Ptr;
    typedef Mutex MutexType;

    Scheduler(size_t threads = 1, bool use_caller = true, const std::string& name = "Scheduler");

    virtual ~Scheduler();

    const std::string& getName() const { return m_name; }

    static Scheduler* GetCurrentScheduler();

    static Fiber* GetMainFiber();

    template <class FiberOrCallback>
    void schedule(FiberOrCallback fc, int thread = -1) {
        bool need_tickle = false;
        {
            MutexType::Lock lock(m_mutex);
            need_tickle = scheduleNoLock(fc, thread);
        }

        if (need_tickle) {
            tickle();
        }
    }

    void start();

    void stop();

protected:
    virtual void tickle();

    void run();

    virtual void idle();

    virtual bool stopping();

    virtual void setThis();

    bool hasIdleThreads() const { return m_idleThreadCount > 0; }

private:
    struct ScheduleTask {
        int thread;
        Fiber::Ptr fiber;
        std::function<void()> callback;

        ScheduleTask(int thread, Fiber::Ptr f)
            : thread(thread), fiber(f) {}

        ScheduleTask(int thread, Fiber::Ptr* f) 
            : thread(thread) {
                fiber.swap(*f);
            }

        ScheduleTask(int thread, std::function<void()> f)
            : thread(thread), callback(f) {}

        ScheduleTask(): thread(-1) {}

        void reset() {
            thread = -1;
            fiber = nullptr;
            callback = nullptr;
        }
    };

    std::string m_name;
    MutexType m_mutex;
    std::vector<Thread::Ptr> m_threads;
    std::list<ScheduleTask> m_tasks;
    std::vector<int> m_threadIds;
    size_t m_threadCount = 0;
    std::atomic<size_t> m_activeThreadCount = {0};
    std::atomic<size_t> m_idleThreadCount = {0};

    bool m_useCaller;
    Fiber::Ptr m_rootFiber;
    int m_rootThread = 0;

    bool m_stopping = false;
};
#endif // SCHDULER_H_
