import asyncio
import websockets
import json
import os

async def listen_to_feed():
    uri = "ws://localhost:8080/ws"
    async with websockets.connect(uri) as websocket:
        print("Connected to ApexBook Real-Time Feed!")
        while True:
            try:
                message = await websocket.recv()
                data = json.loads(message)
                
                os.system('clear' if os.name == 'posix' else 'cls')
                print("================ APEX BOOK ================")
                print(f"{'ASKS (Price | Qty)':^39}")
                print("---------------------------------------")
                
                # Reverse asks so lowest price is at the bottom (closest to spread)
                asks = data.get("asks", [])
                for price, qty in reversed(asks):
                    print(f"\033[91m{price:>15} | {qty:<15}\033[0m")
                    
                print("---------------------------------------")
                print(f"{'BIDS (Price | Qty)':^39}")
                print("---------------------------------------")
                
                bids = data.get("bids", [])
                for price, qty in bids:
                    print(f"\033[92m{price:>15} | {qty:<15}\033[0m")
                    
                print("=======================================")
                
            except websockets.exceptions.ConnectionClosed:
                print("Connection closed by server.")
                break

if __name__ == "__main__":
    asyncio.run(listen_to_feed())
