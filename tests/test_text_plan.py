import time
import unittest
from backend.text_plan import compute,evaluate,units
from backend.text_tools import mapped_parameters


class TextPlanTests(unittest.TestCase):
    def list(self,source,kind='numbered',**options):
        return self.plan(source,'list',**{'kind':kind,'marker':'auto','custom_marker':'','numbering':'decimal','start':1,**options})

    def test_multilevel_lists_restart_children_and_preserve_blank_lines_and_styles(self):
        source='Parent\n\tChild 😀\n\tSecond\n\t\tDeep\n\nNext\n\tChild'
        value=compute(source,{'revision':'r','draft':source,'operation':'list','kind':'numbered','marker':'auto','custom_marker':'','numbering':'hierarchical','start':3})
        self.assertEqual(value['value'],'3.\tParent\r    3.1.\tChild 😀\r    3.2.\tSecond\r        3.2.1.\tDeep\r\r4.\tNext\r    4.1.\tChild')
        self.assertEqual(value['count'],6)
        self.assertEqual(self.list(source,'bullet')['value'],'•\tParent\r    ◦\tChild 😀\r    ◦\tSecond\r        ▪\tDeep\r\r•\tNext\r    ◦\tChild')
        self.assertTrue(all(a<=b for a,b in zip(value['origins'],value['origins'][1:])))
        self.assertEqual(value['origins'][value['value'].index('😀')],source.index('😀'))
        nested=self.list(' \tBody','bullet')
        self.assertEqual(nested['value'],'    ◦\tBody');self.assertEqual(nested['origins'][4:6],[2,2])

    def test_list_styles_custom_symbols_idempotence_and_safe_marker_removal(self):
        base={'revision':'r','draft':'First\n\tChild\nSecond','operation':'list','kind':'numbered','marker':'auto','custom_marker':'','numbering':'mixed','start':1}
        self.assertEqual(compute(base['draft'],base)['value'],'1.\tFirst\r    a)\tChild\r2.\tSecond')
        for style,expected in [('right-paren','27)'),('parens','(27)'),('lower-alpha','aa)'),('upper-alpha','AA)'),('lower-roman','xxvii)'),('upper-roman','XXVII)')]:
            result=compute('Text',{**base,'draft':'Text','numbering':style,'start':27})
            self.assertEqual(result['value'],expected+'\tText')
        source='• First\n    a) Child\n        iv) Deep\n1.2. Next\n2026年\n-2度'
        value=compute(source,{**base,'draft':source,'kind':'bullet','marker':'custom','custom_marker':'→'})['value']
        self.assertEqual(value,'→\tFirst\r    →\tChild\r        →\tDeep\r→\tNext\r→\t2026年\r→\t-2度')
        self.assertFalse(compute(value,{**base,'draft':value,'kind':'bullet','marker':'custom','custom_marker':'→'})['changed'])
        self.assertEqual(compute(value,{**base,'draft':value,'kind':'remove'})['value'],'First\r    Child\r        Deep\rNext\r2026年\r-2度')
        numbered=compute(source,{**base,'draft':source})['value']
        again=compute(numbered,{**base,'draft':numbered})
        self.assertFalse(again['changed']);self.assertEqual(again['origins'],list(range(units(numbered)+1)))

    def test_list_option_limits_are_checked_before_a_write_plan(self):
        base={'revision':'r','draft':'Text','operation':'list','kind':'numbered','marker':'auto','custom_marker':'','numbering':'decimal','start':'9'}
        self.assertEqual(compute('Text',base)['value'],'9.\tText')
        for patch in ({'kind':'other'},{'numbering':'bad'},{'marker':'bad'},{'start':0},{'start':True},{'start':'1.5'},{'start':10000},{'custom_marker':'xx'},{'kind':'bullet','marker':'custom','custom_marker':'a'}):
            with self.assertRaises(ValueError):compute('Text',{**base,**patch})
        with self.assertRaisesRegex(ValueError,'6级'):compute('Text',{**base,'draft':'\t'*6+'Text'})
        with self.assertRaisesRegex(ValueError,'3999'):compute('Text',{**base,'numbering':'upper-roman','start':4000})

    def test_remove_only_consumes_prefixes_and_preserves_prose(self):
        source='No. 5\n→ Direction\n😀 Smile\n• Imported\n→\tManaged'
        self.assertEqual(self.list(source,'remove')['value'],'No. 5\r→ Direction\r😀 Smile\rImported\rManaged')

    def plan(self,source,operation='edit',draft=None,**extras):
        result=compute(source,{'revision':'r','draft':source if draft is None else draft,'operation':operation,**extras})
        params={key:value for key,value in result.items() if key in {'value','origins','count','breaks','layout'}}
        mapped_parameters({'revision':'r',**params},operation=='split')
        self.assertEqual(len(result['origins']),units(result['value'])+1)
        self.assertTrue(all(a<=b for a,b in zip(result['origins'],result['origins'][1:])))
        return result

    def test_edit_utf16_preserves_unchanged_character_styles(self):
        p=self.plan('A猫😀\rBC',draft='A猫😀\nBX!')
        self.assertEqual(p['value'],'A猫😀\rBX!');self.assertEqual(p['origins'][:6],[0,1,2,3,4,5])
        self.plan('😀',draft='😀尾')
        self.assertEqual(self.plan('x'*19999,draft='x'*19999+'尾')['value'],'x'*19999+'尾')

    def test_regex_groups_literal_backslashes_ignore_case_and_zero_width(self):
        p=self.plan('A12 a34','replace',find=r'(a)(\d+)',replacement=r'\2-\1',regex=True,ignore_case=True)
        self.assertEqual(p['value'],'12-A 34-a');self.assertEqual(p['count'],2)
        self.assertEqual(p['origins'],[0,0,0,0,3,4,4,4,4,4])
        p=self.plan('a','replace',find='a',replacement=r'\1',regex=False,ignore_case=False)
        self.assertEqual(p['value'],r'\1')
        self.assertEqual(self.plan('ab','replace',find='(?=b)',replacement='X',regex=True,ignore_case=False)['value'],'aXb')

    def test_match_ranges_are_utf8_bytes_in_normalized_draft_and_do_not_change_replacement(self):
        p=self.plan('猫😀A12\r尾 a34','replace',find=r'(a)(\d+)',replacement=r'\2-\1',regex=True,ignore_case=True)
        self.assertEqual(p['matches'],[[7,10],[15,18]])
        self.assertEqual(p['preview'],'猫😀12-A\n尾 34-a')
        self.assertEqual(self.plan('a\r\nb','replace',find=r'(?m)^b',replacement='B',regex=True,ignore_case=False)['matches'],[[2,3]])
        p=self.plan('猫😀尾','replace',find=r'(?=😀)',replacement='X',regex=True,ignore_case=False)
        self.assertEqual(p['matches'],[]);self.assertEqual(p['count'],1)
        self.assertEqual(self.plan('xx','replace',find='q',replacement='!',regex=False,ignore_case=False)['matches'],[])

    def test_python_named_groups_lookaround_inline_flags_and_conditionals(self):
        p=self.plan('ID12 id34','replace',find=r'(?i)(?P<key>id)(?P<n>\d+)',replacement=r'\g<n>-\g<key>',regex=True,ignore_case=False)
        self.assertEqual(p['value'],'12-ID 34-id')
        p=self.plan('x12 x34','replace',find=r'(?<=x)\d+(?!\d)',replacement='N',regex=True,ignore_case=False)
        self.assertEqual(p['value'],'xN xN')
        p=self.plan('a ab','replace',find=r'(a)?(?(1)b|c)',replacement='X',regex=True,ignore_case=False)
        self.assertEqual(p['value'],'a X')

    def test_cleanup_case_expansion_and_join_preserve_source_positions(self):
        self.assertEqual(self.plan('  A  \r\r B \r','trim')['value'],'A\r\rB\r')
        p=self.plan('\r A\r \rB\r','remove-empty');self.assertEqual(p['value'],' A\rB');self.assertEqual(p['origins'][:4],[1,2,3,6])
        self.assertEqual(self.plan('A\t  B','space')['value'],'A B')
        self.assertEqual(self.plan('ß a','upper')['origins'][:4],[0,0,1,2])
        self.assertEqual(self.plan('ΟΣ','lower')['value'],'ος')
        self.assertEqual(self.plan('A\rB','join',separator=' · ')['value'],'A · B')

    def test_split_without_newlines_keeps_delimiters_and_maps_styles(self):
        p=self.plan('猫,😀,尾','split',mode='literal',groups='',separator=',',layout='lines')
        self.assertEqual(p['value'],'猫,\r😀,\r尾');self.assertEqual(p['breaks'],[3,7,8]);self.assertEqual(p['count'],3)
        self.assertEqual(p['origins'],[0,1,2,2,3,4,5,5,5])
        p=self.plan('猫,狗;尾','split',mode='regex',groups='',separator='[,;]',layout='lines')
        self.assertEqual(p['value'],'猫,\r狗;\r尾')

    def test_lists_preserve_blank_lines_indent_and_character_styles(self):
        source='  猫😀\r\r\t狗\r尾'
        p=self.plan(source,'bullet')
        self.assertEqual(p['value'],'  • 猫😀\r\r\t• 狗\r• 尾')
        self.assertEqual(p['origins'][:8],[0,1,2,2,2,3,4,5])
        self.assertEqual(self.plan(source,'numbered')['value'],'  1. 猫😀\r\r\t2. 狗\r3. 尾')

    def test_list_conversion_is_idempotent_and_removes_only_markers(self):
        source='• 猫\r  8) 狗\r- 尾\r2026年\r-2度'
        bullet=self.plan(source,'bullet')['value']
        self.assertEqual(bullet,'• 猫\r  • 狗\r• 尾\r• 2026年\r• -2度')
        self.assertFalse(self.plan(bullet,'bullet')['changed'])
        numbered=self.plan(source,'numbered')['value']
        self.assertEqual(numbered,'1. 猫\r  2. 狗\r3. 尾\r4. 2026年\r5. -2度')
        self.assertFalse(self.plan(numbered,'numbered')['changed'])
        self.assertEqual(self.plan(source,'unlist')['value'],'猫\r  狗\r尾\r2026年\r-2度')
        self.assertEqual(self.plan(' \r\t\r猫','numbered')['value'],' \r\t\r1. 猫')
        with self.assertRaisesRegex(ValueError,'20000'):self.plan('x'*19999,'numbered')

    def test_original_line_layout_and_arbitrary_boundary_refusal(self):
        self.assertEqual(self.plan('A\rB\rC','split',mode='lines',groups='1-2,3',separator='',layout='original')['breaks'],[4,5])
        with self.assertRaisesRegex(ValueError,'2至64'):self.plan('A paragraph that only wraps on screen','split',mode='lines',groups='1',separator='',layout='original')
        with self.assertRaisesRegex(ValueError,'覆盖所有行'):self.plan('A\rB','split',mode='lines',groups='1',separator='',layout='original')
        with self.assertRaisesRegex(ValueError,'逐行排列'):self.plan('A,B','split',mode='literal',groups='',separator=',',layout='original')

    def test_limits_empty_invalid_regex_and_invalid_mapping(self):
        for source in ('','x'*20001,'\ud800'):
            with self.assertRaises(ValueError):self.plan(source)
        with self.assertRaises(Exception):self.plan('a','replace',find='[',replacement='',regex=True,ignore_case=False)
        with self.assertRaises(ValueError):self.plan('a','replace',find='a',replacement='',regex=False,ignore_case=False)
        with self.assertRaises(ValueError):mapped_parameters({'revision':'r','value':'ab','origins':[1,0,1],'count':1})

    def test_real_owned_worker_finishes_and_regex_timeout_stops_only_worker(self):
        from backend import text_plan
        import win32api,win32con,win32event,win32process
        p={'revision':'r','draft':'猫1','operation':'replace','find':r'\d','replacement':'2','regex':True,'ignore_case':False}
        first=evaluate('猫1',p);self.assertEqual(first['value'],'猫2');self.assertEqual(first['matches'],[[3,4]])
        worker=text_plan._evaluator
        with self.assertRaisesRegex(ValueError,'unknown group'):evaluate('猫1',{**p,'replacement':r'\g<missing>'})
        self.assertIs(text_plan._evaluator,worker)
        self.assertEqual(evaluate('猫1',p),first)
        self.assertIs(text_plan._evaluator,worker)
        self.assertEqual(win32process.GetExitCodeProcess(worker.process),win32con.STILL_ACTIVE)
        self.assertFalse((worker.path/'request.json').exists())
        self.assertFalse((worker.path/'response.json').exists())
        parent=win32api.GetCurrentProcess()
        process=win32api.DuplicateHandle(parent,worker.process,parent,0,False,win32con.DUPLICATE_SAME_ACCESS)
        p.update(draft='a'*10000+'!',find='(a+)+$',replacement='x')
        start=time.monotonic()
        try:
            with self.assertRaisesRegex(ValueError,'超时'):evaluate(p['draft'],p)
            self.assertEqual(win32event.WaitForSingleObject(process,0),win32con.WAIT_OBJECT_0)
        finally:process.Close()
        self.assertLess(time.monotonic()-start,5)
        self.assertIsNone(text_plan._evaluator)
        self.assertIsNone(worker.process)
        self.assertEqual(evaluate('x',{'revision':'r','draft':'y','operation':'edit'})['value'],'y')
        self.assertIsNot(text_plan._evaluator,worker)

    def test_evaluator_rejects_overlap_and_rejects_a_stale_response(self):
        from backend import text_plan
        from unittest.mock import patch
        import win32con
        params={'revision':'r','draft':'y','operation':'edit'}
        with text_plan._evaluation_lock:
            with self.assertRaisesRegex(ValueError,'本次未接受'):evaluate('x',params)
        evaluate('x',params)
        def stale(*_):
            (text_plan._evaluator.path/'response.json').write_text('{"id":"old"}','utf-8')
            return win32con.WAIT_OBJECT_0
        with patch('win32event.SetEvent'),patch('win32event.WaitForMultipleObjects',side_effect=stale):
            with self.assertRaisesRegex(ValueError,'身份不一致'):evaluate('x',params)
        self.assertIsNone(text_plan._evaluator)
        self.assertEqual(evaluate('x',params)['value'],'y')
