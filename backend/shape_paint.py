"""Strict, bounded paint values shared by native metadata and API requests."""
import math

def number(value,low,high):
    return type(value) in (int,float) and math.isfinite(value) and low<=value<=high

def color(value):
    if 'native' in value:
        native=value['native']
        if not isinstance(native,dict) or set(native)!={'model','values'}:raise ValueError('无效原生颜色')
        limits={'cmyk':[(0,100)]*4,'lab':[(0,100),(-128,127),(-128,127)],'gray':[(0,100)]}.get(native['model']) if isinstance(native['model'],str) else None
        values=native['values']
        if not limits or not isinstance(values,list) or len(values)!=len(limits) or any(not number(v,*limit) for v,limit in zip(values,limits)):raise ValueError('无效原生颜色通道')
        return {'native':{'model':native['model'],'values':list(values)}}
    values=value.get('rgb')
    if not isinstance(values,list) or len(values)!=3 or any(not number(v,0,255) for v in values):raise ValueError('无效RGB颜色')
    return {'rgb':list(values)}

GRADIENT_STYLES=('linear','radial','angle','reflected','diamond')

def gradient_stops(value,opacity=False):
    if not isinstance(value,list) or not 2<=len(value)<=32:raise ValueError('渐变需要2至32个节点')
    stops=[];previous=-1
    for item in value:
        if not isinstance(item,dict) or type(item.get('location')) is not int or not previous<=item['location']<=4096:raise ValueError('无效渐变位置')
        channels={'opacity':item.get('opacity')} if opacity else color(item)
        if opacity and not number(channels['opacity'],0,100):raise ValueError('无效渐变不透明度')
        stop={**channels,'location':item['location']}
        if 'midpoint' in item:
            if type(item['midpoint']) is not int or not 0<=item['midpoint']<=100:raise ValueError('无效渐变中点')
            stop['midpoint']=item['midpoint']
        if not opacity and 'source' in item:
            if item['source'] not in ('userStop','foregroundColor','backgroundColor'):raise ValueError('无效色标来源')
            stop['source']=item['source']
        if not opacity and 'origin' in item:
            if type(item['origin']) is not int or not 0<=item['origin']<32:raise ValueError('无效原始色标位置')
            stop['origin']=item['origin']
        if set(item)!=set(stop):raise ValueError('无效渐变节点参数')
        stops.append(stop);previous=stop['location']
    return stops

def gradient_options(value):
    if not isinstance(value,dict):raise ValueError('无效渐变属性')
    limits={'angle':(-360,360),'scale':(1,1000),'smoothness':(0,4096)}
    for key,v in value.items():
        if key in limits and number(v,*limits[key]) and (key!='smoothness' or type(v) is int):continue
        if key in ('reverse','dither','align') and type(v) is bool:continue
        if key=='style' and v in GRADIENT_STYLES:continue
        raise ValueError('无效渐变属性：'+key)
    return dict(value)

def paint(value,stroke=False):
    if not isinstance(value,dict) or type(value.get('enabled')) is not bool:raise ValueError('无效填充／描边')
    kind=value.get('kind');keys={'kind','enabled'}|({'width'} if stroke else set())
    if kind=='solid':
        channels=color(value);keys.update(channels);result={'kind':kind,'enabled':value['enabled'],**channels}
    elif kind=='gradient':
        keys.add('stops');result={'kind':kind,'enabled':value['enabled'],'stops':gradient_stops(value.get('stops'))}
        if 'opacity_stops' in value:keys.add('opacity_stops');result['opacity_stops']=gradient_stops(value['opacity_stops'],opacity=True)
        if 'options' in value:keys.add('options');result['options']=gradient_options(value['options'])
        if 'method' in value:
            keys.add('method')
            if value['method'] not in ('perceptual','linear','classic'):raise ValueError('无效渐变插值方式')
            result['method']=value['method']
    elif kind=='pattern':
        keys.add('name');name=value.get('name')
        if not isinstance(name,str) or not name or len(name)>128:raise ValueError('无效图案预设名称')
        result={'kind':kind,'enabled':value['enabled'],'name':name}
    else:raise ValueError('未知填充／描边类型')
    if set(value)!=keys:raise ValueError('填充／描边包含未知参数')
    if stroke:
        if not number(value.get('width'),0,1000):raise ValueError('无效描边宽度')
        result['width']=value['width']
    return result

