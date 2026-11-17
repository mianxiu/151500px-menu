"""Photoshop installation/runtime version floor; metadata reads only."""
import os
from pathlib import Path
import re

MIN_PHOTOSHOP = (23,3,0)
MIN_PHOTOSHOP_TEXT = '.'.join(map(str,MIN_PHOTOSHOP))

def version_tuple(value):
    if not isinstance(value,str) or not re.fullmatch(r'\d+(?:\.\d+){1,3}',value):
        raise ValueError('无法识别 Photoshop 版本')
    parts=tuple(int(p) for p in value.split('.'))
    return (parts+(0,0,0))[:3]

def require_version(value):
    if version_tuple(value)<MIN_PHOTOSHOP:
        raise RuntimeError('需要 Photoshop '+MIN_PHOTOSHOP_TEXT+' 或更新版本，检测到 '+value)
    return value

def exe_version(path):
    import win32api
    value=win32api.GetFileVersionInfo(str(path),'\\')
    return '.'.join(str(n) for n in (value['FileVersionMS']>>16,value['FileVersionMS']&65535,value['FileVersionLS']>>16))

def installed_paths(explicit=None):
    if explicit:return [Path(explicit)]
    candidates=[]
    import winreg
    def locations(hive, view, root, field, display_prefix=None):
        try:
            with winreg.OpenKey(hive,root,0,winreg.KEY_READ|view) as key:
                for i in range(winreg.QueryInfoKey(key)[0]):
                    try:
                        with winreg.OpenKey(key,winreg.EnumKey(key,i)) as entry:
                            if display_prefix and not str(winreg.QueryValueEx(entry,'DisplayName')[0]).startswith(display_prefix):
                                continue
                            location=winreg.QueryValueEx(entry,field)[0]
                            if isinstance(location,str) and location.strip(' "'):
                                yield Path(os.path.expandvars(location.strip(' "')))/'Photoshop.exe'
                    except OSError:
                        continue
        except OSError:
            return

    for hive in (winreg.HKEY_LOCAL_MACHINE,winreg.HKEY_CURRENT_USER):
        for view in (winreg.KEY_WOW64_64KEY,winreg.KEY_WOW64_32KEY):
            candidates.extend(locations(hive,view,r'SOFTWARE\Adobe\Photoshop','ApplicationPath'))
            candidates.extend(locations(hive,view,r'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall','InstallLocation','Adobe Photoshop'))
    for name in ('ProgramFiles','ProgramW6432','ProgramFiles(x86)'):
        if os.environ.get(name):
            candidates.extend((Path(os.environ[name])/'Adobe').glob('Adobe Photoshop*/Photoshop.exe'))
    return list(dict.fromkeys(p for p in candidates if p.is_file()))

def check_install_hosts(paths):
    found=[]
    for path in paths:
        try:found.append({'path':str(path),'version':exe_version(path)})
        except OSError:continue
    supported=[item for item in found if version_tuple(item['version'])>=MIN_PHOTOSHOP]
    if not supported:
        versions=', '.join(item['version'] for item in found) or '未检测到安装'
        raise RuntimeError('安装要求 Photoshop '+MIN_PHOTOSHOP_TEXT+' 或更新版本（'+versions+'）；自定义目录可用 --photoshop-exe 指定 Photoshop.exe')
    return supported
