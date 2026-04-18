import asyncio
import json
import websockets
from textual.app import App, ComposeResult
from textual.containers import Horizontal, Vertical
from textual.widgets import Header, Footer, DataTable, Log, Static
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
    #left_pane {
        width: 1fr;
        border: solid green;
    }
    #right_pane {
        width: 1fr;
        border: solid blue;
    }
    DataTable {
        height: 1fr;
    }
    Log {
        height: 1fr;
    }
    .bid-row {
        color: green;
    }
    .ask-row {
        color: red;
    }
    """
    
    BINDINGS = [
        ("q", "quit", "Quit"),
        ("c", "clear", "Clear Tape"),
    ]

    def compose(self) -> ComposeResult:
        yield Header(show_clock=True)
        with Horizontal(id="main_container"):
            with Vertical(id="left_pane"):
                yield Static("📊 Live Limit Order Book", classes="pane_title")
                yield DataTable(id="orderbook")
            with Vertical(id="right_pane"):
                yield Static("📈 Trade Tape", classes="pane_title")
                yield Log(id="tape", highlight=True)
        yield Footer()

    def on_mount(self) -> None:
        table = self.query_one("#orderbook", DataTable)
        table.add_columns("Type", "Price", "Quantity")
        self.connect_to_feed()

    def action_clear(self) -> None:
        self.query_one("#tape", Log).clear()

    @work(exclusive=True, thread=False)
    async def connect_to_feed(self) -> None:
        uri = "ws://localhost:8080/ws"
        try:
            async with websockets.connect(uri) as ws:
                self.query_one("#tape", Log).write_line(f"[bold green]Connected to {uri}[/]")
                async for message in ws:
                    data = json.loads(message)
                    self.process_message(data)
        except Exception as e:
            self.query_one("#tape", Log).write_line(f"[bold red]Connection failed: {e}[/]")

    def process_message(self, data: dict) -> None:
        msg_type = data.get("type")
        if msg_type == "SNAPSHOT":
            self.update_orderbook(data.get("data", {}))
        elif msg_type == "TRADE":
            self.log_trade(data)

    def update_orderbook(self, book_data: dict) -> None:
        table = self.query_one("#orderbook", DataTable)
        table.clear()
        
        asks = book_data.get("asks", [])
        # Asks are sorted lowest first, so we reverse them to put highest ask at top of table
        for price, qty in reversed(asks):
            table.add_row("ASK", f"${price}", qty, label="🔴")
            
        bids = book_data.get("bids", [])
        for price, qty in bids:
            table.add_row("BID", f"${price}", qty, label="🟢")

    def log_trade(self, trade: dict) -> None:
        tape = self.query_one("#tape", Log)
        price = trade.get("price")
        qty = trade.get("qty")
        buyer = trade.get("buyer")
        seller = trade.get("seller")
        
        # Color the trade line based on aggressive side (hack: random color if we don't know who crossed)
        # We will just print it simply for now
        line = f"⚡ TRADE | Px: ${price} | Qty: {qty} | BuyerID: {buyer} | SellerID: {seller}"
        tape.write_line(line)

if __name__ == "__main__":
    app = ApexBookTUI()
    app.run()
