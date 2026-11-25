const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const source = fs.readFileSync(path.join(__dirname, '../jsx/text_object.jsx'), 'utf8');
const functions = source.slice(source.indexOf('    function proofStep('), source.indexOf('    function renderProof('));
const documentFileStamp = source.slice(source.indexOf('    function documentFileStamp('), source.indexOf('    function proofStamp('));
const name = '151500px text acceptance ' + 'a'.repeat(32) + ' 72';

const saveSourcePdf = source.slice(source.indexOf('    function saveSourcePdf('), source.indexOf('    function renderSource('));
for (const failSave of [false, true]) {
    const layers = [{visible: true}, {visible: true}];
    const ctx = {layers, pdf: {}, PDFSaveOptions: class {}, Extension: {LOWERCASE: 'lowercase'},
        work: {saveAs() {
            assert.ok(layers.every(layer => !layer.visible));
            if (failSave) throw new Error('Save failed');
        }}};
    const exportPDF = () => vm.runInNewContext(saveSourcePdf + 'saveSourcePdf(layers)', ctx);
    if (failSave) assert.throws(exportPDF, /Save failed/); else exportPDF();
    assert.ok(layers.every(layer => layer.visible));
}

function fixture({limit = 20, documentName = name + '.psd', undoRestores = true, width = 600,
    fileExists = true, fileReference = true, unsaved = false} = {}) {
    const doc = {id: 7, name: documentName, historyStates: [41, 42], activeHistoryState: {name: 'before'},
        get fullName() {
            if (unsaved) throw Object.assign(new Error('Unsaved'), {number: 8103});
            return {exists: fileExists, fsName: 'document.psd'};
        },
        saved: !unsaved, mode: 'rgb', bitsPerChannel: 8, colorProfileName: 'sRGB IEC61966-2.1', resolution: 72,
        width: {as: () => width}, height: {as: () => 400}};
    const ctx = {
        doc, proof: false, inPlaceFixture: name, proofUnknown: false, work: null, historyId: 42,
        request: {id: 'b'.repeat(32)},
        source: {schema: 1, canvas: {width: 600, height: 400, resolution: 72, mode: 'RGB', depth: 8, profile: doc.colorProfileName}},
        app: {version: '24.0.1', documents: [doc], activeDocument: doc, preferences: {numberOfHistoryStates: limit}},
        historyAt: index => doc.historyStates[index - 1],
        historyIds: () => doc.historyStates.slice(),
        currentHistoryId: () => doc.historyStates.at(-1),
        proofStamp: () => JSON.stringify([doc.historyStates, vm.runInContext(documentFileStamp + 'documentFileStamp()', ctx)]),
        s: value => value,
        ActionReference: class {putIdentifier(type, id) { assert.equal(type, 'document'); assert.equal(id, doc.id); }},
        DescValueType: {ALIASTYPE: 'alias'},
        executeActionGet: () => ({hasKey: () => fileReference, getType: () => 'alias', getPath: () => ({fsName: 'document.psd'})}),
        DocumentMode: {RGB: 'rgb'}, BitsPerChannelType: {EIGHT: 8},
        DialogModes: {NO: 0},
        ActionDescriptor: class {},
        renders: 0,
        groups: 0
    };
    ctx.grouped = (operation, title) => {
        ++ctx.groups;
        doc.activeHistoryState = {name: title};
        doc.historyStates.push(43);
        operation();
    };
    ctx.renderSource = inPlace => {
        ++ctx.renders;
        assert.equal(inPlace, true);
        assert.equal(ctx.work, doc);
        assert.deepEqual(ctx.app.documents, [doc]);
    };
    ctx.executeAction = action => {
        assert.equal(action, 'undoEvent');
        if (undoRestores) doc.historyStates.pop();
    };
    return vm.createContext(ctx);
}

const restored = fixture({width: 600.0000000000001});
const renderOwned = functions + 'renderInPlace(currentSourceStamp())';
assert.deepEqual([...vm.runInContext(renderOwned, restored)], [41, 42]);
assert.equal(restored.renders, 1);
assert.equal(restored.work, null);
assert.equal(restored.proofUnknown, false);
for (const options of [{limit: 2}, {documentName: 'an open PSD'}, {width: 601}, {fileExists: false}]) {
    const rejected = fixture(options);
    assert.throws(() => vm.runInContext(renderOwned, rejected), /需要匹配的文字文档/);
    assert.equal(rejected.groups, 0);
    assert.equal(rejected.renders, 0);
}
const unknown = fixture({undoRestores: false});
assert.throws(() => vm.runInContext(renderOwned, unknown), /状态不一致/);
assert.equal(unknown.work, null);
assert.equal(unknown.groups, 1);
assert.equal(unknown.renders, 1);
assert.equal(unknown.proofUnknown, true);

