const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync('jsx/legacy_shape_preview.jsx','utf8');
const parser=source.slice(source.indexOf('    function decode('),source.indexOf('    function same('));
const box={};vm.runInNewContext(parser+'; this.parse=decode;',box);
for(const v of [null,true,false,0,-3.25,1e20,'中文\n"\\\u2028',[1,'x',false],{paint:{fill:{kind:'gradient',stops:[{rgb:[12,34,56],location:4096}]},mask:{feather:1000,density:0}}}])
 assert.deepEqual(JSON.parse(JSON.stringify(box.parse(JSON.stringify(v)))),v);
for(const text of ['{"__proto__":{}}','{"constructor":{}}','{"a":1,"a":2}','[1,]','{"x":}','01','NaN','Infinity','1e9999','{} trailing','"line\nfeed"','"\\uXX00"','[[]','{"x":(this.compromised=true)}','['.repeat(18)+'0'+']'.repeat(18),'"'+'x'.repeat(16384)+'"'])assert.throws(()=>box.parse(text));
assert.equal(box.compromised,undefined);
console.log('PASS legacy mailbox JSON: bounded strict parsing, Unicode, numeric limits, depth, duplicate/prototype keys and no code evaluation');
