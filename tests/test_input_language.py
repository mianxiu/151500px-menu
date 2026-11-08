import unittest
from backend.input_language import LanguagePolicy,InputLanguage,process_exited
from types import SimpleNamespace
from backend.input_accessibility import control_state
import tempfile
import threading
import time
from pathlib import Path
from unittest.mock import Mock,patch
from contextlib import ExitStack
import json


class ConcurrentAccessibilityTests(unittest.TestCase):
    def test_independent_mta_clients_can_initialize_together(self):
        import pythoncom,comtypes.client
        from backend.input_accessibility import FocusAccessibility
        ready=threading.Barrier(2);errors=[]
        def create():
            pythoncom.CoInitializeEx(pythoncom.COINIT_MULTITHREADED)
            reader=None
            try:
                reader=FocusAccessibility();ready.wait(5)
            except Exception as error:errors.append(error);ready.abort()
            finally:reader=None;pythoncom.CoUninitialize()
        workers=[threading.Thread(target=create) for _ in range(2)]
        for worker in workers:worker.start()
        for worker in workers:worker.join(10)
        self.assertFalse(any(worker.is_alive() for worker in workers))
        self.assertEqual(errors,[])


class ExitedLanguageOwnerTests(unittest.TestCase):
    def owner(self):
        return {'root':1,'identity':[1,2,'original-process',3],'focus':10,
                'wanted':0x4090409,'original':0x8040804,'switch':True,'time':0}

    def controller(self,folder):
        controller=InputLanguage(SimpleNamespace(runtime=Path(folder)))
        controller.policy.owned=self.owner();controller.policy.attempted='old-intent'
        path=Path(folder)/'input-language-owned.json'
        path.write_text(json.dumps({'owned':self.owner(),'pending':None}),'utf-8')
        return controller,path

    def test_missing_pid_requires_both_specific_error_and_successful_absence_check(self):
        import pywintypes
        for code,pids,expected in ((87,[],True),(87,[2],False),(5,[],False)):
            with patch('win32api.OpenProcess',side_effect=pywintypes.error(code,'OpenProcess','test')),patch('win32process.EnumProcesses',return_value=pids):
                self.assertEqual(process_exited(self.owner()['identity']),expected)
        with patch('win32api.OpenProcess',side_effect=pywintypes.error(87,'OpenProcess','test')),patch('win32process.EnumProcesses',side_effect=OSError('unknown')):
            self.assertFalse(process_exited(self.owner()['identity']))

    def test_creation_identity_and_exit_code_distinguish_running_reused_and_exited_process(self):
        for created,exit_code,expected in (('original-process',259,False),('reused-pid',259,True),('original-process',0,True)):
            handle=Mock()
            with patch('win32api.OpenProcess',return_value=handle),patch('win32process.GetProcessTimes',return_value={'CreationTime':created}),patch('win32process.GetExitCodeProcess',return_value=exit_code):
                self.assertEqual(process_exited(self.owner()['identity']),expected)
            handle.Close.assert_called_once()
        handle=Mock()
        with patch('win32api.OpenProcess',return_value=handle),patch('win32process.GetProcessTimes',side_effect=OSError('unknown')):
            self.assertFalse(process_exited(self.owner()['identity']))
        handle.Close.assert_called_once()

    def test_confirmed_owner_archives_before_new_identity_can_switch_without_old_restore(self):
        with tempfile.TemporaryDirectory() as folder:
            controller,path=self.controller(folder)
            with patch('backend.input_language.process_exited',return_value=True):controller.retire_exited_owner()
            self.assertEqual(json.loads(path.read_text()),{'owned':None,'pending':None})
            archive=list(Path(folder).glob('input-language-retired-*.json'));self.assertEqual(len(archive),1)
            self.assertEqual(json.loads(archive[0].read_text()),{'owned':self.owner(),'reason':'original-process-exited','restore_sent':False})
            sample={'root':4,'identity':[4,5,'new-process',6],'focus':11,'layout':0x8040804,'state':'non-input'}
            request=controller.policy.step(sample,1,0x4090409,1)
            self.assertTrue(request['switch']);self.assertEqual(request['identity'],sample['identity'])
            self.assertEqual(controller.report['retired_exited_owners'],1)

    def test_unknown_pending_and_unproven_exit_never_release_records(self):
        for pending,exited in ((self.owner(),True),(None,False)):
            with tempfile.TemporaryDirectory() as folder:
                controller,path=self.controller(folder);controller.policy.pending=pending;before=path.read_bytes()
                with patch('backend.input_language.process_exited',return_value=exited) as probe:controller.retire_exited_owner()
                if pending:probe.assert_not_called()
                self.assertEqual(controller.policy.owned,self.owner());self.assertEqual(controller.policy.pending,pending)
                self.assertEqual(path.read_bytes(),before);self.assertEqual(list(Path(folder).glob('input-language-retired-*.json')),[])

    def test_archive_or_active_journal_failure_cannot_release_in_memory_ownership(self):
        from backend.settings import atomic_json
        for fail_at in (1,2):
            with tempfile.TemporaryDirectory() as folder:
                controller,path=self.controller(folder);before=path.read_bytes();writes=[]
                def write(target,value):
                    writes.append(target)
                    if len(writes)==fail_at:raise OSError('disk unavailable')
                    atomic_json(target,value)
                with patch('backend.input_language.process_exited',return_value=True),patch('backend.settings.atomic_json',side_effect=write):
                    with self.assertRaises(OSError):controller.retire_exited_owner()
                self.assertEqual(controller.policy.owned,self.owner());self.assertEqual(path.read_bytes(),before)

    def test_actual_child_process_lifetime_is_observed_without_termination(self):
        import subprocess,sys,win32api,win32process
        with subprocess.Popen([sys.executable,'-c','import time; time.sleep(1)'],creationflags=0x08000000) as process:
            handle=win32api.OpenProcess(0x1000,False,process.pid)
            try:created=str(win32process.GetProcessTimes(handle)['CreationTime'])
            finally:handle.Close()
            identity=[1,process.pid,created,2]
            self.assertFalse(process_exited(identity));self.assertEqual(process.wait(timeout=5),0)
            self.assertTrue(process_exited(identity))