const production = fixture({documentName: 'editable.psd'});
production.inPlaceFixture = undefined;
assert.deepEqual([...vm.runInContext(renderOwned, production)], [41, 42]);
for (const options of [{limit: 2}, {fileExists: false}, {unsaved: true}]) {
    const fallback = fixture(options);
    fallback.inPlaceFixture = undefined;
    assert.equal(vm.runInContext(functions + 'currentSourceStamp()', fallback), null);
    assert.equal(fallback.groups, 0);
}
const untitled = fixture({documentName: name, unsaved: true, fileReference: false});
assert.deepEqual([...vm.runInContext(renderOwned, untitled)], [41, 42]);
assert.equal(untitled.doc.saved, false);
const withRedo = fixture();
withRedo.doc.historyStates.push(43);
assert.equal(vm.runInContext(functions + 'currentSourceStamp()', withRedo), null);
const sixteenBit = fixture();
sixteenBit.doc.bitsPerChannel = 16;
assert.equal(vm.runInContext(functions + 'currentSourceStamp()', sixteenBit), null);

const canUpdateOriginal = source.slice(source.indexOf('    function canUpdateOriginal('), source.indexOf('    function replace(factor)'));
const anchor = {id: 31, kind: 'smart', contentId: 'embedded-A'};
const layer = {id: 32, kind: 'smart', contentId: 'embedded-B'};
const group = layers => ({typename: 'LayerSet', layers});
function preservesLayer({layers = [anchor, group([layer])], schema = 1, proof = false, linked = false,
    version = '24.0.1'} = {}) {
    const ctx = {
        anchor, proof, doc: {layers}, app: {version}, source: {schema}, prepared: {kind: 'smart-object'},
        LayerKind: {SMARTOBJECT: 'smart'}, object: value => value,
        property: () => ({smartObject: {linked, placed: {_value: 'vectorData'}}}),
        embeddedId: value => {
            if (!value.contentId) throw Error('unreadable content identity');
            return value.contentId;
        }
    };
    return vm.runInNewContext(canUpdateOriginal + 'canUpdateOriginal()', ctx);
}
assert.equal(preservesLayer(), true);
assert.equal(preservesLayer({layers: [anchor, group([group([{...layer, contentId: anchor.contentId}])])]}), false);
assert.equal(preservesLayer({layers: [anchor, {...layer, contentId: ''}]}), false);
assert.equal(preservesLayer({layers: [layer]}), false);
for (const options of [{linked: true}, {schema: 2}, {proof: true}, {version: '25.0.0'}]) {
    assert.equal(preservesLayer(options), false);
}

const restorePlacement = source.slice(source.indexOf('    function restorePlacement('), source.indexOf('    function proofStep('));
function failedPlacement({title = 'Vector text object', history = [41, 42, 43], restoreSaved = true,
    restoreLayer = true, otherDocument = false} = {}) {
    const ctx = fixture();
    const doc = ctx.doc;
    doc.historyStates = history.slice();
    doc.activeHistoryState = {name: title};
    doc.activeLayer = {id: 12};
    doc.saved = false;
    if (otherDocument) ctx.app.activeDocument = {id: 8};
    Object.assign(ctx, {
        placementTitle: 'Vector text object', placementBaseline: {histories: '[41,42]', saved: true}, updatedOriginal: false,
        encode: JSON.stringify, prepared: {before_xmp: 'baseline'}, metadata: () => 'baseline', undos: 0
    });
    ctx.request.target = {active_layer_id: 9};
    ctx.executeAction = action => {
        assert.equal(action, 'undoEvent');
        ++ctx.undos;
        doc.historyStates.pop();
        if (restoreSaved) doc.saved = true;
        if (restoreLayer) doc.activeLayer.id = 9;
    };
    return ctx;
}
const placed = failedPlacement();
vm.runInContext(restorePlacement + 'restorePlacement()', placed);
assert.equal(placed.undos, 1);
const damagedContent = failedPlacement();
damagedContent.updatedOriginal = true;
damagedContent.prepared.before_render = 'expected';
damagedContent.renderHash = suffix => {
    assert.equal(suffix, 'rollback');
    return 'other';
};
assert.throws(() => vm.runInContext(restorePlacement + 'restorePlacement()', damagedContent), /rollback content mismatch/);
assert.equal(damagedContent.undos, 1);
for (const options of [{title: 'Another operation'}, {history: [41, 42, 44, 43]}, {otherDocument: true}]) {
    const foreign = failedPlacement(options);
    assert.throws(() => vm.runInContext(restorePlacement + 'restorePlacement()', foreign), /rollback target changed|Cannot identify/);
    assert.equal(foreign.undos, 0);
}
for (const options of [{restoreLayer: false}, {restoreSaved: false}, {history: [42, 43]}]) {
    const partial = failedPlacement(options);
    assert.throws(() => vm.runInContext(restorePlacement + 'restorePlacement()', partial), /rollback state mismatch/);
    assert.equal(partial.undos, 1);
}
console.log('In-place vector prototype: single export; full history or foreign fixture rejected; unknown restore stops.');
