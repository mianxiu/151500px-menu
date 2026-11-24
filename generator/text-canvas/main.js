"use strict";

const fs = require("fs");
const path = require("path");
const crypto = require("crypto");
const idPattern = /^[a-f0-9]{32}$/;
const identityScript = "(function(){if(!app.documents.length)return '';var d=app.activeDocument,s=stringIDToTypeID,r=new ActionReference();r.putEnumerated(s('historyState'),s('ordinal'),s('targetEnum'));return [d.id,d.activeLayer.id,executeActionGet(r).getInteger(s('ID')),d.width.as('px'),d.height.as('px'),d.mode===DocumentMode.RGB&&d.bitsPerChannel===BitsPerChannelType.EIGHT?1:0].join('|');})()";
const textGeometryScript = `(function(){
    try {
    var d=app.activeDocument,l=d.activeLayer,s=stringIDToTypeID;
    if(l.typename!=='ArtLayer'||l.kind!==LayerKind.TEXT)return '';
    var r=new ActionReference();
    r.putProperty(s('property'),s('textKey'));r.putEnumerated(s('layer'),s('ordinal'),s('targetEnum'));
    var a=executeActionGet(r).getObjectValue(s('textKey')),t=l.textItem,shapes=a.getList(s('textShape'));
    if(shapes.count!==1||typeIDToStringID(a.getEnumerationValue(s('orientation')))!=='horizontal')return '';
    if(a.hasKey(s('warp'))&&typeIDToStringID(a.getObjectValue(s('warp')).getEnumerationValue(s('warpStyle')))!=='warpNone')return '';
    // An axis-aligned canvas editor cannot represent rotated or skewed text.
    function upright(desc){
        if(!desc.hasKey(s('transform')))return true;
        var m=desc.getObjectValue(s('transform'));
        return Math.abs(m.getDouble(s('xy')))<.00001&&Math.abs(m.getDouble(s('yx')))<.00001
            &&Math.abs(m.getDouble(s('xx'))-1)<.00001&&Math.abs(m.getDouble(s('yy'))-1)<.00001;
    }
    if(!upright(a)||!upright(shapes.getObjectValue(0)))return '';
    var box=t.kind===TextType.PARAGRAPHTEXT,p=t.position,x=p[0].as('px'),y=p[1].as('px');
    var em=t.size.as('pt')*d.resolution/72;
    var color=t.color.rgb;
    return [box?'box':'point',x,y,box?t.width.as('px'):0,box?t.height.as('px'):0,em,color.red,color.green,color.blue].join('|');
    }catch(e){return '';}
})()`;

