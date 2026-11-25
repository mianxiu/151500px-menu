// Diagnostic only. The caller must verify the active service/session is idle.
// All filter and placement commands are restricted to a newly owned document.
(function () {
    var s = stringIDToTypeID, c = charIDToTypeID;
    var original = app.documents.length ? app.activeDocument : null;
    var before = original && snapshot(original);
    var previousDialogs = app.displayDialogs, owned = null, other = null;
    var result = {};

    function snapshot(doc) {
        var r = new ActionReference();
        r.putEnumerated(s('historyState'), s('ordinal'), s('targetEnum'));
        return [doc.id, doc.activeLayer.id, doc.saved, executeActionGet(r).getInteger(s('ID'))].join('|');
    }
    function activate(layer) {
        if (!owned || app.activeDocument.id !== owned.id) throw Error('Test document lost activation');
        owned.activeLayer = layer;
    }
    function objectId(doc, layer) {
        var r = new ActionReference();
        r.putProperty(s('property'), s('smartObjectMore'));
        r.putIdentifier(s('layer'), layer.id);
        r.putIdentifier(s('document'), doc.id);
        return executeActionGet(r).getObjectValue(s('smartObjectMore')).getString(s('ID'));
    }
    function place(event, path) {
        if (app.activeDocument.id !== owned.id) throw Error('Wrong placement document');
        var descriptor = new ActionDescriptor();
        descriptor.putPath(s('null'), new File(path));
        descriptor.putInteger(s('crop'), 2);
        executeAction(event, descriptor, DialogModes.NO);
    }
    function exportPdf(doc, layer, tag) {
        if (app.activeDocument.id !== doc.id || (doc !== owned && doc !== other))
            throw Error('Test document lost activation');
        doc.activeLayer = layer;
        var file = new File(TEST_DIRECTORY + '/' + tag + '.pdf');
        if (file.exists) throw Error('Readback file already exists');
        var d = new ActionDescriptor();
        d.putPath(s('null'), file);
        executeAction(s('placedLayerExportContents'), d, DialogModes.NO);
        if (!file.exists || file.length < 100) throw Error('Could not export PDF evidence');
    }

    try {
        app.displayDialogs = DialogModes.NO;
        owned = app.documents.add(144, 144, 72, '151500px vector identity test', NewDocumentMode.RGB, DocumentFill.TRANSPARENT);
        var layer = owned.artLayers.add();
        layer.kind = LayerKind.TEXT;
        layer.textItem.position = [UnitValue(10, 'px'), UnitValue(40, 'px')];
        layer.textItem.contents = 'editable';
        activate(layer);
        var layerId = layer.id, count = owned.layers.length;
        layer.textItem.contents = 'updated';
        result.ordinary = layer.id === layerId && owned.layers.length === count && layer.textItem.contents === 'updated';
        try { layer.applyGaussianBlur(1); result.filter = 'applied'; }
        catch (error) { result.filter = String(error); }
        result.filterPreservedEditableText = layer.kind === LayerKind.TEXT && layer.textItem.contents === 'updated';

        var first = TEST_DIRECTORY + '/source-a.pdf', second = TEST_DIRECTORY + '/source-b.pdf';
        place(c('Plc '), first);
        var smart = owned.activeLayer, smartId = smart.id;
        other = app.documents.add(144, 144, 72, '151500px cross-document peer test', NewDocumentMode.RGB, DocumentFill.TRANSPARENT);
        app.activeDocument = owned;
        activate(smart);
        var cross = smart.duplicate(other, ElementPlacement.PLACEATBEGINNING);
        app.activeDocument = owned;
        result.crossDocumentSharesId = objectId(other, cross) === objectId(owned, smart);
        count = owned.layers.length;
        place(s('placedLayerReplaceContents'), second);
        result.uniqueSameLayer = owned.activeLayer.id === smartId && owned.layers.length === count;
        exportPdf(owned, smart, 'unique-target-after');
        app.activeDocument = other;
        exportPdf(other, cross, 'cross-after');
        app.activeDocument = owned;
        activate(smart);
        place(s('placedLayerReplaceContents'), first);
        var shared = smart.duplicate();
        var sharedId = shared.id;
        activate(smart);
        var baseObject = objectId(owned, smart);
        activate(shared);
        result.sharedBefore = objectId(owned, shared) === baseObject;
        if (!result.sharedBefore) throw Error('Could not prove that both placements start with shared content');
        exportPdf(owned, shared, 'shared-before');
        activate(smart);
        count = owned.layers.length;
        place(s('placedLayerReplaceContents'), second);
        result.sameLayer = owned.activeLayer.id === smartId && owned.layers.length === count;
        exportPdf(owned, smart, 'target-after');
        result.peerSameLayer = shared.id === sharedId;
        exportPdf(owned, shared, 'peer-after');
        if (!result.sameLayer || !result.peerSameLayer) throw Error('Replacement changed layer identity');
    } finally {
        if (other) {
            if (app.activeDocument.id !== other.id && app.activeDocument.id !== owned.id) throw Error('Test document ownership changed before cleanup');
            app.activeDocument = other;
            other.close(SaveOptions.DONOTSAVECHANGES);
        }
        if (owned) {
            app.activeDocument = owned;
            owned.close(SaveOptions.DONOTSAVECHANGES);
        }
        app.displayDialogs = previousDialogs;
        if (original) {
            app.activeDocument = original;
            if (snapshot(original) !== before)
                throw Error('Original document state changed');
        }
    }
    return [result.ordinary, result.filterPreservedEditableText, result.sharedBefore,
        result.sameLayer, result.peerSameLayer, result.crossDocumentSharesId,
        result.uniqueSameLayer, result.filter].join('|');
})()
