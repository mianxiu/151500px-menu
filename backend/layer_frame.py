"""The original F request owns both its temporary view and its cleanup."""
import asyncio
import json
import threading
import time
from pathlib import Path


class FramePreview:
    def __init__(self, identity):
        self.identity = identity
        self.phase = 'preview'
        self.seq = 1
        self.binding = None
        self.lock = threading.Lock()
        self.execution_check = None
        self.publish_lock = threading.Lock()

    def finish(self, restore=False):
        with self.lock:
            if self.phase == 'preview':
                self.phase = 'cancel' if restore else 'commit'
                self.seq += 1

    def bind(self, folder, request):
        with self.lock:
            self.binding = (Path(folder), {k: request[k] for k in ('id', 'session', 'token')})
        self.publish()

    def publish(self):
        with self.publish_lock:self._publish()

    def _publish(self):
        from .uxp_bridge import publish_request
        with self.lock:
            if not self.binding:return
            folder, identity = self.binding
            value = {**identity, 'seq': self.seq, 'phase': self.phase,
                     'lease_until_ms': int(time.time()*1000)+4000}
        temporary = folder/(identity['id']+'.frame.tmp')
        temporary.write_text(json.dumps(value), 'utf-8')
        publish_request(temporary, folder/('mask-preview-'+identity['id']+'.json'))

    async def pump(self, desktop, stopped):
        started = time.monotonic()
        while not stopped.is_set():
            health = desktop.hook_health
            if health and health[1].get('frame_id') == self.identity:
                phase = health[1].get('frame_phase')
                if phase in (1, 2):self.finish(phase == 2)
            if (time.monotonic()-started > 4 and (not health or time.monotonic()-health[0] > 3.5 or health[1].get('frame_id') != self.identity) or
                    not await asyncio.to_thread(desktop._scope)):
                self.finish(True)
            # Publication failure stops lease renewal; PS restores the original view.
            await asyncio.to_thread(self.publish)
            try:await asyncio.wait_for(stopped.wait(), .08)
            except asyncio.TimeoutError:pass
