(function(){
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
})()
