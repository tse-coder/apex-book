#include "orderbook.h"
#include "engine_manager.h"
#include "spsc_queue.h"
#include "event_journal.h"
#include "crow.h"
#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <random>
#include <unordered_set>

int main() {
    SPSCQueue<InboundEvent> inbound(1024 * 1024);
    SPSCQueue<OutboundEvent> outbound(1024 * 1024);

    EngineManager engine(inbound, outbound);
    
    for (uint64_t u = 1; u <= 10; ++u) {
        engine.deposit(u, 0, 100000000); // 100M USD
        engine.deposit(u, 1, 1000000);   // 1M BTC
        engine.deposit(u, 2, 1000000);   // 1M ETH
    }

    engine.start();

    std::unordered_set<crow::websocket::connection*> clients;
    std::mutex clients_mutex;

    crow::SimpleApp app;

    CROW_WEBSOCKET_ROUTE(app, "/ws")
      .onopen([&](crow::websocket::connection& conn) {
          std::lock_guard<std::mutex> lock(clients_mutex);
          clients.insert(&conn);
      })
      .onclose([&](crow::websocket::connection& conn, const std::string& reason) {
          std::lock_guard<std::mutex> lock(clients_mutex);
          clients.erase(&conn);
      });

    // Market Data Publisher + Event Sourcing
    std::thread mdp_thread([&]() {
        EventJournal journal("apex_journal.bin");
        
        while(true) {
            OutboundEvent out;
            if (outbound.pop(out)) {
                // 1. Zero-Copy Append to Memory-Mapped File (O(1))
                journal.append(out);

                // 2. Broadcast / Log Event
                if (out.type == OutboundEventType::TRADE) {
                    std::cout << "[TRADE] Symbol: " << out.symbol_id 
                              << " Price: " << out.price 
                              << " Qty: " << out.quantity << "\n";
                } else if (out.type == OutboundEventType::ORDER_REJECTED) {
                    std::cout << "[REJECTED] Order " << out.order_id << " (Insufficient Funds)\n";
                }
            } else {
                std::this_thread::yield();
            }
        }
    });

    std::thread gateway_thread([&]() {
        uint64_t id = 1;
        std::mt19937 gen(1337);
        std::uniform_int_distribution<uint64_t> price_dist(950, 1050);
        std::uniform_int_distribution<uint64_t> qty_dist(1, 100);
        std::uniform_int_distribution<int> side_dist(0, 1);
        std::uniform_int_distribution<uint32_t> symbol_dist(1, 2);
        std::uniform_int_distribution<uint64_t> user_dist(1, 10);

        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            for(int i=0; i<5; ++i) {
                InboundEvent ev;
                ev.type = EventType::ADD_ORDER;
                ev.symbol_id = symbol_dist(gen);
                ev.order_id = id++;
                ev.user_id = user_dist(gen);
                ev.price = price_dist(gen);
                ev.quantity = qty_dist(gen);
                ev.side = side_dist(gen) == 0 ? Side::BUY : Side::SELL;
                
                while(!inbound.push(ev)) {}
            }

            engine.stop();
            std::string snap = engine.get_snapshot(1);
            engine.start();

            {
                std::lock_guard<std::mutex> cl_lock(clients_mutex);
                for(auto* conn : clients) {
                    conn->send_text(snap);
                }
            }
        }
    });

    std::cout << "Starting ApexBook Core Exchange on ws://localhost:8080/ws\n";
    app.port(8080).multithreaded().run();

    engine.stop();
    gateway_thread.join();
    mdp_thread.join();
    return 0;
}
