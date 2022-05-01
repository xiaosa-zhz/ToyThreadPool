#include "PoolContext.h"
#include <thread>
#include <chrono>
#include <latch>
#include <fmt/core.h>

using namespace std::literals;

int main()
{
	myutil::PoolContext ctx{ 8 };
	std::latch l{ 1000 };
	std::atomic_int num = 0;
	for (std::size_t count = 0; count < 1000; ++count) {
		ctx.submit([count, &l, &num] { num.fetch_add(count); l.count_down(); });
		if (count % 13 == 5) { ctx.add_workers(2); }
		if (count % 13 == 11) { ctx.remove_workers(2); }
		fmt::print("count: {}, workers: {}\n", count, ctx.workers());
	}
	l.wait();

	fmt::print("result: {}", num.load());

	return 0;
}