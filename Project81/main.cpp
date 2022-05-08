#include <thread>
#include <chrono>
#include <latch>
#include <fmt/core.h>
#include <algorithm>
#include <vector>
#include <random>
#include <execution>
#include <tuple>
#include "PoolContext.h"

using namespace std::literals;

auto partition(std::ranges::range auto&& r) {
	auto head = std::ranges::begin(r);
	auto result_range = std::ranges::partition(head + 1, std::ranges::end(r), [head](const auto& other) { return other < *head; });
	auto result = std::ranges::begin(result_range);
	auto temp = std::move(*head);
	*head = std::move(*(result - 1));
	*(result - 1) = std::move(temp);
	auto front_range = std::ranges::subrange{ head, result };
	return std::pair{ front_range, result_range };
}

void sort_normal(std::ranges::range auto&& r) {
	if (std::ranges::size(r) < 2) { return; }
	auto&& [front, back] = partition(r);
	sort_normal(front);
	sort_normal(back);
}

constexpr std::size_t threshold = 2048;

void sort_fallback(std::ranges::range auto&& r, std::latch& latch) {
	if (std::ranges::size(r) < 2) { latch.count_down(std::ranges::ssize(r)); return; }
	auto&& [front, back] = partition(r);
	sort_normal(front);
	sort_normal(back);
	latch.count_down(std::ranges::ssize(r));
}

#define DISPATCH(range) \
if (std::ranges::size(range) * sizeof(std::ranges::range_value_t<decltype(r)>) > threshold) { \
	ctx.submit([range, &ctx, &latch] { sort_parallel(range, ctx, latch); }); \
} \
else { \
	sort_fallback(range, latch); \
} \

void sort_parallel(std::ranges::range auto&& r, myutil::PoolContext& ctx, std::latch& latch) {
	if (std::ranges::size(r) < 2) { latch.count_down(std::ranges::ssize(r)); return; }
	auto&& [r1, r2] = partition(r);
	if (std::ranges::size(r1) * sizeof(std::ranges::range_value_t<decltype(r)>) > threshold) {
		auto&& [r11, r12] = partition(r1);
		DISPATCH(r11);
		DISPATCH(r12);
	} else {
		sort_fallback(r1, latch);
	}
	if (std::ranges::size(r2) * sizeof(std::ranges::range_value_t<decltype(r)>) > threshold) {
		auto&& [r21, r22] = partition(r2);
		DISPATCH(r21);
		DISPATCH(r22);
	} else {
		sort_fallback(r2, latch);
	}
}

void sort(std::ranges::range auto&& r, myutil::PoolContext& ctx) {
	if (std::ranges::size(r) < 2) { return; }
	if (std::ranges::size(r) * sizeof(std::ranges::range_value_t<decltype(r)>) > threshold) {
		std::latch latch{ std::ranges::ssize(r) };
		auto range = r | std::views::all;
		ctx.submit([range, &ctx, &latch] { sort_parallel(range, ctx, latch); });
		latch.wait();
	}
	else {
		sort_normal(r);
	}
}

void benchmark()
{
	myutil::PoolContext ctx{ 8 };
	std::random_device rd;
	std::default_random_engine re{ rd() };
	std::uniform_int_distribution<int> dist{ -1000, 1000 };
	std::vector<int> v;
	constexpr std::size_t test = 1000000;
	constexpr std::size_t pass = 200;
	v.resize(test);

	fmt::print("Start Test 1.\n");

	auto dur = 0us;
	for (std::size_t p = 0; p < pass; ++p) {
		for (int c = 0; c < test; ++c) {
			v[c] = dist(re);
		}

		auto start = std::chrono::steady_clock::now();
		sort(v, ctx);
		auto finish = std::chrono::steady_clock::now();
		dur += std::chrono::duration_cast<std::chrono::microseconds>(finish - start);
		fmt::print("Pass {} complete in {}us\n", p, std::chrono::duration_cast<std::chrono::microseconds>(finish - start).count());
	}

	auto av1 = dur.count() / pass;

	fmt::print("Start Test 2.\n");

	dur = 0us;
	for (std::size_t p = 0; p < pass; ++p) {
		for (int c = 0; c < test; ++c) {
			v[c] = dist(re);
		}

		auto start = std::chrono::steady_clock::now();
		std::sort(std::execution::par_unseq, v.begin(), v.end());
		auto finish = std::chrono::steady_clock::now();
		dur += std::chrono::duration_cast<std::chrono::microseconds>(finish - start);
		fmt::print("Pass {} complete in {}us\n", p, std::chrono::duration_cast<std::chrono::microseconds>(finish - start).count());
	}

	auto av2 = dur.count() / pass;

	fmt::print("Start Test 3.\n");

	dur = 0us;
	for (std::size_t p = 0; p < pass; ++p) {
		for (int c = 0; c < test; ++c) {
			v[c] = dist(re);
		}

		auto start = std::chrono::steady_clock::now();
		std::sort(v.begin(), v.end());
		auto finish = std::chrono::steady_clock::now();
		dur += std::chrono::duration_cast<std::chrono::microseconds>(finish - start);
		fmt::print("Pass {} complete in {}us\n", p, std::chrono::duration_cast<std::chrono::microseconds>(finish - start).count());
	}

	auto av3 = dur.count() / pass;
	
	fmt::print("{}us\n{}us\n{}us\n", av1, av2, av3);

	system("pause");
}

int main()
{
	myutil::PoolContext ctx{ 8 };
	constexpr std::size_t size = 15000;
	constexpr std::size_t pass = 1000;
	auto total = 0us;
	for (std::size_t p = 0; p < pass; ++p) {
		std::latch latch{ size };
		auto begin = std::chrono::steady_clock::now();
		for (std::size_t count = 0; count < size; ++count) {
			ctx.submit([&latch] { latch.count_down(); });
		}
		latch.wait();
		auto end = std::chrono::steady_clock::now();
		total += std::chrono::duration_cast<std::chrono::microseconds>(end - begin);
		//fmt::print("Pass {} finished in {}us\n",
		//	p, std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count());
	}
	fmt::print("Average: {}us\n", total.count() / pass);

	return 0;
}
