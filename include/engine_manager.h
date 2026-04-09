#pragma once
#include "orderbook.h"
#include "spsc_queue.h"
#include "events.h"
#include <unordered_map>
#include <thread>
#include <atomic>
#include <iostream>

class EngineManager {
private:
    std::unordered_map<uint32_t, OrderBook> books;
    SPSCQueue<InboundEvent>& inbound_queue;
    SPSCQueue<OutboundEvent>& outbound_queue;
    std::atomic<bool> running{false};
    std::thread engine_thread;

    void process_event(const InboundEvent& ev) {
        // Auto-create book if it doesn't exist
        if (books.find(ev.symbol_id) == books.end()) {
            books.emplace(ev.symbol_id, OrderBook(1000000));
        }

        OrderBook& book = books.at(ev.symbol_id);

        if (ev.type == EventType::ADD_ORDER) {
            book.add_order(ev.order_id, ev.price, ev.quantity, ev.side);
        } else if (ev.type == EventType::CANCEL_ORDER) {
            book.cancel_order(ev.order_id);
        }

        // Poll trades and push to outbound queue
        for (const Trade& t : book.poll_trades()) {
            OutboundEvent out;
            out.type = OutboundEventType::TRADE;
            out.symbol_id = ev.symbol_id;
            out.buyer_id = t.buyer_order_id;
            out.seller_id = t.seller_order_id;
            out.price = t.price;
            out.quantity = t.quantity;
            
            while(!outbound_queue.push(out)) {
                // Spin if outbound queue is full (backpressure)
            }
        }
        
        // Push a generic book update event
        OutboundEvent out_update;
        out_update.type = OutboundEventType::BOOK_UPDATE;
        out_update.symbol_id = ev.symbol_id;
        while(!outbound_queue.push(out_update)) {}
    }

    void run_loop() {
        // Optional: Pin thread to core here using pthread_setaffinity_np
        while (running.load(std::memory_order_relaxed)) {
            InboundEvent ev;
            if (inbound_queue.pop(ev)) {
                process_event(ev);
            }
            // If no events, just spin (100% CPU on this core for lowest latency)
            // or use _mm_pause() to reduce power slightly
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
    
    // Unsafe: Only call from engine thread or when stopped
    std::string get_snapshot(uint32_t symbol_id) {
        if (books.find(symbol_id) != books.end()) {
            return books.at(symbol_id).get_snapshot();
        }
        return "{}";
    }
};
