"""Native PDF generation under the durable UXP reservation, never a retry path."""
from contextlib import ExitStack
import hashlib
import json
from io import BytesIO
import logging
import secrets
from threading import Event, Thread
import time
from pypdf import PdfReader, PdfWriter
from pypdf.errors import PyPdfError
from pypdf.generic import NameObject, RectangleObject
from . import text_object
from .paths import ROOT
from .settings import atomic_json


def crop_pdf(source,bounds,canvas):
    reader=PdfReader(BytesIO(source),strict=True)
    if reader.is_encrypted or len(reader.pages)!=1:
        raise ValueError('需要单页未加密的 Photoshop PDF')
    writer=PdfWriter(clone_from=reader)
    page=writer.pages[0]
    media=page.mediabox
    if page.rotation or float(page.user_unit)!=1:
        raise ValueError('Photoshop PDF 页面坐标无法核对')
    width,height=float(media.width),float(media.height)
    scale=72/canvas['resolution']
    if (abs(width-canvas['width']*scale)>.02 or abs(height-canvas['height']*scale)>.02 or
            any(abs(a-b)>.02 for a,b in zip(map(float,page.cropbox),map(float,media)))):
        raise ValueError('Photoshop PDF 尺寸与排版画布不一致')
    left,top,right,bottom=bounds
    horizontal,vertical=width/canvas['width'],height/canvas['height']
    x=float(media.left)+left*horizontal
    y=float(media.top)-bottom*vertical
    page.add_transformation((1,0,0,1,-x,-y))
    rect=(0,0,(right-left)*horizontal,(bottom-top)*vertical)
    for box in ('/MediaBox','/CropBox','/ArtBox','/BleedBox','/TrimBox'):
        page[NameObject(box)]=RectangleObject(rect)
    output=BytesIO()
    writer.write(output)
    data=output.getvalue()
    if not 100<=len(data)<=16777216:raise ValueError('紧凑文字 PDF 超出允许的大小')
    return data


def locked_pdf_digest(path, handles, expected_size=None):
    from win32con import FILE_SHARE_READ, GENERIC_READ, OPEN_EXISTING
    from win32file import CreateFile, GetFileSize, ReadFile

    # Both Photoshop and the backend can read the staged file during placement.
    # Keep it protected against writes or replacement until native readback ends.
    handle=CreateFile(str(path),GENERIC_READ,FILE_SHARE_READ,None,OPEN_EXISTING,0,None)
    handles.callback(handle.Close)
    size=GetFileSize(handle)
    if not 100<=size<=16777216 or expected_size is not None and size!=expected_size:
        raise ValueError('Photoshop PDF 导出大小不一致')
    _,data=ReadFile(handle,size)
    if len(data)!=size or not data.startswith(b'%PDF-'):
        raise ValueError('Photoshop PDF 导出格式不一致')
    return hashlib.sha256(data).hexdigest()


def _hash_placed_pdf(folder, identity, nonce, stop):
    # Photoshop exports the placed content inside its existing undo step.
    # The COM caller is blocked until that step returns, so hash on a separate
    # local thread and hold the read-only file lease until COM exits.
    request_id=identity['id']
    request=folder/('text-object-hash-request-'+request_id+'.json')
    response=folder/('text-object-hash-response-'+request_id+'.txt')
    try:
        while not stop.wait(.01):
            if not request.is_file():continue
            if request.stat().st_size>4096 or response.exists():return
            value=json.loads(request.read_text('utf-8-sig'))
            expected={**identity,'schema':1,'nonce':nonce}
            size=value.get('size')
            if (set(value)!=set(expected)|{'size'} or any(value[k]!=v for k,v in expected.items()) or
                    type(size) is not int or not 100<=size<=16777216):
                return
            with ExitStack() as handles:
                digest=locked_pdf_digest(folder/('text-object-render-'+request_id+'.pdf'),handles,size)
                temporary=response.with_suffix('.tmp')
                with temporary.open('x',encoding='ascii') as stream:
                    stream.write(f'{request_id}|{nonce}|{size}|{digest}')
                temporary.replace(response)
                stop.wait()
            return
    except Exception as error:
        logging.warning('Placed PDF digest worker request=%s error=%s',request_id,type(error).__name__)


