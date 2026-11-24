/* One PDF text object transaction. Called only while UXP reserves this request. */
(function(){
    var request=__PS_MENU_TEXT_OBJECT_REQUEST__,s=stringIDToTypeID,c=charIDToTypeID;
    var prepared=request.prepared,source=request.source,doc=null,anchor=null,work=null,history=null,historyId=null,changed=false,result,colorType,colorValue,documentsBefore=null,dialogs=null,identityConflict=false;
    var proof=request.object_proof===true,scale=request.proof_scale||1,proofUnknown=false,fontFallbacks=[];
    var isolated=proof&&request.proof_isolated===true;
    var inPlaceFixture=request.in_place_fixture,inPlaceStamp=null;
    var folder=new Folder(request.folder),pdf=new File(folder+'/text-object-'+request.id+'.pdf'),receipt=new File(folder+'/legacy-result-'+request.id+'.json');
    var proofFile=new File(folder+'/text-proof-'+request.id+'.png');
    __PS_MENU_TEXT_HASH_HELPER__
    function encode(v){
        if(v===null)return 'null';
        if(typeof v==='string')return '"'+v.replace(/["\\\x00-\x1f]/g,function(ch){return '\\u'+('0000'+ch.charCodeAt(0).toString(16)).slice(-4);})+'"';
        if(typeof v==='number'){if(!isFinite(v))throw Error('Invalid source number');return String(v);}
        if(typeof v==='boolean')return String(v);
        var a=[],i,keys=[];if(v instanceof Array){for(i=0;i<v.length;i++)a.push(encode(v[i]));return '['+a.join(',')+']';}
        for(i in v)if(v.hasOwnProperty(i)&&typeof v[i]!=='undefined')keys.push(i);
        keys.sort();for(i=0;i<keys.length;i++)a.push(encode(keys[i])+':'+encode(v[keys[i]]));return '{'+a.join(',')+'}';
    }
    function name(id){return typeIDToStringID(id)||typeIDToCharID(id);}
    function documents(){var ids=[];for(var i=0;i<app.documents.length;i++)ids.push(app.documents[i].id);ids.sort(function(a,b){return a-b;});return ids;}
    function contains(ids,id){for(var i=0;i<ids.length;i++)if(ids[i]===id)return true;return false;}
    function unit(id){if(id===c('#Pxl'))return 'pixelsUnit';if(id===c('#Pnt'))return 'pointsUnit';if(id===c('#Prc'))return 'percentUnit';if(id===c('#Ang'))return 'angleUnit';return name(id);}
    function object(d){var o={},i,k;for(i=0;i<d.count;i++){k=d.getKey(i);o[name(k)]=item(d,k);}return o;}
    function item(d,k){
        var t=d.getType(k),o,i;
        if(t===DescValueType.OBJECTTYPE){o=object(d.getObjectValue(k));o._obj=name(d.getObjectType(k));return o;}
        if(t===DescValueType.LISTTYPE){o=[];var a=d.getList(k);for(i=0;i<a.count;i++)o.push(item(a,i));return o;}
        if(t===DescValueType.ENUMERATEDTYPE)return {_enum:name(d.getEnumerationType(k)),_value:name(d.getEnumerationValue(k))};
        if(t===DescValueType.UNITDOUBLE)return {_unit:unit(d.getUnitDoubleType(k)),_value:d.getUnitDoubleValue(k)};
        if(t===DescValueType.DOUBLETYPE)return d.getDouble(k);if(t===DescValueType.INTEGERTYPE)return d.getInteger(k);
        if(t===DescValueType.BOOLEANTYPE)return d.getBoolean(k);if(t===DescValueType.STRINGTYPE)return d.getString(k);
        throw Error('Unsupported text descriptor');
    }
    function property(key){var r=new ActionReference();r.putProperty(s('property'),s(key));r.putIdentifier(s('layer'),app.activeDocument.activeLayer.id);return executeActionGet(r);}
    function currentHistoryId(){var r=new ActionReference();r.putEnumerated(s('historyState'),s('ordinal'),s('targetEnum'));return executeActionGet(r).getInteger(s('ID'));}
    function rawText(){return object(property('textKey')).textKey;}
    function metadata(){var d=property('XMPMetadataAsUTF8');return d.hasKey(s('XMPMetadataAsUTF8'))?d.getString(s('XMPMetadataAsUTF8')):'';}
    function setMetadata(xml){var r=new ActionReference(),a=new ActionDescriptor(),v=new ActionDescriptor();r.putProperty(s('property'),s('XMPMetadataAsUTF8'));r.putIdentifier(s('layer'),app.activeDocument.activeLayer.id);a.putReference(s('null'),r);v.putString(s('XMPMetadataAsUTF8'),xml);a.putObject(s('to'),s('layer'),v);executeAction(s('set'),a,DialogModes.NO);}
    function renderHash(suffix){var file=new File(folder+'/text-object-'+suffix+'-'+request.id+'.pdf');if(file.exists)throw Error('Render evidence already exists');var a=new ActionDescriptor();a.putPath(c('null'),file);executeAction(s('placedLayerExportContents'),a,DialogModes.NO);if(file.length>16777216)throw Error('Vector source exceeds16MiB');file.encoding='BINARY';if(!file.open('r'))throw Error('Cannot read vector source');try{var bytes=file.read();if(bytes.slice(0,5)!=='%PDF-')throw Error('Vector source format mismatch');return sha256(bytes);}finally{file.close();}}
    function finalMetadata(){
        var digest=renderHash('render');if(!ExternalObject.AdobeXMPScript)ExternalObject.AdobeXMPScript=new ExternalObject('lib:AdobeXMPScript');
        var ns='https://15100px.menu/ns/text-object/1.0/';XMPMeta.registerNamespace(ns,'pxText');
        var meta=new XMPMeta(request.xmp);meta.setProperty(ns,'render',digest);
        if(fontFallbacks.length)meta.setProperty(ns,'fontFallbacks',encode(fontFallbacks));
        else meta.deleteProperty(ns,'fontFallbacks');
        request.xmp=meta.serialize();
        var value={schema:1,id:request.id,session:request.session,operation:request.operation,token:request.token,xmp:request.xmp},file=new File(folder+'/text-object-final-'+request.id+'.json');if(file.exists)throw Error('Final source evidence already exists');file.encoding='UTF-8';if(!file.open('w'))throw Error('Cannot persist final source evidence');try{file.write(encode(value));}finally{file.close();}
    }
    function target(start){
        if(!app.documents.length||app.activeDocument.id!==request.target.document_id||app.activeDocument.activeLayer.id!==request.target.active_layer_id)throw Error('Text object target changed');
        if(request.target.layer_ids.length!==1)throw Error('Select one text object');
        if(start){
            var now=new Date().getTime();
            if(source.renderer==='list-v1'){
                if(!(prepared.render_started_ms<=request.expires_at_ms&&prepared.render_started_ms>request.expires_at_ms-5000&&prepared.render_finished_ms>=prepared.render_started_ms&&now>=prepared.render_finished_ms-1000&&now-prepared.render_started_ms<=45000&&now-prepared.render_finished_ms<=5000))throw Error('文字对象排版身份已过期；未提交修改');
            }else if(now>request.expires_at_ms)throw Error('Text object request expired before execution');
        }
    }
    function put(d,key,v,list){
        var k=list?null:s(key),t=typeof v;
        function call(method,args){if(!list)args.unshift(k);d[method].apply(d,args);}
        if(t==='string')call('putString',[v]);else if(t==='boolean')call('putBoolean',[v]);
        else if(t==='number')call(v===Math.floor(v)?'putInteger':'putDouble',[v]);
        else if(v instanceof Array){var a=new ActionList();for(var i=0;i<v.length;i++)put(a,null,v[i],true);call('putList',[a]);}
        else if(v&&v._unit)call('putUnitDouble',[s(v._unit),v._value]);
        else if(v&&v._enum)call('putEnumerated',[s(v._enum),s(v._value)]);
        else if(v&&v._obj)call('putObject',[s(v._obj),descriptor(v)]);else throw Error('Unsupported source value');
    }
    function descriptor(value){var d=new ActionDescriptor();for(var key in value)if(value.hasOwnProperty(key)&&key!=='_obj')put(d,key,value[key],false);return d;}
    function applyText(raw){
        var to={_obj:'textLayer'},keys=['textKey','textStyleRange','paragraphStyleRange','antiAlias','orientation','textGridding','textShape','textClickPoint','transform','warp','kerningRange'];
        for(var i=0;i<keys.length;i++)if(raw.hasOwnProperty(keys[i]))to[keys[i]]=raw[keys[i]];
        if(to.kerningRange)to.kerningRange=to.kerningRange.slice().reverse();
        var a=new ActionDescriptor(),r=new ActionReference();r.putEnumerated(s('textLayer'),s('ordinal'),s('targetEnum'));a.putReference(s('null'),r);a.putObject(s('to'),s('textLayer'),descriptor(to));executeAction(s('set'),a,DialogModes.NO);
    }
    function signature(style){var o={},key;if(style.baseParentStyle)o=signature(style.baseParentStyle);for(key in style)if(style.hasOwnProperty(key)&&key!=='_obj'&&key!=='baseParentStyle'&&key!=='styleSheetHasParent'&&key!=='fontAvailable'&&key!=='fontName'&&key!=='fontStyleName'&&key.indexOf('implied')!==0)o[key]=style[key];return o;}
    function checkText(actual,wanted,original,edit){
        if(actual.textKey!==wanted.textKey)throw Error('Text source content readback mismatch');
        function pairs(value){
            var out=[],rows=value.kerningRange||[],previous=0;
            for(var i=0;i<rows.length;i++){
                var row=rows[i];
                if(row._obj!=='kerningRange'||row.from<previous||row.to<=row.from||
                   row.to>value.textKey.length||row.kerning!==Math.floor(row.kerning))throw Error('Invalid native kerning range');
                for(var at=row.from;at<row.to;){
                    var code=value.textKey.charCodeAt(at),next=at+(code>=0xd800&&code<=0xdbff?2:1);
                    if(next>row.to||(code>=0xdc00&&code<=0xdfff))throw Error('Invalid native kerning boundary');
                    out.push([at,next,row.kerning]);at=next;
                }
                previous=row.to;
            }
            return out;
        }
        if(encode(pairs(actual))!==encode(pairs(wanted)))throw Error('Native kerning readback mismatch');
        function glyphAt(text,offset){
            if(typeof offset!=='number'||offset<0||offset>=text.length)return '';
            var code=text.charCodeAt(offset);
            if(code>=0xdc00&&code<=0xdfff&&offset>0)offset--;
            code=text.charCodeAt(offset);
            return text.substr(offset,code>=0xd800&&code<=0xdbff?2:1);
        }
        function allowsFallback(offset,font,replacement){
            if(!original||!edit||!edit.origins||offset>=edit.origins.length)return false;
            var styles=edit.typography_runs||(edit.typography?[edit.typography]:[]);
            for(var i=0;i<styles.length;i++){
                var style=styles[i],range=style.range;
                if(style.fontPostScriptName&&(!range||offset>=range[0]&&offset<range[1]))return false;
            }
            if(edit.list_layout&&edit.list_layout.marker_font==='SegoeUISymbol'&&font==='SegoeUISymbol')return false;
            var origin=edit.origins[offset];
            if(glyphAt(wanted.textKey,offset)!==glyphAt(original.textKey,origin))return true;
            var rows=edit.previous_fallbacks||[],lo=0,hi=rows.length;
            while(lo<hi){
                var middle=Math.floor((lo+hi)/2),row=rows[middle];
                if(origin<row[0])hi=middle;
                else if(origin>=row[1])lo=middle+1;
                else return row[2]===replacement;
            }
            return false;
        }
        var fallbacks=[];
        // Photoshop can expand a sparse Optyca paragraph to explicit defaults on a new text layer.
        var optycaDefaults={
            align:{_enum:'alignmentType',_value:'left'},autoLeadingPercentage:1.2,
            burasagari:{_enum:'burasagari',_value:'burasagariStandard'},hangingRoman:false,hyphenate:false,
            justificationGlyphDesired:1,justificationGlyphMaximum:1,justificationGlyphMinimum:1,
            justificationLetterDesired:0,justificationLetterMaximum:0,justificationLetterMinimum:0,
            justificationWordDesired:1,justificationWordMaximum:1.33,justificationWordMinimum:.8,
            kashidaWidthType:{_enum:'kashidaWidthType',_value:'kashidaWidthMedium'},
            leadingType:{_enum:'leadingType',_value:'leadingBelow'},
            preferredKinsokuOrder:{_enum:'preferredKinsokuOrder',_value:'pushIn'},textEveryLineComposer:true
        };
        var keys=['textStyleRange','paragraphStyleRange'];
        for(var k=0;k<keys.length;k++){
            var field=k===0?'textStyle':'paragraphStyle',a=actual[keys[k]],b=wanted[keys[k]],ai=0,bi=0;
            function effective(rows,index){
                var o=signature(rows[0][field].baseParentStyle||{}),v=signature(rows[index][field]);
                for(var p in v)if(v.hasOwnProperty(p))o[p]=v[p];
                // The PostScript name identifies the face; Photoshop recalculates its script and technology tags.
                if(k===0&&typeof o.fontPostScriptName==='string'){delete o.fontScript;delete o.fontTechnology;}
                if(k===0&&o.autoLeading===true)delete o.leading;
                if(k===1){
                    var defaults={hyphenateLimit:0,hyphenateCapitalized:true,hyphenationZone:36,hyphenatePreLength:2,hyphenatePostLength:2,hyphenateWordSize:6};
                    // Photoshop can materialize a shared dictionary's settings on a paragraph
                    // with hyphenation off. Those fields affect rendering only when it is on.
                    for(var fieldName in defaults)if(defaults.hasOwnProperty(fieldName)){
                        if(o.hyphenate!==true)delete o[fieldName];
                        else if(o[fieldName]===undefined)o[fieldName]=defaults[fieldName];
                    }
                }
                if(k===1&&o.textComposerEngine&&o.textComposerEngine._value==='textOptycaComposer'&&
                    o.directionType&&o.directionType._value==='dirLeftToRight'){
                    for(var fieldName in optycaDefaults)if(optycaDefaults.hasOwnProperty(fieldName)&&near(o[fieldName],optycaDefaults[fieldName]))delete o[fieldName];
                }
                var keys=k===0?['baselineShift']:['startIndent','endIndent','firstLineIndent','spaceBefore','spaceAfter'];
                for(var q=0;q<keys.length;q++){
                    var key=keys[q],b=o[key];
                    if(b===undefined||b===0)o[key]=0;
                    else if(b&&b._unit==='pointsUnit')o[key]=Math.round(b._value*source.canvas.resolution/72*1000)/1000;
                    else if(b&&b._unit==='pixelsUnit')o[key]=Math.round(b._value*1000)/1000;
                }
                if(k===1)for(var key in o)if(o.hasOwnProperty(key)&&/^justification(?:Word|Letter|Glyph)(?:Minimum|Desired|Maximum)$/.test(key)&&typeof o[key]==='number')
                    o[key]=Math.round(o[key]*100000)/100000;
                var complete=encode(o),kern=o.autoKern,font=o.fontPostScriptName,fields={};
                for(var key in o)if(o.hasOwnProperty(key))fields[key]=o[key];
                var available=false;
                if(k===0){
                    var style=rows[index][field],parent=style.baseParentStyle;
                    available=style.fontAvailable===true;
                    if(!available&&parent)available=parent.fontAvailable===true;
                }
                delete o.autoKern;
                var kernless=encode(o);
                if(kern)o.autoKern=kern;
                delete o.fontPostScriptName;
                return {complete:complete,kerning:kern?kern._value:null,kernless:kernless,font:font,fontless:encode(o),available:available,fields:fields};
            }
            var left=[],right=[],j;for(j=0;j<a.length;j++)left.push(effective(a,j));for(j=0;j<b.length;j++)right.push(effective(b,j));
            for(var offset=0;offset<wanted.textKey.length;offset++){
                while(ai<a.length-1&&offset>=a[ai].to)ai++;
                while(bi<b.length-1&&offset>=b[bi].to)bi++;
                var spanA=left[ai],spanB=right[bi],text=wanted.textKey;
                var noPair=k===0&&(offset===text.length-1||text.charAt(offset)==='\r'||text.charAt(offset+1)==='\r');
                var terminalKern=noPair&&spanA.kerning==='metricsKern'&&spanB.kerning==='opticalKern'&&spanA.kernless===spanB.kernless;
                // Photoshop selects an installed font for a newly entered glyph missing in the current face.
                var fallback=k===0&&spanA.available&&typeof spanA.font==='string'&&spanA.font!==spanB.font&&
                    spanA.fontless===spanB.fontless&&allowsFallback(offset,spanB.font,spanA.font);
                if(spanA.complete!==spanB.complete&&!terminalKern&&!fallback){
                    var fieldNames={},different=[];
                    for(var name in spanA.fields)if(spanA.fields.hasOwnProperty(name))fieldNames[name]=true;
                    for(var name in spanB.fields)if(spanB.fields.hasOwnProperty(name))fieldNames[name]=true;
                    for(var name in fieldNames)if(fieldNames.hasOwnProperty(name)){
                        var x=spanA.fields[name],y=spanB.fields[name];
                        var leftValue=typeof x==='undefined'?'undefined':encode(x);
                        var rightValue=typeof y==='undefined'?'undefined':encode(y);
                        if(leftValue!==rightValue)different.push(name+(k===1&&
                            (typeof x==='number'||typeof x==='boolean'||typeof x==='undefined')&&
                            (typeof y==='number'||typeof y==='boolean'||typeof y==='undefined')?
                            '='+leftValue+'/'+rightValue:''));
                    }
                    throw Error('Text source style readback mismatch: '+field+' at '+offset+' '+different.sort().join(', '));
                }
                if(fallback){
                    var previous=fallbacks[fallbacks.length-1];
                    if(previous&&previous[1]===offset&&previous[2]===spanA.font)previous[1]++;
                    else fallbacks.push([offset,offset+1,spanA.font]);
                }
            }
        }
        if(fallbacks.length>1024)throw Error('Too many resolved font fallback ranges');
        return fallbacks;
    }
    function checkSourceFonts(raw){var runs=raw.textStyleRange;for(var i=0;i<runs.length;i++){var font=signature(runs[i].textStyle).fontPostScriptName;if(typeof font!=='string'||!font)throw Error('Source font identity missing');try{app.fonts.getByName(font);}catch(e){throw Error('Source font unavailable: '+font);}}}
    function layout(value){var shape=value.textShape&&value.textShape[0];return {orientation:value.orientation,transform:value.transform,warp:value.warp&&value.warp.warpStyle&&value.warp.warpStyle._value==='warpNone'?value.warp.warpStyle:value.warp,shape:{'char':shape['char'],orientation:shape.orientation,transform:shape.transform,bounds:shape['char']._value==='box'?shape.bounds:null}};}
    function near(a,b){if(typeof a==='number'&&typeof b==='number')return Math.abs(a-b)<.00001;if(a===b)return true;if(!a||!b||typeof a!=='object'||typeof b!=='object')return false;var k,keys=0;for(k in a)if(a.hasOwnProperty(k)){keys++;if(!b.hasOwnProperty(k)||!near(a[k],b[k]))return false;}for(k in b)if(b.hasOwnProperty(k))keys--;return keys===0;}
    function drawTable(canvas,rows){
        var columns=rows[0].length,cw=canvas.width/columns,rh=canvas.height/rows.length;
        var rule=Math.max(.5,Math.min(canvas.resolution/72,cw/12,rh/12)),padding=Math.max(rule,Math.min(4*canvas.resolution/72,cw/8,rh/8));
        var font=app.fonts.getByName('MicrosoftYaHei'),ink=new SolidColor();ink.rgb.hexValue='202020';
        var pt=72/canvas.resolution,commands=[],bytes=0;
        function num(n){if(!isFinite(n))throw Error('Invalid table path coordinate');return n.toFixed(5);}
        function emit(op){bytes+=op.length+1;if(bytes>15000000)throw Error('Table vector paths exceed 15 MB');commands.push(op);}
        function line(x,y,w,h){emit([num(x*pt),num(y*pt),num(w*pt),num(h*pt),'re f'].join(' '));}
        function point(p){
            function coord(v){if(v._unit==='pixelsUnit')return num(v._value*pt);if(v._unit==='distanceUnit'||v._unit==='pointsUnit')return num(v._value);throw Error('Unsupported glyph path units');}
            return coord(p.horizontal)+' '+coord(p.vertical);
        }
        function outline(ti){
            ti.createPath();
            var ref=new ActionReference();ref.putProperty(s('property'),s('pathContents'));ref.putEnumerated(s('path'),s('ordinal'),s('targetEnum'));
            var shapes=object(executeActionGet(ref)).pathContents.pathComponents;
            if(!shapes.length)throw Error('Missing table glyph outlines');
            for(var n=0;n<shapes.length;n++){
                if(shapes[n].shapeOperation._value!=='add')throw Error('Unsupported glyph path operation');
                var curves=shapes[n].subpathListKey;
                for(var j=0;j<curves.length;j++){
                    var pts=curves[j].points;if(!curves[j].closedSubpath||!pts.length)throw Error('Incomplete glyph path');
                    emit(point(pts[0].anchor)+' m');
                    for(var k=0;k<pts.length;k++){
                        var from=pts[k],to=pts[(k+1)%pts.length],end=point(to.anchor),out=point(from.forward||from.anchor),incoming=point(to.backward||to.anchor);
                        emit(out===point(from.anchor)&&incoming===end?end+' l':[out,incoming,end,'c'].join(' '));
                    }
                    emit('h');
                }
                emit('f*');
            }
            var path=null;
            for(var w=0;w<work.pathItems.length;w++)if(work.pathItems[w].kind===PathKind.WORKPATH){
                if(path)throw Error('Multiple work paths');path=work.pathItems[w];
            }
            if(!path)throw Error('Generated work path missing');
            path.remove();
        }
        emit('0.125 g');emit('1 0 0 -1 0 '+num(canvas.height*pt)+' cm');
        for(var i=0;i<=columns;i++)line(Math.min(i*cw,canvas.width-rule),0,rule,canvas.height);
        for(var j=0;j<=rows.length;j++)line(0,Math.min(j*rh,canvas.height-rule),canvas.width,rule);
        for(var y=0;y<rows.length;y++)for(var x=0;x<columns;x++){
            var value=rows[y][x];if(!value.replace(/\s/g,''))continue;
            var l=work.artLayers.add();l.kind=LayerKind.TEXT;var ti=l.textItem;
            ti.kind=TextType.POINTTEXT;ti.font=font.postScriptName;ti.size=UnitValue(Math.min(12,rh*.55*72/canvas.resolution),'pt');
            ti.color=ink;ti.justification=Justification.LEFT;ti.contents=value;
            ti.position=[UnitValue(x*cw+padding,'px'),UnitValue(y*rh+rh*.65,'px')];
            var b=l.bounds,ww=b[2].as('px')-b[0].as('px'),hh=b[3].as('px')-b[1].as('px');
            var factor=Math.min(1,(cw-2*padding)/Math.max(1,ww),(rh-2*padding)/Math.max(1,hh));
            if(factor<1)ti.size=UnitValue(ti.size.as('pt')*factor*.98,'pt');
            b=l.bounds;
            l.translate(UnitValue(x*cw+padding-b[0].as('px'),'px'),UnitValue(y*rh+(rh-(b[3].as('px')-b[1].as('px')))/2-b[1].as('px'),'px'));
            b=l.bounds;
            if(ti.contents!==value||b[0].as('px')<x*cw-.5||b[2].as('px')>(x+1)*cw+.5||b[1].as('px')<y*rh-.5||b[3].as('px')>(y+1)*rh+.5)throw Error('Cell text does not fit at row '+(y+1)+', column '+(x+1));
            outline(ti);l.remove();
        }
        var stream=commands.join('\n')+'\n';
        var objects=['<< /Type /Catalog /Pages 2 0 R >>','<< /Type /Pages /Kids [3 0 R] /Count 1 >>',
            '<< /Type /Page /Parent 2 0 R /MediaBox [0 0 '+num(canvas.width*pt)+' '+num(canvas.height*pt)+'] /Resources << >> /Contents 4 0 R >>',
            '<< /Length '+stream.length+' >>\nstream\n'+stream+'endstream'];
        var data='%PDF-1.4\n',offsets=['0000000000 65535 f '];
        for(var o=0;o<objects.length;o++){offsets.push(('0000000000'+data.length).slice(-10)+' 00000 n ');data+=(o+1)+' 0 obj\n'+objects[o]+'\nendobj\n';}
        var xref=data.length;data+='xref\n0 5\n'+offsets.join('\n')+'\ntrailer\n<< /Size 5 /Root 1 0 R >>\nstartxref\n'+xref+'\n%%EOF\n';
        pdf.encoding='BINARY';if(pdf.exists||!pdf.open('w'))throw Error('Cannot create table vector PDF');
        try{if(!pdf.write(data))throw Error('Incomplete table vector PDF');}finally{pdf.close();}
        if(pdf.length!==data.length)throw Error('Table vector PDF size mismatch');
    }
    function replace(factor){
        factor=factor||1;
        target();changed=true;
        if(prepared.kind==='smart-object'){
            executeAction(s('placedLayerMakeCopy'),new ActionDescriptor(),DialogModes.NO);
            replaceContents();
        }else{
            var a=new ActionDescriptor();a.putPath(c('null'),pdf);a.putEnumerated(c('FTcs'),c('QCSt'),c('Qcsa'));a.putInteger(s('crop'),2);executeAction(c('Plc '),a,DialogModes.NO);
        }
        var created=doc.activeLayer;if(created.id===anchor.id||created.kind!==LayerKind.SMARTOBJECT)throw Error('Vector object creation failed');
        var vector=object(property('smartObject')).smartObject;if(vector.placed._value!=='vectorData'||vector.linked)throw Error('Embedded vector source readback mismatch');
        var more=object(property('smartObjectMore')).smartObjectMore,frame=more.transform;
        var expected=prepared.kind==='smart-object'?prepared.transform:[0,0,source.canvas.width,0,source.canvas.width,source.canvas.height,0,source.canvas.height];
        if(factor!==1&&expected){var scaled=[];for(var n=0;n<expected.length;n++)scaled.push(factor*expected[n]);expected=scaled;}
        if(prepared.kind==='new-table'){
            var w=source.canvas.width*factor,h=source.canvas.height*factor;
            // Photoshop snaps each edge of a newly centred placement to a pixel.
            var fw=frame[2]-frame[0],fh=frame[5]-frame[1],px=source.canvas.resolution*factor/72;
            if(!more.size||Math.abs(more.size.width*px-w)>.01||Math.abs(more.size.height*px-h)>.01||fw<=0||fh<=0||Math.abs(fw-w)>1.01||Math.abs(fh-h)>1.01)throw Error('Table intrinsic size or placement mismatch');
            expected=[frame[0],frame[1],frame[0]+fw,frame[1],frame[0]+fw,frame[1]+fh,frame[0],frame[1]+fh];
        }
        if(prepared.kind==='text'&&Math.abs(frame[2]-frame[0]-expected[2])<.01&&Math.abs(frame[5]-frame[1]-expected[5])<.01&&Math.abs(frame[3]-frame[1])<.01&&Math.abs(frame[6]-frame[0])<.01){created.translate(UnitValue(-frame[0],'px'),UnitValue(-frame[1],'px'));frame=object(property('smartObjectMore')).smartObjectMore.transform;}
        if(!expected||frame.length!==8)throw Error('Text object frame missing');for(var i=0;i<8;i++)if(Math.abs(frame[i]-expected[i])>.01)throw Error('Text object placement readback mismatch: '+encode({crop:more.crop,frame:frame,expected:expected,size:more.size}));
        if(created.parent.typename!==anchor.parent.typename||created.parent.id!==anchor.parent.id)throw Error('Vector object parent changed');
        finalMetadata();setMetadata(request.xmp);if(metadata()!==request.xmp)throw Error('Text object metadata readback mismatch');
        var label=new ActionDescriptor(),value=new ActionDescriptor(),ref=new ActionReference();ref.putIdentifier(s('layer'),created.id);label.putReference(s('null'),ref);value.putEnumerated(s('color'),colorType,colorValue);label.putObject(s('to'),s('layer'),value);executeAction(s('set'),label,DialogModes.NO);
        if(property('color').getEnumerationValue(s('color'))!==colorValue)throw Error('Text object color label readback mismatch');
        if(prepared.kind==='new-table')created.name='表格';
        else{created.name=anchor.name;created.visible=anchor.visible;anchor.remove();}
        if(doc.activeLayer.id!==created.id)throw Error('Text object selection readback mismatch');
        result={verified:true,count:1,layer_id:created.id,source_sha256:prepared.source_sha256,engine:'native-pdf-vector'};
    }
    function replaceContents(){
        var a=new ActionDescriptor();a.putPath(s('null'),pdf);a.putInteger(s('crop'),2);
        executeAction(s('placedLayerReplaceContents'),a,DialogModes.NO);
    }
    function saveSourcePdf(layers){
        var options=new PDFSaveOptions(),hidden=[],restoreError=null;
        options.preserveEditing=false;options.embedColorProfile=true;
        try{
            for(var i=0;i<layers.length;i++){hidden.push(layers[i]);layers[i].visible=false;}
            work.saveAs(pdf,options,true,Extension.LOWERCASE);
        }finally{
            // Visibility can survive history navigation in Photoshop. Restore
            // every layer before leaving the suspended export history step.
            for(var j=hidden.length-1;j>=0;j--)try{
                hidden[j].visible=true;
                if(!hidden[j].visible)throw Error('导出后图层可见状态无法恢复');
            }catch(error){restoreError=error;}
            if(restoreError)throw restoreError;
        }
    }
    function renderSource(inPlace){
        var canvas=source.canvas;
        if(source.schema===3){drawTable(canvas,source.table);return;}
        checkSourceFonts(source.layer.text);
        var layers=[];
        if(inPlace)for(var i=0;i<work.layers.length;i++)if(work.layers[i].visible)layers.push(work.layers[i]);
        var raw=source.layer.text,shape=raw.textShape&&raw.textShape[0],matrix=raw.transform||{xx:1,yy:1,xy:0,yx:0};
        if(!shape||raw.textShape.length!==1||(shape['char']._value!=='paint'&&shape['char']._value!=='box')||raw.orientation._value!=='horizontal'||raw.warp&&raw.warp.warpStyle._value!=='warpNone'||(shape.columnCount||1)!==1||(shape.rowCount||1)!==1||Math.abs(matrix.xx-1)>.00001||Math.abs(matrix.yy-1)>.00001||Math.abs(matrix.xy)>.00001||Math.abs(matrix.yx)>.00001)throw Error('Vector source currently requires horizontal, unwarped, single-column text');
        var copy=work.artLayers.add();copy.kind=LayerKind.TEXT;
        if(inPlace&&copy.parent.typename!=='Document')copy.move(work.layers[0],ElementPlacement.PLACEBEFORE);
        if(inPlace&&copy.parent.typename!=='Document')throw Error('无法隔离草稿文字层');
        copy.visible=true;copy.textItem.kind=shape['char']._value==='box'?TextType.PARAGRAPHTEXT:TextType.POINTTEXT;
        var click=source.layer.text.textClickPoint;
        if(!click||!click.horizontal||!click.vertical||click.horizontal._unit!=='percentUnit'||click.vertical._unit!=='percentUnit')throw Error('Unsupported native text anchor units');
        var position=[UnitValue(click.horizontal._value*canvas.width/100,'px'),UnitValue(click.vertical._value*canvas.height/100,'px')];
        var hasKerns=raw.kerningRange&&raw.kerningRange.length;
        // Repositioning after the descriptor write can collapse adjacent manual pairs.
        if(hasKerns)copy.textItem.position=position;
        applyText(raw);
        if(!hasKerns)copy.textItem.position=position;
        var edit=request.edit;
        var original=prepared.before_text||(edit&&typeof edit.original_text==='string'&&{textKey:edit.original_text});
        var actualText=rawText();fontFallbacks=checkText(actualText,raw,original,edit);
        if(!near(layout(actualText),layout(raw)))throw Error('Text source layout readback mismatch');
        if(Math.abs(actualText.textClickPoint.horizontal._value-click.horizontal._value)>.00001||Math.abs(actualText.textClickPoint.vertical._value-click.vertical._value)>.00001)throw Error('Text anchor readback mismatch');
        saveSourcePdf(layers);
    }
    function historyAt(index){
        var r=new ActionReference();r.putIndex(s('historyState'),index);return executeActionGet(r).getInteger(s('ID'));
    }
    function historyIds(){
        var ages=[];
        for(var i=1;i<=doc.historyStates.length;i++)ages.push(historyAt(i));
        return ages;
    }
    function proofStamp(){
        var layers=[],paths=[];
        function visibility(parent){
            for(var j=0;j<parent.layers.length;j++){
                var l=parent.layers[j];layers.push([l.id,l.visible]);
                if((isolated||inPlaceFixture)&&l.typename==='LayerSet')visibility(l);
            }
        }
        visibility(doc);
        for(var k=0;k<doc.pathItems.length;k++){var p=doc.pathItems[k];paths.push([p.name,String(p.kind)]);}
        function docProperty(name){
            var r=new ActionReference();r.putProperty(s('property'),s(name));
            r.putEnumerated(s('document'),s('ordinal'),s('targetEnum'));
            return item(executeActionGet(r),s(name));
        }
        var state=[doc.saved,currentHistoryId(),historyIds(),layers,paths,doc.activeLayer.id,documents(),metadata(),
            doc.width.as('px'),doc.height.as('px'),doc.resolution,docProperty('zoom'),docProperty('center')];
        if(inPlaceFixture){
            var r=new ActionReference(),selection=s('targetLayersIDs'),ids=[];
            r.putProperty(s('property'),selection);r.putEnumerated(s('document'),s('ordinal'),s('targetEnum'));
            var selected=executeActionGet(r).getList(selection);
            for(var n=0;n<selected.count;n++)ids.push(selected.getReference(n).getIdentifier());
            state.push(doc.name,doc.fullName.fsName,ids);
        }
        return encode(state);
    }
    function grouped(operation,name){
        var key='__psMenuTextObject_'+request.id;if($.global[key])throw Error('Text object already running');$.global[key]=operation;
        try{doc.suspendHistory(name,'$.global['+encode(key)+']()');}finally{delete $.global[key];}
    }
    function proofStep(operation,name){
        var before=proofStamp(),failure=null;
        try{grouped(operation,name);}catch(e){failure=e;}
        try{
            if(!app.documents.length||app.activeDocument.id!==doc.id)throw Error('草稿文档已改变');
            if(currentHistoryId()!==historyId){
                if(doc.activeHistoryState.name!==name)throw Error('无法确认草稿历史归属');
                executeAction(s('undoEvent'),new ActionDescriptor(),DialogModes.NO);
            }
            if(proofStamp()!==before)throw Error('草稿后原文档状态不一致');
        }catch(e){proofUnknown=true;throw e;}
        if(failure)throw failure;
    }
    function renderInPlace(){
        var canvas=source.canvas,count=doc.historyStates.length,limit=app.preferences.numberOfHistoryStates;
        if(proof||source.schema!==1||!/^151500px text acceptance [a-f0-9]{32} (72|300)$/.test(inPlaceFixture)||
           (doc.name!==inPlaceFixture&&doc.name!==inPlaceFixture+'.psd')||!count||!limit||count>=limit||historyAt(count)!==historyId||
           canvas.mode!=='RGB'||canvas.depth!==8||Math.abs(canvas.width-doc.width.as('px'))>.00001||Math.abs(canvas.height-doc.height.as('px'))>.00001||
           canvas.profile!==String(doc.colorProfileName)||Math.abs(canvas.resolution-doc.resolution)>.001)
            throw Error('原位导出需要匹配的隔离文字文档和可用的撤销历史');
        inPlaceStamp=proofStamp();
        var ages=historyIds();
        work=doc;
        try{proofStep(function(){renderSource(true);},'排版生成 PDF '+request.id);}
        finally{work=null;}
        return ages;
    }
    function renderProof(){
        var width=Math.round(doc.width.as('px')),height=Math.round(doc.height.as('px')),canvas=source.canvas;
        if(scale!==Math.floor(scale)||scale<1||scale>8||scale*width>8192||scale*height>8192||scale*scale*width*height>16777216)throw Error('草稿预览分辨率超出范围');
        // History at capacity would evict an older user step even if our own step is undone.
        var count=doc.historyStates.length,limit=app.preferences.numberOfHistoryStates;
        if(!count||!limit||count>=limit||historyAt(count)!==historyId)throw Error('当前撤销历史无法安全生成矢量草稿');
        if(canvas.profile!==String(doc.colorProfileName)||Math.abs(canvas.resolution-doc.resolution)>.001||
           source.schema===1&&(canvas.width!==width||canvas.height!==height))throw Error('矢量源和当前文档的画布规格不同');
        if(source.schema===3)for(var i=0;i<doc.pathItems.length;i++)if(doc.pathItems[i].kind===PathKind.WORKPATH)
            throw Error('当前文档已有工作路径，无法安全生成表格草稿');
        if(source.renderer!=='list-v1'){
            work=doc;
            try{proofStep(function(){renderSource(true);},'排版草稿 PDF '+request.id);}
            finally{work=null;}
        }
        proofStep(function(){
            if(scale>1){
                // Place the same PDF at the preview density so Photoshop rebuilds its vector pixels.
                doc.resizeImage(UnitValue(width*scale,'px'),UnitValue(height*scale,'px'),doc.resolution*scale,ResampleMethod.BICUBIC);
                if(Math.round(doc.width.as('px'))!==width*scale||Math.round(doc.height.as('px'))!==height*scale)
                    throw Error('高清草稿尺寸或图层未确认');
            }
            replace(scale);
            var hidden=[];
            try{
                if(isolated){
                    var layer=doc.activeLayer;
                    while(layer.typename!=='Document'){
                        var parent=layer.parent;
                        for(var i=0;i<parent.layers.length;i++){
                            var sibling=parent.layers[i];
                            if(sibling.id!==layer.id&&sibling.visible){
                                hidden.push(sibling);sibling.visible=false;
                                if(sibling.visible)throw Error('无法隐藏草稿背景图层');
                            }
                        }
                        layer=parent;
                    }
                }
                var png=new PNGSaveOptions();png.compression=1;png.interlaced=false;
                doc.saveAs(proofFile,png,true,Extension.LOWERCASE);
            }finally{
                var visibilityError=null;
                for(var h=0;h<hidden.length;h++)try{hidden[h].visible=true;}catch(e){visibilityError=e;}
                if(visibilityError)throw visibilityError;
            }
            if(!proofFile.exists||proofFile.length<8)throw Error('没有可读取的草稿像素');
        },'排版草稿 PNG '+request.id);
        result={verified:true,restored:true,frame_id:request.id,document_id:doc.id,layer_id:request.target.active_layer_id,
            history:historyId,width:width*scale,height:height*scale,scale:scale};
        if(isolated)result.isolated=true;
    }
    try{
        if(!/^[0-9a-f]{32}$/.test(request.id))throw Error('Invalid text object identity');
        if(receipt.exists||proofFile.exists||pdf.exists&&source.renderer!=='list-v1'||new File(folder+'/legacy-result-'+request.id+'.tmp').exists||new File(folder+'/text-object-before-'+request.id+'.pdf').exists||new File(folder+'/text-object-render-'+request.id+'.pdf').exists||new File(folder+'/text-object-final-'+request.id+'.json').exists){identityConflict=true;throw Error('Text object identity already used; preserve original unknown');}
        if(request.command!=='text.object'||!source||(source.schema!==1&&source.schema!==3&&!(source.schema===2&&source.renderer==='list-v1'))||!prepared)throw Error('Invalid text object command');
        if(sha256('abc')!=='ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad')throw Error('SHA256 self-check failed');
        target(true);doc=app.activeDocument;anchor=doc.activeLayer;history=doc.activeHistoryState;historyId=currentHistoryId();documentsBefore=documents();
        if(proof&&(!prepared.proofBefore||prepared.proofBefore.history!==historyId))throw Error('矢量草稿的文档历史已改变');
        dialogs=app.displayDialogs;app.displayDialogs=DialogModes.NO;
        var label=property('color');colorType=label.getEnumerationType(s('color'));colorValue=label.getEnumerationValue(s('color'));
        if(metadata()!==prepared.before_xmp)throw Error('Text object metadata changed');
        if(prepared.before_text&&encode(rawText())!==encode(prepared.before_text))throw Error('Text source changed');
        if(prepared.before_render&&renderHash('before')!==prepared.before_render)throw Error('Vector content changed outside this tool; original object was not updated');
        if(prepared.kind==='text'){
            var check=new ActionReference();check.putIdentifier(s('layer'),anchor.id);var state=executeActionGet(check);
            if(anchor.opacity!==100||anchor.fillOpacity!==100||anchor.blendMode!==BlendMode.NORMAL||anchor.grouped||state.hasKey(s('layerEffects'))||state.hasKey(s('hasUserMask'))&&state.getBoolean(s('hasUserMask'))||state.hasKey(s('hasVectorMask'))&&state.getBoolean(s('hasVectorMask')))throw Error('Text object creation currently requires normal text without masks, effects or clipping');
        }
        if(source.renderer==='list-v1'){
            if(!/^[0-9a-f]{64}$/.test(prepared.vector_pdf_sha256)||!pdf.exists||pdf.length>16777216)throw Error('矢量文字输出凭据缺失');
            pdf.encoding='BINARY';if(!pdf.open('r'))throw Error('无法核对矢量文字输出');var bytes;try{bytes=pdf.read();}finally{pdf.close();}if(sha256(bytes)!==prepared.vector_pdf_sha256)throw Error('矢量文字输出已改变');
        }
        if(request.proof_history!==undefined&&request.proof_history!==historyId)throw Error('画布自草稿预览后已改变');
        if(proof)renderProof();
        else{
            var retainedHistory=null;
            if(inPlaceFixture)retainedHistory=renderInPlace();
            else if(source.renderer!=='list-v1'){
                var canvas=source.canvas;if(canvas.mode!=='RGB'||canvas.depth!==8)throw Error('Vector text currently requires RGB8 source');
                work=app.documents.add(UnitValue(canvas.width,'px'),UnitValue(canvas.height,'px'),canvas.resolution,'Text object '+request.id,NewDocumentMode.RGB,DocumentFill.TRANSPARENT,1,BitsPerChannelType.EIGHT,canvas.profile);
                if(contains(documentsBefore,work.id))throw Error('Source workspace identity invalid');
                renderSource(false);
                work.close(SaveOptions.DONOTSAVECHANGES);work=null;app.activeDocument=doc;target();
                if(encode(documents())!==encode(documentsBefore))throw Error('Source document cleanup mismatch');
            }
            if(currentHistoryId()!==historyId||metadata()!==prepared.before_xmp||prepared.before_text&&encode(rawText())!==encode(prepared.before_text))throw Error('Original changed during source generation');
            grouped(replace,'Vector text object');
            if(retainedHistory){
                var committedHistory=historyIds(),last=committedHistory.pop();
                if(last===historyId||last!==currentHistoryId()||encode(committedHistory)!==encode(retainedHistory))
                    throw Error('原位导出没有保留原有撤销历史及一次确认写入');
                result.source_workspace='in-place';
                result.retained_history_count=retainedHistory.length;
            }
        }
        if(encode(documents())!==encode(documentsBefore))throw Error('Final document inventory mismatch');
        result={schema:1,id:request.id,session:request.session,operation:request.operation,token:request.token,phase:'completed',result:result};
    }catch(error){
        var unknown=identityConflict||proofUnknown,restore='';
        try{if(work){if(contains(documentsBefore,work.id))throw Error('Source workspace ownership unavailable');app.activeDocument=work;work.close(SaveOptions.DONOTSAVECHANGES);work=null;}if(doc){app.activeDocument=doc;if(changed&&!proof){doc.activeHistoryState=history;doc.activeLayer=anchor;if(metadata()!==prepared.before_xmp)throw Error('Metadata restore mismatch');if(prepared.before_text&&encode(rawText())!==encode(prepared.before_text))throw Error('Text restore mismatch');}if(encode(documents())!==encode(documentsBefore))throw Error('Source document cleanup unknown');if(inPlaceStamp&&proofStamp()!==inPlaceStamp)throw Error('原位导出后文档历史或状态待核对');}}catch(e){unknown=true;restore=String(e);}
        result={schema:1,id:request.id,session:request.session,operation:request.operation,token:request.token,phase:'failed',error:String(error),error_line:error.line,result_unknown:unknown,not_executed:!changed&&!unknown,rollback_error:restore};
    }
    try{if(dialogs!==null)app.displayDialogs=dialogs;}catch(error){result.phase='failed';result.result_unknown=true;result.error='Photoshop dialog state restore unknown: '+String(error);}
    var text=encode(result),temporary=new File(folder+'/legacy-result-'+request.id+'.tmp');
    if(receipt.exists)throw Error('Native receipt already exists');temporary.encoding='UTF-8';if(!temporary.open('w'))throw Error('Cannot persist native result');try{temporary.write(text);}finally{temporary.close();}if(!temporary.rename(receipt.name))throw Error('Cannot publish native result');return text;
})();
