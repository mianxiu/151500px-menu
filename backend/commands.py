"""Versioned command contracts shared by the API and COM execution path."""
from dataclasses import asdict, dataclass
from types import MappingProxyType

from .layer_labels import LABEL_ACTIONS


ACTIONS = {
    "select": {"label": "选择组内图层", "shortcut": "CapsLock+A"},
    "color": {"label": "橙色图层标签", "shortcut": "CapsLock+C"},
    "moveToTop": {"label": "移到顶部", "shortcut": "CapsLock+W"},
    "moveToBottom": {"label": "移到底部", "shortcut": "CapsLock+S"},
}
QUICK_ACTIONS = {
    'layer.new': {'label':'新建空白图层'},
    'layer.solid': {'label':'新建纯色填充图层'},
    'layer.group': {'label':'选中图层打组'},
    'adjustment.curves': {'label':'新建曲线调整图层'},
    'adjustment.hueSaturation': {'label':'新建色相饱和度调整图层'},
    'adjustment.colorBalance': {'label':'新建色彩平衡调整图层'},
    'adjustment.blackWhite': {'label':'新建黑白调整图层'},
}
CAPS_ACTIONS = {
    "moveDocumentTop":{"label":"移到文档顶部"}, "moveDocumentBottom":{"label":"移到文档底部"},
    "selectAbove":{"label":"单选上一个图层"}, "selectBelow":{"label":"单选下一个图层"},
    "extendAbove":{"label":"向上扩展图层多选"}, "extendBelow":{"label":"向下扩展图层多选"},
    "moveUp":{"label":"上移一层"}, "moveDown":{"label":"下移一层"},
    "copyStyle":{"label":"复制图层样式"}, "pasteStyle":{"label":"粘贴图层样式"},
    "editStyle":{"label":"打开图层样式"},
    "convertSmartObject":{"label":"转换为智能对象"},
    "convertSmartObjectsIndividually":{"label":"逐层转智能对象"},
}
COMMANDS = {**CAPS_ACTIONS,**ACTIONS, **LABEL_ACTIONS, **QUICK_ACTIONS,'shape.paint':{'label':'调整形状填充与描边'},
    'layer.focus':{'label':'定位图层'},
    'layer.frame':{'label':'聚焦所选图层'},
    'text.replace':{'label':'文本查找替换'},
    'text.split':{'label':'拆分文字图层'},
    'text.object':{'label':'写回矢量文字对象'},
    'shape.copyPaint':{'label':'复制形状填充与描边'},'shape.pastePaint':{'label':'粘贴形状填充与描边'}}


@dataclass(frozen=True)
class Command:
    id: str
    label: str
    expected_result: str
    version: int = 2
    channel: str = "com-jsx"
    conditions: tuple = ("photoshop-connected", "document-open", "action-focus", "not-busy")
    target_document: str = "captured-id; same-COM-instance; cancel-if-active-document-changed"
    target_layers: str = "captured-selection-ids-and-active-layer; cancel-if-changed"
    cancellation: str = "before-dispatch-only; running COM cannot be interrupted"
    recovery: str = "no-auto-retry; inspect Photoshop after timeout or failure"
    parameters: tuple = ()


INTERNAL_COMMANDS = MappingProxyType({
    'layer.thumbnail': Command('layer.thumbnail','生成图层缩略图','temporary-document-closed; original-selection-preserved',channel='uxp-local-files'),
    'stability.internal': Command('stability.internal','隔离稳定性测试','owned-document-only; verified-readback',channel='uxp-local-files'),
})
REGISTRY = MappingProxyType({
    **{
    name: Command(name, info["label"],
                  "selected-layer-colors-read-back" if name in LABEL_ACTIONS else "script-returned; document-effect-not-verified")
    for name, info in COMMANDS.items() if name not in QUICK_ACTIONS and name not in CAPS_ACTIONS
    },
    **{name:Command(name,info['label'],'selection-style-or-order-read-back' if name in CAPS_ACTIONS else 'created-layer-kind-and-group-members-read-back',
        channel='uxp-local-files') for name,info in {**QUICK_ACTIONS,**CAPS_ACTIONS}.items()},
    'document.recent': Command('document.recent','按最近预设新建文档','new-document-dimensions-returned',
        channel='uxp-local-files', conditions=('uxp-ready','photoshop-home','action-focus','not-busy'),
        target_document='captured-UXP-session-and-document-set; cancel-if-changed', target_layers='not-applicable'),
    'shape.paint':Command('shape.paint','调整形状填充与描边','paint-and-vector-identity-read-back',
        channel='uxp-local-files',parameters=('revision','fill','stroke')),
    'layer.frame':Command('layer.frame','聚焦所选图层','view-and-selection-read-back; document-content-unchanged',channel='uxp-local-files'),
    'layer.focus':Command('layer.focus','定位图层','destination-and-selection-read-back',channel='uxp-local-files'),
    'text.replace':Command('text.replace','文本查找替换','text-style-and-layout-read-back',channel='uxp-local-files'),
    'text.split':Command('text.split','拆分文字图层','text-style-position-and-order-read-back',channel='uxp-local-files'),
    'text.object':Command('text.object','写回矢量文字对象','vector-pdf-source-metadata-and-selection-read-back',channel='uxp-local-files'),
    **{name:Command(name,COMMANDS[name]['label'],'native-paint-path-mask-order-read-back',channel='uxp-local-files')
       for name in ('shape.copyPaint','shape.pastePaint')},
})


def validate_command(name, params=None):
    if not isinstance(name, str) or name not in REGISTRY and name not in INTERNAL_COMMANDS:
        raise ValueError("未知的图层操作")
    if name=='shape.paint' and params is not None:
        from .shape_paint import parameters
        parameters(params)
    elif params is not None and (not isinstance(params, dict) or params):
        raise ValueError("此命令不接受参数")
    return REGISTRY.get(name) or INTERNAL_COMMANDS[name]


def command_contracts():
    return [asdict(command) for command in REGISTRY.values()]
