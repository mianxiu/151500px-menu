const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const source = fs.readFileSync(path.join(__dirname, '../jsx/text_object.jsx'), 'utf8');
const functions = source.slice(source.indexOf('    function proofStep('), source.indexOf('    function renderProof('));
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

function fixture({limit = 20, documentName = name + '.psd', undoRestores = true, width = 600} = {}) {
    const doc = {id: 7, name: documentName, historyStates: [41, 42], activeHistoryState: {name: 'before'},
        colorProfileName: 'sRGB IEC61966-2.1', resolution: 72, width: {as: () => width}, height: {as: () => 400}};
    const ctx = {
        doc, proof: false, inPlaceFixture: name, proofUnknown: false, work: null, historyId: 42,
        request: {id: 'b'.repeat(32)},
        source: {schema: 1, canvas: {width: 600, height: 400, resolution: 72, mode: 'RGB', depth: 8, profile: doc.colorProfileName}},
        app: {documents: [doc], activeDocument: doc, preferences: {numberOfHistoryStates: limit}},
        historyAt: index => doc.historyStates[index - 1],
        historyIds: () => doc.historyStates.slice(),
        currentHistoryId: () => doc.historyStates.at(-1),
        proofStamp: () => JSON.stringify(doc.historyStates),
        s: value => value,
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
assert.deepEqual([...vm.runInContext(functions + 'renderInPlace()', restored)], [41, 42]);
assert.equal(restored.renders, 1);
assert.equal(restored.work, null);
assert.equal(restored.proofUnknown, false);
for (const options of [{limit: 2}, {documentName: 'an open PSD'}, {width: 601}]) {
    const rejected = fixture(options);
    assert.throws(() => vm.runInContext(functions + 'renderInPlace()', rejected), /需要匹配的隔离文字文档/);
    assert.equal(rejected.groups, 0);
    assert.equal(rejected.renders, 0);
}
const unknown = fixture({undoRestores: false});
assert.throws(() => vm.runInContext(functions + 'renderInPlace()', unknown), /状态不一致/);
assert.equal(unknown.work, null);
assert.equal(unknown.groups, 1);
assert.equal(unknown.renders, 1);
assert.equal(unknown.proofUnknown, true);
console.log('In-place vector prototype: single export; full history or foreign fixture rejected; unknown restore stops.');
