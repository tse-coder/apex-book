#include "orderbook.h"
#include "engine_manager.h"
#include "spsc_queue.h"
#include "crow.h"
#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <random>
#include <unordered_set>

int main() {
    // 1. Initialize Lock-Free Queues (Must be power of 2)
    SPSCQueue<InboundEvent> inbound(1024 * 1024);
    SPSCQueue<OutboundEvent> outbound(1024 * 1024);

    // 2. Start the Engine Manager (Pinned Thread)
    EngineManager engine(inbound, outbound);
    engine.start();

    std::unordered_set<crow::websocket::connection*> clients;
    std::mutex clients_mutex;
    
    // We will keep a local cache of the snapshot since the engine is lock-free
    std::string latest_snapshot = "{}";
    std::mutex snapshot_mutex;

    crow::SimpleApp app;

    CROW_WEBSOCKET_ROUTE(app, "/ws")
      .onopen([&](crow::websocket::connection& conn) {
          std::lock_guard<std::mutex> lock(clients_mutex);
          clients.insert(&conn);
      })
      .onclose([&](crow::websocket::connection& conn, const std::string& reason) {
          std::lock_guard<std::mutex> lock(clients_mutex);
          clients.erase(&conn);
      })
      .onmessage([&](crow::websocket::connection& /*conn*/, const std::string& data, bool is_binary) {
          // If we receive orders via WS, we parse and push to inbound queue here
      });

    // 3. Market Data Publisher (Reads outbound queue)
    std::thread mdp_thread([&]() {
        while(true) {
            OutboundEvent out;
            if (outbound.pop(out)) {
                if (out.type == OutboundEventType::TRADE) {
                    std::cout << "[TRADE] Symbol: " << out.symbol_id 
                              << " Price: " << out.price 
                              << " Qty: " << out.quantity << "\n";
                } else if (out.type == OutboundEventType::BOOK_UPDATE) {
                    // For Phase 1 simulation, we just tell the main thread to grab a snapshot.
                    // (In Phase 3 this will be proper delta publishing)
                }
            } else {
                std::this_thread::yield();
            }
        }
    });

    // 4. Network/Gateway Thread (Simulating incoming orders)
    std::thread gateway_thread([&]() {
        uint64_t id = 1;
        std::mt19937 gen(1337);
        std::uniform_int_distribution<uint64_t> price_dist(950, 1050);
        std::uniform_int_distribution<uint64_t> qty_dist(1, 100);
        std::uniform_int_distribution<int> side_dist(0, 1);
        std::uniform_int_distribution<uint32_t> symbol_dist(1, 2); // 1 = BTC, 2 = ETH

        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Push 5 random orders per tick
            for(int i=0; i<5; ++i) {
                InboundEvent ev;
                ev.type = EventType::ADD_ORDER;
                ev.symbol_id = symbol_dist(gen);
                ev.order_id = id++;
                ev.price = price_dist(gen);
                ev.quantity = qty_dist(gen);
                ev.side = side_dist(gen) == 0 ? Side::BUY : Side::SELL;
                
                while(!inbound.push(ev)) { /* Spin if full */ }
            }

            // Since the engine doesn't have a mutex, we must stop it to safely grab a snapshot for the UI.
            // In a real system, the MDP builds the snapshot from deltas.
            engine.stop();
            std::string snap = engine.get_snapshot(1); // Get BTC snapshot for WebSocket
            engine.start();

            {
                std::lock_guard<std::mutex> cl_lock(clients_mutex);
                for(auto* conn : clients) {
                    conn->send_text(snap);
                }
            }
        }
    });

    std::cout << "Starting Phase 1: Lock-Free Scaled Architecture on ws://localhost:8080/ws\n";
    app.port(8080).multithreaded().run();

    engine.stop();
    gateway_thread.join();
    mdp_thread.join();
    return 0;
}
