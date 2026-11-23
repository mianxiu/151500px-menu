"""Persist view preferences without serializing any document or draft."""

from .settings import atomic_json
import json


DEFAULTS={'background':0,'rgb':[1.0,1.0,1.0],'zoom':1.0,'scope':0}


def validate(value):
    if (not isinstance(value,dict) or set(value)!=set(DEFAULTS) or
            type(value['background']) is not int or not 0<=value['background']<=2 or
            type(value['scope']) is not int or not 0<=value['scope']<=3 or
            type(value['zoom']) not in (int,float) or not .25<=value['zoom']<=8 or
            not isinstance(value['rgb'],list) or len(value['rgb'])!=3 or
            any(type(channel) not in (int,float) or not 0<=channel<=1 for channel in value['rgb'])):
        raise ValueError('排版视图设置无效')
    return {**value,'rgb':list(value['rgb'])}


def load(path):
    try:return validate(json.loads(path.read_text('utf-8')))
    except (OSError,ValueError):return validate(DEFAULTS)


def save(path,value):
    preferences=validate(DEFAULTS if value is None else value)
    atomic_json(path,preferences)
    return preferences


def validate_style(entry):
    from .text_tools import CHARACTER_STYLES,PARAGRAPH_STYLES,typography
    if not isinstance(entry,dict) or set(entry)!={'kind','values'} or entry['kind'] not in ('character','paragraph'):
        raise ValueError('排版预设格式无效')
    values=typography(entry['values'])
    fields=CHARACTER_STYLES if entry['kind']=='character' else PARAGRAPH_STYLES
    if set(values)-set(fields):raise ValueError('排版预设包含其他范围的属性')
    return {'kind':entry['kind'],'values':values}


def load_styles(path):
    try:
        raw=json.loads(path.read_text('utf-8'))
        if not isinstance(raw,dict) or len(raw)>64:raise ValueError('排版预设超出上限')
        return {name:validate_style(entry) for name,entry in raw.items()
                if 1<=len(name)<=48 and not any(ord(c)<32 for c in name)}
    except (OSError,ValueError):return {}


def save_style(path,name,entry):
    if not isinstance(name,str) or not 1<=len(name.strip())<=48 or any(ord(c)<32 for c in name):
        raise ValueError('请输入 1 至 48 个字符的样式名称')
    styles=load_styles(path)
    name=name.strip()
    if name not in styles and len(styles)>=64:raise ValueError('排版预设最多可保存 64 个')
    styles[name]=validate_style(entry)
    atomic_json(path,styles)
    return styles
