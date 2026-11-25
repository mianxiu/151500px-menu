import json
import tempfile
import unittest
from pathlib import Path

from backend.text_tools import mapped_parameters, style_ranges, summarize_ranges, typography, validate_style_scope
from backend.typeset_settings import DEFAULTS, load, load_styles, save, save_style


class TypesetDraftTests(unittest.TestCase):
    def test_view_settings_can_reset_without_replacing_style_presets(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[1]) as folder:
            view=Path(folder)/'typeset-view.json'
            styles=Path(folder)/'typeset-styles.json'
            settings={**DEFAULTS,'background':2,'rgb':[.1,.2,.3],'zoom':2,'scope':2}
            save(view,settings)
            save_style(styles,'正文',{'kind':'character','values':{'size':14}})
            self.assertEqual(load(view),settings)
            self.assertEqual(save(view,None),DEFAULTS)
            self.assertEqual(load_styles(styles)['正文']['values'],{'size':14})
            with self.assertRaises(ValueError):save(view,{**settings,'draft':'不得落盘'})
            self.assertNotIn('draft',json.loads(view.read_text('utf-8')))
            with self.assertRaises(ValueError):
                save_style(styles,'范围',{'kind':'character','values':{'size':14,'range':[0,1]}})

    def test_multi_range_plan_and_mixed_readback(self):
        text='AZ\rBC'
        plan={'revision':'r','value':text,'origins':list(range(len(text)+1)),'count':0,
              'typography_runs':[{'range':[0,1],'fontPostScriptName':'First'},
                                 {'range':[3,4],'spaceBefore':5,'justifyWord':[80,100,120]}]}
        self.assertEqual(mapped_parameters(plan),plan)
        box={**plan,'box_size':[160,120],'box_shift_x':32}
        self.assertEqual(mapped_parameters(box),box)
        with self.assertRaises(ValueError):mapped_parameters({**plan,'box_shift_x':32})
        with self.assertRaises(ValueError):mapped_parameters({**box,'box_shift_x':float('nan')})
        with self.assertRaises(ValueError):mapped_parameters({**plan,'typography':{'size':12}})
        with self.assertRaises(ValueError):
            mapped_parameters({**plan,'typography_runs':[{'justifyWord':[100,80,120]}]})
        raw={'textStyleRange':[{'from':0,'to':3,'textStyle':{'fontPostScriptName':'First'}},
                               {'from':3,'to':6,'textStyle':{'fontPostScriptName':'Second'}}],
             'paragraphStyleRange':[{'from':0,'to':6,'paragraphStyle':{'textEveryLineComposer':False}}]}
        profiles=style_ranges(raw,72)
        self.assertEqual(profiles['textStyle'][1]['values']['fontPostScriptName'],'Second')
        self.assertEqual(summarize_ranges(profiles)['fontPostScriptName'],'混合')
        self.assertEqual(summarize_ranges(profiles)['textEveryLineComposer'],'逐行')

    def test_east_asian_choices_preserve_paragraph_scope_and_inheritance(self):
        style=typography({'range':[2,3],'kinsokuSetName':'Soft','mojiKumiName':'Photoshop6MojiKumiNone',
                          'burasagari':'burasagariStrong','preferredKinsokuOrder':'pushOut'})
        supported={'east_asian_paragraph_supported':True,'paragraph_indents_supported':True,
                   'style_scope_supported':True,'typography_runs_supported':True}
        validate_style_scope(style,supported,{})
        for overrides in ({'east_asian_paragraph_supported':False},{'paragraph_indents_supported':False}):
            with self.assertRaises(ValueError):validate_style_scope(style,{**supported,**overrides},{})
        with self.assertRaises(ValueError):validate_style_scope(style,supported,{'operation':'list'})
        with self.assertRaises(ValueError):typography({'burasagari':True})
        raw={'textStyleRange':[{'from':0,'to':4,'textStyle':{}}],
             'paragraphStyleRange':[
                 {'from':0,'to':2,'paragraphStyle':{'kinsokuSetName':{'_enum':'kinsokuSetName','_value':'Hard'}}},
                 {'from':2,'to':4,'paragraphStyle':{'baseParentStyle':{
                     'kinsokuSetName':{'_enum':'kinsokuSetName','_value':'Soft'},
                     'mojiKumiName':{'_enum':'mojiKumiName','_value':'Photoshop6MojiKumiSet1'}}}},
             ]}
        rows=style_ranges(raw,72)['paragraphStyle']
        self.assertEqual([row['values']['kinsokuSetName'] for row in rows],['JIS 最大','JIS 弱'])
        self.assertEqual([row['values']['mojiKumiName'] for row in rows],['关闭','组合 1'])
