#pragma once
#include <unordered_map>
#include <cstdint>

struct Balance {
    uint64_t available = 0;
    uint64_t locked = 0;
};

// 0 = USD (Quote)
// 1 = BTC (Base for Symbol 1)
// 2 = ETH (Base for Symbol 2)
// For simplicity: symbol_id 1 is BTC/USD, symbol_id 2 is ETH/USD.

class AccountManager {
private:
    std::unordered_map<uint64_t, std::unordered_map<uint32_t, Balance>> accounts;

public:
    void deposit(uint64_t user_id, uint32_t asset_id, uint64_t amount) {
        accounts[user_id][asset_id].available += amount;
    }

    bool lock_funds(uint64_t user_id, uint32_t asset_id, uint64_t amount) {
        auto& bal = accounts[user_id][asset_id];
        if (bal.available >= amount) {
            bal.available -= amount;
            bal.locked += amount;
            return true;
        }
        return false;
    }

    void unlock_funds(uint64_t user_id, uint32_t asset_id, uint64_t amount) {
        auto& bal = accounts[user_id][asset_id];
        // We assume they were locked previously
        bal.locked -= amount;
        bal.available += amount;
    }

    void settle_trade(uint64_t buyer_id, uint64_t seller_id, uint32_t base_asset, uint64_t price, uint64_t quantity) {
        uint32_t quote_asset = 0; // Hardcoded USD for simplicity
        uint64_t quote_amount = price * quantity;
        
        // Buyer: Locked Quote -> Seller Available Quote
        accounts[buyer_id][quote_asset].locked -= quote_amount;
        accounts[seller_id][quote_asset].available += quote_amount;

        // Seller: Locked Base -> Buyer Available Base
        accounts[seller_id][base_asset].locked -= quantity;
        accounts[buyer_id][base_asset].available += quantity;
    }

    std::string get_balances_snapshot() {
        std::string out = "[";
        bool first_user = true;
        for (const auto& [user_id, balances] : accounts) {
            if (!first_user) out += ",";
            out += "{\"user_id\":" + std::to_string(user_id) + ",\"assets\":[";
            bool first_asset = true;
            for (const auto& [asset_id, bal] : balances) {
                if (!first_asset) out += ",";
                out += "{\"asset_id\":" + std::to_string(asset_id) + ",\"available\":" + std::to_string(bal.available) + ",\"locked\":" + std::to_string(bal.locked) + "}";
                first_asset = false;
            }
            out += "]}";
            first_user = false;
        }
        out += "]";
        return out;
    }
};
