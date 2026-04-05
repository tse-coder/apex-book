#include <benchmark/benchmark.h>
#include "orderbook.h"

static void BM_AddOrder(benchmark::State& state) {
    OrderBook book;
    uint64_t id = 1;
    for (auto _ : state) {
        book.add_order(id++, 100, 10, Side::BUY);
    }
}
BENCHMARK(BM_AddOrder);

static void BM_MatchOrders(benchmark::State& state) {
    OrderBook book;
    uint64_t id = 1;
    for (auto _ : state) {
        book.add_order(id++, 100, 10, Side::BUY);
        book.add_order(id++, 100, 10, Side::SELL);
    }
}
BENCHMARK(BM_MatchOrders);

static void BM_CancelOrder(benchmark::State& state) {
    OrderBook book;
    for (auto _ : state) {
        state.PauseTiming();
        book.add_order(1, 100, 10, Side::BUY);
        state.ResumeTiming();
        
        book.cancel_order(1);
    }
}
BENCHMARK(BM_CancelOrder);

BENCHMARK_MAIN();
