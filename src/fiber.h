#ifndef FIBER_H_
#define FIBER_H_

#define _XOPEN_SOURCE 700 // for ucontext.h on macOS

#include <iostream>
#include <memory>
#include <functional>
#include <ucontext.h>


#define SYLAR_LOG_INFO std::cout << "[INFO] " << __FILE__ << ":" << __LINE__ << " "

#define SYLAR_ASSERT(x) \
    if (!(x)) { \
        std::cout << "ASSERTION: " << #x << std::endl; \
        std::cout << "file: " << __FILE__ << std::endl; \
        std::cout << "line: " << __LINE__ << std::endl; \
        abort(); \
    }

#define SYLAR_ASSERT2(x, w) \
    if (!(x)) { \
        std::cout << "ASSERTION: " << #x << std::endl; \
        std::cout << "file: " << __FILE__ << std::endl; \
        std::cout << "line: " << __LINE__ << std::endl; \
        std::cout << "message: " << w << std::endl; \
        abort(); \
    }

class Fiber : public std::enable_shared_from_this<Fiber> { 
/* 
std::enable_shared_from_this is a template class that allows 
an object to create a shared_ptr to itself, preventing the object
from being deleted by a shared_ptr that is the last owner of the object.
*/
public:
    typedef std::shared_ptr<Fiber> Ptr;

    enum State {
        READY,
        RUNNING,
        TERM
    };

    // create a sub-fiber
    explicit Fiber(std::function<void()> callback, size_t stackSize = 0, bool runInScheduler = true);
    
    ~Fiber();

    void reset(std::function<void()> callback);
    void resume();
    void yield();

    uint64_t getId() const { return m_id; }
    State getState() const { return m_state; }

    static void SetCurrent(Fiber* f);
    static Fiber::Ptr GetCurrent(); // get the current fiber, initialize the main fiber if it doesn't exist
    static uint64_t TotalFibers();
    static void MainFunc();
    static uint64_t GetFiberId();

private:

    // initialize the main fiber 
    Fiber();

    ucontext_t m_ctx;

    State m_state = READY;
    bool m_runInScheduler = false;

    uint64_t m_id = 0;
    uint64_t m_stackSize = 0;
    void* m_stack = nullptr;

    std::function<void()> m_callback;
};

#endif // FIBER_H_
