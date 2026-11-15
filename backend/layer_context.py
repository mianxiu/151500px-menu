"""Small immutable UXP selection cache; menu opening never performs IPC."""
import time
from .tasks import TaskCancelled
from .commands import QUICK_ACTIONS, CAPS_ACTIONS, SHAPE_TOOLS

KINDS={'unknown':0,'none':0,'background':1,'pixel':2,'group':3,'text':4,'smart':5,
       'shape-fill':6,'adjustment':7,'mixed':8,'other':9}
LABELS={'unknown':'图层识别待更新','none':'未选择图层','background':'背景图层','pixel':'像素图层',
        'group':'图层组','text':'文字图层','smart':'智能对象','shape-fill':'形状／填充图层',
        'adjustment':'调整图层','mixed':'混合类型','other':'其他图层'}

def unavailable():
    return {'kind':'unknown','mask':0,'label':LABELS['unknown'],'layer_ids':[], 'plugin_time_ms':0}

def fresh(value,now_ms=None):
    age=(time.time()*1000 if now_ms is None else now_ms)-value.get('plugin_time_ms',0)
    return 0<=age<=2000

def validate(value,session,now_ms=None):
    if not isinstance(value,dict) or value.get('schema')!=1 or value.get('session')!=session:
        return unavailable()
    ids=value.get('layer_ids');kind=value.get('kind');mask=value.get('mask')
    if kind not in KINDS or type(mask) is not int or not 0<=mask<=15 or not isinstance(ids,list) or len(ids)>1024:
        return unavailable()
    if type(value.get('plugin_time_ms')) not in (int,float):return unavailable()
    if any(type(i) is not int or i<=0 for i in ids) or ids!=sorted(set(ids)):
        return unavailable()
    if not fresh(value,now_ms) or ids and type(value.get('document_id')) is not int:
        return unavailable()
    if kind in ('none','unknown') and mask or not ids and mask or value.get('background') and mask&14:
        return unavailable()
    quick=value.get('quick_mask',0)
    if type(quick) is not int or not 0<=quick<=127 or quick and not ids:
        return unavailable()
    result={**value,'label':LABELS[kind]}
    if 'shape_editor' in value:
        from .shape_paint import metadata,editor_matches
        shape=metadata(value['shape_editor'])
        result['shape_editor']=shape if editor_matches(shape,value) and not value.get('locked') and not value.get('background') else {'available':False}
    return result

def flags(value):
    """Packed into the existing single prepare message, with no new round trip."""
    value=value if fresh(value) else unavailable()
    return (64 | (value['mask']<<8) | (KINDS[value['kind']]<<12) |
            (int(bool(value.get('background')))<<16) |
            (min(255,len(value.get('layer_ids',[])))<<17) |
            (int(bool(value.get('locked')))<<25) |
            (int(value.get('same_parent') is False)<<26) |
            (value.get('quick_mask',0)<<32))

def require_menu_target(value,name,target):
    if name in ('shape.paint','shape.copyPaint','shape.pastePaint'):
        from .shape_paint import editor_matches
        if (not value or not editor_matches(value.get('shape_editor',{}),value) or value.get('locked')
                or value.get('background') or not value.get('shape_editor',{}).get('available')):
            raise TaskCancelled('颜色／蒙版调整目标不可用，请重新打开菜单')
        if name=='shape.copyPaint' and (len(value.get('layer_ids',[]))!=1 or not value.get('shape_editor',{}).get('vector')):
            raise TaskCancelled('请单选形状图层复制填充与描边')
        if name=='shape.pastePaint' and not value.get('shape_editor',{}).get('vector'):
            raise TaskCancelled('仅支持向形状图层粘贴填充与描边')
        if any(value.get(k)!=target.get(k) for k in ('session','document_id','layer_ids')):raise TaskCancelled('文档或图层选择已改变，请重新打开菜单；本次未修改图层')
        return
    if name in CAPS_ACTIONS or name in SHAPE_TOOLS:
        if name=='convertSmartObjectsIndividually' and (not value or value.get('locked') or value.get('background')):
            raise TaskCancelled('请先解锁要逐层转换的图层')
        if name in ('moveDocumentTop','moveDocumentBottom') and (not value or value.get('locked') or value.get('background') or value.get('same_parent') is False):raise TaskCancelled('请先解锁并选择同一父组内的图层')
        if not value or not value.get('layer_ids') or value.get('kind') in ('unknown','none'):
            raise TaskCancelled('图层状态待确认，请重新打开菜单')
        if name in ('copyStyle','editStyle') and len(value['layer_ids'])!=1:
            raise TaskCancelled('此操作需要单选图层')
        if any(value.get(k)!=target.get(k) for k in ('session','document_id','layer_ids')):
            raise TaskCancelled('菜单目标已改变；本次未修改')
        return
    quick=name in QUICK_ACTIONS
    bit=(1<<list(QUICK_ACTIONS).index(name)) if quick else (2 if name=='color' or name.startswith('label.') else {'select':1,'moveToTop':4,'moveToBottom':8}.get(name,0))
    if not value or not value.get('quick_mask' if quick else 'mask',0)&bit:
        raise TaskCancelled('当前菜单的图层操作不可用，请重新打开菜单')
    if any(value.get(k)!=target.get(k) for k in ('session','document_id','layer_ids')):
        raise TaskCancelled('文档或图层选择已改变，请重新打开菜单；本次未修改图层')
