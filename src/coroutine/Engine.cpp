#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

namespace Afina {
namespace Coroutine {

void Engine::Store(context &ctx) {
    char stack_end;
    ctx.Hight = StackBottom;
    ctx.Low = &stack_end;
    
    char * &buffer = std::get<0>(ctx.Stack);
    uint32_t &av_size = std::get<1>(ctx.Stack);
    auto size = ctx.Hight - ctx.Low;
    
    if (av_size < size) {
        delete[] buffer;
        buffer = new char[size];
        av_size = size;
    }
    
    memcpy(buffer, ctx.Low, size);
}

void Engine::Restore(context &ctx) {
    char stack_end;
    if (&stack_end >= ctx.Low) {
        Restore(ctx);
    }
    
    char* &buffer = std::get<0>(ctx.Stack);
    auto size = ctx.Hight - ctx.Low;
    
    memcpy(ctx.Low, buffer, size);
    longjmp(ctx.Environment, 1);
}

void Engine::Enter(context& ctx) {
    if (cur_routine && cur_routine != idle_ctx) {
        if (setjmp(cur_routine->Environment) > 0) {
            return;
        }
        Store(*cur_routine);
    }
    
    cur_routine = &ctx;
    Restore(ctx);
}

void Engine::yield() {
    if (alive) {
        Enter(*alive);
    }
}

void Engine::sched(void *routine_) {
    if (routine_) {
        Enter(*(static_cast<context *>(routine_)));
    }
    
    if (cur_routine) {
        return;
    }
    
    yield();
}

} // namespace Coroutine
} // namespace Afina
