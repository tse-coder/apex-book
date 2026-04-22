#include "orderbook.h"
#include "engine_manager.h"
#include "spsc_queue.h"
#include "event_journal.h"
#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <random>
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

    // Market Data Publisher + Event Sourcing
    std::thread mdp_thread([&]() {
        EventJournal journal("apex_journal.bin");
        
        while(true) {
            OutboundEvent out;
            if (outbound.pop(out)) {
                journal.append(out);

                if (out.type == OutboundEventType::TRADE) {
                    std::cout << "{\"type\": \"TRADE\", \"symbol\": " << out.symbol_id 
                       << ", \"price\": " << out.price 
                       << ", \"qty\": " << out.quantity 
                       << ", \"buyer\": " << out.buyer_id
                       << ", \"seller\": " << out.seller_id << "}\n";
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
            std::string bal_snap = engine.get_balances_snapshot();
            engine.start();

            std::cout << "{\"type\": \"SNAPSHOT\", \"data\": " << snap << ", \"balances\": " << bal_snap << "}\n";
        }
    });

    std::cout << "{\"type\": \"INFO\", \"msg\": \"Starting ApexBook Core Exchange\"}\n";
    
    // Run forever
    engine.stop();
    gateway_thread.join();
    mdp_thread.join();
    return 0;
}
