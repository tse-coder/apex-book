import asyncio
import websockets
import json
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("ApexSDK")

class LocalOrderBook:
    def __init__(self):
        self.bids = []
        self.asks = []
    
    def update_from_snapshot(self, data):
        self.bids = data.get("bids", [])
        self.asks = data.get("asks", [])
        
    def get_best_bid(self):
        return self.bids[0][0] if self.bids else None
        
    def get_best_ask(self):
        return self.asks[0][0] if self.asks else None

class ApexClient:
    def __init__(self, uri="ws://localhost:8080/ws"):
        self.uri = uri
        self.ws = None
        self.book = LocalOrderBook()

    async def connect(self):
        self.ws = await websockets.connect(self.uri)
        logger.info(f"Connected to {self.uri}")
        asyncio.create_task(self._listen())

    async def _listen(self):
        try:
            async for message in self.ws:
                data = json.loads(message)
                self.book.update_from_snapshot(data)
                # Implement delta update processing here
        except websockets.exceptions.ConnectionClosed:
            logger.error("Connection Closed")

    async def send_order(self, symbol_id, user_id, price, quantity, side):
        payload = {
            "type": "ADD_ORDER",
            "symbol_id": symbol_id,
            "user_id": user_id,
            "price": price,
            "quantity": quantity,
            "side": side
        }
        await self.ws.send(json.dumps(payload))
        logger.info(f"Sent Order: {side} {quantity} @ {price}")
