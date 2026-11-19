"""Validated, atomic local settings shared by the service and native menu."""
import copy
import json
import os
import re
import time
from pathlib import Path
from urllib.parse import urlparse

DEFAULT_GRADIENT_TINT = '#58c778'
DEFAULTS = {"enabled": True, "menu_width": 360, "menu_height": 300, "render_mode":"auto", "remote_compatibility": False,
            "gradient_layer_tint": DEFAULT_GRADIENT_TINT, "gradient_direct_overlay": False,
            "right_click_open": False,
            "right_click_hide": False, "open_control_on_start": False, "update_url": "",
            "shortcuts": {"toggle": "capslock", "hide": "esc", "select": "capslock+a",
                          "color": "", "moveToTop": "", "moveToBottom": "",
                          "selectAbove":"capslock+w", "selectBelow":"capslock+s",
                          "extendAbove":"shift+capslock+w", "extendBelow":"shift+capslock+s",
                          "moveUp":"", "moveDown":"",
                          "copyStyle":"capslock+c", "pasteStyle":"capslock+v", "editStyle":"capslock+f",
                          "convertSmartObject":"f5"}}
KEYS = {"tab": 9, "esc": 27, "space": 32, "capslock": 20, "ctrl": 17, "shift": 16, "alt": 18,
        **{f"f{i}": 111+i for i in range(1, 13)},
        **{chr(i).lower(): i for i in range(65, 91)}, **{str(i): 48+i for i in range(10)}}
MODIFIERS = {16, 17, 18, 91, 92}
ALIASES = {160: 16, 161: 16, 162: 17, 163: 17, 164: 18, 165: 18}


def chord(value):
    if not isinstance(value, str):
        raise ValueError("快捷键必须为文本")
    if not value.strip():return ()
    names = value.lower().strip().split("+")
    if not names or any(name not in KEYS for name in names) or len(set(names)) != len(names):
        raise ValueError("快捷键支持 ctrl/alt/shift/capslock/tab 加字母、数字、F1–F12、Space、Esc")
    if any(name not in ("ctrl", "alt", "shift", "capslock", "tab") for name in names[:-1]):
        raise ValueError("组合键前缀仅支持 ctrl、alt、shift、capslock、tab")
    if names[-1] in ("ctrl", "alt", "shift") or (names[-1] == "capslock" and len(names) > 1):
        raise ValueError("快捷键需要一个触发键")
    if len(names) == 1 and names[-1] not in ("capslock", "tab", "esc", "space") and not names[-1].startswith("f"):
        raise ValueError("单键快捷键仅支持 CapsLock、Tab、Esc、Space、F1–F12")
    return tuple(KEYS[name] for name in names)


def validate(data):
    if isinstance(data,dict):data={"render_mode":"auto","remote_compatibility":False,"gradient_layer_tint":DEFAULT_GRADIENT_TINT,
                                  "gradient_direct_overlay":False,**data}
    if not isinstance(data, dict) or set(data) != set(DEFAULTS):
        raise ValueError("设置字段不完整或包含未知字段")
    result = copy.deepcopy(data)
    tint = result['gradient_layer_tint']
    if not isinstance(tint,str) or tint and not re.fullmatch(r'#[0-9a-fA-F]{6}',tint):
        raise ValueError('渐变图层高亮颜色必须为六位十六进制 RGB')
    result['gradient_layer_tint'] = tint.lower()
    if result['render_mode'] not in ('auto','gpu','cpu'):raise ValueError('渲染模式必须为自动、GPU或CPU')
    for key in ("enabled", "right_click_open", "right_click_hide", "open_control_on_start", "remote_compatibility", "gradient_direct_overlay"):
        if type(result[key]) is not bool:
            raise ValueError("开关必须为布尔值")
    # Retain the field for existing settings clients, but never take over PS's menu.
    result['right_click_open'] = False
    for key, low, high in (("menu_width", 300, 1200), ("menu_height", 240, 1000)):
        if type(result[key]) is not int or not low <= result[key] <= high:
            raise ValueError(f"{key} 必须为 {low}–{high} 的整数")
    if not isinstance(result["update_url"], str):
        raise ValueError("更新地址必须为文本")
    url = result["update_url"].strip()
    if url:
        parsed = urlparse(url)
        if parsed.scheme != "https" or not parsed.hostname or parsed.username or parsed.password:
            raise ValueError("更新清单必须使用不含账号密码的 HTTPS 地址")
    result["update_url"] = url
    shortcuts = result["shortcuts"]
    if not isinstance(shortcuts, dict) or set(shortcuts) != set(DEFAULTS["shortcuts"]):
        raise ValueError("快捷键设置不完整")
    # User removed step movement shortcuts; these remain menu-only commands.
    shortcuts['moveUp'] = shortcuts['moveDown'] = ''
    parsed = []
    for name, value in shortcuts.items():
        keys = chord(value)
        if not keys:
            if name in ("toggle","hide"):raise ValueError("唤出与收起快捷键不能为空")
            continue
        normalized = tuple(sorted(keys[:-1])) + keys[-1:]
        if normalized in parsed:
            raise ValueError("快捷键不能重复")
        parsed.append(normalized)
        shortcuts[name] = value.lower().strip()
    return result


def publish_json(temporary,path):
    deadline=time.monotonic()+.25
    while True:
        try:
            os.replace(temporary, path)
            return
        except PermissionError as error:
            if getattr(error,'winerror',None) not in (5,32) or time.monotonic()>=deadline:raise
            time.sleep(.004)


def atomic_json(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
    publish_json(temporary,path)


class Settings:
    def __init__(self, path):
        self.path = Path(path)
        if self.path.exists():
            data = json.loads(self.path.read_text(encoding="utf-8"))
            # Migrate the additive invocation setting without dropping unknown fields.
            if isinstance(data, dict):
                if "right_click_open" not in data and data.get("menu_width")==420 and data.get("menu_height")==360:
                    data["menu_width"], data["menu_height"] = 360, 300
                data.setdefault("right_click_open", DEFAULTS["right_click_open"])
                shortcuts = data.get('shortcuts', {})
                legacy = {'select':'capslock+a','color':'tab+a','moveToTop':'tab+w','moveToBottom':'tab+s'}
                if isinstance(shortcuts, dict) and all(shortcuts.get(k)==v for k,v in legacy.items()):
                    for key in ('color','moveToTop','moveToBottom'):
                        shortcuts[key] = DEFAULTS['shortcuts'][key]
            shortcuts=data.get('shortcuts',{})
            if isinstance(shortcuts,dict):
                if 'convertSmartObject' not in shortcuts:
                    # F5 now belongs to conversion. Later explicit edits keep
                    # their saved assignments, including disabling this key.
                    for key,value in list(shortcuts.items()):
                        if isinstance(value,str) and value.strip().lower()=='f5':shortcuts[key]=''
                # Only replace stock legacy collisions; retain custom chords.
                for key,old in {'color':'capslock+c','moveToTop':'capslock+w','moveToBottom':'capslock+s'}.items():
                    if shortcuts.get(key)==old:shortcuts[key]=''
                for key,value in DEFAULTS['shortcuts'].items():
                    if key not in shortcuts:
                        # Existing custom assignments win over new stock keys.
                        shortcuts[key]=value if not value or value not in shortcuts.values() else ''
            self.data = validate(data)
        else:
            self.data = copy.deepcopy(DEFAULTS)

    def save(self, data):
        data = validate(data)
        atomic_json(self.path, data)
        self.data = data
        return copy.deepcopy(data)

    def get(self):
        return copy.deepcopy(self.data)
