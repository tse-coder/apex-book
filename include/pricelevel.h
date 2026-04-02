#pragma once
#include "types.h"

class PriceLevel {
public:
    uint64_t price;
    Order* head = nullptr;
    Order* tail = nullptr;

    PriceLevel(uint64_t p) : price(p) {}
    PriceLevel() : price(0) {}

    void append(Order* order) {
        if (!head) {
            head = tail = order;
        } else {
            tail->next = order;
            order->prev = tail;
            tail = order;
        }
    }

    void remove(Order* order) {
        if (order->prev) order->prev->next = order->next;
        if (order->next) order->next->prev = order->prev;
        if (head == order) head = order->next;
        if (tail == order) tail = order->prev;
        order->prev = nullptr;
        order->next = nullptr;
    }

    bool empty() const {
        return head == nullptr;
    }
};
