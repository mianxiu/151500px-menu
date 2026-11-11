/* Standalone ExtendScript transaction. Never called from executeAsModal. */
(function () {
    var request = __PS_MENU_LEGACY_REQUEST__;
    var s = stringIDToTypeID, c = charIDToTypeID, prepared = request.prepared;
    var doc = null, anchor = null, result, changed = false;
    var originalHistory = null, historyStarted = false,stage='capture-target';
    var folder = new Folder(request.folder), mailbox = new File(folder + '/mask-preview-' + request.id + '.json');
    var receipt = new File(folder + '/legacy-result-' + request.id + '.json');
    function encode(value) {
        if (value === null) return 'null';
        if (typeof value === 'string') return '"' + value.replace(/["\\\x00-\x1f]/g, function (v) {
            var n = v.charCodeAt(0).toString(16); return '\\u' + ('0000' + n).slice(-4);
        }) + '"';
        if (typeof value === 'number') { if (!isFinite(value)) throw Error('Nonfinite value'); return String(value); }
        if (typeof value === 'boolean') return String(value);
        var out = [], i, keys = [];
        if (value instanceof Array) { for (i = 0; i < value.length; i++) out.push(encode(value[i])); return '[' + out.join(',') + ']'; }
        for (i in value) if (value.hasOwnProperty(i) && typeof value[i] !== 'undefined') keys.push(i);
        keys.sort(); for (i = 0; i < keys.length; i++) out.push(encode(keys[i]) + ':' + encode(value[keys[i]]));
        return '{' + out.join(',') + '}';
    }
    // Presentation only: no host calls, no credentials leave the local mailbox.
    var progressSerial=0,progressStage='',progressAt=0,progressFrame=0,restoring=false;
    function progress(name,done,total,force){
        if(restoring && name==='verify')name='restore-verify';
        if(restoring && name==='redraw')name='restore-redraw';
        var now=new Date().getTime();
        if(!force && name===progressStage && done!==total && now-progressAt<100)return;
        progressStage=name;progressAt=now;
        var f=new File(folder+'/preview-progress-'+request.id+'.json');
        try{f.encoding='UTF-8';if(f.open('w')){try{f.write(encode({id:request.id,session:request.session,token:request.token,
            serial:++progressSerial,stage:name,done:done,total:total,frame:progressFrame}));}finally{f.close();}}}catch(_){}
    }
    // Parse mailbox data, never eval it. Bound size, depth, duplicate keys and numbers.
    function decode(text) {
        if (text.length > 16384) throw Error('Mailbox size limit');
        var at = 0;
        function ws() { while (/\s/.test(text.charAt(at)) && at < text.length) at++; }
        function str() {
            if (text.charAt(at++) !== '"') throw Error('String expected');
            var out = '', ch, hex, map = {'"':'"','\\':'\\','/':'/','b':'\b','f':'\f','n':'\n','r':'\r','t':'\t'};
            while (at < text.length) {
                ch = text.charAt(at++); if (ch === '"') return out;
                if (ch === '\\') {
                    ch = text.charAt(at++);
                    if (ch === 'u') { hex = text.substr(at, 4); if (!/^[a-fA-F0-9]{4}$/.test(hex)) throw Error('Invalid escape'); at += 4; out += String.fromCharCode(parseInt(hex, 16)); }
                    else if (map.hasOwnProperty(ch)) out += map[ch]; else throw Error('Invalid escape');
                } else { if (ch.charCodeAt(0) < 32) throw Error('Control character'); out += ch; }
            }
            throw Error('Unclosed string');
        }
        function value(depth) {
            if (depth > 16) throw Error('JSON depth limit'); ws();
            var ch = text.charAt(at), out, key, match;
            if (ch === '"') return str();
            if (ch === '{' || ch === '[') {
                at++; out = ch === '{' ? {} : []; ws();
                if (text.charAt(at) === (ch === '{' ? '}' : ']')) { at++; return out; }
                while (true) {
                    ws(); if (ch === '{') {
                        key = str(); if (/^(?:__proto__|constructor|prototype)$/.test(key) || out.hasOwnProperty(key)) throw Error('Invalid object key');
                        ws(); if (text.charAt(at++) !== ':') throw Error('Colon expected'); out[key] = value(depth + 1);
                    } else out.push(value(depth + 1));
                    ws(); key = text.charAt(at++); if (key === (ch === '{' ? '}' : ']')) return out;
                    if (key !== ',') throw Error('Separator expected');
                }
            }
            match = /^(?:true|false|null|-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)/.exec(text.slice(at));
            if (!match) throw Error('Invalid JSON value'); at += match[0].length;
            if (match[0] === 'true') return true; if (match[0] === 'false') return false; if (match[0] === 'null') return null;
            out = Number(match[0]); if (!isFinite(out)) throw Error('Nonfinite number'); return out;
        }
        var out = value(0); ws(); if (at !== text.length) throw Error('Trailing JSON'); return out;
    }
    function same(a, b) { return encode(a) === encode(b); }
    function unit(id) {
        if (id === c('#Pxl')) return 'pixelsUnit'; if (id === c('#Prc')) return 'percentUnit';
        if (id === c('#Ang')) return 'angleUnit'; if (id === c('#Pnt')) return 'pointsUnit';
        return typeIDToStringID(id) || typeIDToCharID(id);
    }
    function name(id) { return typeIDToStringID(id) || typeIDToCharID(id); }
    function object(d) {
        var out = {}, i, k; for (i = 0; i < d.count; i++) { k = d.getKey(i); out[name(k)] = item(d, k); } return out;
    }
    function item(d, k) {
        var t = d.getType(k), out, i;
        if (t === DescValueType.OBJECTTYPE) { out = object(d.getObjectValue(k)); out._obj = name(d.getObjectType(k)); return out; }
        if (t === DescValueType.LISTTYPE) { out = []; var list = d.getList(k); for (i = 0; i < list.count; i++) out.push(item(list, i)); return out; }
        if (t === DescValueType.ENUMERATEDTYPE) return {_enum:name(d.getEnumerationType(k)), _value:name(d.getEnumerationValue(k))};
        if (t === DescValueType.UNITDOUBLE) return {_unit:unit(d.getUnitDoubleType(k)), _value:d.getUnitDoubleValue(k)};
        if (t === DescValueType.DOUBLETYPE) return d.getDouble(k);
        if (t === DescValueType.INTEGERTYPE) return d.getInteger(k);
        if (t === DescValueType.BOOLEANTYPE) return d.getBoolean(k);
        if (t === DescValueType.STRINGTYPE) return d.getString(k);
        throw Error('Unsupported style descriptor');
    }
    function layerDescriptor() { var r = new ActionReference(); r.putIdentifier(c('Lyr '), anchor.id); return executeActionGet(r); }
    function clone(d) { var copy = new ActionDescriptor(); copy.fromStream(d.toStream()); return copy; }
    function pathStream() { if (!prepared.raw.vector) return null; var r = new ActionReference(); r.putEnumerated(s('path'), s('path'), s('vectorMask')); return executeActionGet(r).toStream(); }
    function mask(d) { return {feather:d.getDouble(s('vectorMaskFeather')), density:d.getInteger(s('vectorMaskDensity')) * 100 / 255}; }
    function vectorMaskChanged(d, expected) {
        // ExtendScript does not reliably give && precedence over ||.
        // Branch before touching vector-only state on ordinary fill layers.
        if (!prepared.raw.vector) return false;
        return !maskClose(mask(d), expected);
    }
    function maskClose(a, b) { return Math.abs(a.feather - b.feather) < .01 && Math.abs(a.density - b.density) <= 100 / 255 + .001; }
    function raw(d) {
        var fillList = d.getList(s('adjustment')), fill = object(fillList.getObjectValue(0)); fill._obj = name(fillList.getObjectType(0));
        var stroke = prepared.raw.stroke;
        if (d.hasKey(s('AGMStrokeStyleInfo'))) { stroke = object(d.getObjectValue(s('AGMStrokeStyleInfo'))); stroke._obj = name(d.getObjectType(s('AGMStrokeStyleInfo'))); }
        return {fill:fill, stroke:stroke, vector:d.hasKey(s('hasVectorMask')) && d.getBoolean(s('hasVectorMask'))};
    }
    function selected() {
        var r = new ActionReference(); r.putProperty(c('Prpr'), s('targetLayersIDs')); r.putEnumerated(c('Dcmn'), c('Ordn'), c('Trgt'));
        var d = executeActionGet(r), list = d.getList(s('targetLayersIDs')), ids = [], i;
        for (i = 0; i < list.count; i++) ids.push(list.getReference(i).getIdentifier()); return ids;
    }
    function target() {
        if (app.activeDocument.id !== request.target.document_id || anchor.id !== request.target.active_layer_id || doc.activeLayer.id !== anchor.id || selected().join(',') !== String(anchor.id)) throw Error('Preview target changed');
        var layer = anchor; while (layer.typename === 'ArtLayer' || layer.typename === 'LayerSet') { if (layer.allLocked) throw Error('Preview target locked'); layer = layer.parent; }
    }
    function order() { var out = [], layers = anchor.parent.layers, i; for (i = 0; i < layers.length; i++) out.push(layers[i].id); return out; }
    function parentID() { return anchor.parent.typename === 'LayerSet' ? anchor.parent.id : null; }
    function pixelMask(d) { return {exists:d.hasKey(s('hasUserMask')) && d.getBoolean(s('hasUserMask')), feather:d.hasKey(s('userMaskFeather')) ? d.getDouble(s('userMaskFeather')) : null, density:d.hasKey(s('userMaskDensity')) ? d.getInteger(s('userMaskDensity')) : null}; }
    function rgb(d) {
        var keys=['red','grain','blue'], scale=1, result=[];
        if(d.hasKey(s('redFloat'))){keys=['redFloat','greenFloat','blueFloat'];scale=255;}
        for(var i=0;i<3;i++)result.push(Math.round(Math.max(0,Math.min(255,d.getDouble(s(keys[i]))*scale))));
        return result;
    }
    function gradientDetails(value) {
        if(value.kind!=='gradient')return false;
        if(value.options || value.opacity_stops)return true;
        for(var i=0;i<value.stops.length;i++)if(value.stops[i].midpoint!==undefined || value.stops[i].source!==undefined || value.stops[i].origin!==undefined)return true;
        return false;
    }
    function paint(d, type, details) {
        if (type === s('solidColorLayer')) return {kind:'solid', rgb:rgb(d.getObjectValue(c('Clr ')))};
        if (type === s('patternLayer')) return {kind:'pattern', name:d.getObjectValue(s('pattern')).getString(c('Nm  '))};
        if (type !== s('gradientLayer')) throw Error('Unsupported paint kind');
        var g=d.getObjectValue(s('gradient')),colors=g.getList(s('colors')),stops=[],i,stop;
        for(i=0;i<colors.count;i++) {
            stop=colors.getObjectValue(i);
            var value={rgb:rgb(stop.getObjectValue(c('Clr '))),location:stop.getInteger(s('location'))};
            if(details){value.midpoint=stop.getInteger(s('midpoint'));value.source=stop.hasKey(c('Type'))?name(stop.getEnumerationValue(c('Type'))):'userStop';}
            stops.push(value);
        }
        var result={kind:'gradient',stops:stops,method:d.hasKey(s('gradientsInterpolationMethod'))?name(d.getEnumerationValue(s('gradientsInterpolationMethod'))):'classic'};
        if(details){
            result.options={style:d.hasKey(c('Type'))?name(d.getEnumerationValue(c('Type'))):'linear',
                angle:d.hasKey(c('Angl'))?d.getUnitDoubleValue(c('Angl')):0,scale:d.hasKey(c('Scl '))?d.getUnitDoubleValue(c('Scl ')):100,
                reverse:d.hasKey(s('reverse')) && d.getBoolean(s('reverse')),dither:d.hasKey(s('dither')) && d.getBoolean(s('dither')),
                align:d.hasKey(s('align'))?d.getBoolean(s('align')):true,smoothness:g.hasKey(s('interfaceIconFrameDimmed'))?g.getInteger(s('interfaceIconFrameDimmed')):4096};
            result.opacity_stops=[];
            if(g.hasKey(s('transparency'))){
                var alpha=g.getList(s('transparency'));
                for(i=0;i<alpha.count;i++){
                    stop=alpha.getObjectValue(i);result.opacity_stops.push({location:stop.getInteger(s('location')),
                        opacity:stop.getUnitDoubleValue(c('Opct')),midpoint:stop.getInteger(s('midpoint'))});
                }
            }else result.opacity_stops=[{location:0,opacity:100,midpoint:50},{location:4096,opacity:100,midpoint:50}];
        }
        return result;
    }
    function samePaint(actual, wanted) {
        if (actual.kind !== wanted.kind) return false;
        function colors(a, b) { return Math.abs(a[0]-b[0])<=1 && Math.abs(a[1]-b[1])<=1 && Math.abs(a[2]-b[2])<=1; }
        if (actual.kind === 'solid') return colors(actual.rgb, wanted.rgb);
        if (actual.kind === 'pattern') return actual.name === wanted.name;
        if (wanted.method !== undefined && actual.method !== wanted.method) return false;
        if(wanted.options)for(var key in wanted.options)if(wanted.options.hasOwnProperty(key)){
            var a=actual.options[key],b=wanted.options[key];
            if(typeof b==='number'){
                if(typeof a!=='number')return false;
                var delta=Math.abs(a-b);
                if(key==='angle'){delta=delta%360;delta=Math.min(delta,360-delta);}
                if(delta>.01)return false;
            }else if(a!==b)return false;
        }
        if(wanted.opacity_stops){
            if(actual.opacity_stops.length!==wanted.opacity_stops.length)return false;
            for(var n=0;n<wanted.opacity_stops.length;n++){
                var from=actual.opacity_stops[n],to=wanted.opacity_stops[n];
                // Photoshop can store gradient opacity in 255 discrete steps.
                var quantized=Math.round(to.opacity*255/100)*100/255;
                if(from.location!==to.location || Math.min(Math.abs(from.opacity-to.opacity),Math.abs(from.opacity-quantized))>.01 || from.midpoint!==(to.midpoint===undefined?50:to.midpoint))return false;
            }
        }
        if (actual.stops.length !== wanted.stops.length) return false;
        for(var i=0;i<actual.stops.length;i++){
            var first=actual.stops[i],second=wanted.stops[i];
            if(first.location!==second.location || !colors(first.rgb,second.rgb))return false;
            if(second.midpoint!==undefined && first.midpoint!==second.midpoint)return false;
            if(second.source!==undefined && first.source!==second.source)return false;
        }
        return true;
    }
    function validate(p) {
        if (!p || p.revision !== request.paint.revision || typeof p.fill !== 'object' || typeof p.stroke !== 'object' || (p.hasOwnProperty('mask') !== request.paint.hasOwnProperty('mask'))) throw Error('Preview revision changed');
        function color(v) { return v instanceof Array && v.length === 3 && range(v[0],0,255) && range(v[1],0,255) && range(v[2],0,255); }
        function integer(v,lo,hi){return range(v,lo,hi) && Math.floor(v)===v;}
        function stops(values,opacity){
            if(!(values instanceof Array) || values.length<2 || values.length>32)throw Error('Invalid gradient stops');
            for(var i=0;i<values.length;i++){
                var stop=values[i];
                if(!stop || !integer(stop.location,0,4096))throw Error('Invalid stop position');
                if(i && stop.location<values[i-1].location)throw Error('Unordered stops');
                if(stop.midpoint!==undefined && !integer(stop.midpoint,0,100))throw Error('Invalid midpoint');
                if(opacity){if(!range(stop.opacity,0,100))throw Error('Invalid opacity');}
                else {
                    if(!color(stop.rgb))throw Error('Invalid RGB');
                    if(stop.origin!==undefined && !integer(stop.origin,0,31))throw Error('Invalid stop origin');
                    if(stop.source!==undefined && !/^(userStop|foregroundColor|backgroundColor)$/.test(stop.source))throw Error('Invalid stop source');
                }
            }
        }
        function one(v) {
            if (!v || typeof v.enabled !== 'boolean') throw Error('Invalid paint');
            if (v.kind === 'solid') { if (!color(v.rgb)) throw Error('Invalid RGB'); }
            else if (v.kind === 'gradient') {
                if (v.method !== undefined && v.method!=='classic' && v.method!=='linear' && v.method!=='perceptual') throw Error('Invalid gradient interpolation');
                stops(v.stops,false);
                if(v.opacity_stops!==undefined)stops(v.opacity_stops,true);
                if(v.options!==undefined){
                    if(!v.options || typeof v.options!=='object' || v.options instanceof Array)throw Error('Invalid options');
                    for(var key in v.options)if(v.options.hasOwnProperty(key)){
                        var value=v.options[key];
                        if(key==='style' && /^(linear|radial|angle|reflected|diamond)$/.test(value))continue;
                        if(key==='angle' && range(value,-360,360))continue;
                        if(key==='scale' && range(value,1,1000))continue;
                        if(key==='smoothness' && integer(value,0,4096))continue;
                        if(/^(reverse|dither|align)$/.test(key) && typeof value==='boolean')continue;
                        throw Error('Invalid gradient option');
                    }
                }
            } else if (v.kind === 'pattern') { if (typeof v.name !== 'string' || !v.name || v.name.length > 128) throw Error('Invalid pattern'); }
            else throw Error('Invalid paint kind');
        }
        one(p.fill); one(p.stroke); if (!range(p.stroke.width,0,1000)) throw Error('Invalid stroke width');
        if (p.mask && (!range(p.mask.feather,0,1000) || !range(p.mask.density,0,100))) throw Error('Invalid mask');
        if (!prepared.raw.vector && (!same(p.stroke, prepared.before.stroke) || !p.fill.enabled || p.fill.kind !== prepared.before.fill.kind)) throw Error('Ordinary fill type must remain unchanged');
    }
    function range(v, lo, hi) { return typeof v === 'number' && isFinite(v) && v >= lo && v <= hi; }
    function rgbDescriptor(v) { var d = new ActionDescriptor(); d.putDouble(s('red'),v[0]); d.putDouble(s('grain'),v[1]); d.putDouble(s('blue'),v[2]); return d; }
    var baseline, originalFill, originalFillType, originalStroke, originalPath, originalPixel, originalOrder, originalParent, originalMask, expectedRaw, applied, latest, lastSeq=0, updates=0, redraws=0;
    function build(value, previous, previousType) {
        var typeName;
        if(value.kind==='solid')typeName='solidColorLayer';else if(value.kind==='gradient')typeName='gradientLayer';else typeName='patternLayer';
        var type = s(typeName);
        var d = value.kind!=='solid' && type === previousType ? clone(previous) : new ActionDescriptor();
        if (value.kind === 'solid') d.putObject(c('Clr '),s('RGBColor'),rgbDescriptor(value.rgb));
        else if (value.kind === 'pattern') {
            var existing = previousType === type && previous.getObjectValue(s('pattern')).getString(c('Nm  ')) === value.name;
            var allowed = existing, i; for(i=0;i<prepared.before.patterns.length;i++) if(prepared.before.patterns[i]===value.name) allowed=true;
            if (!allowed) throw Error('Pattern catalog changed');
            if (!existing) { var pattern = new ActionDescriptor(); pattern.putString(c('Nm  '), value.name); d.putObject(s('pattern'),s('pattern'),pattern); }
        } else {
            if (value.method !== undefined) d.putEnumerated(s('gradientsInterpolationMethod'),s('gradientInterpolationMethodType'),s(value.method));
            if (type !== previousType) { d.putEnumerated(c('Type'),s('gradientType'),s('linear')); d.putUnitDouble(c('Angl'),c('#Ang'),0); d.putUnitDouble(c('Scl '),c('#Prc'),100); }
            var opts=value.options || {};
            if(opts.style!==undefined)d.putEnumerated(c('Type'),s('gradientType'),s(opts.style));
            if(opts.angle!==undefined)d.putUnitDouble(c('Angl'),c('#Ang'),opts.angle);
            if(opts.scale!==undefined)d.putUnitDouble(c('Scl '),c('#Prc'),opts.scale);
            var flags=['reverse','dither','align'];for(var f=0;f<flags.length;f++)if(opts[flags[f]]!==undefined)d.putBoolean(s(flags[f]),opts[flags[f]]);
            var g = d.hasKey(s('gradient')) ? clone(d.getObjectValue(s('gradient'))) : new ActionDescriptor();
            if (!g.hasKey(c('Nm  '))) g.putString(c('Nm  '),'Menu gradient');
            g.putEnumerated(s('gradientForm'),s('gradientForm'),s('customStops'));
            if(opts.smoothness!==undefined)g.putInteger(s('interfaceIconFrameDimmed'),opts.smoothness);
            else if(!g.hasKey(s('interfaceIconFrameDimmed')))g.putInteger(s('interfaceIconFrameDimmed'),4096);
            var old=g.hasKey(s('colors'))?g.getList(s('colors')):null,colors=new ActionList(),stop;
            for(var n=0;n<value.stops.length;n++){
                var wanted=value.stops[n],index=wanted.origin===undefined?n:wanted.origin;
                var prior=old && index<old.count?old.getObjectValue(index):null;
                stop=prior?clone(prior):new ActionDescriptor();
                if(!prior || !same(rgb(prior.getObjectValue(c('Clr '))),wanted.rgb))stop.putObject(c('Clr '),s('RGBColor'),rgbDescriptor(wanted.rgb));
                stop.putEnumerated(c('Type'),s('colorStopType'),s(wanted.source || 'userStop'));
                stop.putInteger(s('location'),wanted.location);
                stop.putInteger(s('midpoint'),wanted.midpoint===undefined?(prior?prior.getInteger(s('midpoint')):50):wanted.midpoint);
                colors.putObject(s('colorStop'),stop);
            }
            g.putList(s('colors'),colors);
            if(value.opacity_stops || !g.hasKey(s('transparency'))){
                var opacity=value.opacity_stops || [{location:0,opacity:100},{location:4096,opacity:100}],transparency=new ActionList();
                for(var k=0;k<opacity.length;k++){
                    stop=new ActionDescriptor();stop.putUnitDouble(c('Opct'),c('#Prc'),opacity[k].opacity);
                    stop.putInteger(s('location'),opacity[k].location);stop.putInteger(s('midpoint'),opacity[k].midpoint===undefined?50:opacity[k].midpoint);
                    transparency.putObject(s('transferSpec'),stop);
                }
                g.putList(s('transparency'),transparency);
            }
            d.putObject(s('gradient'),s('gradientClassEvent'),g);
        }
        return {type:type, descriptor:d};
    }
    function defaultStroke() {
        var d=new ActionDescriptor();d.putInteger(s('strokeStyleVersion'),2);d.putBoolean(s('strokeEnabled'),false);d.putBoolean(s('fillEnabled'),true);d.putUnitDouble(s('strokeStyleLineWidth'),c('#Pxl'),1);var fill=new ActionDescriptor();fill.putObject(c('Clr '),s('RGBColor'),rgbDescriptor([0,0,0]));d.putObject(s('strokeStyleContent'),s('solidColorLayer'),fill);return d;
    }
    function verifyPaint(d, wanted) {
        var list=d.getList(s('adjustment')), fill=paint(list.getObjectValue(0),list.getObjectType(0),gradientDetails(wanted.fill));
        var stroke=d.hasKey(s('AGMStrokeStyleInfo'))?d.getObjectValue(s('AGMStrokeStyleInfo')):defaultStroke();
        if (!samePaint(fill,wanted.fill)) throw Error('Fill readback mismatch'+(request.preview_trace && gradientDetails(wanted.fill)?': '+encode(fill):''));
        if(prepared.raw.vector){var actual=paint(stroke.getObjectValue(s('strokeStyleContent')),stroke.getObjectType(s('strokeStyleContent')),gradientDetails(wanted.stroke));if(!samePaint(actual,wanted.stroke) || stroke.getBoolean(s('strokeEnabled'))!==wanted.stroke.enabled || stroke.getBoolean(s('fillEnabled'))!==wanted.fill.enabled)throw Error('Stroke readback mismatch');var width=stroke.getUnitDoubleValue(s('strokeStyleLineWidth'));if(stroke.getUnitDoubleType(s('strokeStyleLineWidth'))===c('#Pnt'))width*=doc.resolution/72;if(Math.abs(width-wanted.stroke.width)>.01)throw Error('Stroke width mismatch');}
        if(wanted.mask && !maskClose(mask(d),wanted.mask))throw Error('Mask readback mismatch');
    }
    function setMaskProperty(key, unitID, value) { stage='write-'+key;target();var d=new ActionDescriptor(),r=new ActionReference(),v=new ActionDescriptor();r.putIdentifier(c('Lyr '),anchor.id);d.putReference(c('null'),r);v.putUnitDouble(s(key),c(unitID),value);d.putObject(c('T   '),c('Lyr '),v);executeAction(c('setd'),d,DialogModes.NO); }
    function apply(p) {
        progress('read',0,1,true);stage='read-before-frame';validate(p);target();var d=layerDescriptor();
        if(!same(raw(d),expectedRaw) || vectorMaskChanged(d,applied.mask || originalMask))throw Error('Style changed outside preview');
        progress('read',1,1);progress('write',0,1,true);
        var fillChanged=!same(p.fill,applied.fill),strokeChanged=!same(p.stroke,applied.stroke),frameChanged=false,to,newFill,newStroke,r,command;
        if(fillChanged || strokeChanged){
            to=prepared.raw.vector?new ActionDescriptor():null;
            if(fillChanged){newFill=build(p.fill,originalFill,originalFillType);if(to)to.putObject(s('fillContents'),newFill.type,newFill.descriptor);else to=newFill.descriptor;}
            if(prepared.raw.vector && (strokeChanged || p.fill.enabled!==applied.fill.enabled)){
                newStroke=clone(originalStroke);newStroke.putInteger(s('strokeStyleVersion'),2);newStroke.putBoolean(s('fillEnabled'),p.fill.enabled);
                if(strokeChanged){newStroke.putBoolean(s('strokeEnabled'),p.stroke.enabled);var oldContent=originalStroke.getObjectValue(s('strokeStyleContent'));var content=build(p.stroke,oldContent,originalStroke.getObjectType(s('strokeStyleContent')));newStroke.putObject(s('strokeStyleContent'),content.type,content.descriptor);newStroke.putUnitDouble(s('strokeStyleLineWidth'),c('#Pxl'),p.stroke.width);}
                to.putObject(s('strokeStyle'),s('strokeStyle'),newStroke);
            }
            stage='write-fill-stroke';target();command=new ActionDescriptor();r=new ActionReference();r.putEnumerated(s('contentLayer'),c('Ordn'),c('Trgt'));command.putReference(c('null'),r);command.putObject(c('T   '),prepared.raw.vector?s('shapeStyle'):newFill.type,to);executeAction(c('setd'),command,DialogModes.NO);changed=true;frameChanged=true;
        }
        if(p.mask){if(Math.abs(p.mask.feather-(applied.mask || originalMask).feather)>.00001){setMaskProperty('vectorMaskFeather','#Pxl',p.mask.feather);changed=true;frameChanged=true;}if(Math.abs(p.mask.density-(applied.mask || originalMask).density)>.00001){setMaskProperty('vectorMaskDensity','#Prc',p.mask.density);changed=true;frameChanged=true;}}
        progress('write',1,1);progress('verify',0,1,true);stage='verify-frame';target();d=layerDescriptor();verifyPaint(d,p);expectedRaw=raw(d);applied=p;updates++;
        progress('verify',1,1);if(frameChanged){progress('redraw',0,0,true);stage='redraw';app.refresh();redraws++;stage='verify-after-redraw';target();}progress('ready',1,1,true);
    }
    function readControl() {
        if(!mailbox.exists || mailbox.length>16384)return null;mailbox.encoding='UTF-8';if(!mailbox.open('r'))return null;var text;try{text=mailbox.read();}finally{mailbox.close();}return decode(text.replace(/^\uFEFF/,''));
    }
    function loop() {
        while(true){
            var value=null,now=new Date().getTime();try{value=readControl();}catch(_){}
            if(value && value.id===request.id && value.session===request.session && value.token===request.token && range(value.seq,1,2147483647) && Math.floor(value.seq)===value.seq && range(value.lease_until_ms,now,now+5000)){
                if(value.seq<lastSeq)throw Error('Preview sequence regressed');
                leaseUntil=value.lease_until_ms;
                if(value.phase==='cancel')return;
                if(value.phase!=='preview' && value.phase!=='commit')throw Error('Invalid preview phase');
                if(value.seq>lastSeq){progressFrame=value.seq;apply(value.paint);latest=value.paint;lastSeq=value.seq;}
                if(value.phase==='commit'){commit=true;return;}
            }
            if(now>leaseUntil)return;$.sleep(20);
        }
    }
    var commit=false,leaseUntil=new Date().getTime()+4000;
    function invariants() { target();var d=layerDescriptor();if(parentID()!==originalParent || order().join(',')!==originalOrder.join(',') || pathStream()!==originalPath || !same(pixelMask(d),originalPixel))throw Error('Document structure or pixel mask changed');return d; }
    function restoreSingleSelection() {
        // History states also store the selection at their creation. A later
        // selection without a history step must survive our own rollback.
        // Only called immediately after restoring our captured history, with
        // target() checked before that write. Never repair an external change.
        if(app.activeDocument.id!==request.target.document_id)throw Error('Preview document changed during rollback');
        var r=new ActionReference(),d=new ActionDescriptor();
        r.putIdentifier(c('Lyr '),request.target.active_layer_id);r.putIdentifier(s('document'),request.target.document_id);
        d.putReference(c('null'),r);d.putBoolean(c('MkVs'),false);executeAction(c('slct'),d,DialogModes.NO);target();
    }
    function rollback() { restoring=true;progress('restore',0,0,true);target();var current=layerDescriptor(),dirty=changed || !same(raw(current),raw(baseline)) || vectorMaskChanged(current,originalMask);stage='restore-history';doc.activeHistoryState=originalHistory;restoreSingleSelection();if(dirty){progress('redraw',0,0,true);stage='restore-redraw';app.refresh();redraws++;}progress('verify',0,1,true);stage='restore-verify';var d=invariants();if(!same(raw(d),raw(baseline)) || vectorMaskChanged(d,originalMask))throw Error('Legacy rollback readback mismatch');progress('verify',1,1,true); }
    function batchRun() {
        var records=[],batchHistory=null,batchStarted=false,batchChanged=false,batchCommit=false,batchSeq=0,batchUpdates=0,batchRedraws=0,batchLatest=request.paint;
        var batchAbort={},actionResidue='',batchNativeCalls=0,nativeReads=false;
        var keys=['layerKind','adjustment','AGMStrokeStyleInfo','hasVectorMask','hasUserMask','userMaskFeather','userMaskDensity'];
        var ids=request.target.layer_ids.slice().sort(function(a,b){return a-b;}),lease=new Date().getTime()+4000;
        var selectionStream=null,selectionIDs=ids.join(',');
        function batchIdentity() {
            if(!app.documents.length || app.activeDocument.id!==request.target.document_id || doc.activeLayer.id!==request.target.active_layer_id)throw Error('Batch selection changed');
            // Always read the live selection before each direct write or native batch. Exact native
            // bytes avoid repeatedly decoding every selected reference; this is
            // not a hash, cached host read, or authority to skip a fresh read.
            var r=new ActionReference();r.putProperty(s('property'),s('targetLayersIDs'));r.putEnumerated(s('document'),s('ordinal'),s('targetEnum'));
            var d=executeActionGet(r),stream=d.toStream();
            if(selectionStream!==null && stream===selectionStream)return;
            // A different native encoding/order is accepted only after checking
            // the full ID set from this same fresh descriptor. Failures propagate.
            var list=d.getList(s('targetLayersIDs')),current=[];
            for(var j=0;j<list.count;j++)current.push(list.getReference(j).getIdentifier());
            if(current.sort(function(a,b){return a-b;}).join(',')!==selectionIDs)throw Error('Batch selection changed');
            selectionStream=stream;
        }
        function unlocked(l){var kind=l.typename;while(kind==='ArtLayer'||kind==='LayerSet'){if(l.allLocked || (kind==='ArtLayer' && (l.pixelsLocked||l.positionLocked||l.transparentPixelsLocked||l.isBackgroundLayer)))throw Error('Batch layer locked');l=l.parent;kind=l.typename;}}
        function batchTarget(){batchIdentity();if(nativeReads){nativeLocks(null);return;}for(var i=0;i<records.length;i++)unlocked(records[i].layer);}
        function batchValidate(p) {
            if(!p || p.mask_batch!==true || p.revision!==request.paint.revision || !p.mask || p.fill || p.stroke)throw Error('Invalid batch parameters');
            var n=0;for(var k in p.mask)if(p.mask.hasOwnProperty(k)){if((k!=='feather'&&k!=='density') || !range(p.mask[k],0,k==='feather'?1000:100))throw Error('Invalid batch mask');n++;}if(!n)throw Error('Empty batch mask');
        }
        function desc(id){var r=new ActionReference();r.putIdentifier(c('Lyr '),id);return executeActionGet(r);}
        // Native reads are scoped to ONE synchronous validation. No DOM proxy,
        // descriptor, parent or lock survives a yield or a later validation.
        // Small selections keep the original path. Scan the complete fresh AM
        // tree (including section ends) to prove parents and sibling order.
        function nativeLocked(d,group){
            var locks=d.getObjectValue(s('layerLocking'));
            if(locks.getBoolean(s('protectAll')))throw Error('Batch layer locked');
            if(!group && (locks.getBoolean(s('protectComposite')) || locks.getBoolean(s('protectPosition')) || locks.getBoolean(s('protectTransparency')) || d.getBoolean(s('background'))))throw Error('Batch layer locked');
        }
        function nativeLocks(members){
            var seen={};
            function check(id){
                if(seen[id]===1)throw Error('Invalid batch lock ancestry');if(seen[id]===2)return;var d=desc(id);
                if(d.getInteger(s('layerID'))!==id)throw Error('Batch lock identity changed');
                var section=name(d.getEnumerationValue(s('layerSection')));
                if(section!=='layerSectionContent' && section!=='layerSectionStart')throw Error('Batch layer missing');
                nativeLocked(d,section==='layerSectionStart');seen[id]=1;
                var parent=d.getInteger(s('parentLayerID'));if(parent>0)check(parent);seen[id]=2;
            }
            if(members===null){for(var j=0;j<records.length;j++)check(records[j].original.id);}
            else for(var j=0;j<members.length;j++)check(records[members[j]].original.id);
        }
        function nativeTree(){
            var r=new ActionReference();r.putProperty(s('property'),s('numberOfLayers'));r.putIdentifier(s('document'),request.target.document_id);
            var count=executeActionGet(r).getInteger(s('numberOfLayers')),first=1;
            try{doc.backgroundLayer;first=0;}catch(_){}
            var tree={},orders={},stack=[],total=count+(first===0?1:0);
            if(count<1 || total>4096)throw Error('Native batch tree range unavailable');
            for(var index=count;index>=first;index--){
                r=new ActionReference();r.putIndex(s('layer'),index);r.putIdentifier(s('document'),request.target.document_id);
                var d=executeActionGet(r),id=d.getInteger(s('layerID')),section=name(d.getEnumerationValue(s('layerSection'))),parent=d.getInteger(s('parentLayerID'));
                if(d.getInteger(s('itemIndex'))!==index-first+1)throw Error('Native batch tree index changed');
                if(section==='layerSectionEnd'){
                    if(!stack.length || stack[stack.length-1]!==parent)throw Error('Unbalanced native batch tree');stack.pop();continue;
                }
                if((section!=='layerSectionContent' && section!=='layerSectionStart') || id<=0 || tree[id])throw Error('Invalid native batch tree');
                if(parent<=0)parent=null;
                if(parent!==(stack.length?stack[stack.length-1]:null))throw Error('Native batch parent changed');
                var key='p'+parent;if(!orders.hasOwnProperty(key))orders[key]=[];orders[key].push(id);
                tree[id]={id:id,parent:parent,descriptor:d,group:section==='layerSectionStart'};
                if(section==='layerSectionStart')stack.push(id);
            }
            if(stack.length)throw Error('Unbalanced native batch tree');
            var checked={};function check(id){if(checked[id])return;var row=tree[id];if(!row)throw Error('Batch target missing');nativeLocked(row.descriptor,row.group);checked[id]=true;if(row.parent!==null)check(row.parent);}
            for(var j=0;j<ids.length;j++)check(ids[j]);
            batchIdentity();return {tree:tree,orders:orders};
        }
        function nativeMember(id,snapshot,nativeOnly){
            var row=snapshot.tree[id];if(!row)throw Error('Batch target missing');var d=row.descriptor;
            if(!d.hasKey(s('hasVectorMask'))||!d.getBoolean(s('hasVectorMask')))throw Error('Vector mask missing');
            var path=batchPath(id);return {id:id,mask:mask(d),content:nativeOnly?null:contents(d),path:nativeOnly?null:item(path,s('pathContents')),
                content_bytes:contentStream(d),path_bytes:path.toStream(),parent_id:row.parent,order:snapshot.orders['p'+row.parent]};
        }
        function batchPath(id){var r=new ActionReference();r.putProperty(s('property'),s('pathContents'));r.putEnumerated(s('path'),s('path'),s('vectorMask'));r.putIdentifier(c('Lyr '),id);var p=executeActionGet(r);return p;}
        function contents(d){var v={};for(var j=0;j<keys.length;j++){var k=s(keys[j]);v[keys[j]]=d.hasKey(k)?item(d,k):null;}return v;}
        // Preserve all invariant values and their native types without decoding
        // every nested path/style node on every frame. Bytes are compared exactly,
        // never hashed. Initial dispatch still compares the semantic UXP snapshot.
        function contentStream(d){
            var out=new ActionDescriptor();
            for(var j=0;j<keys.length;j++){
                var k=s(keys[j]);if(!d.hasKey(k))continue;var t=d.getType(k);
                if(t===DescValueType.OBJECTTYPE)out.putObject(k,d.getObjectType(k),d.getObjectValue(k));
                else if(t===DescValueType.LISTTYPE)out.putList(k,d.getList(k));
                else if(t===DescValueType.ENUMERATEDTYPE)out.putEnumerated(k,d.getEnumerationType(k),d.getEnumerationValue(k));
                else if(t===DescValueType.UNITDOUBLE)out.putUnitDouble(k,d.getUnitDoubleType(k),d.getUnitDoubleValue(k));
                else if(t===DescValueType.DOUBLETYPE)out.putDouble(k,d.getDouble(k));
                else if(t===DescValueType.INTEGERTYPE)out.putInteger(k,d.getInteger(k));
                else if(t===DescValueType.BOOLEANTYPE)out.putBoolean(k,d.getBoolean(k));
                else if(t===DescValueType.STRINGTYPE)out.putString(k,d.getString(k));
                else throw Error('Unsupported invariant descriptor');
            }
            return out.toStream();
        }
        // Reuse only local DOM values within this synchronous read; every verify
        // starts a new orders map and rereads every layer, path and mask.
        function membersOrder(parent){var v=[],ls=parent.layers,n=ls.length;for(var j=0;j<n;j++)v.push(ls[j].id);return v;}
        function readMember(layer,orders,nativeOnly){var id=layer.id,container=layer.parent,parent=container.typename==='LayerSet'?container.id:null,key='p'+parent;if(!orders.hasOwnProperty(key))orders[key]=membersOrder(container);var d=desc(id);if(!d.hasKey(s('hasVectorMask'))||!d.getBoolean(s('hasVectorMask')))throw Error('Vector mask missing');
            var path=batchPath(id);return {id:id,mask:mask(d),content:nativeOnly?null:contents(d),path:nativeOnly?null:item(path,s('pathContents')),
                content_bytes:contentStream(d),path_bytes:path.toStream(),parent_id:parent,order:orders[key]};
        }
        function sameMember(a,b,wanted){return a.id===b.id && maskClose(a.mask,wanted) && (typeof b.content_bytes==='string' ? a.content_bytes===b.content_bytes && a.path_bytes===b.path_bytes : same(a.content,b.content) && same(a.path,b.path)) && a.parent_id===b.parent_id && same(a.order,b.order);}
        // Dispatch compares exact observed originals. Decimal numbers parsed
        // by ExtendScript can lose precision; compare IEEE-754 bytes instead
        // of widening the tolerance. A one-double descriptor ends in its value.
        function maskDoubleHex(value){var d=new ActionDescriptor();d.putDouble(s('value'),value);var bytes=d.toStream().slice(-8),hex='';for(var i=0;i<bytes.length;i++)hex+=('00'+(bytes.charCodeAt(i)&255).toString(16)).slice(-2);return hex;}
        function samePreparedMember(a,b){var exact=b.mask_exact;return sameMember(a,b,b.mask) && exact && /^[0-9a-f]{16}$/.test(exact.feather) && maskDoubleHex(a.mask.feather)===exact.feather && Math.round(a.mask.density*255/100)===exact.density;}
        function verify(wanted){progress('verify',0,records.length,true);var snapshot=null;if(nativeReads){batchIdentity();snapshot=nativeTree();}else batchTarget();var orders={};for(var j=0;j<records.length;j++){var current=snapshot?nativeMember(records[j].original.id,snapshot,true):readMember(records[j].layer,orders,true);if(!sameMember(current,records[j].original,wanted[j]))throw Error('Batch mask/content/path/order mismatch');progress('verify',j+1,records.length);}}
        function setter(id,key,value){var r=new ActionReference();r.putIdentifier(c('Lyr '),id);r.putIdentifier(s('document'),request.target.document_id);var d=new ActionDescriptor(),v=new ActionDescriptor();d.putReference(c('null'),r);v.putUnitDouble(s(key==='feather'?'vectorMaskFeather':'vectorMaskDensity'),c(key==='feather'?'#Pxl':'#Prc'),value);d.putObject(c('T   '),c('Lyr '),v);return d;}
        function set(id,key,value){executeAction(c('setd'),setter(id,key,value),DialogModes.NO);}
        // Native action containers amortize per-executeAction dispatch. Only
        // fixed document/layer IDs and the two validated mask properties enter.
        // Format: ATN v16, no shortcuts or dialogs, opaque native descriptors.
        function actionBytes(name,commands){
            function number(v,n){var out='';for(var b=n-1;b>=0;b--)out+=String.fromCharCode((v>>>(b*8))&255);return out;}
            function unicode(v){var out=number(v.length+1,4);for(var j=0;j<v.length;j++)out+=number(v.charCodeAt(j),2);return out+number(0,2);}
            if(!commands.length || commands.length>32)throw Error('Invalid native batch size');
            var out=number(16,4)+unicode(name)+number(0,1)+number(1,4)+number(0,2)+number(0,1)+number(0,1)+number(0,2)+unicode('Apply')+number(0,1)+number(commands.length,4);
            for(var j=0;j<commands.length;j++){var bytes=commands[j].toStream();if(bytes.substr(0,4)!==number(16,4))throw Error('Unsupported native descriptor stream');out+=number(0,1)+number(1,1)+number(0,1)+number(0,1)+'long'+'setd'+number(3,4)+'Set'+number(-1,4)+bytes.substr(4);}
            return out;
        }
        function nativeBatch(commands,members,chunk){
            var name='PSMenuPreview_'+request.id+'_'+progressFrame+'_'+chunk,file=new File(folder+'/native-actions-'+request.id+'-'+progressFrame+'-'+chunk+'.atn');
            if(file.exists)throw Error('Native batch already prepared');var bytes=actionBytes(name,commands);
            file.encoding='BINARY';if(!file.open('w'))throw Error('Cannot prepare native batch');try{if(!file.write(bytes))throw Error('Cannot save native batch');}finally{file.close();}
            var loaded=false;actionResidue=name;
            try{
                app.load(file);loaded=true;
                // Loading can yield to PS. Recheck fresh identity and every
                // affected layer/ancestor immediately before the bounded play.
                batchIdentity();if(nativeReads)nativeLocks(members);else for(var j=0;j<members.length;j++){var record=records[members[j]];if(record.layer.id!==record.original.id)throw Error('Batch target changed');unlocked(record.layer);}
                batchChanged=true;for(var j=0;j<members.length;j++)records[members[j]].touched=true;
                app.doAction('Apply',name);batchNativeCalls++;
            }finally{
                if(loaded){var ref=new ActionReference(),remove=new ActionDescriptor();ref.putName(s('actionSet'),name);remove.putReference(s('null'),ref);executeAction(s('delete'),remove,DialogModes.NO);actionResidue='';file.remove();}
            }
            var packet=null,now=new Date().getTime();try{packet=readControl();}catch(_){}
            if(packet && packet.id===request.id && packet.session===request.session && packet.token===request.token && range(packet.seq,progressFrame,2147483647) && Math.floor(packet.seq)===packet.seq && range(packet.lease_until_ms,now,now+5000) && packet.phase==='cancel')throw batchAbort;
        }

        function applyBatch(p){
            batchValidate(p);stage='batch-verify-frame';verify(records.mapMask);var wanted=[],changedFrame=false;
            for(var j=0;j<records.length;j++){var base=records[j].original.mask;wanted.push({feather:p.mask.hasOwnProperty('feather')?p.mask.feather:base.feather,density:p.mask.hasOwnProperty('density')?p.mask.density:base.density});}
            progress('write',0,records.length,true);
            if(records.length<8){
                for(var j=0;j<records.length;j++){for(var k=0;k<2;k++){var key=k?'density':'feather';if(Math.abs(wanted[j][key]-records.mapMask[j][key])>(k?100/255+.001:.00001)){stage='batch-write-'+key;batchIdentity();unlocked(records[j].layer);batchChanged=true;records[j].touched=true;set(records[j].layer.id,key,wanted[j][key]);changedFrame=true;}}progress('write',j+1,records.length);}
            }else{
                for(var first=0;first<records.length;first+=16){var commands=[],members=[],end=Math.min(first+16,records.length);
                    for(var j=first;j<end;j++){var dirty=false;for(var k=0;k<2;k++){var key=k?'density':'feather';if(Math.abs(wanted[j][key]-records.mapMask[j][key])>(k?100/255+.001:.00001)){commands.push(setter(records[j].original.id,key,wanted[j][key]));dirty=true;}}if(dirty)members.push(j);}
                    if(commands.length){stage='batch-native-write';nativeBatch(commands,members,first);changedFrame=true;}
                    progress('write',end,records.length,true);
                }
            }
            stage='batch-readback';verify(wanted);records.mapMask=wanted;batchLatest=p;batchUpdates++;
            if(changedFrame){progress('redraw',0,0,true);stage='batch-redraw';app.refresh();batchRedraws++;verify(wanted);}progress('ready',records.length,records.length,true);
        }
        function batchAck(seq){
            // Opt-in diagnostics only: publish after native readback and redraw.
            if(request.preview_trace!==true)return;
            var f=new File(folder+'/mask-preview-status-'+request.id+'.json');
            try{f.encoding='UTF-8';if(f.open('w')){try{f.write(encode({id:request.id,session:request.session,seq:seq,phase:'preview',mask_batch:true,count:records.length,mask:batchLatest.mask,updates:batchUpdates,redraw_completed:true,redraw_method:'standalone-app.refresh'}));}finally{f.close();}}}catch(_){}
        }
        function batchLoop(){
            while(true){var packet=null,now=new Date().getTime();try{packet=readControl();}catch(_){}
                if(packet && packet.id===request.id && packet.session===request.session && packet.token===request.token && range(packet.seq,1,2147483647) && Math.floor(packet.seq)===packet.seq && range(packet.lease_until_ms,now,now+5000)){
                    if(packet.seq<batchSeq)throw Error('Batch sequence regressed');lease=packet.lease_until_ms;
                    if(packet.phase==='cancel')return;
                    if(packet.phase!=='preview'&&packet.phase!=='commit')throw Error('Invalid batch phase');
                    if(packet.seq>batchSeq){progressFrame=packet.seq;try{applyBatch(packet.paint);}catch(e){if(e===batchAbort)return;throw e;}batchSeq=packet.seq;batchAck(batchSeq);}
                    if(packet.phase==='commit'){batchCommit=true;return;}
                }
                if(now>lease)return;$.sleep(20);
            }
        }
        function originals(){var out=[];for(var j=0;j<records.length;j++)out.push(records[j].original.mask);return out;}
        function restoreBatchSelection(){
            if(!app.documents.length || app.activeDocument.id!==request.target.document_id)throw Error('Batch document changed during rollback');
            // History restoration may restore a pre-selection single layer. Rebuild
            // the frozen selection, active ID last, without changing visibility.
            var current=selected().sort(function(a,b){return a-b;});
            if(same(current,ids) && doc.activeLayer.id===request.target.active_layer_id)return;
            var ordered=[];for(var j=0;j<ids.length;j++)if(ids[j]!==request.target.active_layer_id)ordered.push(ids[j]);ordered.push(request.target.active_layer_id);
            for(var j=0;j<ordered.length;j++){var r=new ActionReference(),d=new ActionDescriptor();r.putIdentifier(c('Lyr '),ordered[j]);d.putReference(c('null'),r);d.putBoolean(s('makeVisible'),false);if(j)d.putEnumerated(s('selectionModifier'),s('selectionModifierType'),s('addToSelection'));executeAction(s('select'),d,DialogModes.NO);}
        }
        function rollbackBatch(){restoring=true;progress('restore',0,0,true);batchTarget();doc.activeHistoryState=batchHistory;restoreBatchSelection();if(batchChanged){progress('redraw',0,0,true);app.refresh();batchRedraws++;}verify(originals());}
        function envelope(value){return {schema:1,id:request.id,session:request.session,operation:request.operation,token:request.token,phase:'completed',result:value};}
        try {
            if(!app.documents.length || ids.length<2 || ids.length>128)throw Error('Invalid batch selection');doc=app.activeDocument;batchTarget();batchValidate(request.paint);
            if(prepared.before.revision!==request.paint.revision || !same(prepared.identity,request.target))throw Error('Prepared batch identity changed');
            var wantedIDs={};for(var j=0;j<ids.length;j++)wantedIDs[ids[j]]=true;
            // Large trees retain the established path rather than imposing a new
            // document-size limit. The native path is a bounded optimization.
            var sizeRef=new ActionReference();sizeRef.putProperty(s('property'),s('numberOfLayers'));sizeRef.putIdentifier(s('document'),request.target.document_id);
            nativeReads=ids.length>=8 && executeActionGet(sizeRef).getInteger(s('numberOfLayers'))<4096 && request.legacy_dom_reads!==true;
            var snapshot=nativeReads?nativeTree():null;
            var found={};function walk(layers){var n=layers.length;for(var j=0;j<n;j++){var l=layers[j],id=l.id;if(wantedIDs[id])found[id]=l;if(l.typename==='LayerSet')walk(l.layers);}}if(!nativeReads)walk(doc.layers);
            progress('read',0,ids.length,true);stage='batch-baseline';var baselineOrders={};for(var j=0;j<ids.length;j++){
                var l=nativeReads?{id:ids[j]}:found[ids[j]];if(!l)throw Error('Batch target missing');var baseline=nativeReads?nativeMember(ids[j],snapshot,false):readMember(l,baselineOrders);
                var matches=prepared.native_batch_validation===2?samePreparedMember(baseline,prepared.members[j]):sameMember(baseline,prepared.members[j],prepared.members[j].mask);
                if(!matches)throw Error('Batch changed before native dispatch');records.push({layer:l,original:baseline,touched:false});progress('read',j+1,ids.length);
            }
            batchTarget();records.mapMask=originals();batchHistory=doc.activeHistoryState;
            var key='__psMenuBatch_'+request.id;if($.global[key])throw Error('Batch request already active');$.global[key]=batchLoop;batchStarted=true;
            stage='batch-history';try{doc.suspendHistory('批量羽化／密度','$.global['+encode(key)+']()');}finally{delete $.global[key];}
            if(!batchCommit)rollbackBatch();else {progress('commit',0,0,true);verify(records.mapMask);}
            var finalMasks=[],count=0;for(var j=0;j<records.length;j++){finalMasks.push({id:records[j].layer.id,mask:batchCommit?records.mapMask[j]:records[j].original.mask});if(batchCommit&&records[j].touched)count++;}
            var value={verified:true,mask_batch:true,count:count,placement_moves:0,layer_ids:ids,paint:batchLatest,masks:finalMasks,preview:{native_reads:nativeReads,native_batches:batchNativeCalls,updates:batchUpdates,redraws:batchRedraws,last_seq:batchSeq,engine:'standalone-extendscript'}};
            if(!batchCommit){value.cancelled=true;value.rollback_verified=true;}return envelope(value);
        }catch(error){var unknown=false;if(batchStarted){try{rollbackBatch();}catch(_){unknown=true;}}
            return {schema:1,id:request.id,session:request.session,operation:request.operation,token:request.token,phase:'failed',error:String(error),stage:stage,result_unknown:unknown||!!actionResidue,action_residue:actionResidue};}
    }

    progress('prepare',0,0,true);
    if(request.paint && request.paint.mask_batch===true)result=batchRun();
    else try {
        if(!app.documents.length)throw Error('Preview document closed');doc=app.activeDocument;anchor=doc.activeLayer;originalHistory=doc.activeHistoryState;
        target();validate(request.paint);if(prepared.before.revision!==request.paint.revision)throw Error('Prepared revision changed');stage='read-baseline';baseline=layerDescriptor();
        if(!same(raw(baseline),{fill:prepared.raw.fill,stroke:prepared.raw.stroke,vector:prepared.raw.vector}) || (prepared.before.mask && !maskClose(mask(baseline),prepared.before.mask)))throw Error('Prepared style changed before native dispatch');
        stage='clone-original';var list=baseline.getList(s('adjustment'));originalFill=clone(list.getObjectValue(0));originalFillType=list.getObjectType(0);originalStroke=baseline.hasKey(s('AGMStrokeStyleInfo'))?clone(baseline.getObjectValue(s('AGMStrokeStyleInfo'))):defaultStroke();
        stage='snapshot-invariants';originalPath=pathStream();originalPixel=pixelMask(baseline);originalOrder=order();originalParent=parentID();originalMask=prepared.raw.vector?mask(baseline):null;expectedRaw=raw(baseline);applied=prepared.before;latest=request.paint;
        if(originalParent!==prepared.parent_id || originalOrder.join(',')!==prepared.order.join(','))throw Error('Prepared parent or order changed');
        var key='__psMenuLegacy_'+request.id;if($.global[key])throw Error('Native request already active');$.global[key]=loop;historyStarted=true;
        stage='history-preview';try{doc.suspendHistory('Menu shape paint','$.global['+encode(key)+']()');}finally{delete $.global[key];}
        if(!commit){rollback();result={cancelled:true,rollback_verified:true,verified:true,paint:prepared.before};}
        else {progress('commit',0,0,true);var finalDescriptor=invariants();verifyPaint(finalDescriptor,latest);result={verified:true,count:changed?1:0,placement_moves:0,fill_kind:latest.fill.kind,stroke_kind:latest.stroke.kind,paint:latest};if(latest.mask)result.mask=latest.mask;}
        result.preview={updates:updates,redraws:redraws,last_seq:lastSeq,engine:'standalone-extendscript'};
        result={schema:1,id:request.id,session:request.session,operation:request.operation,token:request.token,phase:'completed',result:result};
    } catch(error) {
        var unknown=false,failureStage=stage,rollbackError='',failureLine=error.line;
        if(historyStarted){try{rollback();}catch(restoreError){unknown=true;rollbackError=String(restoreError);}}
        result={schema:1,id:request.id,session:request.session,operation:request.operation,token:request.token,phase:'failed',error:String(error),stage:failureStage,result_unknown:unknown};
        if(typeof failureLine==='number')result.error_line=failureLine;
        if(rollbackError){result.rollback_error=rollbackError;result.rollback_stage=stage;}
    }
    var text=encode(result),temporary=new File(folder+'/legacy-result-'+request.id+'.tmp');
    if(receipt.exists)throw Error('Native receipt already exists; never overwrite');
    temporary.encoding='UTF-8';if(!temporary.open('w'))throw Error('Cannot save native receipt');try{temporary.write(text);}finally{temporary.close();}
    if(!temporary.rename(receipt.name))throw Error('Cannot publish native receipt');
    return text;
})();