class LanguagePolicyTests(unittest.TestCase):
    def sample(self,state='non-input',layout=0x8040804,focus=10):
        return {'root':1,'identity':[1,2,'created',3],'focus':focus,'layout':layout,'state':state}

    def test_unknown_input_outside_and_no_english_never_switch(self):
        for sample in [self.sample('unknown'),self.sample('input'),{'state':'outside'}]:
            self.assertIsNone(LanguagePolicy().step(sample,7,0x4090409,0))
        self.assertIsNone(LanguagePolicy().step(self.sample(),7,None,0))

    def test_one_request_then_readback_before_ownership_and_restore(self):
        p=LanguagePolicy();request=p.step(self.sample(),7,0x4090409,0)
        self.assertEqual(request['wanted'],0x4090409);self.assertIsNone(p.owned)
        self.assertIsNone(p.step(self.sample(),7,0x4090409,.1))
        self.assertIsNone(p.step(self.sample(layout=0x4090409),7,0x4090409,.2))
        self.assertEqual(p.confirmed_switches,1)
        request=p.step(self.sample('input',0x4090409,11),7,0x4090409,.3)
        self.assertEqual(request['wanted'],0x8040804);self.assertFalse(request['switch'])
        self.assertIsNotNone(p.owned)
        p.step(self.sample('input',focus=11),7,0x4090409,.4)
        self.assertIsNone(p.owned);self.assertEqual(p.confirmed_restores,1)

    def test_unknown_result_is_retained_and_not_reposted(self):
        p=LanguagePolicy();request=p.step(self.sample(),7,0x4090409,0)
        for tick in [2,3,100]:self.assertIsNone(p.step(self.sample(),8,0x4090409,tick))
        self.assertEqual(p.pending,request);self.assertEqual(p.state,'request-not-confirmed')

    def test_manual_change_releases_ownership_without_overwrite(self):
        p=LanguagePolicy();p.step(self.sample(),7,0x4090409,0)
        p.step(self.sample(layout=0x4090409),7,0x4090409,.1)
        self.assertIsNone(p.step(self.sample('input',layout=0x4110411),7,0x4090409,.2))
        self.assertIsNone(p.owned);self.assertEqual(p.state,'manual-change')

    def test_other_process_or_reused_window_never_receives_restore(self):
        p=LanguagePolicy();p.step(self.sample(),7,0x4090409,0)
        p.step(self.sample(layout=0x4090409),7,0x4090409,.1)
        changed=self.sample('input',layout=0x4090409);changed['identity'][2]='new process'
        self.assertIsNone(p.step(changed,7,0x4090409,.2))
        self.assertIsNone(p.step({'state':'outside'},7,0x4090409,.3))
        self.assertIsNotNone(p.owned)

    def test_failed_restore_is_not_queued_every_poll(self):
        p=LanguagePolicy();p.step(self.sample(),7,0x4090409,0)
        p.step(self.sample(layout=0x4090409),7,0x4090409,.1)
        self.assertIsNotNone(p.step(self.sample('input',0x4090409),7,0x4090409,.2))
        p.pending=None
        self.assertIsNone(p.step(self.sample('input',0x4090409),7,0x4090409,.3))

    def test_repeated_independent_intents_each_restore_once(self):
        p=LanguagePolicy()
        for intent in range(1,4):
            self.assertIsNotNone(p.step(self.sample(),intent,0x4090409,intent))
            p.step(self.sample(layout=0x4090409),intent,0x4090409,intent+.1)
            self.assertIsNotNone(p.step(self.sample('input',0x4090409),intent,0x4090409,intent+.2))
            p.step(self.sample('input'),intent,0x4090409,intent+.3)
        self.assertEqual(p.confirmed_switches,3);self.assertEqual(p.confirmed_restores,3)

    def test_input_intent_invalidates_old_click_without_clearing_ownership(self):
        controller=InputLanguage(SimpleNamespace());controller.policy.owned={'preserved':True}
        controller.click(1,2);self.assertIsNotNone(controller.intent)
        controller.protect();self.assertIsNone(controller.intent)
        self.assertEqual(controller.policy.owned,{'preserved':True});self.assertTrue(controller.wake.is_set())

    def test_confirmed_restore_allows_same_ps_focus_to_return_to_eng(self):
        p=LanguagePolicy()
        for tick in (0,1):
            self.assertIsNotNone(p.step(self.sample(),7,0x4090409,tick))
            p.step(self.sample(layout=0x4090409),7,0x4090409,tick+.1)
            p.step(self.sample('input',0x4090409),7,0x4090409,tick+.2)
            p.step(self.sample('input'),7,0x4090409,tick+.3)
        self.assertEqual(p.confirmed_switches,2);self.assertEqual(p.confirmed_restores,2)

    def test_activity_invalidates_tool_evidence_without_becoming_a_click(self):
        controller=InputLanguage(SimpleNamespace())
        controller.protect();epoch=controller.epoch;serial=controller.serial
        controller.activity()
        self.assertFalse(controller.protected);self.assertIsNone(controller.intent)
        self.assertEqual(controller.epoch,epoch);self.assertGreater(controller.serial,serial)

    def test_unknown_restore_does_not_start_an_autonomous_switch_loop(self):
        p=LanguagePolicy();p.step(self.sample(),7,0x4090409,0)
        p.step(self.sample(layout=0x4090409),7,0x4090409,.1)
        p.step(self.sample('unknown',0x4090409),None,0x4090409,.2)
        p.step(self.sample(),7,0x4090409,.3)
        for tick in (1,2,3):self.assertIsNone(p.step(self.sample(),7,0x4090409,tick))
        self.assertIsNotNone(p.step(self.sample(),8,0x4090409,4))


