#include "engine_manager.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    SPSCQueue<InboundEvent> inbound(1024);
    SPSCQueue<OutboundEvent> outbound(1024);

    EngineManager engine(inbound, outbound);
    
    // Deposit funds to user 1 and user 2
    engine.deposit(1, 0, 50000); // 50k USD
    engine.deposit(1, 1, 100);   // 100 BTC
    engine.deposit(2, 0, 50000);
    engine.deposit(2, 1, 100);

    engine.start();

    // User 1 buys 10 @ 1000 = 10,000 USD
    InboundEvent ev1 = {EventType::ADD_ORDER, 1, 1, 1, 1000, 10, Side::BUY};
    while (!inbound.push(ev1));

    // User 2 sells 10 @ 1000 = 10 BTC
    InboundEvent ev2 = {EventType::ADD_ORDER, 1, 2, 2, 1000, 10, Side::SELL};
    while (!inbound.push(ev2));
    
    // User 3 tries to buy without funds (should reject)
    InboundEvent ev3 = {EventType::ADD_ORDER, 1, 3, 3, 1000, 10, Side::BUY};
    while (!inbound.push(ev3));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    OutboundEvent out;
    while (outbound.pop(out)) {
        if (out.type == OutboundEventType::TRADE) {
            std::cout << "TRADE: Symbol=" << out.symbol_id 
                      << " Px=" << out.price 
                      << " Qty=" << out.quantity 
                      << " Buyer=" << out.buyer_id 
                      << " Seller=" << out.seller_id << std::endl;
        } else if (out.type == OutboundEventType::ORDER_REJECTED) {
            std::cout << "REJECTED: Order=" << out.order_id << std::endl;
        }
    }

    engine.stop();
    return 0;
}