def invoke(application,folder,request,prepared,model,xmp):
    from pywintypes import error as WindowsFileError

    if (request.get('command')!='text.object' or prepared.get('phase')!='prepared' or
            any(prepared.get(k)!=request.get(k) for k in ('id','session','operation','token'))):
        raise ValueError('文字对象执行身份不一致')
    text_object.source(model)
    if text_object.read(xmp)!=model:raise ValueError('文字对象源与显示草稿不一致')
    value={**request,'folder':str(folder),'prepared':prepared['prepared'],'source':model,'xmp':xmp}
    source=(ROOT/'jsx/text_object.jsx').read_text('utf-8').replace('__PS_MENU_TEXT_HASH_HELPER__',(ROOT/'jsx/text_object_sha256.jsx').read_text('utf-8'))
    def run(payload):
        return application.DoJavaScript(source.replace('__PS_MENU_TEXT_OBJECT_REQUEST__',json.dumps(payload,ensure_ascii=True,allow_nan=False)))
    if model['schema']!=1 or request.get('object_proof') is True:return run(value)
    receipt=run({**value,'render_stage':'prepare'})
    rendered=json.loads(receipt)
    identity={key:request[key] for key in ('id','session','operation','token')}
    if rendered.get('schema')!=1 or any(rendered.get(key)!=expected for key,expected in identity.items()):
        raise RuntimeError('导出的 Photoshop PDF 执行身份不一致')
    if rendered.get('phase')=='failed':return receipt
    if (rendered.get('phase')!='rendered' or not isinstance(rendered.get('stamp'),str) or
            not 0<len(rendered['stamp'])<=3*1024*1024):
        raise RuntimeError('Photoshop PDF 导出状态未确认')
    start=time.monotonic()
    pdf=folder/('text-object-'+request['id']+'.pdf')
    original_digest=value['prepared'].get('before_render')
    with ExitStack() as handles:
        try:
            if original_digest:
                original=folder/('text-object-before-'+request['id']+'.pdf')
                digest=locked_pdf_digest(original,handles)
                if digest!=original_digest:
                    raise ValueError('矢量内容已在外部改变，原对象未更新')
                rendered['original_sha256']=digest
            size=rendered.get('pdf_size')
            if type(size) is not int or not 100<=size<=16777216 or pdf.stat().st_size!=size:
                raise ValueError('Photoshop PDF 导出大小不一致')
            if rendered['bounds'] is not None:
                bounds=text_object.page_bounds(rendered['bounds'],model['canvas'])
                compact=crop_pdf(pdf.read_bytes(),bounds,model['canvas'])
                temporary=pdf.with_suffix('.crop.tmp')
                with temporary.open('xb') as stream:stream.write(compact)
                temporary.replace(pdf)
                rendered['pdf_size']=len(compact)
            rendered['pdf_sha256']=locked_pdf_digest(pdf,handles,rendered['pdf_size'])
        except FileExistsError:
            raise
        except (OSError,ValueError,PyPdfError,WindowsFileError) as error:
            rejected={**identity,'schema':1,'phase':'failed','error':str(error),'not_executed':True,'result_unknown':False}
            atomic_json(folder/('legacy-result-'+request['id']+'.json'),rejected)
            return json.dumps(rejected)
        rendered['compact_ms']=round((time.monotonic()-start)*1000)
        nonce=secrets.token_hex(16)
        stop=Event()
        worker=Thread(target=_hash_placed_pdf,args=(folder,identity,nonce,stop),name='photoshop-placed-pdf-digest',daemon=True)
        worker.start()
        try:return run({**value,'render_stage':'place','rendered':rendered,'native_hash_nonce':nonce})
        finally:
            stop.set()
            worker.join()
