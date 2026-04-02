#pragma once
#include "types.h"
#include <vector>

class OrderPool {
    std::vector<Order> pool;
    std::vector<Order*> free_list;
public:
    OrderPool(size_t size) {
        pool.resize(size);
        free_list.reserve(size);
        for(size_t i = 0; i < size; ++i) {
            free_list.push_back(&pool[i]);
        }
    }

    Order* allocate(uint64_t id, uint64_t price, uint64_t qty, Side side) {
        if(free_list.empty()) return nullptr;
        Order* o = free_list.back();
        free_list.pop_back();
        o->id = id;
        o->price = price;
        o->quantity = qty;
        o->side = side;
        o->prev = nullptr;
        o->next = nullptr;
        return o;
    }

    void deallocate(Order* o) {
        free_list.push_back(o);
    }
};
