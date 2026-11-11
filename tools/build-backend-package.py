"""Distribute one main EXE with UXP resources and management in its backend."""
import hashlib
import argparse
from pathlib import Path
import sys
import zipfile
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from backend.version import APP_VERSION
from backend.uxp_management import UxpManager
from PyInstaller.archive.readers import CArchiveReader
import win32api

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--exe',type=Path,default=ROOT/'dist/151500px-menu.exe')
parser.add_argument('--output-dir',type=Path,default=ROOT/'dist')
args=parser.parse_args()
exe=args.exe.resolve()
output_dir=args.output_dir.resolve()
output_dir.mkdir(parents=True,exist_ok=True)
assert exe.name=='151500px-menu.exe', 'The unified package must contain 151500px-menu.exe'
parts=[int(p) for p in APP_VERSION.split('.')]
info=win32api.GetFileVersionInfo(str(exe),'\\')
assert (info['FileVersionMS'],info['FileVersionLS'])==((parts[0]<<16)|parts[1],parts[2]<<16)
archive=CArchiveReader(str(exe));names={n.replace(chr(92),'/'):n for n in archive.toc}
resources={n for n in names if n.startswith('uxp/offline-probe/')}
assert resources=={'uxp/offline-probe/'+n for n in ('manifest.json','main.js','labels.js')},resources
for n in resources:assert archive.extract(names[n])==(ROOT/n).read_bytes()
for n in ('native/MenuHost.exe','native/HotkeyHost.exe','native/InputContext.dll','native/InputService.dll'):assert archive.extract(names[n])==(ROOT/n).read_bytes(),n
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
exe_checksum=output_dir/'151500px-menu.exe.sha256'
exe_checksum.write_text(sha(exe)+'  151500px-menu.exe\n','ascii')
plugin=output_dir/('151500px-menu-UXP-'+APP_VERSION+'.zip')
plugin.write_bytes(UxpManager(ROOT/'.runtime',ROOT/'uxp/offline-probe',ROOT/'.runtime/unused-package-root').export_builtin())
output=output_dir/('151500px-menu-Backend-'+APP_VERSION+'.zip')
with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED) as bundle:
    for p in (exe,exe_checksum,ROOT/'OFFLINE-INSTALL.md',ROOT/'PERFORMANCE.md',ROOT/'RELEASES.md',ROOT/'LICENSE',ROOT/'THIRD_PARTY_NOTICES.md',plugin):bundle.write(p,p.name)
    bundle.write(ROOT/'docs/CONTEXT-COVERAGE.md','docs/CONTEXT-COVERAGE.md')
    bundle.write(ROOT/'docs/UPDATES.md','docs/UPDATES.md')
    bundle.write(ROOT/'docs/DIAGNOSTICS.md','docs/DIAGNOSTICS.md')
    bundle.write(ROOT/'docs/UXP-LOADING.md','docs/UXP-LOADING.md')
    bundle.write(ROOT/'docs/READ-STRATEGIES.md','docs/READ-STRATEGIES.md')
    bundle.write(ROOT/'docs/CREATION-TREES.md','docs/CREATION-TREES.md')
    bundle.write(ROOT/'docs/CAPS-EXECUTION.md','docs/CAPS-EXECUTION.md')
    bundle.write(ROOT/'docs/DIVERSE-LAYERS.md','docs/DIVERSE-LAYERS.md')
    bundle.write(ROOT/'docs/UI-GUIDE.md','docs/UI-GUIDE.md')
    bundle.write(ROOT/'docs/UI-TYPOGRAPHY.md','docs/UI-TYPOGRAPHY.md')
    bundle.write(ROOT/'docs/MENU-RECOVERY.md','docs/MENU-RECOVERY.md')
    bundle.write(ROOT/'docs/SHORTCUTS.md','docs/SHORTCUTS.md')
    bundle.write(ROOT/'docs/MASK-PREVIEW.md','docs/MASK-PREVIEW.md')
    bundle.write(ROOT/'docs/GRADIENT-EDITOR.md','docs/GRADIENT-EDITOR.md')
    bundle.write(ROOT/'docs/PREVIEW-PROGRESS.md','docs/PREVIEW-PROGRESS.md')
    bundle.write(ROOT/'docs/PREVIEW-EXECUTION.md','docs/PREVIEW-EXECUTION.md')
    bundle.write(ROOT/'docs/BATCH-MASKS.md','docs/BATCH-MASKS.md')
    bundle.write(ROOT/'docs/SHAPE-PAINT-CLIPBOARD.md','docs/SHAPE-PAINT-CLIPBOARD.md')
    bundle.write(ROOT/'docs/NATIVE-COLOR-CHANNELS.md','docs/NATIVE-COLOR-CHANNELS.md')
    bundle.write(ROOT/'docs/PERCEPTUAL-COLOR.md','docs/PERCEPTUAL-COLOR.md')
    bundle.write(ROOT/'docs/LEGACY-PREVIEW.md','docs/LEGACY-PREVIEW.md')
    bundle.write(ROOT/'docs/VERSION-HEALTH.md','docs/VERSION-HEALTH.md')
    bundle.write(ROOT/'docs/TEXT-TOOLS.md','docs/TEXT-TOOLS.md')
    bundle.write(ROOT/'docs/TEXT-LIST-OBJECTS.md','docs/TEXT-LIST-OBJECTS.md')
    bundle.write(ROOT/'docs/TEXT-VECTOR-OBJECTS.md','docs/TEXT-VECTOR-OBJECTS.md')
    bundle.write(ROOT/'docs/TEXT-STYLE-PLAN.md','docs/TEXT-STYLE-PLAN.md')
    bundle.write(ROOT/'docs/TEXT-PREVIEW-PERFORMANCE.md','docs/TEXT-PREVIEW-PERFORMANCE.md')
    bundle.write(ROOT/'docs/INDIVIDUAL-SMART-OBJECTS.md','docs/INDIVIDUAL-SMART-OBJECTS.md')
    bundle.write(ROOT/'docs/LAYER-SORTING.md','docs/LAYER-SORTING.md')
    bundle.write(ROOT/'docs/LAYER-FOCUS.md','docs/LAYER-FOCUS.md')
    bundle.write(ROOT/'docs/INPUT-LANGUAGE-ASSESSMENT.md','docs/INPUT-LANGUAGE-ASSESSMENT.md')
    bundle.write(ROOT/'docs/HOOK-COMPATIBILITY.md','docs/HOOK-COMPATIBILITY.md')
    bundle.write(ROOT/'docs/MENU-SHADOW.md','docs/MENU-SHADOW.md')
    bundle.write(ROOT/'docs/MENU-RESPONSIVENESS.md','docs/MENU-RESPONSIVENESS.md')
    bundle.write(ROOT/'docs/STABILITY-TEST.md','docs/STABILITY-TEST.md')
    bundle.write(ROOT/'tools/inspect-panels.ps1','tools/inspect-panels.ps1')
    bundle.write(ROOT/'tools/inspect-uxp.ps1','tools/inspect-uxp.ps1')
    for notice in sorted((ROOT/'licenses').glob('*.txt')):bundle.write(notice,'licenses/'+notice.name)
    for source,target in ((ROOT/'native/fonts/OFL-1.1.txt','licenses/fonts-OFL-1.1.txt'),(ROOT/'native/fonts/COPYRIGHT.txt','licenses/fonts-COPYRIGHT.txt'),(ROOT/'native/vendor/imgui/LICENSE.txt','licenses/imgui.txt'),(ROOT/'native/vendor/json/LICENSE.MIT','licenses/json.txt'),(ROOT/'native/vendor/manifest.json','licenses/native-manifest.json')):bundle.write(source,target)
output.with_suffix('.zip.sha256').write_text(sha(output)+'  '+output.name+'\n','ascii')
print('Built unified backend:',output)
