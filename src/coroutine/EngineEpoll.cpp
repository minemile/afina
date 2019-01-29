#include <afina/coroutine/EngineEpoll.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <iostream>

namespace Afina {
namespace Network {
namespace Coroutine {

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
    context * candidate = alive;

    if (candidate && candidate == cur_routine) {
        candidate = candidate->next;
    }

    if (candidate) {
        Enter(*candidate);
    }

    if (cur_routine && !cur_routine->block) {
        return;
    }

    if (cur_routine != idle_ctx) {
        Enter(*idle_ctx);
    }
}

void Engine::sched(void *routine_) {
    if (cur_routine == routine_) {
        return;
    }

    if (routine_) {
        Enter(*(static_cast<context *>(routine_)));
    }
    else {
        yield();
    }
}

void Engine::Wait() {
    if (!cur_routine->block) {
        _swap_list(alive, blocked, cur_routine);
        cur_routine->block = true;
    }
    cur_routine->events = 0;
    yield();
}

void Engine::Notify(context& ctx) {
    if (ctx.block) {
        _swap_list(blocked, alive, &ctx);
        ctx.block = false;
    }
}

void Engine::_swap_list(context * &list1, context * &list2, context * const &routine) {
    if (routine->prev != nullptr) {
        routine->prev->next = routine->next;
    }

    if (routine->next != nullptr) {
        routine->next->prev = routine->prev;
    }

    if (list1 == routine) {
        list1 = list1->next;
    }

    routine->next = list2;
    list2 = routine;

    if (routine->next != nullptr) {
        routine->next->prev = routine;
    }
}

int Engine::GetCurEvents() const {
    return cur_routine->events;
}

void Engine::SetEventsAndNotify(void * ptr, int events) {
    context * con_ptr = static_cast<context *>(ptr);
    con_ptr->events |= events;
    Notify(*con_ptr);
}

void Engine::NotifyAll() {
    for (auto ptr = blocked; ptr; ptr = blocked) {
        Notify(*ptr);
    }
}

bool Engine::NeedWait() const {
    return blocked && !alive;
}

void * Engine::GetCurRoutinePointer() const {
    return cur_routine;
}

} // namespace Coroutine
} // namespace Network
} // namespace Afina
