#pragma once
#include <cstdint>
#include <string>

enum class Side { BUY, SELL };

struct Order {
    uint64_t id;
    uint64_t user_id;
    uint64_t price;
    uint64_t quantity;
    Side side;

    Order* prev = nullptr;
    Order* next = nullptr;
};

struct Trade {
    uint64_t buyer_order_id;
    uint64_t seller_order_id;
    uint64_t buyer_user_id;
    uint64_t seller_user_id;
    uint64_t price;
    uint64_t quantity;
};
