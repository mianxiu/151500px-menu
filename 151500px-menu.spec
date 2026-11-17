# Build one windowed executable, including its native-menu worker mode.
from pathlib import Path
import subprocess
import sys
from comtypes.client import GetModule

root = Path(SPECPATH)
GetModule('UIAutomationCore.dll')
subprocess.run([sys.executable, '-m', 'unittest', 'discover', '-s', str(root/'tests'),
                '-p', 'test_menu_ui_contract.py'], cwd=root, check=True)
a = Analysis(
    [str(root / 'main.py')], pathex=[str(root)], binaries=[],
    datas=[(str(root / 'tools/inspect-panels.ps1'), 'tools'),(str(root / 'tools/inspect-uxp.ps1'), 'tools'),(str(root / 'frontend/dist'), 'frontend/dist'),
                      (str(root / 'frontend/public/icon.ico'), 'frontend/public'),
                      (str(root / 'native/HotkeyHost.exe'), 'native'),
                      (str(root / 'native/MenuHost.exe'), 'native'),
                      (str(root / 'native/InputContext.dll'), 'native'),
                      (str(root / 'native/InputService.dll'), 'native'),
                      (str(root / 'native/control'), 'native/control'),
                      (str(root / 'native/fonts'), 'native/fonts'),
                      (str(root / 'native/vendor/imgui/LICENSE.txt'), 'native/licenses/imgui'),
                      (str(root / 'native/vendor/json/LICENSE.MIT'), 'native/licenses/json'),
                      (str(root / 'native/vendor/manifest.json'), 'native/licenses'),
                      (str(root / 'uxp/offline-probe/manifest.json'), 'uxp/offline-probe'),
                      (str(root / 'uxp/offline-probe/main.js'), 'uxp/offline-probe'),
                      (str(root / 'uxp/offline-probe/labels.js'), 'uxp/offline-probe'),
                      (str(root / 'jsx'), 'jsx')],
    hiddenimports=['win32timezone', 'backend.overlay', 'comtypes.gen.UIAutomationClient'],
    hookspath=[], runtime_hooks=[], excludes=['tkinter', 'matplotlib', 'dearpygui'], noarchive=False,
)
pyz = PYZ(a.pure)
exe = EXE(pyz, a.scripts, a.binaries, a.datas, [], name='151500px-menu',
          debug=False, bootloader_ignore_signals=False, strip=False, upx=False,
          console=False, disable_windowed_traceback=False,
          icon=str(root / 'frontend/public/icon.ico'),
          version=str(root / 'tools/windows-version.txt'),
          uac_admin=False)
