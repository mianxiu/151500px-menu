"""One selected text layer, read-only preview and explicit single-history commit."""
import asyncio
import math
import secrets
from .layer_search import require_available
from .operation_target import validate_target
from .text_plan import compute,evaluate,normalize,parameters,units
from . import text_list,text_object

INDENTS=('startIndent','endIndent','firstLineIndent')
PARAGRAPH_DISTANCES=('spaceBefore','spaceAfter',*INDENTS)
OPENTYPE_SWITCHES=('contextualLigatures','stylisticAlternates','swash','titling','ornaments','fractions','ordinals')
STYLE_SWITCHES=('syntheticBold','syntheticItalic','ligature','noBreak',*OPENTYPE_SWITCHES,'hyphenate')
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
                  *STYLE_CHOICES,*STYLE_SWITCHES[:-1],'color')
EXTENDED_STYLES={'baselineShift','horizontalScale','verticalScale','align'}
ALIGNMENT={'left':'左对齐','center':'居中','right':'右对齐','justifyLeft':'两端对齐，末行左',
           'justifyCenter':'两端对齐，末行居中','justifyRight':'两端对齐，末行右','justifyAll':'全部两端对齐'}

def typography(value):
    if not isinstance(value,dict) or not set(value)-{'range'} or set(value)-set((*CHARACTER_STYLES,*PARAGRAPH_DISTANCES,'hyphenate','align','range')):
        raise ValueError('排版参数无效')
    result={}
    for key,item in value.items():
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
        if key=='tracking':
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


def style_summary(raw,resolution):
    summary={}
    for field,keys in [('textStyle',CHARACTER_STYLES),('paragraphStyle',(*PARAGRAPH_DISTANCES,'align','hyphenate'))]:
        runs=raw[field+'Range']
        for key in keys:
            values=set()
            for run in runs:
                chain=[run[field]]
                while isinstance(chain[-1].get('baseParentStyle'),dict):chain.append(chain[-1]['baseParentStyle'])
                chain.append(runs[0][field].get('baseParentStyle') or {})
                item=next((style[key] for style in chain if key in style),None)
                if key=='leading' and next((style['autoLeading'] for style in chain if 'autoLeading' in style),False):item='自动'
                if key=='align' and isinstance(item,dict):
                    item=ALIGNMENT.get(item.get('_value'))
                if key in STYLE_CHOICES and isinstance(item,dict):
                    item=STYLE_CHOICES[key].get(item.get('_value'))
                if key in STYLE_SWITCHES and type(item) is bool:item='开启' if item else '关闭'
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
                values.add(str(item) if item is not None else '继承')
            summary[key]=next(iter(values)) if len(values)==1 else '混合'
    return summary


def font_in_run(style,fallback):
    while isinstance(style,dict):
        if isinstance(style.get('fontPostScriptName'),str):return style['fontPostScriptName']
        style=style.get('baseParentStyle')
    return fallback.get('fontPostScriptName','') if isinstance(fallback,dict) else ''


