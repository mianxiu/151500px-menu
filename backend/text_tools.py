"""One selected text layer, read-only preview and explicit single-history commit."""
import asyncio
import math
import re
import secrets
from bisect import bisect_left
from .layer_search import require_available
from .operation_target import validate_target
from .text_plan import compute,evaluate,normalize,parameters,units
from . import text_canvas,text_list,text_object

INDENTS=('startIndent','endIndent','firstLineIndent')
PARAGRAPH_DISTANCES=('spaceBefore','spaceAfter',*INDENTS)
OPENTYPE_SWITCHES=('contextualLigatures','stylisticAlternates','swash','titling','ornaments','fractions','ordinals')
CHARACTER_SWITCHES=('syntheticBold','syntheticItalic','ligature','noBreak',*OPENTYPE_SWITCHES)
PARAGRAPH_SWITCHES=('hyphenate','hyphenateCapitalized','hangingRoman','textEveryLineComposer')
STYLE_SWITCHES=(*CHARACTER_SWITCHES,*PARAGRAPH_SWITCHES)
HYPHENATION={'hyphenateWordSize':(1,99),'hyphenatePreLength':(0,99),'hyphenatePostLength':(0,99),
             'hyphenateLimit':(0,99),'hyphenationZone':(0,1296)}
JUSTIFICATION={'justifyWord':('Word',0,1000),'justifyLetter':('Letter',-100,500),'justifyGlyph':('Glyph',50,200)}
EAST_ASIAN_PARAGRAPH_CHOICES={
    'kinsokuSetName':{'None':'关闭','Soft':'JIS 弱','Hard':'JIS 最大'},
    'mojiKumiName':{'Photoshop6MojiKumiNone':'关闭',**{f'Photoshop6MojiKumiSet{i}':f'组合 {i}' for i in range(1,5)}},
    'burasagari':{'burasagariNone':'关闭','burasagariStandard':'常规','burasagariStrong':'强制'},
    'preferredKinsokuOrder':{'pushIn':'优先挤入','pushOutFirst':'优先推出','pushOut':'仅推出'},
}
STYLE_CHOICES={
    'autoKern':{'metricsKern':'度量','opticalKern':'视觉'},
    'fontCaps':{'normal':'正常','allCaps':'大写','smallCaps':'小型大写'},
    'baseline':{'normal':'正常','superScript':'上标','subScript':'下标'},
    'underline':{'underlineOff':'关闭','underlineOnLeftInVertical':'开启','underlineOnRightInVertical':'开启'},
    'strikethrough':{'strikethroughOff':'关闭','xHeightStrikethroughOn':'开启'},
    'figureStyle':{'normal':'默认','proportionalLining':'比例等高','tabularLining':'等宽等高',
                   'proportionalOldStyle':'比例旧式','tabularOldStyle':'等宽旧式'},
}
OPENTYPE_STYLES={*OPENTYPE_SWITCHES,'figureStyle'}
ADVANCED_STYLES={*STYLE_SWITCHES,*STYLE_CHOICES,'color'}
CHARACTER_STYLES=('fontPostScriptName','size','leading','tracking','baselineShift','horizontalScale','verticalScale',
                  *STYLE_CHOICES,*CHARACTER_SWITCHES,'color')
PARAGRAPH_STYLES=(*PARAGRAPH_DISTANCES,'align',*PARAGRAPH_SWITCHES,*HYPHENATION,*JUSTIFICATION,*EAST_ASIAN_PARAGRAPH_CHOICES)
EXTENDED_STYLES={'baselineShift','horizontalScale','verticalScale','align'}
ALIGNMENT={'left':'左对齐','center':'居中','right':'右对齐','justifyLeft':'两端对齐，末行左',
           'justifyCenter':'两端对齐，末行居中','justifyRight':'两端对齐，末行右','justifyAll':'全部两端对齐'}

