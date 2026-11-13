"""Bounded status delivery; a slow UI must never hold the operation lock."""
import asyncio
import json
from tornado.websocket import WebSocketClosedError


class StatusDelivery:
    def __init__(self, clients, max_pending=4, max_bytes=512*1024):
        self.clients=clients
        self.max_pending=max_pending
        self.max_bytes=max_bytes
        self.pending={}

    def send(self, event):
        # These are full status/task snapshots, not an operation queue. Keep
        # ordering for responsive clients; a stalled client reconnects and
        # fetches current state instead of delaying document execution.
        if not self.clients:return
        encoded=json.dumps(event,ensure_ascii=False)
        size=len(encoded.encode('utf-8'))
        for client in tuple(self.clients):
            pending=[(f,n) for f,n in self.pending.get(client,()) if not f.done()]
            if len(pending)>=self.max_pending or size+sum(n for _,n in pending)>self.max_bytes:
                self.clients.discard(client);self.pending.pop(client,None)
                client.close(code=1013,reason='Status consumer is too slow; reconnect for current state')
                continue
            try:
                future=asyncio.ensure_future(client.write_message(encoded))
            except WebSocketClosedError:
                self.clients.discard(client);self.pending.pop(client,None);continue
            pending.append((future,size));self.pending[client]=pending
            def finished(done,consumer=client):
                try:done.result()
                except (Exception,asyncio.CancelledError):self.clients.discard(consumer)
                remaining=[(f,n) for f,n in self.pending.get(consumer,()) if f is not done and not f.done()]
                if remaining:self.pending[consumer]=remaining
                else:self.pending.pop(consumer,None)
            future.add_done_callback(finished)

    def forget(self, client):
        self.pending.pop(client,None)
