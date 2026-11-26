(function(){
    var expected=__CANVAS_TARGET__,s=stringIDToTypeID;
    function history(){
        var reference=new ActionReference();
        reference.putEnumerated(s('historyState'),s('ordinal'),s('targetEnum'));
        return executeActionGet(reference).getInteger(s('ID'));
    }
    function matches(doc){
        return app.documents.length && app.activeDocument.id===doc.id && doc.id===expected.doc &&
            doc.activeLayer.id===expected.layer && history()===expected.history &&
            doc.mode===DocumentMode.RGB && doc.bitsPerChannel===BitsPerChannelType.EIGHT &&
            doc.width.as('px')===expected.width && doc.height.as('px')===expected.height;
    }
    if(!app.documents.length)return 'stale';
    var doc=app.activeDocument;
    if(!matches(doc))return 'stale';
    for(var i=0;i<doc.layers.length;i++){
        var layer=doc.layers[i];
        if(!layer.visible||layer.id===expected.layer)continue;
        if(layer.typename!=='ArtLayer'||layer.kind!==LayerKind.NORMAL||layer.isBackgroundLayer||layer.grouped)return 'unsupported';
        var bounds=layer.bounds;
        for(var n=0;n<4;n++)if(bounds[n].as('px')!==0)return 'unsupported';
        var ref=new ActionReference();ref.putIdentifier(s('layer'),layer.id);
        var desc=executeActionGet(ref);
        if(desc.hasKey(s('layerEffects')) || desc.hasKey(s('hasUserMask'))&&desc.getBoolean(s('hasUserMask')) ||
           desc.hasKey(s('hasVectorMask'))&&desc.getBoolean(s('hasVectorMask')))return 'unsupported';
    }
    var geometry=__TEXT_GEOMETRY__;
    return matches(doc)?'empty|'+geometry:'stale';
})()