def typography(value):
    if not isinstance(value,dict) or not set(value)-{'range'} or set(value)-set((*CHARACTER_STYLES,*PARAGRAPH_STYLES,'range')):
        raise ValueError('排版参数无效')
    result={}
    for key,item in value.items():
        if key in EAST_ASIAN_PARAGRAPH_CHOICES:
            if not isinstance(item,str) or item not in EAST_ASIAN_PARAGRAPH_CHOICES[key]:
                raise ValueError('东亚段落排版选项无效')
            result[key]=item
            continue
        if key=='align':
            if not isinstance(item,str) or item not in ALIGNMENT:raise ValueError('段落对齐无效')
            result[key]=item
            continue
        if key in STYLE_CHOICES:
            if not isinstance(item,str) or item not in STYLE_CHOICES[key]:raise ValueError('字符样式无效')
            result[key]=item
            continue
        if key in STYLE_SWITCHES:
            if type(item) is not bool:raise ValueError('排版开关无效')
            result[key]=item
            continue
        if key in JUSTIFICATION:
            _,low,high=JUSTIFICATION[key]
            if (not isinstance(item,list) or len(item)!=3 or
                    any(type(v) not in (int,float) or not low<=v<=high for v in item) or
                    item!=sorted(item)):
                raise ValueError('两端对齐参数需要按最小、理想、最大排列')
            result[key]=item[:]
            continue
        if key=='color':
            if not isinstance(item,list) or len(item)!=3 or any(type(channel) is not int or not 0<=channel<=255 for channel in item):
                raise ValueError('无效的 RGB 文字颜色')
            result[key]=item[:]
            continue
        if key=='leading' and item=='auto':
            result[key]=item
            continue
        if key=='range':
            if not isinstance(item,list) or len(item)!=2 or any(type(i) is not int for i in item) or not 0<=item[0]<item[1]<=20001:
                raise ValueError('排版选区无效')
            result[key]=list(item)
            continue
        if key=='fontPostScriptName':
            if not isinstance(item,str) or not 1<=len(item)<=200 or any(ord(c)<32 for c in item):
                raise ValueError('字体 PostScript 名称无效')
            result[key]=item
            continue
        if type(item) not in (str,int,float):
            raise ValueError('排版数值无效')
        try:number=float(item)
        except (OverflowError,ValueError):raise ValueError('排版数值无效') from None
        if not math.isfinite(number):raise ValueError('排版数值无效')
        if key in HYPHENATION:
            low,high=HYPHENATION[key]
            if not low<=number<=high or key!='hyphenationZone' and not number.is_integer():
                raise ValueError('断字数值超出范围')
            result[key]=number if key=='hyphenationZone' else int(number)
        elif key=='tracking':
            if not number.is_integer() or abs(number)>1000:raise ValueError('字距范围为 −1000 至 1000')
            result[key]=int(number)
        elif key in (*INDENTS,'baselineShift'):
            if abs(number)>1296 or key in ('startIndent','endIndent') and number<0:raise ValueError('缩进或基线偏移超出范围')
            result[key]=number
        elif key in ('horizontalScale','verticalScale'):
            if not 1<=number<=1000:raise ValueError('字符缩放范围为 1% 至 1000%')
            result[key]=number
        else:
            if not 0<=number<=1296 or key in ('size','leading') and number==0:raise ValueError('字号、行距或段落间距超出范围')
            result[key]=number
    return result


def typography_runs(value):
    if not isinstance(value,list) or not 1<=len(value)<=256:
        raise ValueError('排版草稿需要 1 至 256 组样式')
    return [typography(run) for run in value]


def style_ranges(raw,resolution):
    result={}
    for field,keys in [('textStyle',CHARACTER_STYLES),('paragraphStyle',PARAGRAPH_STYLES)]:
        runs=raw[field+'Range']
        profiles=[]
        root=runs[0][field].get('baseParentStyle') or {}
        for run in runs:
            chain=[run[field]]
            while isinstance(chain[-1].get('baseParentStyle'),dict):chain.append(chain[-1]['baseParentStyle'])
            chain.append(root)
            values={}
            for key in keys:
                item=next((style[key] for style in chain if key in style),None)
                if key in JUSTIFICATION:
                    prefix='justification'+JUSTIFICATION[key][0]
                    triple=[next((style[prefix+part] for style in chain if prefix+part in style),None)
                            for part in ('Minimum','Desired','Maximum')]
                    if all(type(v) in (int,float) for v in triple):item=', '.join(format(v*100,'.6g') for v in triple)
                if key=='leading' and next((style['autoLeading'] for style in chain if 'autoLeading' in style),False):item='自动'
                if key=='align' and isinstance(item,dict):
                    item=ALIGNMENT.get(item.get('_value'))
                if key in STYLE_CHOICES and isinstance(item,dict):
                    item=STYLE_CHOICES[key].get(item.get('_value'))
                if key in EAST_ASIAN_PARAGRAPH_CHOICES:
                    if isinstance(item,dict):
                        item=EAST_ASIAN_PARAGRAPH_CHOICES[key].get(item.get('_value')) if item.get('_enum')==key else None
                    elif key=='mojiKumiName' and item is None and isinstance(run[field].get('kinsokuSetName'),dict):
                        item='关闭'
                if key=='textEveryLineComposer' and type(item) is bool:item='整段' if item else '逐行'
                elif key in STYLE_SWITCHES and type(item) is bool:item='开启' if item else '关闭'
                if key=='color' and isinstance(item,dict) and item.get('_obj')=='RGBColor':
                    channels=(item.get('red'),item.get('grain',item.get('green')),item.get('blue'))
                    if all(type(v) in (int,float) and 0<=v<=255 for v in channels):
                        item='#'+''.join(f'{round(v):02X}' for v in channels)
                if key in ('horizontalScale','verticalScale') and type(item) in (int,float):
                    item=format(item,'.7g')
                if isinstance(item,dict):
                    unit=item.get('_unit');number=item.get('_value')
                    if type(number) in (int,float) and unit in ('pixelsUnit','pointsUnit'):
                        item=format(number*72/resolution if unit=='pixelsUnit' else number,'.7g')
                    else:item=None
                values[key]=str(item) if item is not None else '继承'
            profiles.append({'from':run['from'],'to':run['to'],'values':values})
        result[field]=profiles
    return result


