#include "fiber.h"
#include <list>
#include <iostream>

class Scheduler {
public:
    void schedule(Fiber::Ptr task) {
        m_tasks.push_back(task);
    }

    void run() {
        Fiber::Ptr task;
        auto it = m_tasks.begin();
        while (it != m_tasks.end()) {
            task = *it;
            it = m_tasks.erase(it);
            task->resume();
        }
    }

private:
    std::list<Fiber::Ptr> m_tasks;
};

void test_fiber(int i) {
    std::cout << "hello world " << i << std::endl;
}

int main() {
    Fiber::GetCurrent();

    Scheduler sc;

    for (int i = 0; i < 10; ++i) {
        Fiber::Ptr f(new Fiber(std::bind(test_fiber, i)));
        sc.schedule(f);
    }

    sc.run();

    return 0;
}
