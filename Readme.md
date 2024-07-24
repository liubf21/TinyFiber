# TinyFiber

⾯试中，⾯试官经常会问这样的问题，“你知道线程和进程的区别吗？”然后会紧接着追问“你了解协程吗？协程和进程、线程⼜有什么区别？”

我们通过基础知识的学习和如 WebServer 此类项⽬，已经对进程和线程有了⽐较深的理解，但对协程相关知识却知之甚少。

协程作为⼀种强⼤的并发编程技术，可以在需要处理⼤量 I/O 操作或者并发任务的情况下提⾼程序的性能和可维护性，在许多场景应⽤⼴泛。通过实现协程库的项目，来提高自己 C++ 编程能⼒、对操作系统和计算机网络有更深的理解。

## 前置知识

- 编程语言：C++11 的常见特性，如智能指针、lambda 表达式、std::function、std::thread 等
- 操作系统和 Linux 常见系统调用
- 计算机网络基础知识：Linux 下的⽹络编程，常⻅⽹络编程 API、⽹络 IO，IO 多路复⽤、⾼性能⽹络模式：Reactor 和 Proactor 
- 

## 协程基础知识

通用的说法是**协程**是一种“轻量级线程”、“用户态线程”（其上下文切换的时机是靠调用方，即写代码的开发人员去控制的）。

每个协程在创建时都会指定一个入口函数，这点可以类比线程。协程的本质就是*函数和函数运行状态的组合*。

协程和函数的不同之处是，函数一旦被调用，只能从头开始执行，直到函数执行结束退出，而协程则可以执行到一半就退出（yield），暂时让出 CPU 执行权，在后面恰当的时机重新恢复运行（resume），在这段时间其他的协程可以获得 CPU 并运行。

协程能够 yield、resume 的关键是存储了函数在 yield 时间点的执行状态，即**协程上下文**。

协程上下文包含了函数在当前执行状态下的全部 CPU 寄存器的值，记录了函数栈帧、代码的执行位置等信息，如果把这些寄存器的值重新设置给 CPU，就相当于重新恢复了函数的运行。

单线程环境下，协程的 yield 和 resume 一定是同步进行的，一个协程的 yield 一定对应另一个协程的 resume，因为线程不可能没有执行主体，而且协程的 yield 和 resume 是完全由应用程序来控制的。（线程创建后，其运行和调度由操作系统自动完成）

线程挺好的，我们为什么需要协程呢？

因为有些时候我们在执行一些操作（尤其是 IO 操作）时，不希望去做“创建一个新的线程”这种重量级的操作来异步处理。而是希望：在当前线程执行中，暂时切换到其他任务中执行，同时在 IO 真正准备好了之后，再切换回来继续执行！

相比于多开一个线程来操作，使用协程的好处：
- 减少了线程的重复高频创建（线程切换耗时在 2us 左右，而协程在 100ns-200ns）；
- 尽量避免线程的阻塞；
- 提升代码的可维护与可理解性（毕竟不需要考虑多线程那一套东西了）；

### 对称协程和非对称协程

-  对称协程，协程可以不受限制地将控制权交给其他协程。任何一个协程都是相互独立且平等的，调度权可以在任意协程之间转移。
   -  ⼦协程可以直接和⼦协程切换，也就是说每个协程不仅要运⾏⾃⼰的⼊⼝函数代码，还要负责选出下⼀个合适的协程进⾏切换，相当于每个协程都要充当调度器的⻆⾊，这样程序设计起来会⽐较麻烦，并且程序的控制流也会变得复杂和难以管理
-  非对称协程，协程之间存在类似堆栈的调用方-被调用关系，**协程出让调度权的目标只能是它的调用者**。
   -  可以借助专⻔的调度器来负责调度协程，每个协程只需要运⾏⾃⼰的⼊⼝函数，然后结束时将运⾏权交回给调度器，由调度器来选出下⼀个要执⾏的协程即可

### 有栈协程与无栈协程

- 有栈协程：这类协程的实现类似于内核态线程的实现，不同协程间切换还是要切换对应的栈上下文，只是不用陷入内核而已；例如：goroutine、libco
  - 共享栈
    - 所有的协程在运⾏的时候都使⽤同⼀个栈空间，每次协程切换时要把⾃身⽤的共享栈空间拷⻉
    - 对协程调⽤ yield 的时候，该协程栈内容暂时保存起来，保存的时候需要⽤到多少内存就开辟多少，这样就减少了内存的浪费， resume 该协程的时候，协程之前保存的栈内容，会被重新拷⻉到运⾏时栈中
  - 独立栈
    - 每个协程的栈空间都是独⽴的，固定⼤⼩
    - 好处是协程切换的时候，内存不⽤拷⻉来拷⻉去。坏处则是 内存空间浪费。因为栈空间在运⾏时不能随时扩容，否则如果有指针操作执⾏了栈内存，扩容后将导致指针失效
