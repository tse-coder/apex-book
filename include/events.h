#pragma once
#include "types.h"
#include <cstdint>

enum class EventType { ADD_ORDER, CANCEL_ORDER };

// Inbound: Gateway -> Engine
struct InboundEvent {
    EventType type;
    uint32_t symbol_id; 
    uint64_t order_id;
    uint64_t price;
    uint64_t quantity;
    Side side;
};

enum class OutboundEventType { TRADE, BOOK_UPDATE };

// Outbound: Engine -> MDP / Gateway
struct OutboundEvent {
    OutboundEventType type;
    uint32_t symbol_id;
    
    // Trade info
    uint64_t buyer_id;
    uint64_t seller_id;
    uint64_t price;
    uint64_t quantity;
};
