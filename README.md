# Tradbot-Engine

A limit order book and matching engine written in C++17. It matches buy and sell orders using **price-time priority** and supports **iceberg orders**, with a built-in benchmark that measures per-order latency.

## Features

- **Price-time priority matching.** Orders match at the best available price first; at the same price, the earliest order fills first (FIFO).
- **Partial fills.** An order that is only partly filled keeps its place at the front of its price level.
- **Iceberg orders.** Only a small "tip" of a large order is visible on the book. When the tip is filled, it refills from the hidden reserve and moves to the back of its price level, matching how real exchanges treat iceberg orders.
- **Trade log.** Every fill is recorded with the buyer, seller, price, and quantity.
- **Latency benchmark.** Processes 100,000 randomized orders (about 10% icebergs) and reports median, p99, and worst-case latency.

## How it works

Each side of the book is a `std::map` from price to a `std::deque` of orders:

- **Bids** are sorted highest price first and **asks** lowest price first, so the best price is always at `begin()`.
- Each price level is a FIFO queue, which gives time priority within a price.

When an order arrives, it trades against the opposite side for as long as the prices cross and it still has quantity left. Whatever remains rests on the book. One matching function handles both sides, so buy and sell logic can't drift apart.

## Build and run

Requires a C++17 compiler.

```bash
g++ -std=c++17 -O2 main.cpp -o orderbook
./orderbook
```

The program runs a short demo (including an iceberg refill) and then the benchmark.

## Benchmark results

100,000 randomized orders, compiled with `-O2`:

| Metric | Latency |
| --- | --- |
| Median | 200 ns |
| p99 | 2700 ns |
| Worst case | 1.7017e+06 ms |

*Measured on [your machine, e.g. "Intel i7, Windows 11, MinGW g++ 13"]. Results vary by hardware; the benchmark uses a fixed random seed so runs are repeatable.*

## Possible next steps

- Order cancellation and modification by ID
- Market orders
- A custom memory pool to reduce allocation cost and tighten worst-case latency
- Unit tests for edge cases