- 无栈协程
  - 无栈协程：这类协程的上下文都会放到公共内存中，在协程切换时使用状态机来切换，而不用切换对应的上下文（因为都已经在堆中了），因此相比有栈协程要轻量许多；例如：C++20、Rust、JavaScript 中的协程


### 协程的优缺点

- 优点
  - 提高资源利用率，提高程序并发性能
  - 简化异步编程逻辑
- 缺点
  - 无法利用多核资源，代码必须非阻塞，否则所有的协程都会被卡住

以上其实是 N:1 协程模型的特点，即所有的协程运行于一个系统线程中，计算能力和各类 eventloop 库等价；

相比之下，bRPC 中的 bthread 是 M:N 模型，一个 bthread 被卡住不会影响其他 bthread，其中的关键技术有两点：
- work stealing 调度
- butex

前者让 bthread 更快地被调度到更多的核心上，后者让 bthread 和 pthread 可以相互等待和唤醒。

（bthread 的设计比较接近 go 1.0 版本：OS 线程不会动态增加，在有大量的阻塞性 syscall 下，会有影响。而 go 1.1 之后的设计就是动态增减 OS 线程，而且提供了 LockOSThread，可以让 goroutine 和 OS 线程 1:1。）

### 协程的原理

Coroutine（协程）就是可以中断并恢复执行的 Subroutine（函数）。

如何实现？在协程内部存储自身的上下文，并在需要切换的时候把上下文切换；我们知道上下文其实本质上就是寄存器，所以保存上下文实际上就是把寄存器的值保存下来。
- 使用 setjmp/longjmp（对栈信息的支持有限，一般不能作为协程实现的底层机制）
- 使用汇编保存寄存器中的值（libco 就使用了这种方法）
- 使用 ucontext.h 这种封装好的库也可以帮我们完成上下文的相关工作（本项目中使用的方法）

### 协程的应用

协程的目的在于剔除线程的阻塞，尽可能提高 CPU 的利用率。很多服务在处理业务时需要请求第三方服务，向第三方服务发起 RPC 调用；RPC 调用的网络耗时一般耗时在毫秒级别，RPC 服务的处理耗时也可能在毫秒级别，如果当前服务使用同步调用，即 RPC 返回后才进行后续逻辑，那么一条线程每秒处理的业务数量是可以估算的。

假设每次业务处理花费在 RPC 调用上的耗时是 20ms，那么一条线程一秒最多处理 50 次请求。

如果在等待 RPC 返回时当前线程没有被系统调度转换为 Ready 状态，那当前 CPU 核心就会空转，浪费了 CPU 资源！通过增加线程数量提高系统吞吐量的效果非常有限，而且创建大量线程也会造成其他问题。

协程虽然不一定能减少一次业务请求的耗时，但一定可以提升系统的吞吐量：
- 当前业务只有一次第三方 RPC 的调用，那么协程不会减少业务处理的耗时，但可以提升 QPS；
- 当前业务需要多个第三方 RPC 调用，同时创建多个协程可以让多个 RPC 调用一起执行，则当前业务的 RPC 耗时由耗时最长的 RPC 调用决定。

## C++20 中的协程

代码示例：

```cpp
#include <coroutine>
#include <iostream>

struct HelloCoroutine {

    struct HelloPromise {

        HelloCoroutine get_return_object() {
            return std::coroutine_handle<HelloPromise>::from_promise(*this);
        }

        std::suspend_never initial_suspend() { return {}; }

        std::suspend_always final_suspend() noexcept { return {}; }

        void unhandled_exception() {}
    };

    using promise_type = HelloPromise;

    HelloCoroutine(std::coroutine_handle<HelloPromise> h) : handle(h) {}

    std::coroutine_handle<HelloPromise> handle;
};

HelloCoroutine hello() {
    std::cout << "Hello " << std::endl;
    co_await std::suspend_always{};
    std::cout << "world!" << std::endl;
}

int main() {
    HelloCoroutine coro = hello();

    std::cout << "calling resume" << std::endl;
    coro.handle.resume();

    std::cout << "destroy" << std::endl;
    coro.handle.destroy();

    return 0;
}
```

## 协程库实现

实现了非对称、独立栈的协程库。

### 原理

