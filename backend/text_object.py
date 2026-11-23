"""Editable source carried in the vector object's layer XMP, without executable data."""
import hashlib
import json
import math
from xml.dom import Node
from .text_list import packet,RDF

NAMESPACE='https://15100px.menu/ns/text-object/1.0/'
MAX_SOURCE=524288
MAX_XMP=2097152


def table_cells(rows):
    if not isinstance(rows,list) or not 1<=len(rows)<=12 or not isinstance(rows[0],list) or not 1<=len(rows[0])<=8:
        raise ValueError('表格支持 1–12 行、1–8 列')
    for row in rows:
        if not isinstance(row,list) or len(row)!=len(rows[0]):raise ValueError('表格各行列数必须相同')
        for cell in row:
            if not isinstance(cell,str) or len(cell)>128 or any(ord(c)<32 or 0xd800<=ord(c)<=0xdfff for c in cell):
                raise ValueError('单元格支持最多 128 个单行字符')
    return rows


def source(value):
    if (not isinstance(value,dict) or type(value.get('schema')) is not int or
            not (value['schema']==1 and set(value)=={'schema','canvas','layer'} or
                 value['schema']==2 and set(value)=={'schema','canvas','layer','renderer'} and value['renderer']=='list-v1' or
                 value['schema']==3 and set(value)=={'schema','canvas','table'})):
        raise ValueError('不支持的文字对象源版本')
    canvas=value['canvas']
    if not isinstance(canvas,dict) or set(canvas)!={'width','height','resolution','mode','depth','profile'}:
        raise ValueError('文字对象画布信息无效')
    for key,maximum in [('width',30000),('height',30000),('resolution',9600)]:
        n=canvas[key]
        if type(n) not in (int,float) or not math.isfinite(n) or not 0<n<=maximum:raise ValueError('文字对象画布范围无效')
    if canvas['width']*canvas['height']>64000000 or canvas['mode'] not in ('RGB','CMYK','LAB','GRAYSCALE') or type(canvas['depth']) is not int or canvas['depth'] not in (8,16):
        raise ValueError('文字对象画布范围无效')
    if not isinstance(canvas['profile'],str) or len(canvas['profile'])>1000 or '\0' in canvas['profile']:raise ValueError('文字对象色彩配置无效')
    if value['schema']==3:
        table_cells(value['table'])
        return json.dumps(value,ensure_ascii=False,sort_keys=True,separators=(',',':'),allow_nan=False).encode('utf-8')
    layer=value['layer']
    if not isinstance(layer,dict) or set(layer)!={'name','text','xmp'} or not isinstance(layer['name'],str) or len(layer['name'])>2000 or '\0' in layer['name']:
        raise ValueError('文字对象图层信息无效')
    if not isinstance(layer['xmp'],str) or len(layer['xmp'])>131072:raise ValueError('文字对象图层元数据无效')
    packet(layer['xmp'])
    raw=layer['text']
    if not isinstance(raw,dict) or raw.get('_obj')!='textLayer' or not isinstance(raw.get('textKey'),str):raise ValueError('文字对象正文无效')
    try:length=len(raw['textKey'].encode('utf-16-le'))//2
    except UnicodeError:raise ValueError('文字对象Unicode无效') from None
    if not 1<=length<=20000 or '\0' in raw['textKey'] or '\n' in raw['textKey']:raise ValueError('文字对象正文范围无效')
    for key,field in [('textStyleRange','textStyle'),('paragraphStyleRange','paragraphStyle')]:
        ranges=raw.get(key)
        if not isinstance(ranges,list) or not 1<=len(ranges)<=1024:raise ValueError('文字对象样式范围不完整')
        end=0
        for row in ranges:
            if (not isinstance(row,dict) or type(row.get('from')) is not int or type(row.get('to')) is not int or row['from']!=end or not end<row['to']<=length+1 or
                    not isinstance(row.get(field),dict)):raise ValueError('文字对象样式范围不完整')
            end=row['to']
        if end<length:raise ValueError('文字对象样式范围不完整')
    kerns=raw.get('kerningRange',[])
    if not isinstance(kerns,list) or len(kerns)>length:raise ValueError('文字对象字偶范围无效')
    end=0
    for row in kerns:
        if (not isinstance(row,dict) or row.get('_obj')!='kerningRange' or
                any(type(row.get(key)) is not int for key in ('from','to','kerning')) or
                not end<=row['from']<row['to']<=length):
            raise ValueError('文字对象字偶范围无效')
        end=row['to']
    # Preserve unknown style fields, but never accept object prototypes, paths or executable objects.
    stack=[(value,0)];count=0
    while stack:
        item,depth=stack.pop();count+=1
        if depth>32 or count>100000:raise ValueError('文字对象源结构超出上限')
        if isinstance(item,dict):
            if any(not isinstance(k,str) or k in ('__proto__','prototype','constructor','_path','_target') for k in item):raise ValueError('文字对象源包含无效字段')
            stack.extend((v,depth+1) for v in item.values())
        elif isinstance(item,list):stack.extend((v,depth+1) for v in item)
        elif isinstance(item,str):
            if '\0' in item:raise ValueError('文字对象源包含空字符')
        elif item is not None and type(item) not in (bool,int,float):raise ValueError('文字对象源包含无效值')
        elif type(item) in (int,float) and not math.isfinite(item):raise ValueError('文字对象源包含无效数值')
    try:encoded=json.dumps(value,ensure_ascii=False,sort_keys=True,separators=(',',':'),allow_nan=False).encode('utf-8')
    except (ValueError,UnicodeError):raise ValueError('文字对象源编码无效') from None
    if len(encoded)>MAX_SOURCE:raise ValueError('文字对象源超出上限')
    return encoded


