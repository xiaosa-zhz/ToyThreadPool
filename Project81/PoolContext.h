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
#include <fmt/core.h>
#include "Auto.h"
#include "ob_ptr.h"

namespace myutil
{
#include "Predefs.h"

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

		_STD function<void()> pop()
		{
			_STD unique_lock guard{ this->m_task };
			if (task_pool.size() == 0) {
				cv.wait(guard, [this] { return this->size() > 0; });
			}
			// defer pop after front task being moved to returned value
			// and before releasing lock (cause unique_lock is inited before Auto's helper)
			// guarenteed RVO
			Auto(task_pool.pop());
			return MOV(task_pool.front());
		}

		void notify_all()
		{
			cv.notify_all();
		}

		_STD size_t size() { return task_pool.size(); }

	private:
		_STD queue<_STD function<void()>> task_pool{};
		_STD condition_variable cv;
		mutable _STD mutex m_task;
	};

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
			using namespace _STD literals;
			while (this->workers() > 0) {
				{
					_STD unique_lock guard{ this->m_worker };
					for (auto& thread : this->worker_pool) {
						thread.request_stop();
					}
				}
				for (_STD size_t count = 0; count < this->workers(); ++count) {
					this->task_pool.push(nullptr);
				}
				std::this_thread::sleep_for(50us);
			}
		}
		
		template<typename... Args, _STD invocable<Args&&...> F>
		void submit(F&& f, Args&&... args)
		{
			this->do_submit(FWD(f), FWD(args)...);
		}

		_STD size_t workers() const { return this->worker_pool.size(); }

		_STD size_t add_workers(_STD size_t more_workers)
		{
			_STD unique_lock guard{ this->m_worker };
			for (_STD size_t count = 0; count < more_workers; ++count) {
				this->worker_pool.emplace_back(pool_worker_main_func, myutil::ob_ptr{ this });
			}
			return this->workers();
		}

		_STD size_t remove_workers(_STD size_t less_workers)
		{
			_STD unique_lock guard{ this->m_worker };
			if (less_workers > this->workers()) {
				return this->workers();
			}
			std::ranges::for_each_n(this->worker_pool.begin(), less_workers,
				[](auto& thread) { thread.request_stop(); });
			return this->workers();
		}

	private:
		static void pool_worker_main_func(_STD stop_token stop_token, myutil::ob_ptr<PoolContext> parent_context)
		{
			while (!stop_token.stop_requested())
			{
				auto task = parent_context->task_pool.pop();
				if (!task) { continue; }
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
			auto self = std::ranges::find_if(parent_context->worker_pool,
				[](auto& thread) { return thread.get_id() == std::this_thread::get_id(); });
			self->detach();
			parent_context->worker_pool.erase(self);
		}

		template<typename... Args, typename F>
		void do_submit(F&& f, Args&&... args)
		{
			task_pool.push([f{ SFWD(F, f) }, args = _STD make_tuple(SFWD(Args, args)...)]() mutable {
				return [&]<_STD size_t... I>(_STD index_sequence<I...>) -> decltype(auto) {
				return _STD invoke(SFWD(F, f), SFWD(Args, _STD get<I>(args))...);
			} (_STD make_index_sequence<sizeof...(Args)>());
			});
		}

		_STD list<_STD jthread> worker_pool{};
		myutil::TaskPool task_pool{};
		_STD mutex m_worker;
	};

#include "UndefPredefs.h"
} // namespace myutil

#endif // !MY_LIB_POOL_CONTEXT_HEADER