class FocusAccessibilityTests(unittest.TestCase):
    def row(self,kind=50000,**changes):
        row={30002:2,30003:kind,30008:True,30010:True,30020:10,30022:False,
             30033:False,30040:False,30043:False}
        row.update({int(k):v for k,v in changes.items()});return row

    def test_toolbars_layers_tabs_menus_and_dialog_buttons_need_no_canvas_click(self):
        for kind in (50021,50008,50007,50024,50000,50018,50019,50011,50017):
            self.assertEqual(control_state(self.row(kind),2),'non-input',kind)

    def test_canvas_generic_panels_and_home_require_positive_tool_evidence(self):
        for kind in (50025,50030,50032,50033):
            row=self.row(kind)
            self.assertEqual(control_state(row,2,True),'unknown')
            self.assertEqual(control_state(row,2,False,'moveTool'),'unknown')
            for tool in ('moveTool','pathComponentSelectTool','directSelectTool','blurTool','frameTool'):
                self.assertEqual(control_state(row,2,True,tool),'non-input',tool)
            for tool in ('typeCreateOrEditTool','typeVerticalCreateOrEditTool','typeMaskTool'):
                self.assertEqual(control_state(row,2,True,tool),'input',tool)

    def test_native_and_custom_edit_rename_search_numeric_and_text_pattern_are_protected(self):
        for kind in (50003,50004,50016):self.assertEqual(control_state(self.row(kind),2),'input')
        for props in ({'30040':True},{'30043':True,'30046':False},{'30033':True,'30048':False}):
            self.assertEqual(control_state(self.row(50025,**props),2,True,'moveTool'),'input')

    def test_idle_type_tool_allows_shortcuts_but_actual_editing_never_does(self):
        for tool in ('typeCreateOrEditTool','typeVerticalCreateOrEditTool','typeMaskTool','pathComponentSelectTool'):
            for kind in (50025,50030,50032,50033):
                self.assertEqual(control_state(self.row(kind),2,True,{'classID':tool,'isModal':False}),'non-input')
                self.assertEqual(control_state(self.row(kind),2,True,{'classID':tool,'isModal':True}),'input')
            self.assertEqual(control_state(self.row(50004),2,True,{'classID':tool,'isModal':False}),'input')
        for tool in ({'classID':'moveTool'},{'classID':'moveTool','isModal':0},{'classID':'','isModal':False}):
            self.assertEqual(control_state(self.row(50033),2,True,tool),'unknown')

    def test_missing_stale_foreign_offscreen_and_unsupported_properties_never_grant_eng(self):
        for props in ({'30002':3},{'30003':object()},{'30008':False},{'30010':False},{'30022':True},{'30043':None},{'30040':None}):
            self.assertEqual(control_state(self.row(**props),2),'unknown')
        self.assertEqual(control_state({},2,True,'moveTool'),'unknown')


