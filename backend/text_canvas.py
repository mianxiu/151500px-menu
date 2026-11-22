"""Bounded, read-only original canvas from the optional Photoshop Generator plug-in."""
import asyncio
import json
import mmap
import os
import secrets
import time
from pathlib import Path

from .settings import atomic_json


def connection(runtime, hwnd):
    if os.name!='nt' or not hwnd:return None
    import win32api, win32process
    folder = Path(runtime) / 'text-canvas'
    try:
        ready = json.loads((folder/'ready.json').read_text('utf-8'))
        token = (folder/'pairing').read_text('ascii')
        if (ready['version'] not in (1,2,3) or ready['token'] != token or len(token) != 32
                or win32process.GetWindowThreadProcessId(hwnd)[1] != ready['photoshopPid']):
            return None
        def executable(pid):
            handle = win32api.OpenProcess(0x410,False,pid)
            try:return Path(win32process.GetModuleFileNameEx(handle,0))
            finally:handle.Close()
        host=executable(ready['photoshopPid'])
        if host.name.lower()=='photoshop.exe' and executable(ready['pid'])==host.with_name('node.exe'):
            return ready
    except (OSError,ValueError,TypeError,KeyError,win32api.error):
        pass
    return None


async def read(runtime, hwnd, target, *, background=False):
    ready = connection(runtime,hwnd)
    if not ready:raise RuntimeError('当前 Photoshop 尚未连接画布读取组件')
    if background and ready['version']<2:raise RuntimeError('请更新画布读取组件后重开 Photoshop')
    folder = Path(runtime)/'text-canvas'
    identity = secrets.token_hex(16)
    request = folder/('read-'+identity+'.json')
    result = folder/(identity+'.json')
    pixels = folder/(identity+'.rgba')
    atomic_json(request,{'session':ready['session'],'token':ready['token'],'id':identity,
                         'doc':target['document_id'],'layer':target['active_layer_id'],'background':background,'expires':int(time.time()*1000)+12000})
    deadline=time.monotonic()+13
    while not result.is_file():
        if time.monotonic()>=deadline:raise RuntimeError('画布读取未获回执，已保留原读取记录')
        await asyncio.sleep(.015)
    data=json.loads(result.read_text('utf-8'))
    if data.get('session')!=ready['session'] or data.get('id')!=identity:
        raise RuntimeError('画布回执身份不符')
    try:
        if data.get('error'):raise RuntimeError(data['error'])
        w,h=data.get('width'),data.get('height')
        limit=2048 if background else 640
        if (data.get('doc')!=target['document_id'] or data.get('layer')!=target['active_layer_id']
                or data.get('background',False) is not background
                or type(w) is not int or type(h) is not int or not 1<=w<=limit or not 1<=h<=limit
                or pixels.stat().st_size!=w*h*4 or connection(runtime,hwnd)!=ready):
            raise RuntimeError('画布会话、目标或尺寸已改变')
        image=pixels.read_bytes()
        if len(image)!=w*h*4:raise RuntimeError('画布像素数据不完整')
        memory=mmap.mmap(-1,len(image),tagname='Local\\151500px-menu-canvas-'+identity)
        try:memory.write(image)
        except BaseException:memory.close();raise
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
