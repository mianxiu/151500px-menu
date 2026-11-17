from copy import deepcopy
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from backend.photoshop_com_default import ALIAS, PhotoshopComDefault


class PhotoshopComDefaultTests(unittest.TestCase):
    def test_switch_verifies_live_target_and_preserves_original_on_failure_and_restore(self):
        old = {'root':False,'CLSID':[False,None],'CurVer':[False,None]}
        first = {'id':ALIAS+'.170','version':'24.0.0','clsid':'{00000000-0000-0000-0000-000000000170}'}
        second = {'id':ALIAS+'.180','version':'25.5.0','clsid':'{00000000-0000-0000-0000-000000000180}'}
        current = deepcopy(old)
        expected = second
        fail_default = False

        def write(value):
            current.clear()
            current.update(deepcopy(value))

        def read_value(hive, path):
            key = path.rsplit('\\',1)[-1]
            default = first['id'] if key=='CurVer' else first['clsid']
            return True, current[key][1] or default

        def active(name):
            if name not in (ALIAS,expected['id']) or name==ALIAS and fail_default:
                raise RuntimeError('No active object')
            return expected['version']

        module='backend.photoshop_com_default.'
        with tempfile.TemporaryDirectory() as folder, patch.dict('sys.modules',winreg=SimpleNamespace(HKEY_CLASSES_ROOT=1)), \
                patch(module+'versions',return_value=[second,first]), patch(module+'photoshop_process_ids',return_value=(5,)), \
                patch(module+'active_version',side_effect=active), patch(module+'registry_value',side_effect=read_value), \
                patch(module+'user_state',side_effect=lambda:deepcopy(current)), patch(module+'write_user_state',side_effect=write) as writer:
            manager=PhotoshopComDefault(Path(folder))
            with self.assertRaisesRegex(RuntimeError,'先打开'):
                manager.switch(first['id'])
            writer.assert_not_called()
            self.assertEqual(manager.switch(second['id'])['default'],second)
            self.assertEqual(json.loads(manager.backup.read_text('utf-8'))['original'],old)
            changed=deepcopy(current)
            expected,fail_default=first,True
            with self.assertRaisesRegex(RuntimeError,'已恢复切换前'):
                manager.switch(first['id'])
            self.assertEqual(current,changed)
            self.assertEqual(manager.restore()['default'],first)
            self.assertEqual(current,old)
            self.assertFalse(manager.backup.exists())
