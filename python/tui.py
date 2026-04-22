import asyncio
import json
from textual.app import App, ComposeResult
from textual.containers import Horizontal, Vertical
from textual.widgets import Header, Footer, DataTable, Log, Static, Sparkline
from textual import work

class ApexBookTUI(App):
    CSS = """
    Screen {
        layout: vertical;
    }
    #main_container {
        height: 1fr;
        layout: horizontal;
    }
    .pane {
        width: 1fr;
        border: solid green;
    }
    .pane_title {
        text-align: center;
        background: $accent;
        color: $text;
        text-style: bold;
    }
    DataTable {
        height: 1fr;
    }
    Log {
        height: 1fr;
    }
    #spark_container {
        height: 3;
        margin-top: 1;
        border: solid blue;
    }
    """
    
    BINDINGS = [
        ("q", "quit", "Quit"),
        ("c", "clear", "Clear Tape"),
    ]

    def __init__(self):
        super().__init__()
        self.spread_history = []

    def compose(self) -> ComposeResult:
        yield Header(show_clock=True)
        with Horizontal(id="main_container"):
            # Left: OrderBook + Spread Sparkline
            with Vertical(classes="pane"):
                yield Static("Live Limit Order Book", classes="pane_title")
                yield DataTable(id="orderbook")
                with Vertical(id="spark_container"):
                    yield Static("Spread Trend (Ticks)", classes="pane_title")
                    yield Sparkline(id="spread_spark", data=[0]*50, summary_function=max)
                    
            # Middle: Trade Tape
            with Vertical(classes="pane"):
                yield Static("Trade Tape", classes="pane_title")
                yield Log(id="tape", highlight=True)
                
            # Right: Account Balances
            with Vertical(classes="pane"):
                yield Static("Account Balances", classes="pane_title")
                yield DataTable(id="balances")
                
        yield Footer()

    def on_mount(self) -> None:
        # Init Orderbook Table
        ob = self.query_one("#orderbook", DataTable)
        ob.add_columns("Side", "Price", "Qty")
        
        # Init Balances Table
        bal = self.query_one("#balances", DataTable)
        bal.add_columns("User", "USD", "BTC", "ETH")
        
        self.connect_to_feed()

    def action_clear(self) -> None:
        self.query_one("#tape", Log).clear()

    @work(exclusive=True, thread=False)
    async def connect_to_feed(self) -> None:
        try:
            process = await asyncio.create_subprocess_exec(
                "../build/apex_server",
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE
            )
            
            self.query_one("#tape", Log).write_line("Started ApexBook C++ Engine...")
            
            while True:
                line = await process.stdout.readline()
                if not line:
                    break
                    
                line_str = line.decode('utf-8').strip()
                if not line_str:
                    continue
                    
                try:
                    data = json.loads(line_str)
                    self.process_message(data)
                except json.JSONDecodeError:
                    pass

        except Exception as e:
            self.query_one("#tape", Log).write_line(f"Failed to start engine: {e}")

    def process_message(self, data: dict) -> None:
        msg_type = data.get("type")
        if msg_type == "SNAPSHOT":
            self.update_orderbook(data.get("data", {}))
            self.update_balances(data.get("balances", []))
        elif msg_type == "TRADE":
            self.log_trade(data)

    def update_orderbook(self, book_data: dict) -> None:
        table = self.query_one("#orderbook", DataTable)
        table.clear()
        
        asks = book_data.get("asks", [])
        bids = book_data.get("bids", [])
        
        # Update Spread Sparkline
        if asks and bids:
            best_ask = asks[0][0]
            best_bid = bids[0][0]
            spread = best_ask - best_bid
            self.spread_history.append(spread)
            if len(self.spread_history) > 50:
                self.spread_history.pop(0)
            self.query_one("#spread_spark", Sparkline).data = self.spread_history

        # Asks
        for price, qty in reversed(asks):
            table.add_row("ASK", f"${price}", qty)
            
        # Bids
        for price, qty in bids:
            table.add_row("BID", f"${price}", qty)

    def update_balances(self, balances: list) -> None:
        table = self.query_one("#balances", DataTable)
        table.clear()
        
        for user in balances:
            user_id = user.get("user_id")
            usd = btc = eth = 0
            
            for asset in user.get("assets", []):
                aid = asset.get("asset_id")
                avail = asset.get("available")
                # Format balances cleanly
                if aid == 0: usd = avail
                elif aid == 1: btc = avail
                elif aid == 2: eth = avail
                
            table.add_row(f"User {user_id}", f"${usd:,}", f"{btc:,}", f"{eth:,}")

    def log_trade(self, trade: dict) -> None:
        tape = self.query_one("#tape", Log)
        price = trade.get("price")
        qty = trade.get("qty")
        buyer = trade.get("buyer")
        seller = trade.get("seller")
        
        line = f"TRADE | Px: ${price} | Qty: {qty} | B: {buyer} | S: {seller}"
        tape.write_line(line)

if __name__ == "__main__":
    app = ApexBookTUI()
    app.run()
