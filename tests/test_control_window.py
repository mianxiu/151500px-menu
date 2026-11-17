import os
import unittest
from types import SimpleNamespace
from unittest.mock import Mock, patch
from backend.control_window import ControlWindow, TITLE


@unittest.skipUnless(os.name=='nt','Windows management window')
class ControlWindowTests(unittest.TestCase):
    def test_same_title_from_other_process_cannot_be_found_or_closed(self):
        control=ControlWindow('http://localhost')
        control.process=SimpleNamespace(pid=100,poll=Mock(return_value=None),wait=Mock())
        def enum(callback,extra):
            callback(21,extra);callback(20,extra)
        with patch('win32gui.EnumWindows',side_effect=enum),patch('win32gui.GetWindowText',return_value=TITLE), \
             patch('win32gui.IsWindow',return_value=True),patch('win32process.GetWindowThreadProcessId',side_effect=lambda hwnd:(0,100 if hwnd==20 else 200)), \
             patch('win32gui.PostMessage') as close:
            self.assertEqual(control._find_window(),20)
            control.hwnd=21;control.close()
            close.assert_not_called()
            control.hwnd=20;control.close()
            close.assert_called_once()

    def test_missing_runtime_reports_manual_url_without_starting_another_browser(self):
        control=ControlWindow('http://127.0.0.1:15151')
        with patch('pathlib.Path.is_file', return_value=True), \
             patch('backend.control_window.subprocess.Popen', return_value=Mock(poll=Mock(return_value=3))) as launch, \
             patch('backend.control_window.webbrowser.open') as browser:
            with self.assertRaisesRegex(RuntimeError, 'WebView2 Runtime.*127.0.0.1:15151'):
                control.open()
            self.assertTrue(launch.call_args.args[0][0].endswith('ControlHost.exe'))
            browser.assert_not_called()
