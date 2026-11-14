import {test} from 'node:test';
import assert from 'node:assert/strict';
import {diagnosticClock,diagnosticScopes,diagnosticFinding,inputHealthRows,keyDecisions,toolReadReasons} from '../src/diagnostics.mjs';
import {translate} from '../src/i18n.mjs';

test('hook status and F decisions remain distinct and every visible label translates',()=>{
 const rows=inputHealthRows({state:'reporting',events:[{key:70,message:256,decision:'input-protected'},{key:70,message:257,decision:'no-action'}],focus:{state:'unknown'},configuration:{acknowledged:false}});
 assert.equal(new Map(rows).get('最近 F'),'输入保护未放行');
 assert.equal(new Map(inputHealthRows()).get('最近 F'),'尚无本轮按键记录');
 assert.equal(new Map(rows).get('Hook状态'),'健康上报持续更新');
 for(const [key,value]of [...rows,...inputHealthRows()])for(const text of [key,String(value)])if(/[\u3400-\u9fff]/u.test(text))assert.notEqual(translate(text,'en'),text,text);
 for(const text of Object.values(keyDecisions))assert.notEqual(translate(text,'en'),text);
 for(const text of Object.values(toolReadReasons))assert.notEqual(translate(text,'en'),text);
 assert.equal(new Map(inputHealthRows({tool_observation:{state:'failed',reason:'pending-session-changed'}})).get('工具状态读取'),'原工具观察属于旧会话');
 const imeRows=new Map(inputHealthRows({ime:{pending:false,legacy_pending:true,legacy_scope:'original-process-exited'}}));
 assert.equal(imeRows.get('当前IME请求待核对'),'否');assert.equal(imeRows.get('旧布局未知记录'),'历史未知已保留（原PS已退出）');
});

test('scoped checks and uncertain loader evidence are explicit',()=>{
 assert.deepEqual(diagnosticScopes.map(row=>row[0]),['all','menu','uxp','runtime']);
 assert.match(diagnosticFinding('uxp_user_external_scan_not_observed'),/尚非故障根因/);
 assert.equal(diagnosticFinding('untrusted secret'),'检测结果待核对');
 assert.equal(diagnosticClock({phase:'collecting',stage:'uxp',message:'uxp',remaining_ms:0},10).label,'uxp');
});
test('hook latency distinguishes local callbacks from downstream time',()=>{
 const rows=inputHealthRows({health:{keyboard_peak_us:20,pointer_peak_us:0,next_hook_peak_us:283}});
 const peaks=rows.filter(([label])=>label.includes('微秒'));
 assert.deepEqual(peaks.map(([,value])=>value),[20,0,283]);
 for(const [label] of peaks)assert.notEqual(translate(label,'en'),label);
 assert.equal(new Map(inputHealthRows({health:{callback_peak_us:45}})).get('回调峰值（微秒）'),45);
});
test('countdown advances locally and zero never claims completion',()=>{
 const d={phase:'collecting',stage:'prepare',remaining_ms:5000,received_at:1000,message:'prepare'};
 assert.equal(diagnosticClock(d,1000).seconds,5);
 assert.equal(diagnosticClock(d,2200).seconds,4);
 assert.equal(diagnosticClock(d,9000).seconds,0);
 assert.equal(diagnosticClock(d,9000).label,'正在开始采集，请保持PS前台');
 assert.equal(diagnosticClock({...d,stage:'sampling'},9000).label,'正在等待采集结束，请保持PS前台');
 assert.equal(diagnosticClock({...d,phase:'failed'},1000).showSeconds,false);
 assert.equal(diagnosticClock({...d,phase:'completed'},1000).showSeconds,false);
});
