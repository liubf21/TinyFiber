# TinyFiber

⾯试中，⾯试官经常会问这样的问题，“你知道线程和进程区别吗？”然后会紧接着追问“你了解协程吗？协程和进程、线程⼜有什么区别？”

我们通过基础知识的学习和如 WebServer 此类项⽬，已经对进程和线程有了⽐较深的理解，但对协程相关知识却知之甚少。

协程作为⼀种强⼤的并发编程技术，可以在需要处理⼤量I/O操作或者并发任务的情况下提⾼程序的性能和可维护性。在许多场景应⽤⼴泛，通过实现协程库的项目，来提高自己 C++ 编程能⼒、对操作系统和计算机网络有更深的理解。

## 前置知识

- 编程语言：C++11 的常见特性，如智能指针、lambda 表达式、std::function、std::thread 等
- 操作系统和 Linux 常见系统调用
- 计算机网络基础知识：Linux 下的⽹络编程，常⻅⽹络编程 API、⽹络 IO，IO 多路复⽤、⾼性能⽹络模式：Reactor 和 Proactor 
- 

## 协程基础知识

通用的说法是**协程**是一种“轻量级线程”、“用户态线程”。

每个协程在创建时都会指定一个入口函数，这点可以类比线程。协程的本质就是*函数和函数运行状态的组合*。

协程和函数的不同之处是，函数一旦被调用，只能从头开始执行，直到函数执行结束退出，而协程则可以执行到一半就退出（yield），暂时让出 CPU 执行权，在后面恰当的时机重新恢复运行（resume），在这段时间其他的协程可以获得 CPU 并运行。

协程能够 yield、resume 的关键是存储了函数在 yield 时间点的执行状态，即**协程上下文**。

协程上下文包含了函数在当前执行状态下的全部 CPU 寄存器的值，记录了函数栈帧、代码的执行位置等信息，如果把这些寄存器的值重新设置给 CPU，就相当于重新恢复了函数的运行。

单线程环境下，协程的 yield 和 resume 一定是同步进行的，一个协程的 yield 一定对应另一个协程的 resume，因为线程不可能没有执行主体，而且协程的 yield 和 resume 是完全由应用程序来控制的。（线程创建后，其运行和调度由操作系统自动完成）

### 对称协程和非对称协程

-  对称协程，协程可以不受限制地将控制权交给其他协程。任何一个协程都是相互独立且平等的，调度权可以在任意协程之间转移。
-  非对称协程，协程之间存在类似堆栈的调用方-被调用关系，协程出让调度权的目标只能是它的调用者。

### 有栈协程与无栈协程

- 有栈协程
  - 独立栈
  - 共享栈
- 无栈协程


### 协程的优缺点

- 优点
  - 提高资源利用率，提高程序并发性能
  - 简化异步编程逻辑
- 缺点
  - 无法利用多核资源

## 协程库实现

ucontext 机制是 GNU C 提供的一组创建、保存、切换用户态执行上下文的 API，这是协程能够随时切换和恢复的关键。

```c
typedef struct ucontext {
    struct ucontext *uc_link; // 后继上下文
    sigset_t         uc_sigmask; // 信号屏蔽字
    stack_t          uc_stack; // 栈信息
    mcontext_t       uc_mcontext; // 平台相关的上下文具体内容，包含寄存器信息
    ...
} ucontext_t;

// 获取当前上下文
int getcontext(ucontext_t *ucp); 

// 设置当前上下文，函数正常不会返回（失败才返回-1），而是跳转到 ucp 上下文对应的函数中执行，相当于函数调用
int setcontext(const ucontext_t *ucp); 

// 修改由 getcontext 获取的上下文，使其与 func 函数关联
// 调用 makecontext 之前，必须手动设置 ucp->uc_stack，即给 ucp 分配一段内存空间作为栈
// 可以指定 ucp->uc_link，使得 func 函数执行完毕后，自动跳转到 ucp->uc_link 对应的上下文
// 如果不设置 ucp->uc_link，func 函数执行完毕后，必须调用 setcontext 或 swapcontext 重新制定一个有效的上下文，否则程序会跑飞
// makecontext 调用后，可通过调用 setcontext 或 swapcontext 激活 ucp，执行 func 函数
void makecontext(ucontext_t *ucp, void (*func)(), int argc, ...); 

// 恢复 ucp 上下文，保存当前上下文到 oucp
// 和 setcontext 一样，swapcontext 也不会返回，而是跳转到 ucp 上下文对应的函数中执行
int swapcontext(ucontext_t *oucp, const ucontext_t *ucp);
```

一个简单的例子：

```c
// example.c
#include <stdio.h>
#include <ucontext.h>
#include <unistd.h>

int main(int argc, const char *argv[]){
    ucontext_t context;

    getcontext(&context);
    puts("Hello world");
    sleep(1);
    setcontext(&context);
    return 0;
}
/*
cxy@ubuntu:~$ gcc example.c -o example
cxy@ubuntu:~$ ./example 
Hello world
Hello world
Hello world
Hello world
Hello world
Hello world
Hello world
^C
cxy@ubuntu:~$
*/
```

实现用户线程（协程）的过程：

- 首先调用getcontext获得当前上下文
- 修改当前上下文ucontext_t来指定新的上下文，如指定栈空间极其大小，设置用户线程执行完后返回的后继上下文（即主函数的上下文）等
- 调用makecontext创建上下文，并指定用户线程中要执行的函数
- 切换到用户线程上下文去执行用户线程（如果设置的后继上下文为主函数，则用户线程执行完后会自动返回主函数）。

代码示例：

```c
#include <ucontext.h>
#include <stdio.h>

void func1(void * arg)
{
    puts("1");
    puts("11");
    puts("111");
    puts("1111");

}
void context_test()
{
    char stack[1024*128];
    ucontext_t child,main;

    getcontext(&child); //获取当前上下文
    child.uc_stack.ss_sp = stack;//指定栈空间
    child.uc_stack.ss_size = sizeof(stack);//指定栈空间大小
    child.uc_stack.ss_flags = 0;
    child.uc_link = &main;//设置后继上下文

    makecontext(&child,(void (*)(void))func1,0);//修改上下文指向func1函数

    swapcontext(&main,&child);//切换到child上下文，保存当前上下文到main
    puts("main");//如果设置了后继上下文，func1函数指向完后会返回此处
}

int main()
{
    context_test();

    return 0;
}
```

在具体实现中，用线程局部变量（C++11 thread_local）来保存协程的上下文（不同线程的协程相互不影响，每个线程要独自处理当前的协程切换问题）。

在我当前的实现中，用两个线程局部变量（`t_fiber` 和 `t_thread_fiber`）来保存当前协程和主协程。

设置了三种协程状态：就绪（READY）、运行（RUNNING）、结束（TERM）。

对于非对称协程，除了创建语句，只有两种操作，resume（恢复协程运行）和 yield（让出执行）。

协程调度（进程的调度算法：先来先服务、最短作业优先、最⾼响应⽐优先、时间⽚轮转等）

## TODO

- [ ] 协程类的实现
  - [x] 通过简单的协程调度器进行测试
- [ ] 协程调度
- [ ] 协程 + IO
- [ ] 定时器
- [ ] hook 

## 参考资料

- 游双《Linux⾼性能服务器编程》
- 陈硕《Linux多线程服务端编程：使⽤muduo C++⽹络库》
- 开源项目 https://github.com/sylar-yin/sylar
- ucontext https://developer.aliyun.com/article/52886
- sylar 协程模块 https://www.midlane.top/wiki/pages/viewpage.action?pageId=10060957
