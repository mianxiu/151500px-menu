from copy import deepcopy
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from backend.photoshop_com_default import ALIAS, PhotoshopComDefault, running_application


class PhotoshopComDefaultTests(unittest.TestCase):
    def test_running_photoshop_uses_versioned_registration_with_a_mismatched_default(self):
        from unittest.mock import Mock

        with tempfile.TemporaryDirectory() as folder:
            running=Path(folder)/'Photoshop.exe'
            running.touch()
            attached=SimpleNamespace(Version='25.5.1')
            handle=Mock()
            attach=Mock(return_value=attached)
            module='backend.photoshop_com_default.'
            with patch.dict('sys.modules',win32api=SimpleNamespace(OpenProcess=lambda *_:handle),
                            win32process=SimpleNamespace(GetModuleFileNameEx=lambda *_:str(running))), \
                    patch('backend.uxp_management.photoshop_process_ids',return_value=(9,)) as pids, \
                    patch(module+'exe_version',return_value='25.5.1'), \
                    patch(module+'versions',return_value=[{'id':ALIAS+'.180','path':str(running.resolve())}]), \
                    patch(module+'registered_executable',return_value=Path(folder)/'2023'/'Photoshop.exe'):
                self.assertIs(running_application(attach,9),attached)
                attach.assert_called_once_with(ALIAS+'.180')
                handle.Close.assert_called_once()
                pids.return_value=(9,12)
                with self.assertRaisesRegex(RuntimeError,'唯一'):
                    running_application(attach,9)

    def test_switch_resolves_closed_target_and_preserves_original_on_failure_and_restore(self):
        old = {'root':False,'CLSID':[False,None],'CurVer':[False,None]}
        first = {'id':ALIAS+'.170','version':'24.0.0','clsid':'{00000000-0000-0000-0000-000000000170}','path':'2023/Photoshop.exe'}
        second = {'id':ALIAS+'.180','version':'25.5.0','clsid':'{00000000-0000-0000-0000-000000000180}','path':'2024/Photoshop.exe'}
        current = deepcopy(old)
        fail_default = False

        def write(value):
            current.clear()
            current.update(deepcopy(value))

        def read_value(hive, path):
            key = path.rsplit('\\',1)[-1]
            default = first['id'] if key=='CurVer' else first['clsid']
            return True, current[key][1] or default

        def resolve(name):
            self.assertEqual(name, ALIAS)
            return first['clsid'] if fail_default else read_value(None, 'CLSID')[1]

        def executable():
            return Path(next(item['path'] for item in (first,second) if item['id']==read_value(None,'CurVer')[1]))

        module='backend.photoshop_com_default.'
        with tempfile.TemporaryDirectory() as folder, patch.dict('sys.modules',winreg=SimpleNamespace(HKEY_CLASSES_ROOT=1), \
                pywintypes=SimpleNamespace(IID=resolve)), \
                patch(module+'versions',return_value=[second,first]), patch(module+'installed_paths',return_value=[]), \
                patch(module+'registered_executable',side_effect=executable), patch(module+'registry_value',side_effect=read_value), \
                patch(module+'user_state',side_effect=lambda:deepcopy(current)), patch(module+'write_user_state',side_effect=write) as writer:
            manager=PhotoshopComDefault(Path(folder))
            with self.assertRaisesRegex(RuntimeError,'已不可用'):
                manager.switch(ALIAS+'.999')
            writer.assert_not_called()
            self.assertEqual(manager.switch(second['id'])['default'],second)
            self.assertEqual(json.loads(manager.backup.read_text('utf-8'))['original'],old)
            changed=deepcopy(current)
            fail_default=True
            with self.assertRaisesRegex(RuntimeError,'已恢复切换前'):
                manager.switch(second['id'])
            self.assertEqual(current,changed)
            self.assertEqual(manager.restore()['default'],first)
            self.assertEqual(current,old)
            self.assertFalse(manager.backup.exists())

    def test_installation_list_includes_unregistered_and_deduplicates_by_executable(self):
        from backend.photoshop_com_default import installation_list

        with tempfile.TemporaryDirectory() as folder:
            paths=[Path(folder)/year/'Photoshop.exe' for year in ('2024','2025')]
            for path in paths:
                path.parent.mkdir()
                path.touch()
            registered={'id':ALIAS+'.180','version':'25.5.1','path':str(paths[0].resolve())}
            with patch('backend.photoshop_com_default.installed_paths',return_value=paths), \
                    patch('backend.photoshop_com_default.exe_version',return_value='26.0.0'):
                items=installation_list([registered], paths[0])
            self.assertEqual([item['id'] for item in items], [None,registered['id']])
