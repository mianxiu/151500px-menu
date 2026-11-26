"""List options travel with the native text layer; PS text remains authoritative."""
import json
import re
import math
from functools import lru_cache
from xml.dom import Node, minidom
from .text_plan import list_marker_end, parameters

NAMESPACE='https://15100px.menu/ns/list/1.0/'
RDF='http://www.w3.org/1999/02/22-rdf-syntax-ns#'
KEYS={'kind','marker','custom_marker','numbering','start'}


@lru_cache(maxsize=256)
def symbol_glyphs(text):
    """GDI's missing-glyph flag checks the font, without system font linking."""
    import ctypes
    from ctypes import wintypes as w
    api=ctypes.WinDLL('gdi32',use_last_error=True)
    api.CreateCompatibleDC.argtypes=[w.HDC];api.CreateCompatibleDC.restype=w.HDC
    api.CreateFontW.argtypes=[ctypes.c_int]*5+[w.DWORD]*8+[w.LPCWSTR];api.CreateFontW.restype=w.HFONT
    api.SelectObject.argtypes=[w.HDC,w.HGDIOBJ];api.SelectObject.restype=w.HGDIOBJ
    api.GetTextFaceW.argtypes=[w.HDC,ctypes.c_int,w.LPWSTR];api.GetTextFaceW.restype=ctypes.c_int
    api.GetGlyphIndicesW.argtypes=[w.HDC,w.LPCWSTR,ctypes.c_int,ctypes.POINTER(w.WORD),w.DWORD];api.GetGlyphIndicesW.restype=w.DWORD
    api.DeleteObject.argtypes=[w.HGDIOBJ];api.DeleteDC.argtypes=[w.HDC]
    dc=api.CreateCompatibleDC(None);font=api.CreateFontW(24,0,0,0,400,0,0,0,1,0,0,0,0,'Segoe UI Symbol');old=None
    try:
        if not dc or not font:raise ValueError('无法核对列表符号字体')
        old=api.SelectObject(dc,font)
        if not old or old==ctypes.c_void_p(-1).value:raise ValueError('无法核对列表符号字体')
        name=ctypes.create_unicode_buffer(64);length=len(text.encode('utf-16-le'))//2;glyphs=(w.WORD*length)()
        return (api.GetTextFaceW(dc,len(name),name)>0 and name.value=='Segoe UI Symbol' and
                api.GetGlyphIndicesW(dc,text,length,glyphs,1)!=0xffffffff and all(g not in (0,0xffff) for g in glyphs))
    finally:
        if old and old!=ctypes.c_void_p(-1).value:api.SelectObject(dc,old)
        if font:api.DeleteObject(font)
        if dc:api.DeleteDC(dc)


def paragraphs(value):
    if not isinstance(value,list) or not 1<=len(value)<=1024:raise ValueError('列表段落信息无效')
    end=0
    for row in value:
        if (not isinstance(row,dict) or set(row)!={'from','to','start','first'} or type(row['from']) is not int or row['from']!=end or
                type(row['to']) is not int or not end<row['to']<=20001 or any(type(row[k]) not in (int,float) or not math.isfinite(row[k]) or abs(row[k])>1296 for k in ('start','first'))):
            raise ValueError('列表段落信息无效')
        end=row['to']
    return value


def layout(raw):
    _,_,found=packet(raw,field='layout')
    if not found:return None
    desc,node=found[0]
    if desc.getAttributeNS(RDF,'about') or node.nodeType!=Node.ELEMENT_NODE or any(child.nodeType not in (Node.TEXT_NODE,Node.CDATA_SECTION_NODE) for child in node.childNodes):raise ValueError('列表段落标记无效')
    try:value=json.loads(''.join(child.data for child in node.childNodes))
    except (ValueError,TypeError):raise ValueError('列表段落标记无效') from None
    if not isinstance(value,dict) or set(value)!={'schema','paragraphs'} or type(value['schema']) is not int or value['schema']!=1:raise ValueError('列表段落版本不受支持')
    return paragraphs(value['paragraphs'])


def format_parameters(value):
    if (not isinstance(value,dict) or set(value) not in ({'enabled','paragraphs'},{'enabled','paragraphs','marker_font'}) or type(value['enabled']) is not bool or
            'marker_font' in value and (not value['enabled'] or value['marker_font']!='SegoeUISymbol')):raise ValueError('列表排版参数无效')
    rows=value['paragraphs']
    if not isinstance(rows,list):raise ValueError('列表段落信息无效')
    paragraphs([{k:v for k,v in row.items() if k!='prefix'} if isinstance(row,dict) else row for row in rows])
    if any(set(row)!={'from','to','start','first','prefix'} or type(row['prefix']) is not int or not row['from']<=row['prefix']<row['to'] for row in rows):raise ValueError('列表前缀范围无效')
    return value