def summarize_ranges(ranges):
    summary={}
    for profiles in ranges.values():
        for key,first in profiles[0]['values'].items():
            summary[key]=first if all(row['values'][key]==first for row in profiles[1:]) else '混合'
    return summary


def style_summary(raw,resolution):
    return summarize_ranges(style_ranges(raw,resolution))


def validate_style_scope(style,saved,args):
    if style.keys() & OPENTYPE_STYLES and not saved.get('opentype_supported'):
        raise ValueError('请更新插件后重新读取 OpenType 设置')
    if (style.keys() & EXTENDED_STYLES or style.get('leading')=='auto') and not saved.get('extended_typography'):
        raise ValueError('请更新插件后重新读取扩展排版选项')
    if (style.keys() & ADVANCED_STYLES or style.get('align','') not in ('','left','center','right')) and not saved.get('advanced_typography'):
        raise ValueError('请更新插件后重新读取字符和段落选项')
    if 'color' in style and not saved.get('text_rgb'):
        raise ValueError('文字取色需要 sRGB 8 位文档')
    if style.keys() & EAST_ASIAN_PARAGRAPH_CHOICES.keys():
        if not saved.get('east_asian_paragraph_supported'):
            raise ValueError('当前 Photoshop 和插件无法写入东亚段落排版')
        if not saved.get('paragraph_indents_supported'):
            raise ValueError('东亚段落排版需要水平段落文字')
    composition=set(HYPHENATION)|set(JUSTIFICATION)|set(PARAGRAPH_SWITCHES)|set(EAST_ASIAN_PARAGRAPH_CHOICES)
    if style.keys() & ({'align','startIndent','firstLineIndent'}|composition) and (
            args.get('operation')=='list' or args.get('list_options') is not None or
            text_list.read(saved.get('list_xmp','')) is not None):
        raise ValueError('列表使用自身的段落对齐、缩进与断字设置')
    if 'align' in style and not saved.get('paragraph_alignment_supported'):
        raise ValueError('段落对齐需要普通水平文字')
    if (style.get('align','').startswith('justify') or style.keys() & composition) and not saved.get('paragraph_indents_supported'):
        raise ValueError('两端对齐和断字需要水平段落文字')
    if style.keys() & (composition-{'hyphenate'}) and not saved.get('typography_runs_supported'):
        raise ValueError('高级段落排版需要更新执行插件并重新读取文本')
    if style.keys() & set(INDENTS) and not saved.get('paragraph_indents_supported'):
        raise ValueError('请重新读取支持段落缩进的普通文字层')
    if 'range' in style and not saved.get('style_scope_supported'):
        raise ValueError('局部排版需要更新执行插件并重新读取文本')


def map_typography(styles,draft,origins):
    encoded=draft.encode('utf-16-le')
    length=len(encoded)//2
    result=[]
    for style in styles:
        if 'range' not in style:
            result.append(style)
            continue
        start,end=style['range']
        if end>length+1 or any(at<length and encoded[at*2+1]&0xfc==0xdc for at in (start,end)):
            raise ValueError('排版选区已改变，请重新选择')
        if origins is not None:
            start,end=bisect_left(origins,start),bisect_left(origins,end)
        if start<end:result.append({**style,'range':[start,end]})
    return result


def font_in_run(style,fallback):
    while isinstance(style,dict):
        if isinstance(style.get('fontPostScriptName'),str):return style['fontPostScriptName']
        style=style.get('baseParentStyle')
    return fallback.get('fontPostScriptName','') if isinstance(fallback,dict) else ''


def mapped_parameters(value,split=False):
    keys={'revision','value','origins','count'}|({'breaks','layout'} if split else set())
    if not split and isinstance(value,dict) and 'box_size' in value:
        keys.add('box_size');box_dimensions(value['box_size'])
        if 'box_shift_x' in value:
            keys.add('box_shift_x');box_horizontal_shift(value['box_shift_x'])
    if not split and isinstance(value,dict) and 'typography' in value:
        keys.add('typography');value={**value,'typography':typography(value['typography'])}
    if not split and isinstance(value,dict) and 'typography_runs' in value and 'typography' not in value:
        keys.add('typography_runs');value={**value,'typography_runs':typography_runs(value['typography_runs'])}
    if not split and isinstance(value,dict) and 'list_options' in value:
        keys.add('list_options');value={**value,'list_options':text_list.options(value['list_options'])}
    if not split and isinstance(value,dict) and 'kerning' in value:
        keys.add('kerning');value={**value,'kerning':kerning_edits(value['kerning'])}
    if (not isinstance(value,dict) or set(value)!=keys or not isinstance(value['revision'],str) or not 1<=len(value['revision'])<=128 or
            not isinstance(value['value'],str) or not 1<=units(value['value'])<=20000 or '\0' in value['value'] or '\n' in value['value'] or
            type(value['count']) is not int or not 0<=value['count']<=40001 or not isinstance(value['origins'],list) or
            len(value['origins'])!=units(value['value'])+1 or any(type(origin) is not int or not 0<=origin<=20000 or index and origin<value['origins'][index-1] for index,origin in enumerate(value['origins']))):
        raise ValueError('无效的文字预览计划')
    if split:
        split_parameters({'revision':value['revision'],'breaks':value['breaks']})
        if value['breaks'][-1]!=units(value['value']) or value['layout'] not in ('original','lines'):raise ValueError('无效的文字拆分布局')
    return {**value,'origins':list(value['origins']),**({'breaks':list(value['breaks'])} if split else {})}


