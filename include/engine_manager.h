#pragma once
#include "orderbook.h"
#include "spsc_queue.h"
#include "events.h"
#include "account_manager.h"
#include <unordered_map>
#include <thread>
#include <atomic>
#include <iostream>

class EngineManager {
private:
    std::unordered_map<uint32_t, OrderBook> books;
    AccountManager accounts;
    SPSCQueue<InboundEvent>& inbound_queue;
    SPSCQueue<OutboundEvent>& outbound_queue;
    std::atomic<bool> running{false};
    std::thread engine_thread;

    uint32_t get_base_asset(uint32_t symbol_id) {
        return symbol_id; // Simplicity: symbol 1 = BTC/USD (Base 1). symbol 2 = ETH/USD (Base 2).
    }

    void process_event(const InboundEvent& ev) {
        if (books.find(ev.symbol_id) == books.end()) {
            books.emplace(ev.symbol_id, OrderBook(1000000));
        }

        OrderBook& book = books.at(ev.symbol_id);
        uint32_t base_asset = get_base_asset(ev.symbol_id);
        uint32_t quote_asset = 0; // USD

        if (ev.type == EventType::ADD_ORDER) {
            // Pre-trade risk check
            bool risk_passed = false;
            if (ev.side == Side::BUY) {
                uint64_t required_usd = ev.price * ev.quantity;
                risk_passed = accounts.lock_funds(ev.user_id, quote_asset, required_usd);
            } else {
                risk_passed = accounts.lock_funds(ev.user_id, base_asset, ev.quantity);
            }

            if (risk_passed) {
                book.add_order(ev.order_id, ev.user_id, ev.price, ev.quantity, ev.side);
            } else {
                OutboundEvent out;
                out.type = OutboundEventType::ORDER_REJECTED;
                out.symbol_id = ev.symbol_id;
                out.order_id = ev.order_id;
                while(!outbound_queue.push(out)) {}
                return; // Stop processing this event
            }
        } else if (ev.type == EventType::CANCEL_ORDER) {
            auto cancel_res = book.cancel_order_safe(ev.order_id);
            if (cancel_res.success) {
                // Unlock funds
                if (cancel_res.side == Side::BUY) {
                    accounts.unlock_funds(cancel_res.user_id, quote_asset, cancel_res.price * cancel_res.qty);
                } else {
                    accounts.unlock_funds(cancel_res.user_id, base_asset, cancel_res.qty);
                }
            }
        }

        // Process trades
        for (const Trade& t : book.poll_trades()) {
            // Settlement (locked funds -> available funds)
            accounts.settle_trade(t.buyer_user_id, t.seller_user_id, base_asset, t.price, t.quantity);

            OutboundEvent out;
            out.type = OutboundEventType::TRADE;
            out.symbol_id = ev.symbol_id;
            out.buyer_id = t.buyer_order_id;
            out.seller_id = t.seller_order_id;
            out.price = t.price;
            out.quantity = t.quantity;
            
            while(!outbound_queue.push(out)) {}
        }
        
        OutboundEvent out_update;
        out_update.type = OutboundEventType::BOOK_UPDATE;
        out_update.symbol_id = ev.symbol_id;
        while(!outbound_queue.push(out_update)) {}
    }

    void run_loop() {
        while (running.load(std::memory_order_relaxed)) {
            InboundEvent ev;
            if (inbound_queue.pop(ev)) {
                process_event(ev);
            }
        }
    }

public:
    EngineManager(SPSCQueue<InboundEvent>& in, SPSCQueue<OutboundEvent>& out) 
        : inbound_queue(in), outbound_queue(out) {}

    ~EngineManager() {
        stop();
    }

    void start() {
        if (!running) {
            running = true;
            engine_thread = std::thread(&EngineManager::run_loop, this);
        }
    }

    void stop() {
        if (running) {
            running = false;
            if (engine_thread.joinable()) {
                engine_thread.join();
            }
        }
    }
    
    // Test helper to seed accounts before starting
    void deposit(uint64_t user_id, uint32_t asset_id, uint64_t amount) {
        accounts.deposit(user_id, asset_id, amount);
    }

    std::string get_snapshot(uint32_t symbol_id) {
        if (books.find(symbol_id) != books.end()) {
            return books.at(symbol_id).get_snapshot();
        }
        return "{}";
    }
};
