#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <cstring>
#include <iostream>
#include <csetjmp>

namespace Afina {
namespace Coroutine {

void func(int *p) {
    int i;
    if (!p)
        func(&i);
    else if (p < &i)
        printf("Stack grows upward\n");
    else
        printf("Stack grows downward\n");
}

void Engine::Store(context &ctx) {
    char current_address;
    ctx.Low = std::min(&current_address, StackBottom);
    ctx.Hight = std::max(&current_address, StackBottom);
    uint32_t new_size = ctx.Hight - ctx.Low;
    if (new_size > std::get<1>(ctx.Stack)) {
        delete[] std::get<0>(ctx.Stack);
        ctx.Stack = std::make_tuple(new char[new_size], new_size);
    }
    memcpy(std::get<0>(ctx.Stack), ctx.Low, new_size);
}

void Engine::Restore(context &ctx) {
    char current_address;
    if ((&current_address >= ctx.Low) && (&current_address <= ctx.Hight)) {
        Restore(ctx);
    }
    memcpy(ctx.Low, std::get<0>(ctx.Stack), std::get<1>(ctx.Stack));
    longjmp(ctx.Environment, 1);
}

void Engine::yield() {
    context *candidate = alive;
    while(candidate && candidate == cur_routine){
         candidate = candidate->next;
     }
     if(candidate){
         sched(candidate);
     }
}

void Engine::sched(void *routine_) {
    context *ctx = static_cast<context *>(routine_);
    if(cur_routine) {
        if (setjmp(cur_routine->Environment) > 0) {
            return;
        }
        Store(*cur_routine);
    }
    cur_routine = ctx;
    Restore(*cur_routine);
}

} // namespace Coroutine
} // namespace Afina