ucontext 机制是 GNU C 提供的一组创建、保存、切换用户态执行上下文的 API，这是协程能够随时切换和恢复的关键（注意 ucontext 只操作与当前线程相关的 CPU 上下文）。

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
// 如果不设置 ucp->uc_link，func 函数执行完毕后，必须调用 setcontext 或 swapcontext 重新制定一个有效的上下文，否则程序会跑飞（线程退出）
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
```

实现用户线程（协程）的过程：

- 首先调用 getcontext 获得当前上下文
- 修改当前上下文 ucontext_t 来指定新的上下文，如指定栈空间及其大小、设置用户线程执行完后返回的后继上下文（即主函数的上下文）等
- 调用 makecontext 创建上下文，并指定用户线程中要执行的函数
- 切换到用户线程上下文去执行用户线程（如果设置的后继上下文为主函数，则用户线程执行完后会自动返回主函数）

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

### 设计

在具体实现中，Fiber 类代表一个协程，包含了协程的上下文、栈、状态等信息，提供了创建、切换、释放协程的接口。用 static 函数进行全局的协程切换等操作。

在我当前的实现中，用两个线程局部变量（C++11 thread_local）（`t_fiber` 和 `t_thread_fiber`）来保存当前协程和主协程（不同线程的协程相互不影响，每个线程要独自处理当前的协程切换问题）。

设置了三种协程状态：就绪（READY）、运行（RUNNING）、结束（TERM）。

对于非对称协程，除了创建语句，只有两种操作，resume（恢复协程运行）和 yield（让出执行）。

- 所谓协程调度，其实就是创建一批的协程对象，然后再创建一个调度协程，通过调度协程把这些协程对象一个一个消化掉（协程可以在被调度时继续向调度器添加新的调度任务）
- 所谓 IO 协程调度，其实就是在调度协程时，如果发现这个协程在等待 IO 就绪，那就先让这个协程让出执行权，等对应的 IO 就绪后再重新恢复这个协程的运行
- 所谓定时器，就是给调度协程预设一个协程对象，等定时时间到了就恢复预设的协程对象。

#### 协程调度

用户充当调度器 -> 由调度器执行更灵活的调度策略

协程调度（考虑采用进程的调度算法：先来先服务、最短作业优先、最⾼响应⽐优先、时间⽚轮转等）

协程是需要跑在线程上的，那一共需要几种线程？
- 调度器所在的线程（caller 线程），一般也是 main 函数对应的主线程
- 调度线程，用来运行协程（函数）的线程，一个线程同一时刻只能运行一个协程

工作流程：调度器内部维护⼀个任务队列和⼀个调度线程池。开始调度后，线程池从任务队列⾥按顺序取任务执⾏。当全部任务都执⾏完了，线程池停⽌调度，等新的任务进来。添加新任务后，通知线程池有新的任务进来了，线程池重新开始运⾏调度。停⽌调度时，各调度线程退出，调度器停⽌⼯作。

正常情况下，caller 线程负责创建调度器、管理协程，然后调度线程负责运行协程

我们可能希望充分利用线程资源，即**让 caller 线程也作为调度线程**参与调度，那就要涉及到 3 种协程之间的切换：调度器所在的主协程、调度协程、待调度的任务协程

具体实现时，需要三个线程局部变量（`t_fiber`、`t_thread_fiber`、`t_scheduler_fiber`）来保存当前协程、线程主协程、调度协程的上下文

线程主协程 <--> 调度协程 <--> 待调度的任务协程

协程调度器需要哪些操作？
- schedule 添加调度任务
- start
- stop
- tickle 通知协程调度器有任务要执行
- run 协程调度
- idle 无任务调度时执行

IO 协程调度器直接继承协程调度器：
- ⽀持 epoll，在构造函数中创建 epoll 实例并注册事件（管道可读）
- 重载 tickle，实现通知调度协程功能，通过管道通知调度器有任务要调度
- 重载 idle，实现 IO 协程调度功能，阻塞在等待 IO 事件上，idle 退出的时机是 epoll_wait 返回，对应的操作是
tickle 或注册的 IO 事件就绪

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
- C++高性能分布式服务器框架 https://github.com/sylar-yin/sylar
- ucontext https://developer.aliyun.com/article/52886
- sylar 协程模块 https://www.midlane.top/wiki/pages/viewpage.action?pageId=10060957
- 详细了解协程 - 腾讯技术工程的文章 - 知乎 https://zhuanlan.zhihu.com/p/535658398
- 用C++语言编写的工业级RPC框架 https://github.com/apache/brpc
- 微信后台大规模使用的c/c++协程库 https://github.com/Tencent/libco
