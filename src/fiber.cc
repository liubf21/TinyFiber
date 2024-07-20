#include "fiber.h"

#include <atomic>


// static sylar::Logger::ptr g_logger = SYLAR_LOG_INFO_NAME("system");

static std::atomic<uint64_t> s_fiberId {0};
static std::atomic<uint64_t> s_fiberCount {0};

static thread_local Fiber* t_fiber = nullptr;
static thread_local Fiber::Ptr t_threadFiber = nullptr;

// static ConfigVar<uint32_t>::ptr g_fiberStackSize = 
//     Config::Lookup<uint32_t>("fiber.stack_size", 128 * 1024, "fiber stack size");
std::shared_ptr<u_int32_t> g_fiberStackSize = std::make_shared<u_int32_t>(128 * 1024);
    

class MallocStackAllocator {
public:
    static void* Alloc(size_t size) {
        return malloc(size);
    }

    static void Dealloc(void* vp, size_t size) {
        free(vp);
    }
};

using StackAllocator = MallocStackAllocator;

Fiber::Fiber() {
    SetCurrent(this);
    m_state = RUNNING;

    if (getcontext(&m_ctx)) {
        SYLAR_ASSERT2(false, "getcontext");
    }

    ++ s_fiberCount;
    m_id = s_fiberId ++;

    SYLAR_LOG_INFO << "Fiber::Fiber() main id=" << m_id << std::endl;
}

Fiber::Fiber(std::function<void()> callback, size_t stackSize, bool runInScheduler)
    : m_id(s_fiberId ++)
    , m_callback(callback) 
    , m_runInScheduler(runInScheduler) {
    ++ s_fiberCount;
    m_stackSize = stackSize ? stackSize : *g_fiberStackSize;
    m_stack = StackAllocator::Alloc(m_stackSize);

    if (getcontext(&m_ctx)) {
        SYLAR_ASSERT2(false, "getcontext");
    }

    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stackSize;

    makecontext(&m_ctx, &Fiber::MainFunc, 0);

    SYLAR_LOG_INFO << "Fiber::Fiber() id=" << m_id << std::endl;
}

Fiber::~Fiber() {
    -- s_fiberCount;
    if (m_stack) {
        SYLAR_ASSERT(m_state == TERM); 
        StackAllocator::Dealloc(m_stack, m_stackSize);
        SYLAR_LOG_INFO << "dealloc stack, id=" << m_id << std::endl;
    } else {
        SYLAR_ASSERT(!m_callback);
        SYLAR_ASSERT(m_state == RUNNING);

        Fiber* cur = t_fiber;
        if (cur == this) {
            SetCurrent(nullptr);
        }
    }
}

void Fiber::reset(std::function<void()> callback) {
    SYLAR_ASSERT(m_stack);
    SYLAR_ASSERT(m_state == TERM || m_state == READY);
    m_callback = callback;
    if (getcontext(&m_ctx)) {
        SYLAR_ASSERT2(false, "getcontext");
    }

    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stackSize;

    makecontext(&m_ctx, &Fiber::MainFunc, 0);
    m_state = READY;
}

void Fiber::resume() {
    SYLAR_ASSERT(m_state != TERM && m_state != RUNNING);
    SetCurrent(this);
    m_state = RUNNING;

    // if (m_runInScheduler) {
    //     if (swapcontext(&(Scheduler::GetMainFiber()->m_ctx), &m_ctx)) {
    //         SYLAR_ASSERT2(false, "swapcontext");
    //     }
    // } else {
        if (swapcontext(&(t_threadFiber->m_ctx), &m_ctx)) {
            SYLAR_ASSERT2(false, "swapcontext");
        }
    // }
}

void Fiber::yield() {
    SYLAR_ASSERT(m_state == RUNNING || m_state == TERM);
    SetCurrent(t_threadFiber.get());
    if (m_state != TERM) {
        m_state = READY;
    }

    // if (m_runInScheduler) {
    //     if (swapcontext(&m_ctx, &(Scheduler::GetMainFiber()->m_ctx))) {
    //         SYLAR_ASSERT2(false, "swapcontext");
    //     }
    // } else {
        if (swapcontext(&m_ctx, &(t_threadFiber->m_ctx))) {
            SYLAR_ASSERT2(false, "swapcontext");
        }
    // }
}
void Fiber::SetCurrent(Fiber* f) {
    t_fiber = f;
}

Fiber::Ptr Fiber::GetCurrent() {
    if (t_fiber) {
        return t_fiber->shared_from_this();
    }
    Fiber::Ptr main_fiber(new Fiber());
    SYLAR_ASSERT(t_fiber == main_fiber.get());
    t_threadFiber = main_fiber;
    return t_fiber->shared_from_this();
}

uint64_t Fiber::TotalFibers() {
    return s_fiberCount;
}

void Fiber::MainFunc() {
    Fiber::Ptr cur = GetCurrent();
    SYLAR_ASSERT(cur);

    cur->m_callback();
    cur->m_callback = nullptr;
    cur->m_state = TERM;

    auto raw_ptr = cur.get();
    cur.reset();
    raw_ptr->yield();
}

uint64_t Fiber::GetFiberId() {
    if (t_fiber) {
        return t_fiber->getId();
    }
    return 0;
}
