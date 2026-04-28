#include <iostream>
#include <map>
#include <deque>
#include <vector>
using namespace std;

enum class Side {
	buy, ask
};
struct Order {
	long long price;
	long long quantity;
	long long hiddenQuantity;
	long long peakQuantity;
	long long id;
	Side side;
	bool isIceberg;
};

class OrderBook {
private:
	map <long long, deque <Order>, greater <long long>> BOrders;
	map <long long, deque <Order>, less <long long>> AOrders;
	map <long long, deque <Order>> Orders;
public:
	void addOrder(Order order){
		Orders[order.id].push_back(order);
		if (order.side == Side::buy) {
			
			BOrders[order.price].push_back(order);
		}
		else {
			AOrders[order.price].push_back(order);
		}
	};

	void execAlgo(Order order){
		if (order.side == Side::buy) {
			auto itT = AOrders.begin();
			
			while (!itT->second.empty() && itT->first <= order.price) {
				if (order.quantity >= itT->second.front().quantity) {
					order.quantity -= itT->second.front().quantity;
					itT->second.pop_front();
					if (itT->second.empty()) {
						itT = AOrders.erase(itT);
					}
				}
				else {
					itT->second.front().quantity -= order.quantity;
					order.quantity = 0;
					break;
				}


			}
		}

		if (order.side == Side::ask) {
			auto it2 = BOrders.begin();

			while (it2->first >= order.price) {
				if (order.quantity >= it2->second.front().quantity) {
					order.quantity -= it2->second.front().quantity;
					it2->second.pop_front();
					if (it2->second.empty()) {
						it2 = BOrders.erase(it2);
					}
					else it2 = next(it2);
				}
				else {
					it2->second.front().quantity -= order.quantity;
					order.quantity = 0;
					break;
				}


			}
		}
		
		if (order.quantity > 0) {
			addOrder(order);
		}
	}
};


int main() {
    OrderBook book;
    book.addOrder({100, 10, 0, 0, 1, Side::ask, false});
    book.addOrder({99,  8,  0, 0, 2, Side::buy, false});

    cout << "--- Buy hitting ask at 100 ---\n";
    book.execAlgo({100, 7, 0, 0, 3, Side::buy, false});

    cout << "--- Sell hitting bid at 99 ---\n";
    book.execAlgo({99,  3, 0, 0, 4, Side::ask, false});

    cout << "Done.\n";
    return 0;
}
