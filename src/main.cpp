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
#include <sstream>

int main() {
    SPSCQueue<InboundEvent> inbound(1024 * 1024);
    SPSCQueue<OutboundEvent> outbound(1024 * 1024);

    EngineManager engine(inbound, outbound);
    
    for (uint64_t u = 1; u <= 10; ++u) {
        engine.deposit(u, 0, 100000000); 
        engine.deposit(u, 1, 1000000);   
        engine.deposit(u, 2, 1000000);   
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
                journal.append(out);

                if (out.type == OutboundEventType::TRADE) {
                    std::stringstream ss;
                    ss << "{\"type\": \"TRADE\", \"symbol\": " << out.symbol_id 
                       << ", \"price\": " << out.price 
                       << ", \"qty\": " << out.quantity 
                       << ", \"buyer\": " << out.buyer_id
                       << ", \"seller\": " << out.seller_id << "}";
                       
                    std::lock_guard<std::mutex> cl_lock(clients_mutex);
                    for(auto* conn : clients) {
                        conn->send_text(ss.str());
                    }
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
        std::uniform_int_distribution<uint32_t> symbol_dist(1, 1); // Only BTC for clean TUI
        std::uniform_int_distribution<uint64_t> user_dist(1, 10);

        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            
            for(int i=0; i<3; ++i) {
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

            std::stringstream ss;
            ss << "{\"type\": \"SNAPSHOT\", \"data\": " << snap << "}";

            {
                std::lock_guard<std::mutex> cl_lock(clients_mutex);
                for(auto* conn : clients) {
                    conn->send_text(ss.str());
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
