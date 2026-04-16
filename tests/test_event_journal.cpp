#include "engine_manager.h"
#include "event_journal.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    SPSCQueue<InboundEvent> inbound(1024);
    SPSCQueue<OutboundEvent> outbound(1024);

    EngineManager engine(inbound, outbound);
    
    engine.deposit(1, 0, 50000); 
    engine.deposit(2, 1, 100);

    engine.start();

    // Trigger trades
    InboundEvent ev1 = {EventType::ADD_ORDER, 1, 1, 1, 1000, 10, Side::BUY};
    while (!inbound.push(ev1));
    InboundEvent ev2 = {EventType::ADD_ORDER, 1, 2, 2, 1000, 10, Side::SELL};
    while (!inbound.push(ev2));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EventJournal journal("test_journal.bin", 1000);

    OutboundEvent out;
    while (outbound.pop(out)) {
        journal.append(out); // Zero-copy mmap
        
        if (out.type == OutboundEventType::TRADE) {
            std::cout << "TRADE: Symbol=" << out.symbol_id 
                      << " Px=" << out.price 
                      << " Qty=" << out.quantity << std::endl;
        }
    }

    engine.stop();
    return 0;
}
