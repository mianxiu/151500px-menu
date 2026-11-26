"""Bounded, read-only original canvas from the optional Photoshop Generator plug-in."""
import asyncio
import json
import logging
import mmap
import os
import secrets
import time
from pathlib import Path

from .paths import ROOT
from .settings import atomic_json


class CanvasUnavailable(RuntimeError):
    def __init__(self, message, *, state='failed', history=None):
        super().__init__(message)
        self.state=state
        self.history=history


class CanvasTargetChanged(CanvasUnavailable):
    def __init__(self):
        super().__init__('画布目标或历史已改变；已保留草稿，请重新读取文字', state='stale')


def connection(runtime, hwnd):
    if os.name!='nt' or not hwnd:return None
    import win32api, win32process
    folder = Path(runtime) / 'text-canvas'
    try:
        ready = json.loads((folder/'ready.json').read_text('utf-8'))
        token = (folder/'pairing').read_text('ascii')
        if (ready['version'] not in (1,2,3,4) or ready['token'] != token or len(token) != 32
                or win32process.GetWindowThreadProcessId(hwnd)[1] != ready['photoshopPid']):
            return None
        def executable(pid):
            # Windows 10 and later need only query access when no module handle is supplied.
            handle = win32api.OpenProcess(0x1000,False,pid)
            try:return Path(win32process.GetModuleFileNameEx(handle,0))
            finally:handle.Close()
        host=executable(ready['photoshopPid'])
        if host.name.lower()=='photoshop.exe' and executable(ready['pid'])==host.with_name('node.exe'):
            return ready
    except (OSError,ValueError,TypeError,KeyError,win32api.error):
        pass
    return None


def _mapping(identity,image):
    memory=mmap.mmap(-1,len(image),tagname='Local\\151500px-menu-canvas-'+identity)
    try:memory.write(image)
    except BaseException:memory.close();raise
    return memory


def _legacy_text_frame(geometry,width,height):
    kind,*fields=geometry.split('|')
    if len(fields)!=8:return None
    try:x,y,w,h,em,r,g,b=map(float,fields)
    except ValueError:return None
    import math
    if not all(map(math.isfinite,(x,y,w,h,em,r,g,b))) or em<=0:return None
    if kind=='box' and w>0 and h>0:
        box=[x/width,y/height,(x+w)/width,(y+h)/height]
    elif kind=='point':
        box=[x/width,(y-em)/height,(max(x,width)+em)/width,max(y+em,height)/height]
    else:return None
    frame={'box':box,'wrap':kind=='box','em':em/height,'color':[max(0,min(255,int(c+.5))) for c in (r,g,b)]}
    return frame if valid_text_frame(frame) else None


async def _legacy_empty_background(bridge,identity,target,data):
    history=data.get('history')
    trace=data.get('trace',{})
    width,height=trace.get('source_width'),trace.get('source_height')
    if (type(history) is not int or data.get('doc')!=target['document_id'] or data.get('layer')!=target['active_layer_id'] or
            type(width) is not int or type(height) is not int or not (width>0 and height>0 and width*height<=100000000)):
        return None
    proof={'doc':target['document_id'],'layer':target['active_layer_id'],'history':history,'width':width,'height':height}
    script=(ROOT/'jsx/text_canvas_empty.jsx').read_text('utf-8')
    geometry=(ROOT/'generator/text-canvas/text-geometry.jsx').read_text('utf-8').strip()
    script=script.replace('__CANVAS_TARGET__',json.dumps(proof)).replace('__TEXT_GEOMETRY__',geometry)
    def inspect():
        if bridge.busy:return 'unsupported'
        return str(bridge._application().DoJavaScript(script))
    try:value=await asyncio.get_running_loop().run_in_executor(bridge.executor,inspect)
    except Exception:
        logging.debug('Legacy empty text canvas could not be verified',exc_info=True)
        return None
    if value=='stale':raise CanvasTargetChanged()
    if not value.startswith('empty|'):return None
    scale=min(1,2048/max(width,height))
    w,h=(max(1,int(size*scale+.5)) for size in (width,height))
    image=_mapping(identity,bytes(w*h*4))
    return image,{'mapping':identity,'width':w,'height':h,'history':history,'background':True,
                  'box':None,'text_frame':_legacy_text_frame(value[6:],width,height)}


