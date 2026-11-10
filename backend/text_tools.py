"""One selected text layer, read-only preview and explicit single-history commit."""
import asyncio
import secrets
from .layer_search import require_available
from .operation_target import validate_target
from .text_plan import evaluate,parameters,units
from . import text_list,text_object


def mapped_parameters(value,split=False):
    keys={'revision','value','origins','count'}|({'breaks','layout'} if split else set())
    if not split and isinstance(value,dict) and 'list_options' in value:
        keys.add('list_options');value={**value,'list_options':text_list.options(value['list_options'])}
    if (not isinstance(value,dict) or set(value)!=keys or not isinstance(value['revision'],str) or not 1<=len(value['revision'])<=128 or
            not isinstance(value['value'],str) or not 1<=units(value['value'])<=20000 or '\0' in value['value'] or '\n' in value['value'] or
            type(value['count']) is not int or not 0<=value['count']<=40001 or not isinstance(value['origins'],list) or
            len(value['origins'])!=units(value['value'])+1 or any(type(origin) is not int or not 0<=origin<=20000 or index and origin<value['origins'][index-1] for index,origin in enumerate(value['origins']))):
        raise ValueError('无效的文字预览计划')
    if split:
        split_parameters({'revision':value['revision'],'breaks':value['breaks']})
        if value['breaks'][-1]!=units(value['value']) or value['layout'] not in ('original','lines'):raise ValueError('无效的文字拆分布局')
    return {**value,'origins':list(value['origins']),**({'breaks':list(value['breaks'])} if split else {})}


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

    def available(self):
        require_available(self.app)

    async def read(self,args):
        self.available()
        if self.reading:raise RuntimeError('文字读取进行中，请等待结果')
        self.reading=True;self.snapshot=None;self.plan=None
        try:
            result=await asyncio.to_thread(self.app.bridge.transport.call,'inspect',text_inspect=True)
            snapshot=result.get('text')
            if isinstance(snapshot,dict) and 'object_xmp' in snapshot:
                model=text_object.read(snapshot['object_xmp'])
                snapshot={**snapshot,'text':model['layer']['text']['textKey'],'list_xmp':model['layer']['xmp'],'object_source':model,'object_render':text_object.render_hash(snapshot['object_xmp']),'vector_object':True}
            if (not isinstance(snapshot,dict) or not isinstance(snapshot.get('text'),str) or len(snapshot['text'])>20000 or
                    not isinstance(snapshot.get('revision'),str) or not 1<=len(snapshot['revision'])<=128 or
                    not isinstance(snapshot.get('name'),str) or len(snapshot['name'])>2000):
                raise RuntimeError('文字读取结果不完整')
            target=validate_target(snapshot.get('target'))
            if len(target['layer_ids'])!=1 or snapshot['target'].get('session')!=result.get('session'):
                raise RuntimeError('文字读取会话不一致')
            self.snapshot=snapshot
            public={key:value for key,value in snapshot.items() if key not in ('list_xmp','list_paragraphs','object_xmp','object_source','object_render')}
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

    async def vector(self,args,menu_guard=None):
        if isinstance(args,dict) and set(args)=={'revision','plan_id'}:
            if not self.plan or (args['plan_id'],args['revision'])!=(self.plan[0],self.plan[1]['revision']) or self.plan[2]!='text.replace':raise ValueError('文字对象预览已改变')
            _,params,_=self.plan;self.plan=None
        else:
            if not isinstance(args,dict) or set(args) not in ({'revision','draft'},{'revision','draft','list_options'}):raise ValueError('文字对象草稿无效')
            result=await self.preview({**args,'operation':'edit'});_,params,_=self.plan;self.plan=None
        return await self._apply(params,'text.object',menu_guard)

    async def preview(self,args):
        self.available();saved=self.snapshot;self.plan=None
        carried=isinstance(args,dict) and 'list_options' in args
        if carried and args.get('operation') in ('list','split'):raise ValueError('列表草稿参数无效')
        params=parameters({key:value for key,value in args.items() if key!='list_options'} if carried else args)
        if not saved or params['revision']!=saved['revision']:raise ValueError('文字读取已过期，请重新读取')
        result=await asyncio.to_thread(evaluate,saved['text'],params)
        if self.snapshot is not saved:raise ValueError('文字读取已改变，请重新预览')
        split=result['kind']=='split'
        payload={key:result[key] for key in ({'value','origins','count','breaks','layout'} if split else {'value','origins','count'})}
        payload['revision']=saved['revision'];payload=mapped_parameters(payload,split)
        if params['operation']=='list' or carried:
            value=args['list_options'] if carried else (None if params['kind']=='remove' else {key:params[key] for key in text_list.KEYS})
            value=text_list.options(value)
            if 'list_xmp' not in saved:raise ValueError('列表保存需要正常重启PS加载UXP0.3.81')
            text_list.write(saved['list_xmp'],value)
            payload['list_options']=value
        identity=secrets.token_hex(16);self.plan=(identity,payload,'text.split' if split else 'text.replace')
        return {key:result[key] for key in ('preview','value','count','changed','kind','characters','matches')}|{'plan_id':identity}

    async def apply(self,args,menu_guard=None):
        if not isinstance(args,dict) or set(args)!={'revision','plan_id'} or not self.plan or (args['plan_id'],args['revision'])!=(self.plan[0],self.plan[1]['revision']):
            raise ValueError('预览已改变，请重新计算后确认')
        _,params,command=self.plan;self.plan=None
        return await self._apply(params,command,menu_guard)

    async def _apply(self,params,command,menu_guard=None):
        self.available()
        snapshot=self.snapshot
        if not snapshot or params['revision']!=snapshot['revision']:raise ValueError('文本预览已过期，请重新读取')
        if snapshot.get('vector_object'):
            if command=='text.split':raise ValueError('矢量文字对象暂不支持拆成图层')
            command='text.object';params={**params,'object_source':snapshot['object_source'],'object_render':snapshot['object_render']}
        if 'list_options' in params:
            if 'list_xmp' not in snapshot:raise ValueError('列表保存需要正常重启PS加载UXP0.3.81')
            enabled=params['list_options'] is not None
            xml=text_list.write(snapshot['list_xmp'],params['list_options'])
            if command!='text.object':
                if 'list_paragraphs' not in snapshot:raise ValueError('列表排版需要正常重启PS加载UXP0.3.92')
                formatting,xml=text_list.formatting(params['value'],params['origins'],snapshot['list_paragraphs'],xml,enabled)
                params={**params,'list_layout':formatting}
            params={**{key:value for key,value in params.items() if key!='list_options'},'list_xmp':xml}
        app=self.app
        async with app.operation_lock:
            await app.broadcast({'type':'operation','busy':True,'action':command})
            try:
                if menu_guard:
                    await menu_guard()
                    await asyncio.to_thread(app.desktop.focus_search_result,keep_menu=True)
                else:await asyncio.to_thread(app.desktop.focus_search_result)
                if command=='text.object':result=await app.bridge.execute_text_object(snapshot['target'],params,**({'before_dispatch':menu_guard} if menu_guard else {}))
                else:result=await app.bridge.execute_text(snapshot['target'],params,command,**({'before_dispatch':menu_guard} if menu_guard else {}))
                self.snapshot=None;self.plan=None
                app.desktop.layer_context=dict(app.bridge.layer_context)
                return result
            finally:await app.broadcast({'type':'operation','busy':app.bridge.busy})