def write(raw,value):
    payload=source(value)
    doc,root,found=packet(raw,namespace=NAMESPACE,field='source',limit=MAX_XMP)
    if found:read(raw)
    for desc,node in found:
        if node.nodeType==Node.ATTRIBUTE_NODE:desc.removeAttributeNode(node)
        else:desc.removeChild(node)
    desc=doc.createElementNS(RDF,'rdf:Description');desc.setAttribute('xmlns:rdf',RDF);desc.setAttributeNS(RDF,'rdf:about','')
    node=doc.createElementNS(NAMESPACE,'pxText:source');node.setAttribute('xmlns:pxText',NAMESPACE)
    envelope={'sha256':hashlib.sha256(payload).hexdigest(),'source':json.loads(payload)}
    node.appendChild(doc.createTextNode(json.dumps(envelope,ensure_ascii=False,separators=(',',':'))))
    desc.appendChild(node);root.appendChild(desc)
    result=doc.toxml()
    if len(result)>MAX_XMP:raise ValueError('文字对象元数据超出上限')
    return result


def read(raw):
    _,_,found=packet(raw,namespace=NAMESPACE,field='source',limit=MAX_XMP)
    if not found:raise ValueError('该文件没有本工具的可编辑文字源')
    desc,node=found[0]
    if desc.getAttributeNS(RDF,'about'):raise ValueError('文字对象源目标无效')
    if node.nodeType==Node.ATTRIBUTE_NODE:text=node.value
    else:
        if any(child.nodeType not in (Node.TEXT_NODE,Node.CDATA_SECTION_NODE) for child in node.childNodes):raise ValueError('文字对象源结构无效')
        text=''.join(child.data for child in node.childNodes)
    def unique(pairs):
        result={}
        for key,value in pairs:
            if key in result:raise ValueError('文字对象源包含重复字段')
            result[key]=value
        return result
    try:value=json.loads(text,object_pairs_hook=unique)
    except (ValueError,RecursionError):raise ValueError('文字对象源JSON无效') from None
    if not isinstance(value,dict) or set(value)!={'sha256','source'} or not isinstance(value['sha256'],str):raise ValueError('文字对象源校验无效')
    payload=source(value['source'])
    if hashlib.sha256(payload).hexdigest()!=value['sha256']:raise ValueError('文字对象源已损坏')
    return json.loads(payload)


def render_hash(raw):
    _,_,found=packet(raw,namespace=NAMESPACE,field='render',limit=MAX_XMP)
    if not found:raise ValueError('文字对象缺少矢量显示校验，未接受更新')
    desc,node=found[0]
    if desc.getAttributeNS(RDF,'about'):raise ValueError('文字对象显示校验目标无效')
    if node.nodeType==Node.ATTRIBUTE_NODE:value=node.value
    else:
        if any(child.nodeType not in (Node.TEXT_NODE,Node.CDATA_SECTION_NODE) for child in node.childNodes):raise ValueError('文字对象显示校验结构无效')
        value=''.join(child.data for child in node.childNodes)
    import re
    if not re.fullmatch('[0-9a-f]{64}',value):raise ValueError('文字对象显示校验无效')
    return value


def font_fallbacks(raw,text):
    _,_,found=packet(raw,namespace=NAMESPACE,field='fontFallbacks',limit=MAX_XMP)
    if not found:return []
    desc,node=found[0]
    if desc.getAttributeNS(RDF,'about'):raise ValueError('文字对象回退字体目标无效')
    if node.nodeType==Node.ATTRIBUTE_NODE:value=node.value
    else:
        if any(child.nodeType not in (Node.TEXT_NODE,Node.CDATA_SECTION_NODE) for child in node.childNodes):
            raise ValueError('文字对象回退字体记录无效')
        value=''.join(child.data for child in node.childNodes)
    try:rows=json.loads(value)
    except (ValueError,RecursionError):raise ValueError('文字对象回退字体记录无效') from None
    if not isinstance(rows,list) or len(rows)>1024:raise ValueError('文字对象回退字体范围无效')
    length=len(text.encode('utf-16-le'))//2
    end=0
    for row in rows:
        if (not isinstance(row,list) or len(row)!=3 or any(type(offset) is not int for offset in row[:2])
                or not end<=row[0]<row[1]<=length or not isinstance(row[2],str)
                or not 1<=len(row[2])<=255 or any(ord(ch)<32 for ch in row[2])):
            raise ValueError('文字对象回退字体范围无效')
        end=row[1]
    return rows