async def read(runtime, hwnd, target, *, background=False, trace=None, legacy_bridge=None):
    ready = connection(runtime,hwnd)
    if not ready:raise CanvasUnavailable('当前 Photoshop 尚未连接画布读取组件',state='limited')
    if background and ready['version']<2:
        raise CanvasUnavailable('请更新画布读取组件后重开 Photoshop',state='limited')
    folder = Path(runtime)/'text-canvas'
    identity = secrets.token_hex(16)
    request = folder/('read-'+identity+'.json')
    result = folder/(identity+'.json')
    pixels = folder/(identity+'.rgba')
    atomic_json(request,{'session':ready['session'],'token':ready['token'],'id':identity,
                         'doc':target['document_id'],'layer':target['active_layer_id'],'background':background,'expires':int(time.time()*1000)+12000})
    if trace:logging.info('Typeset epoch=%s request=%s generator request=%s stage=published',*trace,identity)
    started=time.monotonic()
    deadline=started+13
    while not result.is_file():
        if time.monotonic()>=deadline:
            atomic_json(folder/(identity+'-unknown.json'),{'id':identity,'session':ready['session'],'phase':'unknown',
                                                           'elapsed_ms':round((time.monotonic()-started)*1000)})
            logging.info('Typeset generator request=%s stage=receipt outcome=unknown elapsed_ms=%d',identity,(time.monotonic()-started)*1000)
            raise CanvasUnavailable('背景读取未获回执，已保留原读取记录')
        await asyncio.sleep(.015)
    data=json.loads(result.read_text('utf-8'))
    if data.get('session')!=ready['session'] or data.get('id')!=identity:
        raise RuntimeError('画布回执身份不符')
    try:
        if connection(runtime,hwnd)!=ready:raise CanvasTargetChanged()
        trace=data.get('trace')
        if isinstance(trace,dict):
            metrics={key:value for key,value in trace.items() if key in
                     ('source_width','source_height','layers','identity_ms','tree_ms','geometry_ms','pixels_ms','publish_ms','total_ms')
                     and type(value) in (int,float) and 0<=value<=1000000}
        else:metrics={}
        logging.info('Typeset generator request=%s stage=receipt elapsed_ms=%d metrics=%s',identity,(time.monotonic()-started)*1000,metrics)
        if data.get('error'):
            if data.get('error_code')=='target':raise CanvasTargetChanged()
            if (background and legacy_bridge is not None and ready['version']<4 and
                    data['error']=='Error: 画布边界属性无效'):
                transparent=await _legacy_empty_background(legacy_bridge,identity,target,{**data,'trace':metrics})
                if transparent:
                    if connection(runtime,hwnd)!=ready:
                        transparent[0].close()
                        raise CanvasTargetChanged()
                    logging.info('Typeset generator request=%s stage=empty-background-verified',identity)
                    return transparent
            history=data.get('history') if data.get('doc')==target['document_id'] and data.get('layer')==target['active_layer_id'] else None
            raise CanvasUnavailable(data['error'],state='limited' if data.get('error_code')=='limited' else 'failed',history=history)
        w,h=data.get('width'),data.get('height')
        limit=2048 if background else 640
        if (data.get('doc')!=target['document_id'] or data.get('layer')!=target['active_layer_id']
                or data.get('background',False) is not background
                or type(w) is not int or type(h) is not int or not 1<=w<=limit or not 1<=h<=limit
                or pixels.stat().st_size!=w*h*4 or type(data.get('history')) is not int):
            raise RuntimeError('画布会话、目标或尺寸已改变')
        image=pixels.read_bytes()
        if len(image)!=w*h*4:raise RuntimeError('画布像素数据不完整')
        memory=_mapping(identity,image)
        box=data.get('box') if background else None
        frame=data.get('text_frame') if background else None
        return memory,{'mapping':identity,'width':w,'height':h,'history':data['history'],'background':background,
                       'box':box if valid_box(box) else None,'text_frame':frame if valid_text_frame(frame) else None}
    finally:
        for path in (pixels,request,result):path.unlink(missing_ok=True)


def valid_box(box):
    return (isinstance(box,list) and len(box)==4 and all(type(i) in (int,float) and -64<i<64 for i in box)
            and box[0]<box[2] and box[1]<box[3])


def valid_text_frame(frame):
    return (isinstance(frame,dict) and valid_box(frame.get('box')) and type(frame.get('wrap')) is bool
            and type(frame.get('em')) in (int,float) and 0<frame['em']<8
            and isinstance(frame.get('color'),list) and len(frame['color'])==3
            and all(type(channel) is int and 0<=channel<=255 for channel in frame['color']))


def staged_frame(folder,response,target,*,scale=1,isolated=False):
    import re
    identity=response['id']
    frame=response['result']
    if (not re.fullmatch('[0-9a-f]{32}',identity) or frame.get('frame_id')!=identity
            or response['session']!=target['session'] or frame.get('document_id')!=target['document_id']
            or frame.get('layer_id')!=target['active_layer_id'] or frame.get('restored') is not True
            or frame.get('scale',1)!=scale or frame.get('isolated',False) is not isolated):
        raise RuntimeError('草稿图像身份或恢复状态不符')
    path=folder/('text-proof-'+identity+'.png')
    try:
        width,height=frame['width'],frame['height']
        size=path.stat().st_size
        if (type(width) is not int or type(height) is not int or not 0<width<=8192 or not 0<height<=8192
                or width*height>16777216 or not 8<=size<=80*1024*1024):
            raise RuntimeError('草稿图像超出读取范围')
        with path.open('rb') as file:
            if file.read(8)!=b'\x89PNG\r\n\x1a\n':raise RuntimeError('草稿像素格式不符')
            file.seek(0)
            memory=mmap.mmap(-1,size,tagname='Local\\151500px-menu-canvas-'+identity)
            try:
                import shutil
                shutil.copyfileobj(file,memory)
                if memory.tell()!=size:raise RuntimeError('草稿像素读取不完整')
            except BaseException:memory.close();raise
        return memory,{'mapping':identity,'encoding':'png','bytes':size,'width':width,'height':height,'history':frame['history'],'scale':scale,
                       **({'isolated':True} if isolated else {})}
    finally:path.unlink(missing_ok=True)
