#ifndef MY_LIB_POOL_CONTEXT_HEADER
#define MY_LIB_POOL_CONTEXT_HEADER 1

#include <thread>
#include <stop_token>
#include <atomic>
#include <mutex>
#include <functional>
#include <queue>
#include <list>
#include <concepts>
#include <tuple>
#include <condition_variable>
#include <algorithm>
#include <chrono>
#include <ranges>
#include <semaphore>
#include <memory>
#include <fmt/core.h>
#include "Auto.h"
#include "ob_ptr.h"
#include "Predefs.h"

#define TaskPoolImpl2

namespace myutil
{
    using namespace _STD literals;

#ifdef TaskPoolImpl2

    class TaskPool
    {
    public:
        TaskPool() = default;
        TaskPool(const TaskPool&) = delete;
        decltype(auto) operator=(const TaskPool&) = delete;
        TaskPool(TaskPool&&) = delete;
        decltype(auto) operator=(TaskPool&&) = delete;
        explicit TaskPool(_STD size_t size) : task_pool_size(size) {}

        void push(auto&&... args)
        {
            auto&& [flag, func] = this->task_pool[this->get_index(this->tail_index)];
            func = _STD function<void()>{ FWD(args)... };
            while (flag.test(_STD memory_order_acquire)) { _STD this_thread::yield(); }
            flag.test_and_set(_STD memory_order_release);
            this->tasks.release();
        }

        template<typename R, typename P>
        _STD function<void()> try_pop(_STD chrono::duration<R, P> dur)
        {
            if (!(this->tasks.try_acquire_for(dur))) {
                return nullptr;
            }
            auto&& [flag, func] = this->task_pool[this->get_index(this->head_index)];
            while (!flag.test(_STD memory_order_acquire)) { _STD this_thread::yield(); }
            Auto(flag.clear(_STD memory_order_release));
            return MOV(func);
        }

        _STD size_t size() { return this->tail_index - this->head_index; }

    private:
        _STD size_t get_index(_STD atomic_unsigned_lock_free& raw_index) {
            return raw_index.fetch_add(1) % this->task_pool_size;
        }

        _STD size_t task_pool_size{ 16384 };
        _STD vector<_STD pair<_STD atomic_flag, _STD function<void()>>> task_pool{ this->task_pool_size };
        _STD counting_semaphore<> tasks{ 0 };
        _STD atomic_unsigned_lock_free head_index{ 0 };
        _STD atomic_unsigned_lock_free tail_index{ 0 };
    };

#else

    class TaskPool
    {
    public:
        TaskPool() = default;
        TaskPool(const TaskPool&) = delete;
        decltype(auto) operator=(const TaskPool&) = delete;
        TaskPool(TaskPool&&) = delete;
        decltype(auto) operator=(TaskPool&&) = delete;

        void push(auto&&... args)
        {
            {
                _STD unique_lock guard{ this->m_task };
                task_pool.emplace(FWD(args)...);
            }
            cv.notify_one();
        }

        template<typename R, typename P>
        _STD function<void()> try_pop(_STD chrono::duration<R, P> dur)
        {
            _STD unique_lock guard{ this->m_task };
            if (task_pool.size() == 0) {
                if (!cv.wait_for(guard, dur, [this] { return this->size() > 0; })) {
                    return nullptr;
                }
            }
            // defer pop after front task being moved to returned value
            // and before releasing lock (cause unique_lock is inited before Auto's helper)
            // guarenteed RVO
            Auto(task_pool.pop());
            return MOV(task_pool.front());
        }

        _STD size_t size() { return task_pool.size(); }

    private:
        _STD queue<_STD function<void()>> task_pool{};
        _STD condition_variable cv;
        mutable _STD mutex m_task;
    };

#endif // TaskPoolImpl2

    class PoolContext
    {
    public:
        PoolContext() = default;
        PoolContext(_STD size_t init_workers)
        {
            this->add_workers(init_workers);
        }
        ~PoolContext()
        {
            this->is_stopped = true;
            this->remove_workers(this->workers());
        }

        template<typename... Args, _STD invocable<Args&&...> F>
        void submit(F&& f, Args&&... args)
        {
            this->do_submit(FWD(f), FWD(args)...);
        }

        _STD size_t workers() const { return this->worker_counter; }

        _STD size_t add_workers(_STD size_t more_workers)
        {
            _STD unique_lock guard{ this->m_worker };
            for (_STD size_t count = 0; count < more_workers; ++count) {
                this->worker_pool.emplace_back(pool_worker_main_func, myutil::ob_ptr{ this });
            }
            this->worker_counter += more_workers;
            return this->workers();
        }

        _STD size_t remove_workers(_STD size_t less_workers)
        {
            _STD unique_lock guard{ this->m_worker };
            if (less_workers > this->workers()) {
                return this->workers();
            }
            auto eligible = this->worker_pool
                | _STD views::filter([](auto& thread) { return thread.get_stop_token().stop_possible(); })
                | _STD views::take(less_workers);
            for (auto& thread : eligible) {
                thread.request_stop();
            }
            this->worker_counter -= less_workers;
            return this->workers();
        }

        _STD size_t debug_unreleased_workers() { return this->worker_pool.size(); }

    private:
        static void pool_worker_main_func(_STD stop_token stop_token, myutil::ob_ptr<PoolContext> parent_context)
        {
            auto last_try = 10ms;
            while (!stop_token.stop_requested())
            {
                auto task = parent_context->task_pool.try_pop(last_try);
                if (!task) {
                    last_try = (2 * last_try) < 160ms ? 2 * last_try : 160ms;
                    continue;
                }
                last_try = 10ms;
                try {
                    task();
                }
                catch (_STD exception& e) {
                    // TODO: pool's global exception handler
                    fmt::print("Exception catched by global handler, message: {}\n", e.what());
                }
                catch (...) {
                    fmt::print("Unknown exception catched by global handler.\n");
                }
            }
            _STD unique_lock guard{ parent_context->m_worker };
            if (!parent_context->is_stopped) {
                auto self = _STD ranges::find_if(parent_context->worker_pool,
                    [](auto& thread) { return thread.get_id() == _STD this_thread::get_id(); });
                self->detach();
                parent_context->worker_pool.erase(self);
            }
        }

        template<typename... Args, typename F>
        void do_submit(F&& f, Args&&... args)
        {
            task_pool.push([f{ SFWD(F, f) }, args_pack = _STD make_tuple(SFWD(Args, args)...)]() mutable {
                return[&]<_STD size_t... I>(_STD index_sequence<I...>) -> decltype(auto) {
                    return _STD invoke(SFWD(F, f), SFWD(Args, _STD get<I>(args_pack))...);
                } (_STD make_index_sequence<sizeof...(Args)>());
            });
        }

        _STD list<_STD jthread> worker_pool{};
        myutil::TaskPool task_pool{};
        _STD mutex m_worker;
        _STD size_t worker_counter{ 0 };
        _STD atomic_bool is_stopped{ false };
    };

} // namespace myutil

#endif // !MY_LIB_POOL_CONTEXT_HEADER
