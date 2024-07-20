#include "fiber.h"

#include <string>
#include <vector>

 
void run_in_fiber2() {
    SYLAR_LOG_INFO << "run_in_fiber2 begin" << std::endl;
    SYLAR_LOG_INFO << "run_in_fiber2 end" << std::endl;
}
 
void run_in_fiber() {
    SYLAR_LOG_INFO << "run_in_fiber begin" << std::endl;
 
    /**
     * 非对称协程，子协程不能创建并运行新的子协程，下面的操作是有问题的，
     * 子协程再创建子协程，原来的主协程就跑飞了
     */
    Fiber::Ptr fiber(new Fiber(run_in_fiber2));
    fiber->resume();
 
    SYLAR_LOG_INFO << "run_in_fiber end" << std::endl;
}
 
int main(int argc, char *argv[]) {

    SYLAR_LOG_INFO << "main begin" << std::endl;
 
    Fiber::GetCurrent();
 
    Fiber::Ptr fiber(new Fiber(run_in_fiber));
    fiber->resume();
 
    SYLAR_LOG_INFO << "main end" << std::endl;
    return 0;
}