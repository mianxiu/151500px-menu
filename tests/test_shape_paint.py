import copy
import math
import unittest
from backend.shape_paint import parameters, metadata
from backend.layer_context import require_menu_target
from backend.tasks import TaskCancelled

class ShapePaintTests(unittest.TestCase):
    def test_paint_cannot_submit_from_a_menu_without_editor_scope(self):
        from backend.desktop import Desktop
        desktop=Desktop('http://localhost',lambda action:None)
        target={'session':'s','document_id':12,'layer_ids':[30],'kind':'shape-fill'}
        desktop.menu_layer_context={**target,'shape_editor':{'available':True}}
        desktop.color_editor_active=False
        with self.assertRaises(TaskCancelled):desktop.require_menu_layer_target('shape.paint',target)
        desktop.color_editor_active=True
        desktop.require_menu_layer_target('shape.paint',target)

    def test_editor_only_in_hold_layers_context_without_input_focus(self):
        from backend.desktop import shape_editor_allowed
        shape={'available':True}
        context={'kind':'shape-fill','layer_ids':[30]}
        for kind in ('canvas','text','home','generic','unknown'):
            self.assertFalse(shape_editor_allowed({'kind':kind},shape,True,context))
        self.assertTrue(shape_editor_allowed({'kind':'layers'},shape,True,context))
        self.assertFalse(shape_editor_allowed({'kind':'layers','input_focused':True},shape,True,context))
        self.assertFalse(shape_editor_allowed({'kind':'layers'},shape,False,context))
        self.assertFalse(shape_editor_allowed({'kind':'layers'},{'available':False},True,context))

    def test_stale_paint_metadata_cannot_enable_nonshape_or_multi_locked_targets(self):
        from backend.desktop import shape_editor_allowed
        base={'session':'s','document_id':12,'kind':'shape-fill','layer_ids':[30],'shape_editor':{'available':True}}
        for change in ({'kind':kind} for kind in ('pixel','text','group','smart','adjustment','mixed','other','unknown')):
            value={**base,**change}
            self.assertFalse(shape_editor_allowed({'kind':'layers'},value['shape_editor'],True,value))
            with self.assertRaises(TaskCancelled):require_menu_target(value,'shape.paint',value)
        for change in ({'layer_ids':[]},{'layer_ids':[30,31]},{'locked':True},{'background':True}):
            value={**base,**change}
            self.assertFalse(shape_editor_allowed({'kind':'layers'},value['shape_editor'],True,value))
            with self.assertRaises(TaskCancelled):require_menu_target(value,'shape.paint',value)

    def setUp(self):
        self.value={'revision':'session:1','fill':{'kind':'solid','enabled':True,'rgb':[12,34,56]},
                    'stroke':{'kind':'solid','enabled':False,'rgb':[0,0,0],'width':1}}

    def test_bounded_payload_preserves_channels_and_is_copied(self):
        result=parameters(self.value)
        result['fill']['rgb'][0]=255
        self.assertEqual(self.value['fill']['rgb'][0],12)
        self.assertEqual(result['stroke']['width'],1)

    def test_native_channels_preserve_values_and_reject_invalid_or_mixed_fields(self):
        for model,values in (('cmyk',[12.345,20,30,4]),('lab',[55.5,-12.75,127]),('gray',[62.5])):
            value=copy.deepcopy(self.value)
            value['fill']={'kind':'solid','enabled':True,'native':{'model':model,'values':values}}
            result=parameters(value)
            self.assertEqual(result['fill']['native']['values'],values)
            result['fill']['native']['values'][0]=0
            self.assertNotEqual(value['fill']['native']['values'][0],0)
            for bad in (True,math.nan,-129,1000):
                invalid=copy.deepcopy(value);invalid['fill']['native']['values'][0]=bad
                with self.assertRaises(ValueError):parameters(invalid)
            value['fill']['rgb']=[0,0,0]
            with self.assertRaises(ValueError):parameters(value)
        value=copy.deepcopy(self.value)
        value['fill']={'kind':'gradient','enabled':True,'stops':[
            {'native':{'model':'lab','values':[50,-10,20]},'location':0},
            {'native':{'model':'lab','values':[80,0,-20]},'location':4096}]}
        self.assertEqual(parameters(value)['fill'],value['fill'])

    def test_vector_mask_ranges_and_metadata_scope(self):
        self.value['mask']={'feather':12.5,'density':62.5}
        value=parameters(self.value);value['mask']['density']=0
        self.assertEqual(self.value['mask']['density'],62.5)
        self.assertTrue(metadata({'available':True,'vector':True,**self.value})['available'])
        self.assertFalse(metadata({'available':True,'vector':False,**self.value})['available'])
        for key,bad in [('feather',-1),('feather',1001),('feather',True),('feather',math.nan),('density',-1),('density',101),('density',False),('density',math.inf)]:
            value=copy.deepcopy(self.value);value['mask'][key]=bad
            with self.assertRaises(ValueError):parameters(value)
        for mask in (None,{}, {'feather':0,'density':100,'script':'no'}):
            value={**self.value,'mask':mask}
            with self.assertRaises(ValueError):parameters(value)

    def test_invalid_color_width_fields_and_identity_rejected(self):
        for bad in (True,-1,256,math.nan,math.inf,'12'):
            value=copy.deepcopy(self.value);value['fill']['rgb'][0]=bad
            with self.assertRaises(ValueError):parameters(value)
        for bad in (True,-1,1001,math.nan,'1'):
            value=copy.deepcopy(self.value);value['stroke']['width']=bad
            with self.assertRaises(ValueError):parameters(value)
        for field in ('revision','fill','stroke'):
            value=copy.deepcopy(self.value);del value[field]
            with self.assertRaises(ValueError):parameters(value)
        value=copy.deepcopy(self.value);value['fill']['script']='not allowed'
        with self.assertRaises(ValueError):parameters(value)

    def test_gradient_order_and_pattern_names(self):
        self.value['fill']={'kind':'gradient','enabled':True,'stops':[{'rgb':[0,0,0],'location':0},{'rgb':[255,255,255],'location':4096}]}
        self.assertEqual(parameters(self.value)['fill']['kind'],'gradient')
        self.value['fill']['stops'].reverse()
        with self.assertRaises(ValueError):parameters(self.value)
        for name in ('','x'*129,None):
            self.value['fill']={'kind':'pattern','enabled':True,'name':name}
            with self.assertRaises(ValueError):parameters(self.value)

    def test_gradient_method_is_explicit_bounded_and_preserved(self):
        value={'kind':'gradient','enabled':True,'stops':[{'rgb':[255,0,0],'location':0},{'rgb':[0,0,255],'location':4096}]}
        self.value['fill']=value
        self.assertNotIn('method',parameters(self.value)['fill'])
        for method in ('perceptual','linear','classic'):
            value['method']=method
            self.assertEqual(parameters(self.value)['fill']['method'],method)
            self.assertTrue(metadata({'available':True,**self.value})['available'])
        for method in (None,True,0,'Perceptual','oklab',''):
            value['method']=method
            with self.assertRaises(ValueError):parameters(self.value)

    def test_preview_capability_is_explicit_and_does_not_change_submission_parameters(self):
        value={'available':True,**self.value}
        self.assertFalse(metadata(value)['live_preview'])
        for enabled in (False,True):
            result=metadata({**value,'live_preview':enabled})
            self.assertEqual(result['live_preview'],enabled)
            self.assertEqual(parameters({k:result[k] for k in ('revision','fill','stroke')}),self.value)
        self.assertFalse(metadata({**value,'live_preview':'true'})['available'])

    def test_untrusted_metadata_disables_editor(self):
        value={'available':True,**self.value,'patterns':['Loaded Pattern']}
        self.assertTrue(metadata(value)['available'])
        value['patterns']=[None]
        self.assertFalse(metadata(value)['available'])

    def test_menu_identity_must_match_before_dispatch(self):
        value={'session':'s','document_id':12,'layer_ids':[30],'kind':'shape-fill','shape_editor':{'available':True}}
        require_menu_target(value,'shape.paint',value)
        for field,bad in (('session','other'),('document_id',13),('layer_ids',[31])):
            target={**value,field:bad}
            with self.assertRaises(TaskCancelled):require_menu_target(value,'shape.paint',target)
        value['shape_editor']['available']=False
        with self.assertRaises(TaskCancelled):require_menu_target(value,'shape.paint',value)
