#include "scheduler.h"

static thread_local Scheduler* t_scheduler = nullptr;
static thread_local Fiber* t_scheduler_fiber = nullptr;

Scheduler::Scheduler(size_t threads, bool use_caller, const std::string& name)
    : m_useCaller(use_caller), m_name(name) {
    SYLAR_ASSERT(threads > 0);

    if (use_caller) {
        Fiber::GetCurrent();
        --threads;

        SYLAR_ASSERT(GetCurrentFiber() == nullptr);
        t_scheduler = this;

        m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, false));
        Thread::SetName(m_name);

        t_scheduler_fiber = m_rootFiber.get();
        m_rootThread = GetThreadId();
        m_threadIds.push_back(m_rootThread);
    } else {
        m_rootThread = -1;
    }

    m_threadCount = threads;
}

Scheduler *Scheduler::GetCurrentScheduler() {
    return t_scheduler;
}

void Scheduler::start() {
    SYLAR_LOG_INFO << "Scheduler start" << std::endl;
    MutexType::Lock lock(m_mutex);
    if (m_stopping) {
        SYLAR_LOG_ERROR << "Scheduler is stopping, cannot start" << std::endl;
        return;
    }

    SYLAR_ASSERT(m_threads.empty());

    m_threads.resize(m_threadCount);
    for (size_t i = 0; i < m_threadCount; ++i) {
        m_threads[i].reset(new Thread(std::bind(&Scheduler::run, this), m_name + "_" + std::to_string(i)));
        m_threadIds.push_back(m_threads[i]->getId());
    }
}

void Scheduler::run() {
    SYLAR_LOG_INFO << "Scheduler run" << std::endl;
    setThis();
    if(GetThreadId() != m_rootThread){
        t_scheduler_fiber = Fiber::GetCurrent().get();
    }
    Fiber::Ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle,this)))
    Fiber::Ptr cb_fiber;
    
    ScheduleTask task;
    while (true){
        task.reset();
        bool tickle_me = false;
        {
            MutexType::Lock lock(m_mutex);
            auto it = m_tasks.begin();
            while (it != m_tasks.end()){
                if(it->thread != -1 && it->thread != GetThreadId()){
                    ++it;
                    tickle_me = true;
                    continue;
                }
                SYLAR_ASSERT(it->fiber || it->callback);
                if(it->fiber ){
                    SYLAR_ASSERT(it->fiber->getState() != Fiber::READY);
                }
                task = *it;
                m_tasks.erase(it);
                ++m_activeThreadCount;
                break;
            }
            tickle_me |= (it != m_tasks.end());
        }
        if(tickle_me){
            tickle();
        }
        if(task.fiber) {
            task.fiber->resume();
            --m_activeThreadCount;
            task.reset();
        } else if(task.callback){
            if(cb_fiber){
                cb_fiber->reset(task.callback);
            } else {
                cb_fiber.reset(new Fiber(task.callback));
            }
            task.reset();
            cb_fiber->resume();
            --m_activeThreadCount;
            cb_fiber.reset();
        } else {
            if(idle_fiber->getState() == Fiber::TERM){
                SYLAR_LOG_INFO << "idle fiber term";
                break;
            }
            ++m_idleThreadCount;
            idle_fiber->resume();
            --m_idleThreadCount;
        }
    }

    SYLAR_LOG_INFO << "Scheduler::run end" << std::endl;
}

void Scheduler::stop() {
    SYLAR_LOG_INFO << "Scheduler stop" << std::endl;
    if(m_stopping){
        SYLAR_LOG_ERROR << "Scheduler is stopping, cannot stop again" << std::endl;
        return;
    }
    m_stopping = true;

    if (m_useCaller) {
        SYLAR_ASSERT(GetCurrentScheduler() == this);
    } else {
        SYLAR_ASSERT(GetCurrentScheduler() != this);
    }
    
    for(size_t i = 0; i < m_threadCount; ++i){
        tickle();
    }

    if (m_rootFiber) {
        tickle();
    }

    if (m_rootFiber) {
        m_rootFiber->resume();
        SYLAR_LOG_INFO << "m_rootFiber end" << std::endl;
    }

    std::vector<Thread::Ptr> threads;
    {
        MutexType::Lock lock(m_mutex);
        threads.swap(m_threads);
    }
    for (auto& i : threads) {
        i->join();
    }
}
