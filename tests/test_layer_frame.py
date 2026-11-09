import asyncio
import json
import tempfile
import time
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock,patch

from backend.layer_frame import FramePreview


class FramePreviewTests(unittest.IsolatedAsyncioTestCase):
    def test_release_before_bind_keeps_original_identity(self):
        with tempfile.TemporaryDirectory() as folder:
            preview=FramePreview(12)
            preview.finish(True)
            preview.finish(False)
            preview.bind(folder,dict(id='request',session='session',token='secret'))
            value=json.loads((Path(folder)/'mask-preview-request.json').read_text())
            self.assertEqual({k:value[k] for k in ('id','session','token','phase','seq')},
                             dict(id='request',session='session',token='secret',phase='cancel',seq=2))
            self.assertGreater(value['lease_until_ms'],time.time()*1000)

    async def test_health_recovers_lost_release_but_ignores_old_gesture(self):
        for identity,phase,expected in ((12,1,'commit'),(12,2,'cancel'),(11,2,'preview')):
            preview=FramePreview(12)
            desktop=SimpleNamespace(hook_health=(time.monotonic(),dict(frame_id=identity,frame_phase=phase)),_scope=lambda:True)
            stopped=asyncio.Event()
            def published():stopped.set()
            preview.publish=published
            await preview.pump(desktop,stopped)
            self.assertEqual(preview.phase,expected)

    async def test_foreground_loss_cancels_instead_of_committing(self):
        preview=FramePreview(12)
        stopped=asyncio.Event()
        preview.publish=stopped.set
        await preview.pump(SimpleNamespace(hook_health=None,_scope=lambda:False),stopped)
        self.assertEqual(preview.phase,'cancel')

    async def test_healthy_hold_has_no_thirty_second_expiry(self):
        preview=FramePreview(12);stopped=asyncio.Event();preview.publish=stopped.set
        desktop=SimpleNamespace(hook_health=(31,dict(frame_id=12,frame_phase=0)),_scope=lambda:True)
        with patch('backend.layer_frame.time',SimpleNamespace(monotonic=Mock(side_effect=[0,31,31]))):
            await preview.pump(desktop,stopped)
        self.assertEqual(preview.phase,'preview')