def box_dimensions(value):
    if not isinstance(value,list) or len(value)!=2 or any(type(side) not in (int,float) or not 1<=side<=100000 for side in value):
        raise ValueError('文字框尺寸超出范围')
    return value


def box_horizontal_shift(value):
    if type(value) not in (int,float) or not -100000<=value<=100000:
        raise ValueError('文字框左边位置超出范围')
    return value


def object_text_geometry(raw,canvas,frame,document,style,pdf_bounds=None):
    if (not isinstance(frame,list) or len(frame)!=8 or any(type(value) not in (int,float) or not math.isfinite(value) for value in frame)
            or not isinstance(document,list) or len(document)!=2 or any(type(value) is not int or value<=0 for value in document)):
        return None
    shape=raw['textShape'][0]
    for transform in (raw.get('transform'),shape.get('transform')):
        if transform and any(abs(transform.get(key,expected)-expected)>.00001 for key,expected in (('xx',1),('yy',1),('xy',0),('yx',0),('tx',0),('ty',0))):
            return None
    left,top,x1,y1,x2,y2,x3,y3=frame
    width,height=x1-left,y3-top
    if width<=0 or height<=0 or max(abs(y1-top),abs(x3-left),abs(x2-x1),abs(y2-y3))>.01:return None
    if pdf_bounds:
        x,y,right,bottom=pdf_bounds
        horizontal,vertical=width/(right-x),height/(bottom-y)
        left-=x*horizontal
        top-=y*vertical
        width,height=canvas['width']*horizontal,canvas['height']*vertical
    click=raw.get('textClickPoint',{})
    anchors=[click.get(key,{}) for key in ('horizontal','vertical')]
    if any(anchor.get('_unit')!='percentUnit' or type(anchor.get('_value')) not in (int,float)
           or not math.isfinite(anchor['_value']) for anchor in anchors):return None
    try:source_em=float(style['size'])*canvas['resolution']/72
    except (TypeError,ValueError,KeyError):return None
    if not math.isfinite(source_em) or source_em<=0:return None
    red,green,blue=(0,0,0)
    color=style.get('color','')
    if re.fullmatch(r'#[0-9A-Fa-f]{6}',color):
        red,green,blue=(int(color[i:i+2],16) for i in (1,3,5))
    x,y=(anchor['_value']/100 for anchor in anchors)
    wrap=shape['char']['_value']=='box'
    if wrap:
        bounds=shape.get('bounds',{})
        if any(type(bounds.get(key)) not in (int,float) for key in ('left','top','right','bottom')):return None
        l,t,r,b=(bounds[key] for key in ('left','top','right','bottom'))
        size=[r-l,b-t]
        if not all(1<=side<=100000 for side in size):return None
    else:
        l,t=0,-source_em
        r=max(0,(1-x)*canvas['width'])+source_em
        b=max(source_em,(1-y)*canvas['height'])
        size=None
    box=[(left+(x+l/canvas['width'])*width)/document[0],(top+(y+t/canvas['height'])*height)/document[1],
         (left+(x+r/canvas['width'])*width)/document[0],(top+(y+b/canvas['height'])*height)/document[1]]
    result={'box':box,'wrap':wrap,'em':source_em*height/canvas['height']/document[1],'color':[red,green,blue]}
    return (size,result) if text_canvas.valid_text_frame(result) else None


def kerning_edits(rows):
    if not isinstance(rows,list) or not 1<=len(rows)<=256:
        raise ValueError('字偶草稿最多支持 256 个修改')
    last=0
    for row in rows:
        if (not isinstance(row,list) or len(row)!=2 or type(row[0]) is not int or not last<row[0]<20000 or
                row[1] is not None and (type(row[1]) is not int or not -1000<=row[1]<=1000)):
            raise ValueError('字偶位置或数值无效')
        last=row[0]
    return [row[:] for row in rows]


