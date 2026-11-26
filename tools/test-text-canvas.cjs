const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const vm = require('node:vm');

const root = path.resolve(__dirname, '../generator/text-canvas');
const folder = fs.mkdtempSync(path.join(os.tmpdir(), 'ps-menu-canvas-'));
const token = 'a'.repeat(32);
let watch;
const localFs = {
    ...fs,
    watch: (_, callback) => { watch = callback; },
    readFileSync: (file, ...args) => file === path.join(root, 'connection.json')
        ? JSON.stringify({folder, token}) : fs.readFileSync(file, ...args)
};
const plugin = {};
vm.runInNewContext(fs.readFileSync(path.join(root, 'main.js'), 'utf8'), {
    exports: plugin, __dirname: root, Buffer, process,
    require: name => name === 'fs' ? localFs : require(name)
});
let composite = {width: 0, height: 0, bitsPerChannel: 0, bounds: {left: 0, top: 0, right: 0, bottom: 0}};
const target = {doc: 7, layer: 9};
plugin.init({
    evaluateJSXString: async script => script.includes('var box=t.kind') ? 'point|1|2|0|0|1|0|0|0' : '7|9|5|4|3|1',
    getDocumentInfo: async () => ({layers: [
        {id: 10, visible: true, type: 'layer', index: 2},
        {id: 9, visible: true, type: 'textLayer', index: 1}
    ]}),
    getPixmap: async () => composite
});
const {session} = JSON.parse(fs.readFileSync(path.join(folder, 'ready.json'), 'utf8'));

async function request(id) {
    const name = `read-${id}.json`;
    fs.writeFileSync(path.join(folder, name), JSON.stringify({session, token, id, ...target,
        background: true, expires: Date.now() + 8000}));
    watch('rename', name);
    await new Promise(setImmediate);
    return JSON.parse(fs.readFileSync(path.join(folder, `${id}.json`), 'utf8'));
}

(async () => {
    try {
        const emptyId = 'b'.repeat(32);
        const empty = await request(emptyId);
        assert.equal(empty.error, undefined);
        assert.equal(empty.text_frame.wrap, false);
        assert.deepEqual(fs.readFileSync(path.join(folder, `${emptyId}.rgba`)), Buffer.alloc(4 * 3 * 4));

        composite = {width: 1, height: 1, bitsPerChannel: 8, bounds: {left: 2, top: 1, right: 3, bottom: 2},
            getPixel: () => ({r: 23, g: 42, b: 19, a: 207})};
        const croppedId = 'c'.repeat(32);
        assert.equal((await request(croppedId)).error, undefined);
        const expected = Buffer.alloc(4 * 3 * 4);
        expected.set([23, 42, 19, 207], 4 * (4 + 2));
        assert.deepEqual(fs.readFileSync(path.join(folder, `${croppedId}.rgba`)), expected);

        composite = {width: 1, height: 1, bitsPerChannel: 0};
        assert.match((await request('d'.repeat(32))).error, /画布边界属性无效/);
        console.log('Transparent composites, cropped pixel placement, and invalid pixmap rejection passed.');
    } finally {
        for (const name of fs.readdirSync(folder)) fs.unlinkSync(path.join(folder, name));
        fs.rmdirSync(folder);
    }
})().catch(error => { console.error(error); process.exitCode = 1; });
