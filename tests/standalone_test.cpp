#include "engine_manager.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    SPSCQueue<InboundEvent> inbound(1024);
    SPSCQueue<OutboundEvent> outbound(1024);

    EngineManager engine(inbound, outbound);
    engine.start();

    // Send some events
    for (int i = 0; i < 5; ++i) {
        InboundEvent ev;
        ev.type = EventType::ADD_ORDER;
        ev.symbol_id = 1;
        ev.order_id = i + 1;
        ev.price = 1000;
        ev.quantity = 10;
        ev.side = (i % 2 == 0) ? Side::BUY : Side::SELL;
        while (!inbound.push(ev));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Read trades
    OutboundEvent out;
    while (outbound.pop(out)) {
        if (out.type == OutboundEventType::TRADE) {
            std::cout << "TRADE: Symbol=" << out.symbol_id 
                      << " Px=" << out.price 
                      << " Qty=" << out.quantity << std::endl;
        }
    }

    engine.stop();
    return 0;
}
