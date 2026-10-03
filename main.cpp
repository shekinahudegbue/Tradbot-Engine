#include <algorithm>
#include <chrono>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <random>
#include <vector>
using namespace std;

enum class Side { buy, ask };

struct Order {
    long long price;
    long long quantity;        // visible quantity (for icebergs: the current "tip")
    long long hiddenQuantity;  // reserve not shown on the book
    long long peakQuantity;    // tip size an iceberg refills to
    long long id;
    Side side;
    bool isIceberg;
};

struct Trade {
    long long buyId, sellId, price, quantity;
};

class OrderBook {
private:
    // Bids: highest price first. Asks: lowest price first.
    // Each price level is a FIFO queue, which gives price-time priority.
    map<long long, deque<Order>, greater<long long>> bids;
    map<long long, deque<Order>, less<long long>> asks;
    vector<Trade> trades;

    // Rest the leftover of an order on its own side of the book.
    void rest(Order order) {
        if (order.isIceberg) {
            // Only the tip is visible; the rest goes into reserve.
            long long total = order.quantity + order.hiddenQuantity;
            order.quantity = min(order.peakQuantity, total);
            order.hiddenQuantity = total - order.quantity;
        }
        if (order.side == Side::buy) bids[order.price].push_back(order);
        else asks[order.price].push_back(order);
    }

    // Match an incoming order against the opposite side.
    // Works for both sides because each book's ordering already puts the
    // best price at begin(); `crosses` says whether that price is tradable.
    template <typename Book, typename Crosses>
    void match(Order& incoming, Book& book, Crosses crosses) {
        while (incoming.quantity > 0 && !book.empty() &&
               crosses(book.begin()->first)) {
            auto level = book.begin();
            deque<Order>& queue = level->second;
            Order& resting = queue.front();

            long long fill = min(incoming.quantity, resting.quantity);
            incoming.quantity -= fill;
            resting.quantity -= fill;

            if (incoming.side == Side::buy)
                trades.push_back({incoming.id, resting.id, level->first, fill});
            else
                trades.push_back({resting.id, incoming.id, level->first, fill});

            if (resting.quantity == 0) {
                if (resting.isIceberg && resting.hiddenQuantity > 0) {
                    // Refill the tip from reserve and lose time priority:
                    // the order moves to the back of its price level.
                    Order refilled = resting;
                    refilled.quantity = min(refilled.peakQuantity, refilled.hiddenQuantity);
                    refilled.hiddenQuantity -= refilled.quantity;
                    queue.pop_front();
                    queue.push_back(refilled);
                } else {
                    queue.pop_front();
                }
                if (queue.empty()) book.erase(level);
            }
        }
    }

public:
    void submit(Order order) {
        // An incoming iceberg trades its full size; the iceberg split only
        // applies to whatever is left to rest on the book.
        if (order.isIceberg) {
            order.quantity += order.hiddenQuantity;
            order.hiddenQuantity = 0;
        }

        long long limit = order.price;
        if (order.side == Side::buy)
            match(order, asks, [limit](long long p) { return p <= limit; });
        else
            match(order, bids, [limit](long long p) { return p >= limit; });

        if (order.quantity > 0) rest(order);
    }

    const vector<Trade>& getTrades() const { return trades; }
    void clearTrades() { trades.clear(); }

    void printBook() const {
        cout << "  ASKS\n";
        for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
            long long visible = 0;
            for (const Order& o : it->second) visible += o.quantity;
            cout << "    " << it->first << " x " << visible << "\n";
        }
        cout << "  BIDS\n";
        for (const auto& [price, queue] : bids) {
            long long visible = 0;
            for (const Order& o : queue) visible += o.quantity;
            cout << "    " << price << " x " << visible << "\n";
        }
    }
};

void printTrades(const OrderBook& book) {
    for (const Trade& t : book.getTrades())
        cout << "  TRADE buy#" << t.buyId << " sell#" << t.sellId << " "
             << t.quantity << " @ " << t.price << "\n";
}

void demo() {
    OrderBook book;
    cout << "=== Demo ===\n";

    // Iceberg sell: 30 total, only 10 visible at a time.
    book.submit({101, 10, 20, 10, 1, Side::ask, true});
    book.submit({101, 5, 0, 0, 2, Side::ask, false});
    book.submit({99, 8, 0, 0, 3, Side::buy, false});
    cout << "Starting book:\n";
    book.printBook();

    cout << "\nBuy 12 @ 101 (fills iceberg tip, which refills and goes to back of queue):\n";
    book.submit({101, 12, 0, 0, 4, Side::buy, false});
    printTrades(book);
    book.clearTrades();
    book.printBook();

    cout << "\nSell 3 @ 99 into an existing bid:\n";
    book.submit({99, 3, 0, 0, 5, Side::ask, false});
    printTrades(book);
    book.clearTrades();
    book.printBook();

    cout << "\nBuy into an empty ask side (should rest, not crash):\n";
    OrderBook empty;
    empty.submit({100, 5, 0, 0, 6, Side::buy, false});
    empty.printBook();
}

void benchmark(int numOrders) {
    OrderBook book;
    mt19937_64 rng(42);  // fixed seed so runs are repeatable
    uniform_int_distribution<long long> price(90, 110), qty(1, 100);
    uniform_int_distribution<int> coin(0, 1), icebergChance(0, 9);

    vector<Order> orders;
    orders.reserve(numOrders);
    for (int i = 0; i < numOrders; ++i) {
        bool iceberg = icebergChance(rng) == 0;  // ~10% icebergs
        long long q = qty(rng);
        orders.push_back({price(rng), q, iceberg ? q * 3 : 0, iceberg ? q : 0, i + 1,
                          coin(rng) ? Side::buy : Side::ask, iceberg});
    }

    vector<double> latencyNs;
    latencyNs.reserve(numOrders);
    auto start = chrono::steady_clock::now();
    for (const Order& o : orders) {
        auto t0 = chrono::steady_clock::now();
        book.submit(o);
        auto t1 = chrono::steady_clock::now();
        latencyNs.push_back(chrono::duration<double, nano>(t1 - t0).count());
    }
    auto end = chrono::steady_clock::now();

    sort(latencyNs.begin(), latencyNs.end());
    double totalMs = chrono::duration<double, milli>(end - start).count();
    auto pct = [&](double p) { return latencyNs[size_t(p * (latencyNs.size() - 1))]; };

    cout << "\n=== Benchmark: " << numOrders << " orders ===\n";
    cout << "  Trades executed: " << book.getTrades().size() << "\n";
    cout << "  Total time:      " << totalMs << " ms\n";
    cout << "  Median latency:  " << pct(0.50) << " ns\n";
    cout << "  p99 latency:     " << pct(0.99) << " ns\n";
    cout << "  Max latency:     " << latencyNs.back() << " ns\n";
}

int main() {
    demo();
    benchmark(100000);
    return 0;
}
