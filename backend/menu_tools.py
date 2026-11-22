"""Caps-owned views of the existing search and single-history text tools."""
import asyncio
import json
from .layer_context import fresh
from .layer_search import LayerSearch, require_available
from .text_tools import TextTools
from . import text_canvas


class MenuTools:
    def __init__(self, app):
        self.app=app
        self.epoch=None
        self.pending=False
        self.last_request=0
        self.preview_request=None
        self.search=LayerSearch(app)
        self.text=TextTools(app)

    async def open(self,args):
        params=args.get('params',{})
        if not isinstance(params,dict) or set(params)-{'mode'}:raise ValueError('无效的工具参数')
        epoch=args.get('menu_epoch')
        if epoch is not None and (type(epoch) is not int or epoch<=0):raise ValueError('无效的菜单会话')
        mode=params.get('mode','search')
        if mode not in ('search','split','typeset'):raise ValueError('未知的Caps工具')
        if mode=='typeset' and not text_canvas.connection(self.app.runtime,self.app.desktop.target):
            raise RuntimeError('当前 Photoshop 尚未连接画布读取组件')
        require_available(self.app)
        if self.pending or self.app.panel_lock.locked():raise RuntimeError('工具正在处理，本次未接受')
        async with self.app.panel_lock:
            guard={'input_guard':args['input_guard']} if 'input_guard' in args else {}
            epoch=await asyncio.to_thread(self.app.desktop.open_menu_tools,mode,args.get('menu_epoch'),**guard)
            try:require_available(self.app)
            except Exception:
                await asyncio.to_thread(self.app.desktop.hide_if_epoch,epoch)
                raise
            self.epoch=epoch
            self.last_request=0
            self.search=LayerSearch(self.app);self.text=TextTools(self.app)
            self.app.desktop.event({'type':'menu.tools.opened','epoch':epoch,'mode':mode})
        return {'message':'Caps工具已打开','epoch':epoch}

    async def request(self,args):
        epoch=args.get('menu_epoch');value=args.get('params',{})
        if (type(epoch) is not int or epoch<=0 or not isinstance(value,dict) or set(value)!={'request','action','args'} or
                type(value['request']) is not int or not 1<=value['request']<=2147483647 or
                value['action'] not in ('layers.search','layers.focus','text.read','text.fonts','text.canvas','text.split','text.preview','text.proof','text.apply','text.write','text.vector','text.table') or not isinstance(value['args'],dict)):
            raise ValueError('无效的Caps工具请求')
        async def guard(_target=None):
            if epoch != self.epoch:raise RuntimeError('工具会话已改变，本次未执行')
            await asyncio.to_thread(self.app.desktop.require_menu_tools,epoch)
        event={'type':'menu.tools.result','epoch':epoch,'request':value['request'],'action':value['action']}
        if self.pending and epoch==self.epoch and value['request']==self.last_request:return {'message':'已忽略重复工具请求'}
        if self.pending or self.preview_request is not None:
            self.app.desktop.event({**event,'ok':False,'error':'操作进行中，本次请求未接受'})
            return {'message':'操作进行中'}
        self.pending=True
        try:
            await guard();require_available(self.app)
            if value['request']<=self.last_request:raise RuntimeError('已忽略重复工具请求')
            self.last_request=value['request']
            action,params=value['action'],value['args']
            if action=='layers.search':
                result=await self.search.search(params)
                await guard()
                await asyncio.to_thread(self.app.desktop.resize_menu_tools,epoch,len(result['items']))
            elif action=='layers.focus':result=await self.search.focus(params,menu_guard=guard)
            elif action=='text.read':
                result=await self.text.read(params,fast=True)
                await guard()
                target=self.app.desktop.menu_layer_context or {}
                if any(target.get(key)!=result['target'].get(key) for key in ('session','document_id','layer_ids')):
                    self.text.snapshot=None
                    raise RuntimeError('文字目标已改变，请重新打开菜单')
                if getattr(self.app,'runtime',None):
                    canvas=text_canvas.connection(self.app.runtime,self.app.desktop.target)
                    result['canvas_available']=bool(canvas)
                    result['canvas_edit_available']=bool(canvas and canvas['version']>=2 and 'table' not in result)
            elif action=='text.fonts':
                saved=self.text.snapshot
                if not saved or set(params)!={'revision'} or params['revision']!=saved['revision'] or saved.get('fonts_available') is not True:
                    raise ValueError('请重新读取文字后选择字体')
                catalog=await asyncio.to_thread(self.app.bridge.transport.call,'inspect',text_fonts=True)
                if catalog.get('session')!=saved['target']['session'] or not isinstance(catalog.get('fonts'),list):
                    raise RuntimeError('PS 字体目录已过期，请重新读取')
                await guard()
                result={'fonts':catalog['fonts']}
            elif action=='text.canvas':
                saved=self.text.snapshot
                background=params.get('background',False)
                if (not saved or params.get('revision')!=saved['revision'] or set(params)-{'revision','background'}
                        or type(background) is not bool or background and 'table' in saved):
                    raise ValueError('请重新读取普通文字目标')
                memory,result=await text_canvas.read(self.app.runtime,self.app.desktop.target,saved['target'],background=background)
                try:
                    if type(saved.get('canvas_history')) is int:
                        if result.get('history')!=saved['canvas_history']:raise RuntimeError('文字或样式已改变，请重新读取画布')
                        latest=saved
                    else:
                        latest=(await asyncio.to_thread(self.app.bridge.transport.call,'inspect',text_inspect=True,text_fast=True))['text']
                        keys=('target','name','object_xmp','proof_size','proof_resolution') if saved.get('vector_object') else ('target','name','text','style_source','resolution','proof_size')
                        if any(latest.get(k)!=saved.get(k) for k in keys):raise RuntimeError('文字或样式已改变，请重新读取画布')
                    await guard()
                    saved['revision']=latest['revision']
                    self.text.plan=None
                    result['revision']=latest['revision']
                except BaseException:memory.close();raise
                asyncio.get_running_loop().call_later(20,memory.close)
            elif action=='text.preview':
                result=await self.text.preview(params)
                await guard()
            elif action=='text.proof':
                memory,result=await self.text.proof(params,menu_guard=guard)
                try:await guard()
                except BaseException:memory.close();raise
                asyncio.get_running_loop().call_later(20,memory.close)
            elif action=='text.apply':result=await self.text.apply(params,menu_guard=guard)
            elif action=='text.write':result=await self.text.write(params,menu_guard=guard)
            elif action=='text.table':result=await self.text.table(params,menu_guard=guard)
            elif action=='text.vector':result=await self.text.vector(params,menu_guard=guard)
            else:result=await self.text.split(params,menu_guard=guard)
            if action in ('text.apply','text.write','text.table','text.vector') and result.get('result'):
                desktop=self.app.desktop
                original=desktop.menu_layer_context or {}
                current=desktop.layer_context
                written=json.loads(result['result'])
                if action=='text.table' or written.get('engine')=='native-pdf-vector':
                    result['continue_editing']=bool(epoch==desktop.menu_epoch and desktop.visible and fresh(current)
                        and written.get('verified') and current.get('layer_ids')==[written.get('layer_id')]
                        and all(original.get(k)==current.get(k) for k in ('session','document_id')))
                    if result['continue_editing']:desktop.menu_layer_context=dict(current)
            self.app.desktop.event({**event,'ok':True,'result':result})
            if action in ('text.split','text.apply','text.write','text.vector','text.table','layers.focus'):
                await self.app.broadcast({'type':'notice','scope':'operation','ok':True,'message':result['message']})
            return result
        except Exception as error:
            self.app.desktop.event({**event,'ok':False,'error':str(error)})
            if value['action'] in ('text.split','text.apply','text.write','text.vector','text.table','layers.focus'):
                await self.app.broadcast({'type':'notice','scope':'operation','ok':False,'message':str(error)})
            return {'message':str(error),'error':str(error)}
        finally:self.pending=False

    async def preview(self,args):
        epoch=args.get('menu_epoch');params=args.get('params',{})
        if (type(epoch) is not int or epoch<=0 or not isinstance(params,dict) or set(params)!={'request','search_id','index'} or
                type(params['request']) is not int or not 1<=params['request']<=2147483647):raise ValueError('无效的图层预览请求')
        event={'type':'menu.tools.preview.result','epoch':epoch,'request':params['request']}
        identity=(epoch,params['request'])
        if self.preview_request==identity:return {'error':'已忽略重复预览请求'}
        acquired=False
        try:
            if self.preview_request is not None or self.pending:raise RuntimeError('查找或预览读取中，本次未接受')
            if epoch!=self.epoch:raise RuntimeError('查找已关闭或重新打开')
            await asyncio.to_thread(self.app.desktop.require_menu_tools,epoch)
            require_available(self.app);self.preview_request=identity;acquired=True
            async def guard(_target=None):
                if epoch!=self.epoch:raise RuntimeError('查找已关闭或重新打开')
                await asyncio.to_thread(self.app.desktop.require_menu_tools,epoch)
            result=await self.search.preview(params,menu_guard=guard)
            if epoch!=self.epoch:return {'message':'已忽略旧预览'}
            await asyncio.to_thread(self.app.desktop.require_menu_tools,epoch)
            self.app.desktop.event({**event,'ok':True,'result':result})
            return {'message':'预览已读取'}
        except Exception as error:
            self.app.desktop.event({**event,'ok':False,'error':str(error)})
            return {'error':str(error)}
        finally:
            if acquired and self.preview_request==identity:self.preview_request=None