def replace_parameters(value,*,allow_xmp=False):
    if allow_xmp and isinstance(value,dict) and 'list_layout' in value:
        if 'list_xmp' not in value:raise ValueError('列表排版缺少持久标记')
        return {**replace_parameters({key:item for key,item in value.items() if key!='list_layout'},allow_xmp=True),'list_layout':text_list.format_parameters(value['list_layout'])}
    if allow_xmp and isinstance(value,dict) and 'list_xmp' in value:
        text_list.read(value['list_xmp'])
        base=replace_parameters({key:item for key,item in value.items() if key!='list_xmp'})
        if 'list_options' in base:raise ValueError('无效的列表写入参数')
        return {**base,'list_xmp':value['list_xmp']}
    if isinstance(value,dict) and 'value' in value:return mapped_parameters(value)
    if (not isinstance(value,dict) or set(value)!={'revision','find','replacement'} or
            not isinstance(value['revision'],str) or not 1<=len(value['revision'])<=128 or
            not isinstance(value['find'],str) or not 1<=len(value['find'])<=200 or
            not isinstance(value['replacement'],str) or len(value['replacement'])>500):
        raise ValueError('无效的文本替换参数')
    return dict(value)


def split_parameters(value):
    if isinstance(value,dict) and 'value' in value:return mapped_parameters(value,split=True)
    if (not isinstance(value,dict) or set(value)!={'revision','breaks'} or
            not isinstance(value['revision'],str) or not 1<=len(value['revision'])<=128 or
            not isinstance(value['breaks'],list) or not 2<=len(value['breaks'])<=64 or
            any(type(end) is not int or not 1<=end<=20000 or index and end<=value['breaks'][index-1] for index,end in enumerate(value['breaks']))):
        raise ValueError('无效的文字分组参数')
    return {'revision':value['revision'],'breaks':list(value['breaks'])}