class LanguageWorkerTests(unittest.TestCase):
    def start_worker(self,*,acknowledge=True,kind=50000,saved=None):
        temp=tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup)
        self.desktop=SimpleNamespace(runtime=Path(temp.name),settings={'enabled':True},visible=False,
                                     stop=threading.Event(),input_language_busy=lambda:False)
        if saved is not None:(self.desktop.runtime/'input-language-owned.json').write_text(json.dumps(saved),'utf-8')
        self.controller=InputLanguage(self.desktop);self.layout=0x8040804;self.kind=kind;self.modal=False;self.posts=[]
        def sample():
            return {'root':1,'identity':[1,2,'test-process',3],'focus':10,'layout':self.layout,'pid':2,
                    'state':'unknown','workspace':True,'class':'test',
                    'accessibility':FocusAccessibilityTests().row(self.kind)}
        self.controller.sample=sample
        self.desktop.input_language_tool=lambda:{'classID':'typeCreateOrEditTool','isModal':self.modal}
        def installed(count,array):array[0]=0x8040804;array[1]=0x4090409;return 2
        def post(hwnd,message,unused,layout):
            self.posts.append(layout)
            if acknowledge:self.layout=layout
            return True
        self.user=SimpleNamespace(GetKeyboardLayout=Mock(),GetKeyboardLayoutList=Mock(side_effect=installed),
                                  PostMessageW=Mock(side_effect=post),GetAsyncKeyState=Mock(return_value=0))
        api=patch('backend.input_language.ctypes.WinDLL',return_value=self.user);api.start();self.addCleanup(api.stop)
        uia=patch('backend.input_accessibility.FocusAccessibility',return_value=object());uia.start();self.addCleanup(uia.stop)
        self.controller.thread.start()
        def stop():
            self.desktop.stop.set();self.controller.wake.set();self.controller.thread.join(2)
            self.assertFalse(self.controller.thread.is_alive())
        self.addCleanup(stop)

    def wait(self,predicate):
        until=time.monotonic()+2
        while not predicate():
            self.assertLess(time.monotonic(),until,self.controller.report)
            time.sleep(.005)

    def test_without_click_all_ps_non_input_focus_switches_restores_and_returns_to_eng(self):
        self.start_worker()
        self.wait(lambda:self.controller.policy.confirmed_switches==1)
        self.assertIsNone(self.controller.intent)
        self.kind=50004;self.controller.activity()
        self.wait(lambda:self.controller.policy.confirmed_restores==1)
        self.kind=50021;self.controller.activity()
        self.wait(lambda:self.controller.policy.confirmed_switches==2)
        self.desktop.stop.set();self.controller.wake.set();self.controller.thread.join(2)
        self.assertEqual(self.posts,[0x4090409,0x8040804,0x4090409,0x8040804])
        self.assertEqual(self.controller.policy.confirmed_restores,2)

    def test_worker_startup_archives_old_confirmed_owner_before_new_ps_switch(self):
        owner=ExitedLanguageOwnerTests().owner()
        with patch('backend.input_language.process_exited',return_value=True):
            self.start_worker(saved={'owned':owner,'pending':None})
            self.wait(lambda:self.controller.policy.confirmed_switches==1)
        self.assertEqual(self.posts,[0x4090409])
        self.assertEqual(self.user.PostMessageW.call_args.args[0],10)
        archive=list(self.desktop.runtime.glob('input-language-retired-*.json'))
        self.assertEqual(json.loads(archive[0].read_text())['owned'],owner)

    def test_worker_restart_keeps_unknown_request_and_sends_nothing_to_new_ps(self):
        owner=ExitedLanguageOwnerTests().owner();pending={**owner,'switch':False}
        with patch('backend.input_language.process_exited',return_value=True) as probe:
            self.start_worker(saved={'owned':owner,'pending':pending})
            self.wait(lambda:self.controller.report.get('pending'))
            self.desktop.stop.set();self.controller.wake.set();self.controller.thread.join(2)
            probe.assert_not_called()
        self.assertEqual(self.posts,[]);self.assertEqual(self.controller.policy.pending['identity'],pending['identity'])
        self.assertEqual(list(self.desktop.runtime.glob('input-language-retired-*.json')),[])

    def test_invalid_saved_ownership_cannot_reach_shutdown_restore(self):
        owner=ExitedLanguageOwnerTests().owner();owner['switch']=False
        self.start_worker(saved={'owned':owner,'pending':None})
        self.wait(lambda:not self.controller.thread.is_alive())
        self.assertEqual(self.controller.report['state'],'journal-invalid')
        self.assertIsNone(self.controller.policy.owned);self.assertEqual(self.posts,[])
        self.assertEqual(list(self.desktop.runtime.glob('input-language-retired-*.json')),[])

    def test_idle_type_tool_allowed_then_modal_editing_restores_without_sending_keys(self):
        self.start_worker(kind=50033);self.wait(lambda:self.controller.policy.confirmed_switches==1)
        self.kind=50033;self.modal=True;self.controller.protect()
        self.wait(lambda:self.controller.policy.confirmed_restores==1)
        self.modal=False;self.controller.activity()
        self.wait(lambda:self.controller.policy.confirmed_switches==2)
        self.assertTrue(all(call.args[1]==0x50 for call in self.user.PostMessageW.call_args_list))

    def test_unconfirmed_switch_is_never_repeated_or_assumed_owned(self):
        self.start_worker(acknowledge=False);self.wait(lambda:len(self.posts)==1)
        self.controller.policy.pending['time']=time.monotonic()-2
        self.controller.activity();self.wait(lambda:self.controller.policy.state=='request-not-confirmed')
        self.assertIsNone(self.controller.policy.owned)
        self.kind=50004;self.controller.activity()
        self.desktop.stop.set();self.controller.wake.set();self.controller.thread.join(2)
        self.assertEqual(self.posts,[0x4090409]);self.assertIsNotNone(self.controller.policy.pending)

    def test_tool_observation_failure_restores_owned_layout_and_never_grants_eng(self):
        self.start_worker();self.wait(lambda:self.controller.policy.confirmed_switches==1)
        self.kind=50033
        self.desktop.input_language_tool=Mock(side_effect=RuntimeError('unknown original observation'))
        self.controller.activity();self.wait(lambda:self.controller.policy.confirmed_restores==1)
        self.assertEqual(self.posts,[0x4090409,0x8040804])

    def test_return_from_busy_rechecks_tool_without_another_click_or_key(self):
        self.start_worker(kind=50033);self.wait(lambda:self.controller.policy.confirmed_switches==1)
        busy=[True];self.desktop.input_language_busy=lambda:busy[0]
        self.controller.activity();self.wait(lambda:self.controller.policy.confirmed_restores==1)
        before=self.controller.report['tool_reads'];busy[0]=False
        self.wait(lambda:self.controller.policy.confirmed_switches==2)
        self.assertGreater(self.controller.report['tool_reads'],before)

    def test_uia_dropout_restores_once_then_requires_fresh_activity(self):
        self.start_worker(kind=50033);self.wait(lambda:self.controller.policy.confirmed_switches==1)
        original=self.controller.sample;unavailable=[True]
        def sample():
            value=original()
            if unavailable[0]:value['accessibility']={}
            return value
        self.controller.sample=sample;self.controller.wake.set()
        self.wait(lambda:self.controller.policy.confirmed_restores==1)
        unavailable[0]=False;self.controller.wake.set();time.sleep(.25)
        self.assertEqual(self.posts,[0x4090409,0x8040804])
        self.controller.activity();self.wait(lambda:self.controller.policy.confirmed_switches==2)
        self.kind=50004;self.controller.wake.set();self.wait(lambda:self.controller.policy.confirmed_restores==2)
        self.kind=50033;self.controller.wake.set();self.wait(lambda:self.controller.policy.confirmed_switches==3)


