#include "orderbook.h"
#include "crow.h"
#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <random>
#include <unordered_set>

int main() {
    OrderBook book;
    std::mutex book_mutex;
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
      })
      .onmessage([&](crow::websocket::connection& /*conn*/, const std::string& data, bool is_binary) {
          // Process incoming orders here if needed
      });

    // Market Data Feed Thread
    std::thread feed_thread([&]() {
        uint64_t id = 1;
        std::mt19937 gen(1337);
        std::uniform_int_distribution<uint64_t> price_dist(950, 1050);
        std::uniform_int_distribution<uint64_t> qty_dist(1, 100);
        std::uniform_int_distribution<int> side_dist(0, 1);

        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            {
                std::lock_guard<std::mutex> lock(book_mutex);
                // Simulate market activity
                for(int i=0; i<5; ++i) {
                    book.add_order(id++, price_dist(gen), qty_dist(gen), side_dist(gen) == 0 ? Side::BUY : Side::SELL);
                }
                
                std::string snapshot = book.get_snapshot();
                std::lock_guard<std::mutex> cl_lock(clients_mutex);
                for(auto* conn : clients) {
                    conn->send_text(snapshot);
                }
            }
        }
    });

    std::cout << "Starting ApexBook Real-Time Data Feed on ws://localhost:8080/ws\n";
    app.port(8080).multithreaded().run();

    feed_thread.join();
    return 0;
}