class TextTools:
    def __init__(self,app):
        self.app=app
        self.snapshot=None
        self.reading=False
        self.plan=None
        self.transient_plan=None
        self.proved=None

    def available(self):
        require_available(self.app)

    async def read(self,args,*,fast=False,trace=None):
        self.available()
        if self.reading:raise RuntimeError('文字读取进行中，请等待结果')
        self.reading=True;self.snapshot=None;self.plan=None;self.proved=None
        try:
            result=await asyncio.to_thread(self.app.bridge.transport.call,'inspect',text_inspect=True,
                                           **({'read_trace':trace} if trace else {}),
                                           **({'text_fast':True} if fast else {}))
            snapshot=result.get('text')
            if isinstance(snapshot,dict) and 'object_xmp' in snapshot:
                model=text_object.read(snapshot['object_xmp'])
                snapshot={**snapshot,**({'text':'','table':model['table'],'typography_supported':False} if model['schema']==3 else {'text':model['layer']['text']['textKey'],'list_xmp':model['layer']['xmp']}),'object_source':model,'object_render':text_object.render_hash(snapshot['object_xmp']),'vector_object':True}
                canvas=model['canvas']
                eligible=(snapshot.get('object_proof') is True and model['schema'] in (1,3) and canvas['profile']=='sRGB IEC61966-2.1'
                          and snapshot.get('proof_resolution')==canvas['resolution']
                          and (model['schema']!=1 or snapshot.get('proof_size')==[canvas['width'],canvas['height']]))
                snapshot.update(object_proof=eligible,proof_available=eligible,proof_encoding='png',
                                isolated_proof=eligible and snapshot.get('isolated_proof') is True)
            if (not isinstance(snapshot,dict) or not isinstance(snapshot.get('text'),str) or len(snapshot['text'])>20000 or
                    not isinstance(snapshot.get('revision'),str) or not 1<=len(snapshot['revision'])<=128 or
                    not isinstance(snapshot.get('name'),str) or len(snapshot['name'])>2000):
                raise RuntimeError('文字读取结果不完整')
            target=validate_target(snapshot.get('target'))
            if len(target['layer_ids'])!=1 or snapshot['target'].get('session')!=result.get('session'):
                raise RuntimeError('文字读取会话不一致')
            if snapshot.get('proof_available') and snapshot.get('proof_encoding')!='png':
                snapshot['proof_available']=False
            self.snapshot=snapshot
            public={key:value for key,value in snapshot.items() if key not in ('style_source','list_xmp','list_paragraphs','object_xmp','object_source','object_render','object_frame')}
            if snapshot.get('typography_supported'):
                model=snapshot.get('object_source')
                if model and model['schema']==2:
                    public['typography_supported']=snapshot['typography_supported']=False
                else:
                    raw=model['layer']['text'] if model else snapshot['style_source']
                    ranges=style_ranges(raw,model['canvas']['resolution'] if model else snapshot['resolution'])
                    if model and model['schema']==1:
                        shapes=raw.get('textShape',[])
                        supported=(len(shapes)==1 and raw.get('orientation',{}).get('_value')=='horizontal'
                                   and raw.get('warp',{}).get('warpStyle',{}).get('_value')=='warpNone'
                                   and shapes[0].get('char',{}).get('_value') in ('paint','box')
                                   and shapes[0].get('columnCount',1)==shapes[0].get('rowCount',1)==1)
                        public['paragraph_indents_supported']=snapshot['paragraph_indents_supported']=(
                            supported and shapes[0]['char']['_value']=='box')
                        public['paragraph_alignment_supported']=snapshot['paragraph_alignment_supported']=raw.get('orientation',{}).get('_value')=='horizontal'
                        if supported:
                            geometry=object_text_geometry(raw,model['canvas'],snapshot.get('object_frame'),
                                snapshot.get('object_canvas_size',snapshot.get('proof_size')),ranges['textStyle'][0]['values'],
                                text_object.render_bounds(snapshot['object_xmp'],model['canvas']))
                            if geometry:
                                size,public['object_text_frame']=geometry
                                if size and snapshot.get('box_resize_available'):public['box_size']=snapshot['box_size']=size
                    public['styles']=summarize_ranges(ranges)
                    if snapshot.get('typography_runs_supported'):public['style_ranges']=ranges
                    if snapshot.get('opentype_supported'):
                        fallback=raw['textStyleRange'][0]['textStyle'].get('baseParentStyle')
                        public['font_runs']=[[run['from'],run['to'],font_in_run(run['textStyle'],fallback)]
                                             for run in raw['textStyleRange']]
                    if snapshot.get('kerning_supported'):
                        if model:
                            shapes=raw.get('textShape',[])
                            public['kerning_supported']=snapshot['kerning_supported']=(model['schema']==1 and len(shapes)==1
                                and shapes[0].get('char',{}).get('_value') in ('paint','box')
                                and raw.get('orientation',{}).get('_value')=='horizontal'
                                and raw.get('warp',{}).get('warpStyle',{}).get('_value')=='warpNone')
                        public['kerning_pairs']=[[row['to'],row['kerning']] for row in raw.get('kerningRange',[])
                                                 if isinstance(row,dict) and type(row.get('to')) is int and type(row.get('kerning')) is int]
            if 'list_xmp' in snapshot:
                try:public['list_options']=text_list.read(snapshot['list_xmp'])
                except ValueError as error:public['list_warning']=str(error)
            return {**public,'message':'文字已读取；预览不会修改PS'}
        finally:self.reading=False

    async def replace(self,args):
        return await self._apply(replace_parameters(args),'text.replace')

    async def split(self,args,menu_guard=None):
        return await self._apply(split_parameters(args),'text.split',menu_guard)

    async def write(self,args,menu_guard=None):
        if not isinstance(args,dict) or set(args) not in ({'revision','draft'},{'revision','draft','list_options'}):raise ValueError('无效的文字写回参数')
        result=await self.preview({**args,'operation':'edit'})
        return await self.apply({'revision':args['revision'],'plan_id':result['plan_id']},menu_guard)

    async def table(self,args,menu_guard=None):
        if not isinstance(args,dict) or set(args)!={'revision','cells'}:raise ValueError('表格参数无效')
        params={'revision':args['revision'],'cells':text_object.table_cells(args['cells'])}
        key=self.table_proof_key(params)
        return await self._apply(params,'text.object',menu_guard,proof_history=self.proved[1] if self.proved and self.proved[0]==key else None)

    @staticmethod
    def table_proof_key(params):
        return (params['revision'],tuple(tuple(row) for row in params['cells']))
    async def vector(self,args,menu_guard=None):
        history=None
        if isinstance(args,dict) and set(args)=={'revision','plan_id'}:
            if not self.plan or args['plan_id']==self.transient_plan or (args['plan_id'],args['revision'])!=(self.plan[0],self.plan[1]['revision']) or self.plan[2]!='text.replace':raise ValueError('文字对象预览已改变，请先选定字体')
            identity,params,_=self.plan;self.plan=None
            if self.proved and self.proved[0]==identity:history=self.proved[1]
        else:
            if not isinstance(args,dict) or set(args) not in ({'revision','draft'},{'revision','draft','list_options'}):raise ValueError('文字对象草稿无效')
            result=await self.preview({**args,'operation':'edit'});_,params,_=self.plan;self.plan=None
        return await self._apply(params,'text.object',menu_guard,proof_history=history)

    async def preview(self,args):
        self.available();saved=self.snapshot;self.plan=None
        transient=False
        styles=None
        style_key=None
        if isinstance(args,dict) and ('typography' in args or 'typography_runs' in args):
            if args.get('operation')=='split' or not saved or not saved.get('typography_supported'):
                raise ValueError('请重新读取支持排版的文字层或矢量文字对象')
            if 'typography_runs' in args:
                if 'typography' in args or not saved.get('typography_runs_supported'):
                    raise ValueError('多范围样式需要更新执行插件并重新读取文本')
                style_key='typography_runs'
                styles=typography_runs(args[style_key])
            else:
                style_key='typography'
                styles=[typography(args[style_key])]
            for style in styles:validate_style_scope(style,saved,args)
            args={key:value for key,value in args.items() if key!=style_key}
        if isinstance(args,dict) and 'font_preview' in args:
            if (args['font_preview'] is not True or not saved or not saved.get('proof_available') or args.get('operation')!='edit' or
                    not styles or not any('fontPostScriptName' in style for style in styles)):
                raise ValueError('字体预览参数无效')
            transient=True
            args={k:v for k,v in args.items() if k!='font_preview'}
        kern=None
        box=None
        shift=None
        if isinstance(args,dict) and 'box_size' in args:
            if not saved or not saved.get('box_size') or args.get('operation')=='split':raise ValueError('当前文字框无法调整尺寸')
            box=box_dimensions(args['box_size'])
            if 'box_shift_x' in args:
                if not saved.get('box_shift_supported'):raise ValueError('当前文字框无法移动左边缘')
                shift=box_horizontal_shift(args['box_shift_x'])
            args={key:value for key,value in args.items() if key not in ('box_size','box_shift_x')}
        if isinstance(args,dict) and 'kerning' in args:
            if not saved or not saved.get('kerning_supported') or args.get('operation')!='edit':
                raise ValueError('手动字偶微调需要打开支持的水平文字并选择编辑')
            kern=kerning_edits(args['kerning'])
            args={key:value for key,value in args.items() if key!='kerning'}
        carried=isinstance(args,dict) and 'list_options' in args
        if carried and args.get('operation') in ('list','split'):raise ValueError('列表草稿参数无效')
        params=parameters({key:value for key,value in args.items() if key!='list_options'} if carried else args)
        if not saved or params['revision']!=saved['revision']:raise ValueError('文字读取已过期，请重新读取')
        # Style-only drafts have identity origins and never need the diff worker.
        if params['operation']=='edit' and normalize(params['draft'])==normalize(saved['text']):
            result=compute(saved['text'],params)
        else:result=await asyncio.to_thread(evaluate,saved['text'],params)
        if self.snapshot is not saved:raise ValueError('文字读取已改变，请重新预览')
        split=result['kind']=='split'
        payload={key:result[key] for key in ({'value','origins','count','breaks','layout'} if split else {'value','origins','count'})}
        draft_origins=result.get('draft_origins')
        if styles:styles=map_typography(styles,normalize(params['draft']),draft_origins)
        if styles:payload[style_key]=styles if style_key=='typography_runs' else styles[0]
        if kern:payload['kerning']=kern
        if box:payload['box_size']=box
        if shift is not None:payload['box_shift_x']=shift
        payload['revision']=saved['revision'];payload=mapped_parameters(payload,split)
        changed_list=None
        if not split and not carried and params['operation']!='list' and result['value']!=saved['text'] and 'list_xmp' in saved:
            changed_list=text_list.read(saved['list_xmp'])
        if params['operation']=='list' or carried:
            value=args['list_options'] if carried else (None if params['kind']=='remove' else {key:params[key] for key in text_list.KEYS})
            value=text_list.options(value)
            if 'list_xmp' not in saved:raise ValueError('列表保存需要正常重启PS加载UXP0.3.81')
            text_list.write(saved['list_xmp'],value)
            payload['list_options']=value
        elif changed_list is not None:
            payload['list_options']=changed_list
        identity=secrets.token_hex(16);self.plan=(identity,payload,'text.split' if split else 'text.replace')
        self.transient_plan=identity if transient else None
        response={key:result[key] for key in ('preview','value','count','changed','kind','characters','matches')}|{'plan_id':identity}
        if draft_origins is not None:response['draft_origins']=draft_origins
        return response

    async def proof(self,args,menu_guard):
        saved=self.snapshot
        if not isinstance(args,dict) or not saved or args.get('revision')!=saved['revision']:
            raise ValueError('请重新读取画布草稿')
        table='cells' in args
        vector=args.get('vector',False)
        if type(vector) is not bool or vector and (table or not saved.get('object_proof')):
            raise ValueError('当前文档尚不支持矢量草稿预览')
        isolated=args.get('isolated',False)
        if type(isolated) is not bool or (isolated and saved.get('isolated_proof') is not True):
            raise ValueError('当前文档尚不支持独立背景预览')
        if table:
            if set(args)-{'revision','cells','scale','isolated'} or not saved.get('object_proof'):
                raise ValueError('当前文档尚不支持表格草稿预览')
            params={'revision':saved['revision'],'cells':text_object.table_cells(args['cells'])}
            key=self.table_proof_key(params)
        else:
            if (set(args)-{'revision','plan_id','scale','isolated','vector'} or not self.plan or args.get('plan_id')!=self.plan[0]
                    or saved['revision']!=self.plan[1]['revision'] or self.plan[2]!='text.replace' or not saved.get('proof_available')):
                raise ValueError('请重新生成当前排版草稿')
            key,params,_=self.plan
        scale=args.get('scale',1)
        size=self.snapshot.get('proof_size')
        if type(scale) is not int or not 1<=scale<=8 or (scale>1 and
                (not isinstance(size,list) or len(size)!=2 or any(type(i) is not int or not 1<=i*scale<=8192 for i in size)
                 or size[0]*size[1]*scale**2>16777216)):
            raise ValueError('草稿预览分辨率超出范围')
        if scale>1 and (not self.proved or self.proved[0]!=key):
            raise ValueError('请先获取当前草稿的原尺寸预览')
        result=await self._apply(params,'text.proof',menu_guard,proof_history=self.proved[1] if scale>1 else None,proof_scale=scale,vector_proof=vector,
                                 **({'proof_isolated':True} if isolated else {}))
        memory,frame=result['frame']
        if self.snapshot is not saved or (not table and (not self.plan or self.plan[0]!=key)):
            memory.close()
            raise ValueError('草稿已改变，请重新预览')
        if scale==1:self.proved=(key,frame['history'])
        return memory,{**frame,**({'table':True} if table else {'plan_id':key})}

    async def apply(self,args,menu_guard=None):
        if not isinstance(args,dict) or set(args)!={'revision','plan_id'} or not self.plan or args['plan_id']==self.transient_plan or (args['plan_id'],args['revision'])!=(self.plan[0],self.plan[1]['revision']):
            raise ValueError('预览已改变，请重新计算后确认')
        identity,params,command=self.plan;self.plan=None
        proof=self.proved;self.proved=None
        return await self._apply(params,command,menu_guard,proof[1] if proof and proof[0]==identity else None)

    async def _apply(self,params,command,menu_guard=None,proof_history=None,proof_scale=1,proof_isolated=False,vector_proof=False):
        self.available()
        snapshot=self.snapshot
        if not snapshot or params['revision']!=snapshot['revision']:raise ValueError('文本预览已过期，请重新读取')
        if 'table' in snapshot and 'cells' not in params:raise ValueError('请在表格页修改单元格')
        proof=command=='text.proof'
        object_operation=command=='text.object' or vector_proof or snapshot.get('vector_object') or 'cells' in params
        if snapshot.get('vector_object'):
            if command=='text.split':raise ValueError('矢量文字对象暂不支持拆成图层')
            params={**params,'object_source':snapshot['object_source'],'object_render':snapshot['object_render']}
        if 'list_options' in params:
            if 'list_xmp' not in snapshot:raise ValueError('列表保存需要正常重启PS加载UXP0.3.81')
            enabled=params['list_options'] is not None
            xml=text_list.write(snapshot['list_xmp'],params['list_options'])
            model=snapshot.get('object_source')
            if not model or model['schema']==1:
                current=snapshot.get('list_paragraphs')
                if current is None and model:
                    rows=model['layer']['text']['paragraphStyleRange']
                    base=rows[0]['paragraphStyle'].get('baseParentStyle',{})
                    def points(value):
                        if value==0:return 0
                        return value['_value']*(72/model['canvas']['resolution'] if value['_unit']=='pixelsUnit' else 1)
                    current=[]
                    for row in rows:
                        style=row['paragraphStyle'];inherited={**base,**style.get('baseParentStyle',{}),**style}
                        current.append({'from':row['from'],'to':row['to'],'start':points(inherited.get('startIndent',0)),
                                        'first':points(inherited.get('firstLineIndent',0))})
                if current is None:raise ValueError('请重新读取支持列表排版的图层')
                formatting,xml=text_list.formatting(params['value'],params['origins'],current,xml,enabled)
                params={**params,'list_layout':formatting}
            params={**{key:value for key,value in params.items() if key!='list_options'},'list_xmp':xml}
        app=self.app
        async with app.operation_lock:
            await app.broadcast({'type':'operation','busy':True,'action':'text.object' if object_operation and not proof else command})
            try:
                if menu_guard:
                    await menu_guard()
                    if command!='text.proof':await asyncio.to_thread(app.desktop.focus_search_result,keep_menu=True)
                else:await asyncio.to_thread(app.desktop.focus_search_result)
                if object_operation:result=await app.bridge.execute_text_object(snapshot['target'],params,
                    **({'before_dispatch':menu_guard} if menu_guard else {}),
                    **({'proof_scale':proof_scale} if proof else {}),
                    **({'proof_isolated':True} if proof_isolated else {}),
                    **({'proof_history':proof_history} if proof_history is not None else {}))
                else:result=await app.bridge.execute_text(snapshot['target'],params,command,
                    **({'before_dispatch':menu_guard} if menu_guard else {}),
                    **({'proof_history':proof_history} if proof_history is not None else {}),
                    **({'proof_isolated':True} if proof_isolated else {}),
                    **({'proof_scale':proof_scale} if proof_scale>1 else {}))
                if command!='text.proof':self.snapshot=None;self.plan=None
                app.desktop.layer_context=dict(app.bridge.layer_context)
                return result
            finally:await app.broadcast({'type':'operation','busy':app.bridge.busy})