class LanguageSamplingTests(unittest.TestCase):
    def sample(self,pair=list,foreign=None,caret=False,caret_visible=True,image='C:\\Adobe\\Photoshop.exe',query_ok=True,calls=1,expect_close=False):
        controller=InputLanguage(SimpleNamespace(visible=False))
        controller.accessibility=SimpleNamespace(read=Mock(return_value=FocusAccessibilityTests().row(50033)))
        def gui_info(thread,pointer):
            pointer._obj.focus=10;pointer._obj.caret=10 if caret else 0;pointer._obj.flags=int(caret and caret_visible)
            return True
        controller.user=SimpleNamespace(GetGUIThreadInfo=gui_info,GetKeyboardLayout=lambda thread:0x8040804)
        handle=SimpleNamespace(Close=Mock())
        handle_type=type('Handle',(),{'__int__':lambda self:7,'Close':lambda self:handle.Close()})
        def query(process,flags,buffer,length):
            self.assertEqual(process,7);self.assertEqual(flags,0)
            buffer.value=image;return query_ok
        kernel=SimpleNamespace(QueryFullProcessImageNameW=Mock(side_effect=query))
        import win32gui,win32process,win32api
        with ExitStack() as stack:
            stack.enter_context(patch.multiple(win32gui,GetForegroundWindow=Mock(return_value=1),
                IsWindowVisible=Mock(return_value=True),IsIconic=Mock(return_value=False),
                IsWindow=Mock(return_value=True),GetClassName=Mock(return_value='OWL.Document'),GetAncestor=Mock(return_value=1)))
            stack.enter_context(patch.multiple(win32process,GetWindowThreadProcessId=Mock(side_effect=lambda hwnd:pair(foreign if hwnd==10 and foreign else (3,2))),
                GetModuleFileNameEx=Mock(side_effect=AssertionError('Must not require process-memory access')),GetProcessTimes=Mock(return_value={'CreationTime':'created'})))
            opened=stack.enter_context(patch.object(win32api,'OpenProcess',return_value=handle_type()))
            stack.enter_context(patch('backend.input_language.ctypes.WinDLL',return_value=kernel))
            stack.enter_context(patch('backend.photoshop_windows.workspace_root',return_value=1))
            for _ in range(calls):value=controller.sample()
            opened.assert_called_once_with(0x1000,False,2)
            if expect_close:handle.Close.assert_called_once()
            else:handle.Close.assert_not_called()
        return value,controller.accessibility.read

    def test_image_query_failure_and_foreign_executable_never_grant_input_permission(self):
        value,reader=self.sample(image='C:\\Other\\Editor.exe')
        self.assertEqual(value,{'state':'outside'});reader.assert_not_called()
        with self.assertRaises(OSError):self.sample(query_ok=False,expect_close=True)

    def test_real_sampling_accepts_pywin32_list_as_well_as_tuple_pairs(self):
        for pair in (list,tuple):
            value,reader=self.sample(pair)
            self.assertEqual(value['identity'],[1,2,'created',3]);self.assertEqual(value['focus'],10)
            self.assertEqual(value['accessibility'][30003],50033);reader.assert_called_once()

    def test_same_foreground_reuses_identity_without_reopening_the_process(self):
        value,_=self.sample(calls=5)
        self.assertEqual(value['identity'],[1,2,'created',3])

    def test_foreign_focus_process_or_thread_still_cannot_grant_permission(self):
        for foreign in ((3,99),(99,2)):
            value,reader=self.sample(foreign=foreign)
            self.assertEqual(value,{'state':'unknown','foreign_focus':10});reader.assert_not_called()

    def test_native_caret_protects_input_before_any_accessibility_query(self):
        value,reader=self.sample(caret=True)
        self.assertEqual(value['state'],'input');reader.assert_not_called()

    def test_hidden_caret_remains_input_and_does_not_query_accessibility(self):
        value,reader=self.sample(caret=True,caret_visible=False)
        self.assertEqual(value['state'],'input');reader.assert_not_called()
