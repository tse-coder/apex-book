#pragma once
#include "types.h"
#include "orderpool.h"
#include "pricelevel.h"
#include <map>
#include <unordered_map>
#include <vector>
#include <string>
#include <algorithm>

class OrderBook {
private:
    OrderPool pool;
    std::map<uint64_t, PriceLevel, std::greater<uint64_t>> bids;
    std::map<uint64_t, PriceLevel, std::less<uint64_t>> asks;
    std::unordered_map<uint64_t, Order*> order_map;
    std::vector<Trade> recent_trades;

    void match() {
        while (!bids.empty() && !asks.empty()) {
            auto best_bid_it = bids.begin();
            auto best_ask_it = asks.begin();

            if (best_bid_it->first < best_ask_it->first) {
                break; // No cross
            }

            PriceLevel& best_bid_level = best_bid_it->second;
            PriceLevel& best_ask_level = best_ask_it->second;

            Order* bid_order = best_bid_level.head;
            Order* ask_order = best_ask_level.head;

            uint64_t trade_price = ask_order->price; // simplified pricing
            uint64_t trade_qty = std::min(bid_order->quantity, ask_order->quantity);

            recent_trades.push_back({bid_order->id, ask_order->id, trade_price, trade_qty});

            bid_order->quantity -= trade_qty;
            ask_order->quantity -= trade_qty;

            if (bid_order->quantity == 0) {
                best_bid_level.remove(bid_order);
                order_map.erase(bid_order->id);
                pool.deallocate(bid_order);
            }
            if (ask_order->quantity == 0) {
                best_ask_level.remove(ask_order);
                order_map.erase(ask_order->id);
                pool.deallocate(ask_order);
            }

            if (best_bid_level.empty()) bids.erase(best_bid_it);
            if (best_ask_level.empty()) asks.erase(best_ask_it);
        }
    }

public:
    OrderBook(size_t max_orders = 1000000) : pool(max_orders) {
        order_map.reserve(max_orders);
    }

    void add_order(uint64_t id, uint64_t price, uint64_t qty, Side side) {
        if (order_map.find(id) != order_map.end()) return;

        Order* order = pool.allocate(id, price, qty, side);
        if (!order) return;

        if (side == Side::BUY) {
            if (bids.find(price) == bids.end()) bids[price] = PriceLevel(price);
            bids[price].append(order);
        } else {
            if (asks.find(price) == asks.end()) asks[price] = PriceLevel(price);
            asks[price].append(order);
        }
        order_map[id] = order;

        match();
    }

    void cancel_order(uint64_t id) {
        auto it = order_map.find(id);
        if (it == order_map.end()) return;

        Order* order = it->second;
        if (order->side == Side::BUY) {
            bids[order->price].remove(order);
            if (bids[order->price].empty()) bids.erase(order->price);
        } else {
            asks[order->price].remove(order);
            if (asks[order->price].empty()) asks.erase(order->price);
        }

        pool.deallocate(order);
        order_map.erase(it);
    }
    
    std::vector<Trade> poll_trades() {
        std::vector<Trade> trades = std::move(recent_trades);
        recent_trades.clear();
        return trades;
    }

    std::string get_snapshot() {
        std::string out = "{\"bids\":[";
        bool first = true;
        int depth = 0;
        for (auto const& [price, level] : bids) {
            if(depth++ >= 10) break;
            if(!first) out += ",";
            uint64_t total_qty = 0;
            Order* curr = level.head;
            while(curr) { total_qty += curr->quantity; curr = curr->next; }
            out += "[" + std::to_string(price) + "," + std::to_string(total_qty) + "]";
            first = false;
        }
        out += "],\"asks\":[";
        first = true;
        depth = 0;
        for (auto const& [price, level] : asks) {
            if(depth++ >= 10) break;
            if(!first) out += ",";
            uint64_t total_qty = 0;
            Order* curr = level.head;
            while(curr) { total_qty += curr->quantity; curr = curr->next; }
            out += "[" + std::to_string(price) + "," + std::to_string(total_qty) + "]";
            first = false;
        }
        out += "]}";
        return out;
    }
};
