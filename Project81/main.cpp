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
	std::atomic_size_t num = 0;
	for (std::size_t count = 0; count < 1000; ++count) {
		ctx.submit([&l](auto count, std::atomic_size_t& num) { num.fetch_add(count); l.count_down(); }, count, std::ref(num));
		if (count % 13 == 5) { ctx.add_workers(2); }
		if (count % 13 == 11) { ctx.remove_workers(2); }
		fmt::print("count: {}, workers: {}\n", count, ctx.workers());
	}
	l.wait();

	fmt::print("result: {}\n", num.load());
	std::this_thread::sleep_for(50us);
	fmt::print("workers: {}\n", ctx.workers());

	return 0;
}