def parameters(value):
    if isinstance(value,dict) and value.get('mask_batch') is True:
        if set(value)!={'revision','mask_batch','mask'} or not isinstance(value['revision'],str) or not 1<=len(value['revision'])<=128:
            raise ValueError('批量蒙版状态已失效')
        mask=value['mask']
        if not isinstance(mask,dict) or not mask or not set(mask)<={'feather','density'} or any(not number(v,0,1000 if k=='feather' else 100) for k,v in mask.items()):
            raise ValueError('无效批量羽化／密度')
        return {'revision':value['revision'],'mask_batch':True,'mask':dict(mask)}
    if not isinstance(value,dict) or set(value) not in ({'revision','fill','stroke'},{'revision','fill','stroke','mask'}):raise ValueError('形状颜色参数不完整')
    revision=value['revision']
    if not isinstance(revision,str) or not 1<=len(revision)<=128:raise ValueError('形状状态已失效，请重新打开菜单')
    result={'revision':revision,'fill':paint(value['fill']),'stroke':paint(value['stroke'],True)}
    if 'mask' in value:
        mask=value['mask']
        if not isinstance(mask,dict) or set(mask)!={'feather','density'} or not number(mask['feather'],0,1000) or not number(mask['density'],0,100):
            raise ValueError('无效矢量蒙版羽化／密度')
        result['mask']=dict(mask)
    return result

def metadata(value):
    if not isinstance(value,dict) or value.get('available') is not True:return {'available':False}
    try:
        if value.get('mask_batch') is True:
            result=parameters({k:value.get(k) for k in ('revision','mask_batch','mask')})
            ids=value.get('layer_ids');active=value.get('active_layer_id');mixed=value.get('mixed')
            if (set(result['mask'])!={'feather','density'} or not isinstance(ids,list) or not 2<=len(ids)<=128
                    or any(type(i) is not int or i<=0 for i in ids) or ids!=sorted(set(ids)) or type(active) is not int or active not in ids
                    or not isinstance(mixed,dict) or set(mixed)!={'feather','density'} or any(type(v) is not bool for v in mixed.values())
                    or type(value.get('live_preview')) is not bool or value.get('preview_engine') not in (None,'legacy-com')):raise ValueError()
            return {'available':True,**result,'vector':True,'layer_ids':list(ids),'active_layer_id':active,'mixed':dict(mixed),
                    'live_preview':value['live_preview'],**({'preview_engine':value['preview_engine']} if value.get('preview_engine') else {})}
        result=parameters({k:value.get(k) for k in ('revision','fill','stroke')+ (('mask',) if 'mask' in value else ())})
        vector=value.get('vector',True)
        if type(vector) is not bool:raise ValueError()
        if 'mask' in result and not vector:raise ValueError()
        patterns=value.get('patterns',[])
        if not isinstance(patterns,list) or len(patterns)>128 or any(not isinstance(x,str) or not x or len(x)>128 for x in patterns):raise ValueError()
        live=value.get('live_preview',False)
        if type(live) is not bool:raise ValueError()
        engine=value.get('preview_engine')
        if engine is not None and engine!='legacy-com':raise ValueError()
        model=value.get('native_model','')
        if model not in ('','cmyk','lab','gray'):raise ValueError()
        return {'available':True,'vector':vector,'native_model':model,'live_preview':live,**result,'patterns':list(patterns),
                **({'gradient_editor':True} if value.get('gradient_editor') is True else {}),
                **({'preview_engine':engine} if engine and live else {})}
    except ValueError:return {'available':False}


def editor_matches(shape,context):
    ids=context.get('layer_ids',[])
    if shape.get('mask_batch') is True:
        return (2<=len(ids)<=128 and shape.get('layer_ids')==ids and shape.get('active_layer_id') in ids)
    return context.get('kind')=='shape-fill' and len(ids)==1