def mapped_parameters(value,split=False):
    keys={'revision','value','origins','count'}|({'breaks','layout'} if split else set())
    if not split and isinstance(value,dict) and 'typography' in value:
        keys.add('typography');value={**value,'typography':typography(value['typography'])}
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

    async def read(self,args,*,fast=False):
        self.available()
        if self.reading:raise RuntimeError('文字读取进行中，请等待结果')
        self.reading=True;self.snapshot=None;self.plan=None;self.proved=None
        try:
            result=await asyncio.to_thread(self.app.bridge.transport.call,'inspect',text_inspect=True,
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
            public={key:value for key,value in snapshot.items() if key not in ('style_source','list_xmp','list_paragraphs','object_xmp','object_source','object_render')}
            if snapshot.get('typography_supported'):
                model=snapshot.get('object_source')
                if model and model['schema']==2:
                    public['typography_supported']=snapshot['typography_supported']=False
                else:
                    raw=model['layer']['text'] if model else snapshot['style_source']
                    if model and model['schema']==1:
                        shapes=raw.get('textShape',[])
                        supported=(len(shapes)==1 and raw.get('orientation',{}).get('_value')=='horizontal'
                                   and raw.get('warp',{}).get('warpStyle',{}).get('_value')=='warpNone'
                                   and shapes[0].get('char',{}).get('_value')=='box'
                                   and shapes[0].get('columnCount',1)==shapes[0].get('rowCount',1)==1)
                        public['paragraph_indents_supported']=snapshot['paragraph_indents_supported']=supported
                        public['paragraph_alignment_supported']=snapshot['paragraph_alignment_supported']=raw.get('orientation',{}).get('_value')=='horizontal'
                    public['styles']=style_summary(raw,model['canvas']['resolution'] if model else snapshot['resolution'])
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
        if isinstance(args,dict) and set(args)=={'revision','plan_id'}:
            if not self.plan or args['plan_id']==self.transient_plan or (args['plan_id'],args['revision'])!=(self.plan[0],self.plan[1]['revision']) or self.plan[2]!='text.replace':raise ValueError('文字对象预览已改变，请先选定字体')
            _,params,_=self.plan;self.plan=None
        else:
            if not isinstance(args,dict) or set(args) not in ({'revision','draft'},{'revision','draft','list_options'}):raise ValueError('文字对象草稿无效')
            result=await self.preview({**args,'operation':'edit'});_,params,_=self.plan;self.plan=None
        return await self._apply(params,'text.object',menu_guard)

    async def preview(self,args):
        self.available();saved=self.snapshot;self.plan=None
        transient=False
        if isinstance(args,dict) and 'font_preview' in args:
            if (args['font_preview'] is not True or not saved or not saved.get('proof_available') or args.get('operation')!='edit' or
                    not isinstance(args.get('typography'),dict) or 'fontPostScriptName' not in args['typography']):
                raise ValueError('字体预览参数无效')
            transient=True
            args={k:v for k,v in args.items() if k!='font_preview'}
        style=None
        kern=None
        if isinstance(args,dict) and 'kerning' in args:
            if not saved or not saved.get('kerning_supported') or args.get('operation')!='edit':
                raise ValueError('手动字偶微调需要打开支持的水平文字并选择编辑')
            kern=kerning_edits(args['kerning'])
            args={key:value for key,value in args.items() if key!='kerning'}
        if isinstance(args,dict) and 'typography' in args:
            if args.get('operation')=='split' or not saved or not saved.get('typography_supported'):
                raise ValueError('请重新读取支持排版的文字层或矢量文字对象')
            style=typography(args['typography'])
            if style.keys() & OPENTYPE_STYLES and not saved.get('opentype_supported'):
                raise ValueError('请更新插件后重新读取 OpenType 设置')
            if (style.keys() & EXTENDED_STYLES or style.get('leading')=='auto') and not saved.get('extended_typography'):
                raise ValueError('请更新插件后重新读取扩展排版选项')
            if (style.keys() & ADVANCED_STYLES or style.get('align','') not in ('','left','center','right')) and not saved.get('advanced_typography'):
                raise ValueError('请更新插件后重新读取字符和段落选项')
            if 'color' in style and not saved.get('text_rgb'):
                raise ValueError('文字取色需要 sRGB 8 位文档')
            if style.keys() & {'align','hyphenate'} and (args.get('operation')=='list' or args.get('list_options') is not None
                                     or text_list.read(saved.get('list_xmp','')) is not None):
                raise ValueError('列表使用自身的段落对齐与断字设置')
            if 'align' in style and not saved.get('paragraph_alignment_supported'):
                raise ValueError('段落对齐需要普通水平文字')
            if (style.get('align','').startswith('justify') or 'hyphenate' in style) and not saved.get('paragraph_indents_supported'):
                raise ValueError('两端对齐和断字需要水平段落文字')
            if style.keys() & set(INDENTS) and not saved.get('paragraph_indents_supported'):
                raise ValueError('请重新读取支持段落缩进的普通文字层')
            if style.keys() & {'startIndent','firstLineIndent'} and (
                    args.get('operation')=='list' or args.get('list_options') is not None
                    or text_list.read(saved.get('list_xmp','')) is not None):
                raise ValueError('列表的左缩进和首行缩进由列表层级控制')
            if 'range' in style and (not saved.get('style_scope_supported') or args.get('operation')!='edit'):
                raise ValueError('局部排版需在排版页确认，请先完成其他正文处理')
            args={key:value for key,value in args.items() if key!='typography'}
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
        if style:payload['typography']=style
        if kern:payload['kerning']=kern
        payload['revision']=saved['revision'];payload=mapped_parameters(payload,split)
        if params['operation']=='list' or carried:
            value=args['list_options'] if carried else (None if params['kind']=='remove' else {key:params[key] for key in text_list.KEYS})
            value=text_list.options(value)
            if 'list_xmp' not in saved:raise ValueError('列表保存需要正常重启PS加载UXP0.3.81')
            text_list.write(saved['list_xmp'],value)
            payload['list_options']=value
        identity=secrets.token_hex(16);self.plan=(identity,payload,'text.split' if split else 'text.replace')
        self.transient_plan=identity if transient else None
        return {key:result[key] for key in ('preview','value','count','changed','kind','characters','matches')}|{'plan_id':identity}

    async def proof(self,args,menu_guard):
        saved=self.snapshot
        if not isinstance(args,dict) or not saved or args.get('revision')!=saved['revision']:
            raise ValueError('请重新读取画布草稿')
        table='cells' in args
        isolated=args.get('isolated',False)
        if type(isolated) is not bool or (isolated and saved.get('isolated_proof') is not True):
            raise ValueError('当前文档尚不支持独立背景预览')
        if table:
            if set(args)-{'revision','cells','scale','isolated'} or not saved.get('object_proof'):
                raise ValueError('当前文档尚不支持表格草稿预览')
            params={'revision':saved['revision'],'cells':text_object.table_cells(args['cells'])}
            key=self.table_proof_key(params)
        else:
            if (set(args)-{'revision','plan_id','scale','isolated'} or not self.plan or args.get('plan_id')!=self.plan[0]
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
        result=await self._apply(params,'text.proof',menu_guard,proof_history=self.proved[1] if scale>1 else None,proof_scale=scale,
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

    async def _apply(self,params,command,menu_guard=None,proof_history=None,proof_scale=1,proof_isolated=False):
        self.available()
        snapshot=self.snapshot
        if not snapshot or params['revision']!=snapshot['revision']:raise ValueError('文本预览已过期，请重新读取')
        if 'table' in snapshot and 'cells' not in params:raise ValueError('请在表格页修改单元格')
        proof=command=='text.proof'
        object_operation=command=='text.object' or snapshot.get('vector_object') or 'cells' in params
        if snapshot.get('vector_object'):
            if command=='text.split':raise ValueError('矢量文字对象暂不支持拆成图层')
            params={**params,'object_source':snapshot['object_source'],'object_render':snapshot['object_render']}
        if 'list_options' in params:
            if 'list_xmp' not in snapshot:raise ValueError('列表保存需要正常重启PS加载UXP0.3.81')
            enabled=params['list_options'] is not None
            xml=text_list.write(snapshot['list_xmp'],params['list_options'])
            if not object_operation:
                if 'list_paragraphs' not in snapshot:raise ValueError('列表排版需要正常重启PS加载UXP0.3.92')
                formatting,xml=text_list.formatting(params['value'],params['origins'],snapshot['list_paragraphs'],xml,enabled)
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