exports.init = function (generator) {
    const config = JSON.parse(fs.readFileSync(path.join(__dirname, "connection.json"), "utf8"));
    if (!path.isAbsolute(config.folder) || !idPattern.test(config.token)) throw Error("Invalid canvas connection");
    const session = crypto.randomBytes(16).toString("hex");
    function publish(name, data) {
        const file = path.join(config.folder, name);
        const tmp = file + "." + session + ".tmp";
        fs.writeFileSync(tmp, data);
        fs.renameSync(tmp, file);
    }
    const ready = {session, token:config.token, pid:process.pid, photoshopPid:process.ppid, version:3};
    publish("ready.json", JSON.stringify(ready));
    let active = false;
    const seen = new Map();
    async function read(name) {
        if (!/^read-[a-f0-9]{32}\.json$/.test(name)) return;
        const id = name.slice(5,-5);
        const started=process.hrtime();
        const elapsed=()=>{const [sec,nano]=process.hrtime(started);return Math.round(sec*1000+nano/1e6);};
        const trace={};
        let accepted = false, ownsRead = false;
        const result = {session,id,trace};
        try {
            const req = JSON.parse(fs.readFileSync(path.join(config.folder,name),"utf8"));
            const now=Date.now();
            if (req.session !== session || req.token !== config.token || req.id !== id || !(now <= req.expires && req.expires <= now+15000)) return;
            for(const [key,expires] of seen)if(expires<now)seen.delete(key);
            if(seen.has(id))return;
            seen.set(id,req.expires);accepted=true;
            if(active)throw Error("上一项背景读取仍在执行，请稍后重试背景");
            active=ownsRead=true;
            const before = String(await generator.evaluateJSXString(identityScript,true));
            const [doc,layer,history,width,height,rgb] = before.split("|").map(Number);
            trace.identity_ms=elapsed();
            if (doc !== req.doc || layer !== req.layer || !Number.isSafeInteger(history)){
                result.error_code="target";throw Error("背景读取目标已改变");
            }
            Object.assign(result,{doc,layer,history});
            Object.assign(trace,{source_width:width,source_height:height});
            if(!rgb || !Number.isFinite(width) || !Number.isFinite(height) || width<=0 || height<=0 || width*height>100000000){
                result.error_code="limited";
                throw Error(!rgb?"背景取图仅支持 RGB 8 位文档":"背景取图最多支持 1 亿像素的源画布");
            }
            const background=req.background===true,maxDimension=background?2048:640;
            const scale = Math.min(1,maxDimension/Math.max(width,height));
            const outW=Math.max(1,Math.round(width*scale)),outH=Math.max(1,Math.round(height*scale));
            const settings={inputRect:{left:0,top:0,right:width,bottom:height},outputRect:{left:0,top:0,right:outW,bottom:outH},clipToDocumentBounds:true,useICCProfile:"sRGB IEC61966-2.1",maxDimension};
            let pix=null,box=null,textFrame=null;
            if(background){
                const treeStarted=elapsed();
                const tree=await generator.getDocumentInfo(doc,{compInfo:false,imageInfo:false,layerInfo:true,expandSmartObjects:false,getTextStyles:false,getFullTextStyles:false,selectedLayers:false,getCompLayerSettings:true,getDefaultLayerFX:false});
                let selected=null,remaining=false,count=0;
                const hidden=[];
                function visit(rows,invisible){
                    for(const item of rows){
                        ++count;
                        const excluded=item.id===layer;
                        if(excluded)selected=item;
                        const hide=invisible || !item.visible || excluded;
                        if(hide)hidden.push(item.index);
                        if(item.type==="layerSection" && item.layers)visit(item.layers,hide);
                        else if(!hide)remaining=true;
                    }
                }
                visit(tree.layers,false);
                trace.tree_ms=elapsed()-treeStarted;trace.layers=count;
                if(!selected || !Number.isInteger(selected.index) || selected.type==="layerSection") throw Error("无法识别要排除的文字图层");
                const b=selected.bounds;
                if(b && [b.left,b.top,b.right,b.bottom].every(Number.isFinite))
                    box=[b.left/width,b.top/height,b.right/width,b.bottom/height];
                if(selected.type==="textLayer"){
                    const geometryStarted=elapsed();
                    const geometry=String(await generator.evaluateJSXString(textGeometryScript,true)).split("|");
                    trace.geometry_ms=elapsed()-geometryStarted;
                    const [x,y,w,h,em,r,g,blue]=geometry.slice(1).map(Number);
                    if(geometry.length===9&&[x,y,w,h,em,r,g,blue].every(Number.isFinite)&&em>0){
                        if(geometry[0]==="box" && w>0 && h>0)
                            textFrame={box:[x/width,y/height,(x+w)/width,(y+h)/height],wrap:true};
                        else if(geometry[0]==="point")
                            textFrame={box:[x/width,(y-em)/height,(Math.max(x,width)+em)/width,Math.max(y+em,height)/height],wrap:false};
                        if(textFrame)Object.assign(textFrame,{em:em/height,color:[r,g,blue].map(v=>Math.max(0,Math.min(255,Math.round(v))))});
                    }
                }
                const pixelsStarted=elapsed();
                if(remaining)pix=await generator.getPixmap(doc,{firstLayerIndex:0,lastLayerIndex:tree.layers[0].index,hidden},settings);
                trace.pixels_ms=elapsed()-pixelsStarted;
            }else{
                const pixelsStarted=elapsed();
                pix=await generator.getDocumentPixmap(doc,settings);
                trace.pixels_ms=elapsed()-pixelsStarted;
            }
            if (String(await generator.evaluateJSXString(identityScript,true))!==before){
                result.error_code="target";throw Error("取图期间画布已改变");
            }
            if(Date.now()>req.expires)throw Error("背景读取已过期，请重试背景");
            const publishStarted=elapsed();
            const rgba=Buffer.alloc(outW*outH*4);
            if(pix){
                const bounds=pix.bounds;
                if (!bounds || pix.bitsPerChannel!==8 || ![bounds.left,bounds.top,bounds.right,bounds.bottom].every(Number.isFinite))throw Error("画布边界属性无效");
                // Photoshop crops the pixmap to visible pixels on a transparent canvas.
                const originX=bounds.left,originY=bounds.top;
                if (!Number.isInteger(originX)||!Number.isInteger(originY)||bounds.right-originX!==pix.width||bounds.bottom-originY!==pix.height||
                    originX<0||originY<0||bounds.right>outW||bounds.bottom>outH)throw Error("画布位置与像素范围不一致 "+JSON.stringify([bounds,pix.width,pix.height,outW,outH]));
                for(let i=0;i<pix.width*pix.height;i++){
                    const p=pix.getPixel(i);
                    const index=((originY+Math.floor(i/pix.width))*outW+originX+i%pix.width)*4;
                    rgba[index]=p.r;rgba[index+1]=p.g;rgba[index+2]=p.b;rgba[index+3]=p.a===undefined?255:p.a;
                }
            }
            publish(id+".rgba",rgba);
            trace.publish_ms=elapsed()-publishStarted;
            Object.assign(result,{width:outW,height:outH,background,box,text_frame:textFrame});
        } catch(e) { result.error=String(e); }
        finally {
            if(ownsRead)active=false;
            if(accepted){trace.total_ms=elapsed();publish(id+".json",JSON.stringify(result));}
        }
    }
    fs.watch(config.folder,(event,name)=>{if(typeof name==="string")read(name);});
};
