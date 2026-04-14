import asyncio
from apex_book import ApexClient

async def run_market_maker():
    client = ApexClient()
    await client.connect()
    
    # Wait for the order book to populate from the gateway thread
    await asyncio.sleep(2)
    
    user_id = 1
    symbol_id = 1
    
    while True:
        best_bid = client.book.get_best_bid()
        best_ask = client.book.get_best_ask()
        
        if best_bid and best_ask:
            spread = best_ask - best_bid
            mid_price = (best_ask + best_bid) // 2
            
            print(f"Spread: {spread}, Mid: {mid_price}")
            
            # Simple MM strategy: Quote 1 tick away from mid price
            my_bid = mid_price - 1
            my_ask = mid_price + 1
            
            # Send quotes
            await client.send_order(symbol_id, user_id, my_bid, 5, "BUY")
            await client.send_order(symbol_id, user_id, my_ask, 5, "SELL")
        else:
            print("Waiting for order book data...")
            
        await asyncio.sleep(1) # Re-quote every second

if __name__ == "__main__":
    asyncio.run(run_market_maker())