def formatting(value,origins,current,raw,enabled):
    base=layout(raw) or paragraphs(current);rows=[];start=0;symbols=set()
    for line in value.split('\r'):
        end=start+len(line.encode('utf-16-le'))//2+1
        origin=origins[start];source=next((row for row in base if row['from']<=origin<row['to']),base[-1])
        marker_start=len(line)-len(line.lstrip())
        marker_end=list_marker_end(line,marker_start,len(line)) if enabled else marker_start
        prefix=start
        if marker_end>marker_start:
            prefix+=len(line[:marker_end].encode('utf-16-le'))//2
            symbol=line[marker_start:marker_end-1]
            if len(symbol)==1 and ord(symbol)>127:symbols.add(symbol)
        rows.append({'from':start,'to':end,'start':source['start'],'first':source['first'],'prefix':prefix});start=end
    params=format_parameters({'enabled':enabled,'paragraphs':rows})
    if symbols:
        if not symbol_glyphs(''.join(sorted(symbols))):raise ValueError('列表符号字体不支持所选字符，请换一个项目符号；未写入PS')
        params['marker_font']='SegoeUISymbol'
    payload={'schema':1,'paragraphs':[{k:v for k,v in row.items() if k!='prefix'} for row in rows]} if enabled else None
    return params,_write(raw,'layout',payload)


def options(value):
    if value is None:return None
    if not isinstance(value,dict) or set(value)!=KEYS:raise ValueError('无效的列表标记')
    result=parameters({**value,'revision':'list','draft':'','operation':'list'})
    if result['kind']=='remove':raise ValueError('无效的列表标记')
    return {key:result[key] for key in KEYS}


def packet(raw,*,namespace=NAMESPACE,field='options',limit=131072):
    if not isinstance(raw,str) or len(raw)>limit or '\0' in raw or re.search(r'<!\s*(DOCTYPE|ENTITY)\b',raw,re.I):raise ValueError('图层元数据过大或包含不支持的XML声明')
    try:doc=minidom.parseString(raw) if raw.strip() else minidom.parseString('<x:xmpmeta xmlns:x="adobe:ns:meta/"><rdf:RDF xmlns:rdf="'+RDF+'"/></x:xmpmeta>')
    except Exception:raise ValueError('图层元数据XML无效，未改动列表标记') from None
    stack=[(doc,0)];count=0
    while stack:
        node,depth=stack.pop();count+=1
        if count>4096 or depth>64:raise ValueError('图层元数据结构超出上限')
        stack.extend((child,depth+1) for child in node.childNodes)
    roots=doc.getElementsByTagNameNS(RDF,'RDF')
    if len(roots)!=1:raise ValueError('图层元数据缺少唯一RDF容器')
    found=[]
    for desc in roots[0].childNodes:
        if desc.nodeType!=Node.ELEMENT_NODE or (desc.namespaceURI,desc.localName)!=(RDF,'Description'):continue
        attr=desc.getAttributeNodeNS(namespace,field)
        if attr:found.append((desc,attr))
        for child in desc.childNodes:
            if child.nodeType==Node.ELEMENT_NODE and (child.namespaceURI,child.localName)==(namespace,field):found.append((desc,child))
    if len(found)>1:raise ValueError('图层包含冲突的列表标记')
    return doc,roots[0],found


def read(raw):
    _,_,found=packet(raw)
    if not found:return None
    desc,node=found[0]
    if desc.getAttributeNS(RDF,'about'):raise ValueError('列表标记目标无效')
    if node.nodeType==Node.ATTRIBUTE_NODE:text=node.value
    else:
        if any(child.nodeType not in (Node.TEXT_NODE,Node.CDATA_SECTION_NODE) for child in node.childNodes):raise ValueError('列表标记结构无效')
        text=''.join(child.data for child in node.childNodes)
    try:value=json.loads(text)
    except (TypeError,ValueError):raise ValueError('列表标记JSON无效') from None
    if not isinstance(value,dict) or set(value)!={'schema','options'} or type(value['schema']) is not int or value['schema']!=1:raise ValueError('列表标记版本不受支持')
    return options(value['options'])


def write(raw,value):
    value=options(value)
    if read(raw)==value:return raw
    return _write(raw,'options',{'schema':1,'options':value} if value is not None else None)


def _write(raw,field,value):
    doc,root,found=packet(raw,field=field)
    for desc,node in found:
        if node.nodeType==Node.ATTRIBUTE_NODE:desc.removeAttributeNode(node)
        else:desc.removeChild(node)
    if value is not None:
        desc=doc.createElementNS(RDF,'rdf:Description');desc.setAttribute('xmlns:rdf',RDF);desc.setAttributeNS(RDF,'rdf:about','')
        node=doc.createElementNS(NAMESPACE,'pxmenu:'+field);node.setAttribute('xmlns:pxmenu',NAMESPACE)
        node.appendChild(doc.createTextNode(json.dumps(value,ensure_ascii=False,separators=(',',':'))))
        desc.appendChild(node);root.appendChild(desc)
    result=doc.toxml()
    if len(result)>131072:raise ValueError('图层元数据超出上限')
    return result
