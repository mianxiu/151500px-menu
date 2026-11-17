"""Select an existing Photoshop COM class for the current Windows user."""
import json
from pathlib import Path
import re
from uuid import UUID

from .photoshop_launch import registered_executable
from .photoshop_version import exe_version, version_tuple
from .settings import atomic_json
from .uxp_management import photoshop_process_ids


ALIAS = 'Photoshop.Application'
USER_ALIAS = r'Software\Classes\Photoshop.Application'
VERSIONED = re.compile(r'Photoshop\.Application\.\d+', re.I)
FIELDS = ('CLSID', 'CurVer')


def registry_value(hive, path):
    import winreg
    try:
        key = winreg.OpenKey(hive, path)
    except FileNotFoundError:
        return False, None
    with key:
        try:
            value, kind = winreg.QueryValueEx(key, None)
        except FileNotFoundError:
            return True, None
    if kind != winreg.REG_SZ:
        raise RuntimeError('Photoshop COM 注册格式无法识别')
    return True, value


def versions():
    import winreg
    with winreg.OpenKey(winreg.HKEY_CLASSES_ROOT, '') as key:
        names = [winreg.EnumKey(key, i) for i in range(winreg.QueryInfoKey(key)[0])]
    items = []
    for name in names:
        if not VERSIONED.fullmatch(name):
            continue
        try:
            clsid = UUID(registry_value(winreg.HKEY_CLASSES_ROOT, name+r'\CLSID')[1])
            path = registered_executable(name)
            items.append({'id':name, 'clsid':'{'+str(clsid)+'}', 'name':path.parent.name,
                          'path':str(path), 'version':exe_version(path)})
        except (OSError, ValueError, TypeError, RuntimeError):
            continue
    return sorted(items, key=lambda item:version_tuple(item['version']), reverse=True)


def active_version(prog_id):
    import pythoncom
    import win32com.client
    pythoncom.CoInitialize()
    try:
        app = win32com.client.GetActiveObject(prog_id)
        try:
            return str(app.Version)
        finally:
            del app
    finally:
        pythoncom.CoUninitialize()


def user_state():
    import winreg
    with_root = registry_value(winreg.HKEY_CURRENT_USER, USER_ALIAS)[0]
    return {'root':with_root, **{
        field:list(registry_value(winreg.HKEY_CURRENT_USER, USER_ALIAS+'\\'+field)) for field in FIELDS}}


def write_user_state(state):
    import winreg
    root = winreg.HKEY_CURRENT_USER

    def remove_if_empty(path):
        try:
            with winreg.OpenKey(root, path) as key:
                subkeys, values, _ = winreg.QueryInfoKey(key)
            if not subkeys and not values:
                winreg.DeleteKey(root, path)
        except FileNotFoundError:
            pass

    for field in FIELDS:
        path = USER_ALIAS+'\\'+field
        exists, value = state[field]
        if exists:
            with winreg.CreateKey(root, path) as key:
                if value is not None:
                    winreg.SetValueEx(key, '', 0, winreg.REG_SZ, value)
                else:
                    try:winreg.DeleteValue(key, '')
                    except FileNotFoundError:pass
        else:
            try:
                with winreg.OpenKey(root, path, 0, winreg.KEY_SET_VALUE) as key:
                    try:winreg.DeleteValue(key, '')
                    except FileNotFoundError:pass
            except FileNotFoundError:
                pass
            remove_if_empty(path)
    if state['root']:
        with winreg.CreateKey(root, USER_ALIAS):
            pass
    else:
        remove_if_empty(USER_ALIAS)
    if user_state() != state:
        raise RuntimeError('未能核对 Windows COM 用户设置')


class PhotoshopComDefault:
    def __init__(self, runtime):
        self.backup = Path(runtime)/'photoshop-com-default.json'

    def status(self):
        import winreg
        clsid = registry_value(winreg.HKEY_CLASSES_ROOT, ALIAS+r'\CLSID')[1] or ''
        preferred = registry_value(winreg.HKEY_CLASSES_ROOT, ALIAS+r'\CurVer')[1]
        items = versions()
        matches = [item for item in items if item['clsid'].lower() == clsid.lower()]
        current = next((item for item in matches if item['id'] == preferred), matches[0] if matches else None)
        return {'versions':items, 'default':current, 'can_restore':self.backup.is_file()}

    def saved(self):
        if not self.backup.is_file():
            return None
        record = json.loads(self.backup.read_text('utf-8'))
        original = record.get('original') if isinstance(record,dict) else None
        if (not isinstance(original,dict) or type(original.get('root')) is not bool or
                any(not isinstance(original.get(key),list) or len(original[key])!=2 or
                    type(original[key][0]) is not bool or
                    original[key][1] is not None and
                    (not original[key][0] or not isinstance(original[key][1],str))
                    for key in FIELDS)):
            raise RuntimeError('Photoshop COM 设置备份无法读取')
        return record

    def switch(self, choice):
        import winreg
        if not isinstance(choice,str) or not VERSIONED.fullmatch(choice):
            raise ValueError('请选择已登记的 Photoshop 版本')
        item = next((item for item in versions() if item['id'] == choice), None)
        if item is None:
            raise RuntimeError('所选 Photoshop 版本已不可用')
        pids = photoshop_process_ids()
        if len(pids) != 1:
            raise RuntimeError('请只打开要设为默认的 Photoshop 版本')
        try:
            running = active_version(choice)
        except Exception as error:
            raise RuntimeError('请先打开所选 Photoshop 版本，再设为默认') from error
        if version_tuple(running)[:2] != version_tuple(item['version'])[:2]:
            raise RuntimeError('运行中的 Photoshop 与所选版本不符')
        if photoshop_process_ids() != pids:
            raise RuntimeError('Photoshop 进程已改变，请刷新后重试')
        original = self.saved()
        if original and original.get('pending'):
            raise RuntimeError('上次 COM 切换待恢复，请先恢复原设置')
        before = user_state()
        record = original or {'original':before}
        atomic_json(self.backup, {**record,'pending':True})
        result = {'root':True, 'CLSID':[True,item['clsid']], 'CurVer':[True,choice]}
        try:
            write_user_state(result)
            effective = registry_value(winreg.HKEY_CLASSES_ROOT, ALIAS+r'\CLSID')[1]
            if (not effective or effective.lower()!=item['clsid'].lower() or
                    active_version(ALIAS)!=running or photoshop_process_ids()!=pids):
                raise RuntimeError('未能在默认入口连到所选 Photoshop 版本')
            response = self.status()
            atomic_json(self.backup, {**record,'pending':False})
            return {**response,'message':'Photoshop COM 默认版本已切换'}
        except Exception as error:
            write_user_state(before)
            if original is None:self.backup.unlink()
            else:atomic_json(self.backup, original)
            raise RuntimeError('COM 默认版本切换失败，已恢复切换前的设置') from error

    def restore(self):
        record = self.saved()
        if record is None:
            return {**self.status(),'message':'已使用原 Photoshop COM 设置'}
        write_user_state(record['original'])
        self.backup.unlink()
        return {**self.status(),'message':'已恢复原 Photoshop COM 设置'}
