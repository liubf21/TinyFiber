#ifndef FIBER_H_
#define FIBER_H_

#define _XOPEN_SOURCE 700 // for ucontext.h on macOS

#include <memory>
#include <functional>
#include <ucontext.h>



class Fiber : public std::enable_shared_from_this<Fiber> { 
/* 
std::enable_shared_from_this is a template class that allows 
an object t that is currently managed by a std::shared_ptr to 
safely generate additional std::shared_ptr instances that refer 
to the same object as the original std::shared_ptr.
*/
public:
    typedef std::shared_ptr<Fiber> Ptr;

    enum State {
        READY,
        RUNNING,
        TERM
    };

    explicit Fiber(std::function<void()> cb, size_t stackSize = 0, bool runInScheduler = true);
    ~Fiber();

    void reset(std::function<void()> cb);
    void resume();
    void yield();

    uint64_t getId() const { return m_id; }
    State getState() const { return m_state; }

    static void SetCurrent(Fiber* f);
    static Fiber::Ptr GetCurrent();
    static uint64_t TotalFibers();
    static void MainFunc();
    static uint64_t GetFiberId();

private:
    Fiber();

    uint64_t m_id = 0;
    uint64_t m_stackSize = 0;
    State m_state = READY;
    ucontext_t m_ctx;
    void* m_stack = nullptr;
    std::function<void()> m_cb;
    bool m_runInScheduler = false;
};

#endif // FIBER_H_
