"""Install the optional, read-only Photoshop Generator companion while Photoshop is closed."""
import argparse
import json
import secrets
import tempfile
from pathlib import Path

from backend.settings import atomic_json
from backend.uxp_management import photoshop_running
from backend.paths import ROOT


def install(photoshop,runtime):
    if photoshop_running():raise RuntimeError('请先正常退出 Photoshop')
    exe=Path(photoshop).resolve(strict=True)
    if exe.name.lower()!='photoshop.exe' or not (exe.parent/'Required/Generator-builtin/lib/generator.js').is_file():
        raise ValueError('请选择装有 Generator 的 Photoshop.exe')
    parent=exe.parent/'Plug-ins/Generator'
    if not parent.is_dir():raise ValueError('未找到 Photoshop Generator 插件目录')
    folder=Path(runtime).resolve(strict=True)/'text-canvas'
    folder.mkdir(exist_ok=True)
    target=parent/'151500px-menu-text-canvas'
    legacy_files={'main.js','package.json','connection.json'}
    files=('main.js','package.json','text-geometry.jsx')
    if target.exists():
        if (target.is_symlink() or target.resolve()!=target or {p.name for p in target.iterdir()} not in (legacy_files,set(files)|{'connection.json'}) or
                json.loads((target/'package.json').read_text('utf-8'))['name']!='151500px-menu-text-canvas' or
                Path(json.loads((target/'connection.json').read_text('utf-8'))['folder'])!=folder):
            raise ValueError('目标目录与当前安装身份不匹配')
    pair=folder/'pairing'
    if not pair.exists():
        with pair.open('x',encoding='ascii') as new:new.write(secrets.token_hex(16))
    token=pair.read_text('ascii')
    if len(token)!=32 or any(c not in '0123456789abcdef' for c in token):raise ValueError('无效的本机配对凭据')
    with tempfile.TemporaryDirectory(dir=parent) as scratch:
        stage=Path(scratch)/target.name;stage.mkdir()
        for name in files:
            (stage/name).write_bytes((ROOT/'generator/text-canvas'/name).read_bytes())
        atomic_json(stage/'connection.json',{'folder':str(folder),'token':token})
        if not target.exists():stage.rename(target)
        else:
            for name in (*files,'connection.json'):(stage/name).replace(target/name)
    return target


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--photoshop',required=True);parser.add_argument('--data-dir',required=True)
    args=parser.parse_args()
    print(install(args.photoshop,args.data_dir))
