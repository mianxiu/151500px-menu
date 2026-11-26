"""Local text plans. Untrusted regex runs only in an owned, bounded process."""
import difflib
import json
from pathlib import Path
import re
import secrets
import subprocess
import sys
import tempfile
import atexit
import threading
import unicodedata


def units(text):
    try:return len(text.encode('utf-16-le'))//2
    except UnicodeError:raise ValueError('文字包含不完整的Unicode字符') from None


def parameters(value):
    common={'revision','draft','operation'}
    extras={'edit':set(),'trim':set(),'remove-empty':set(),'space':set(),'upper':set(),'lower':set(),
            'bullet':set(),'numbered':set(),'unlist':set(),'list':{'kind','marker','custom_marker','numbering','start'},
            'join':{'separator'},'replace':{'find','replacement','regex','ignore_case'},
            'split':{'mode','groups','separator','layout'}}
    if not isinstance(value,dict) or not isinstance(value.get('operation'),str) or value['operation'] not in extras or set(value)!=common|extras[value['operation']]:raise ValueError('无效的文字处理参数')
    if not isinstance(value['revision'],str) or not 1<=len(value['revision'])<=128:raise ValueError('文字读取已过期')
    if not isinstance(value['draft'],str) or units(value['draft'])>20000 or '\0' in value['draft']:raise ValueError('文字上限为20000字符，不能包含空字符')
    for key,limit in (('find',200),('replacement',500),('separator',200),('groups',1000)):
        if key in value and (not isinstance(value[key],str) or units(value[key])>limit or '\0' in value[key]):raise ValueError('查找、替换或分组内容超出上限')
    if value['operation']=='replace':
        if not value['find'] or type(value['regex']) is not bool or type(value['ignore_case']) is not bool:raise ValueError('查找内容不能为空')
    if value['operation']=='split' and (value['mode'] not in ('lines','literal','regex') or value['layout'] not in ('original','lines')):raise ValueError('无效的拆分方式')
    if value['operation']=='list':
        if value['kind'] not in ('bullet','numbered','remove') or value['marker'] not in ('auto','disc','circle','square','dash','check','custom') or value['numbering'] not in ('decimal','right-paren','parens','lower-alpha','upper-alpha','lower-roman','upper-roman','hierarchical','mixed'):raise ValueError('无效的列表样式')
        if not isinstance(value['custom_marker'],str) or len(value['custom_marker'])>1 or units(value['custom_marker'])>2 or '\0' in value['custom_marker']:raise ValueError('项目符号须为一个符号字符')
        if value['kind']=='bullet' and value['marker']=='custom' and (len(value['custom_marker'])!=1 or unicodedata.category(value['custom_marker'])[0] not in ('P','S')):raise ValueError('项目符号须为一个符号字符')
        start=value['start']
        if isinstance(start,str) and re.fullmatch('[1-9][0-9]{0,3}',start):start=int(start)
        if type(start) is not int or not 1<=start<=9999:raise ValueError('起始编号须为1至9999')
        value={**value,'start':start}
    return dict(value)


def normalize(text):return text.replace('\r\n','\n').replace('\r','\n')


def mapping(source,text):
    normalized=[];positions=[];offset=0;skip=False
    for char in source:
        if skip and char=='\n':offset+=1;skip=False;continue
        normalized.append('\n' if char=='\r' else char);positions.append(offset)
        offset+=units(char);skip=char=='\r'
    old=''.join(normalized);origins=[]
    prefix=0
    while prefix<min(len(old),len(text)) and old[prefix]==text[prefix]:prefix+=1
    suffix=0
    while suffix<min(len(old),len(text))-prefix and old[-suffix-1]==text[-suffix-1]:suffix+=1
    old_end=len(old)-suffix;new_end=len(text)-suffix
    operations=[('equal',0,prefix,0,prefix)]
    operations.extend((kind,a+prefix,b+prefix,c+prefix,d+prefix) for kind,a,b,c,d in difflib.SequenceMatcher(None,old[prefix:old_end],text[prefix:new_end],autojunk=False).get_opcodes())
    if suffix:operations.append(('equal',old_end,len(old),new_end,len(text)))
    for kind,a,b,c,d in operations:
        for index in range(c,d):
            origin=positions[a+index-c] if kind=='equal' else (positions[a] if a<len(positions) else max(0,offset-1))
            for unit in range(units(text[index])):origins.append(origin+unit if kind=='equal' else origin)
    origins.append(offset if operations and operations[-1][0]=='equal' else origins[-1] if origins else 0)
    return origins


def line_ends(text,expression):
    ends=[m.end() for m in re.finditer('\n',text)]+[len(text)]
    result=[];following=1
    for token in expression.replace('，',',').split(','):
        match=re.fullmatch(r'\s*([1-9][0-9]*)(?:\s*-\s*([1-9][0-9]*))?\s*',token)
        if not match:raise ValueError('按行分组须依次覆盖所有行，例如1-3,4,5-7')
        first=int(match[1]);last=int(match[2] or first)
        if first!=following or last<first or last>len(ends):raise ValueError('按行分组须依次覆盖所有行')
        end=ends[last-1]
        if end and (not result or result[-1]!=end):result.append(end)
        following=last+1
    if following!=len(ends)+1 or len(result)>64:raise ValueError('按行分组须覆盖所有行，最多64组')
    return result


_NUMBERED_LIST_MARKER=r'(?:[0-9]+\.)+[0-9]+\.?|[0-9]+[.)、]|\([0-9]+\)'
_IMPORTED_LIST_MARKER=re.compile(
    r'(?:[•●○◦■▪□–—✓✔*+\-]|'+_NUMBERED_LIST_MARKER+r'|[A-Za-z][.)]|[A-Za-z]{2,3}\)|[IVXLCDMivxlcdm]{2,15}\))[ \t]+'
)
_TAB_LIST_MARKER=re.compile(r'(?:'+_NUMBERED_LIST_MARKER+r'|[A-Za-z]{1,15}[.)]|[^\s])\t')


def list_marker_end(text,start,end,*,imported=False):
    marker=_TAB_LIST_MARKER.match(text,start,end)
    if marker and (marker.end()-start>2 or unicodedata.category(text[start])[0] in ('P','S')):
        return marker.end()
    if imported:
        marker=_IMPORTED_LIST_MARKER.match(text,start,end)
        if marker:return marker.end()
    return start


def list_edits(text,params):
    counters=[0]*6;edits=[];count=0
    def number(n,style):
        if style in ('lower-alpha','upper-alpha'):
            result=''
            while n:n,digit=divmod(n-1,26);result=chr(97+digit)+result
            return (result.upper() if style=='upper-alpha' else result)+')'
        if style in ('lower-roman','upper-roman'):
            if n>3999:raise ValueError('罗马数字最多3999，请调整起始编号或样式')
            result=''
            for value,symbol in ((1000,'M'),(900,'CM'),(500,'D'),(400,'CD'),(100,'C'),(90,'XC'),(50,'L'),(40,'XL'),(10,'X'),(9,'IX'),(5,'V'),(4,'IV'),(1,'I')):
                times,n=divmod(n,value);result+=symbol*times
            return (result.lower() if style=='lower-roman' else result)+')'
        return '('+str(n)+')' if style=='parens' else str(n)+(')' if style=='right-paren' else '.')
    for line in re.finditer(r'(?m)^[^\S\n]*\S[^\n]*',text):
        leading=re.match(r'[^\S\n]*',line[0])[0];start=line.start()+len(leading)
        end=list_marker_end(text,start,line.end(),imported=True)
        if not text[end:line.end()].strip():continue
        count+=1
        if params['kind']=='remove':
            if end>start:edits.append((start,end,''))
            continue
        level=(len(leading.expandtabs(4))+3)//4
        if level>=6:raise ValueError('列表最多6级，请减少缩进')
        if params['kind']=='bullet':
            symbols={'disc':'•','circle':'◦','square':'▪','dash':'–','check':'✓','custom':params['custom_marker']}
            prefix=('•','◦','▪')[level%3] if params['marker']=='auto' else symbols[params['marker']]
        else:
            for parent in range(level):
                if not counters[parent]:counters[parent]=params['start'] if parent==0 else 1
            counters[level]=counters[level]+1 if counters[level] else params['start'] if level==0 else 1
            counters[level+1:]=[0]*(5-level)
            style=params['numbering']
            prefix='.'.join(map(str,counters[:level+1]))+'.' if style=='hierarchical' else number(counters[level],('decimal','lower-alpha','lower-roman')[level%3] if style=='mixed' else style)
        indent='    '*level;prefix+='\t'
        if leading!=indent:edits.append((line.start(),start,indent))
        if text[start:end]!=prefix:edits.append((start,end,prefix))
    return edits,count


def compute(source,params):
    params=parameters(params)
    if not isinstance(source,str) or units(source)>20000 or '\0' in source:raise ValueError('原文无效或超过处理上限')
    text=normalize(params['draft']);origins=mapping(source,text);operation=params['operation'];count=0;breaks=None;preview=None;matches=[]
    draft_origins=None
    def edit(edits):
        nonlocal text,origins,draft_origins
        if units(text)+sum(units(value)-units(text[start:end]) for start,end,value in edits)>20000:raise ValueError('结果超过20000字符')
        offsets=[0]
        for char in text:offsets.append(offsets[-1]+units(char))
        parts=[];mapped=[];draft_mapped=[];cursor=0
        for start,end,replacement in edits:
            parts.append(text[cursor:start]);mapped.extend(origins[offsets[cursor]:offsets[start]])
            draft_mapped.extend(range(offsets[cursor],offsets[start]))
            parts.append(replacement);origin=origins[min(offsets[start],max(0,len(origins)-2))]
            length=units(replacement)
            mapped.extend([origin]*length)
            draft_mapped.extend([offsets[min(start,max(0,len(text)-1))]]*length)
            cursor=end
        parts.append(text[cursor:]);mapped.extend(origins[offsets[cursor]:-1])
        mapped.append(origins[-1] if cursor<len(text) else mapped[-1] if mapped else 0)
        draft_mapped.extend(range(offsets[cursor],offsets[-1]+1))
        draft_origins=draft_mapped
        text=''.join(parts);origins=mapped
    def substitute(pattern,replacement):
        nonlocal count
        edits=[(match.start(),match.end(),replacement(match)) for match in pattern.finditer(text)]
        count=len(edits);edit(edits)
    if operation=='replace':
        find=normalize(params['find']);replacement=normalize(params['replacement'])
        pattern=re.compile(find if params['regex'] else re.escape(find),re.IGNORECASE if params['ignore_case'] else 0)
        offsets=[0]
        for char in text:offsets.append(offsets[-1]+len(char.encode('utf-8')))
        edits=[]
        for match in pattern.finditer(text):
            if match.start()!=match.end():matches.append([offsets[match.start()],offsets[match.end()]])
            edits.append((match.start(),match.end(),match.expand(replacement) if params['regex'] else replacement))
        count=len(edits);edit(edits)
    elif operation=='trim':substitute(re.compile(r'^[^\S\n]+|[^\S\n]+$',re.MULTILINE),lambda _:'')
    elif operation=='remove-empty':
        kept=[match for match in re.finditer(r'[^\n]+',text) if match[0].strip()]
        if not kept:raise ValueError('整理后的结果为空')
        edits=[(0,kept[0].start(),'')]
        edits.extend((a.end()+1,b.start(),'') for a,b in zip(kept,kept[1:]))
        edits.append((kept[-1].end(),len(text),''));edit(edits)
    elif operation=='space':substitute(re.compile(r'[^\S\n]+'),lambda _:' ')
    elif operation in ('upper','lower'):
        transformed=text.upper() if operation=='upper' else text.lower()
        edit([(index,index+1,char.upper() if operation=='upper' else char.lower()) for index,char in enumerate(text)])
        if units(transformed)!=units(text):raise ValueError('此文字的大小写映射暂不支持')
        text=transformed
    elif operation=='join':substitute(re.compile('\n'),lambda _:normalize(params['separator']))
    elif operation=='list':
        edits,count=list_edits(text,params);edit(edits)
    elif operation in ('bullet','numbered','unlist'):
        edits=[];number=0
        for line in re.finditer(r'(?m)^[^\S\n]*\S[^\n]*',text):
            start=line.start()+len(re.match(r'[^\S\n]*',line[0])[0])
            marker=re.match(r'(?:[•●▪◦*+\-][^\S\n]+|[0-9]+[.)、][^\S\n]+)',text[start:line.end()])
            end=start+(marker.end() if marker else 0)
            if not text[end:line.end()].strip():continue
            number+=1
            prefix='• ' if operation=='bullet' else str(number)+'. ' if operation=='numbered' else ''
            edits.append((start,end,prefix))
        edit(edits)
    elif operation=='split':
        if params['mode']=='lines':ends=line_ends(text,params['groups'])
        else:
            if not params['separator']:raise ValueError('分隔符或正则不能为空')
            pattern=re.compile(params['separator'] if params['mode']=='regex' else re.escape(normalize(params['separator'])))
            ends=[]
            for match in pattern.finditer(text):
                end=match.end()
                if end and (not ends or end!=ends[-1]):ends.append(end)
                if len(ends)>64:raise ValueError('拆分最多64组，请缩小匹配范围')
            if not ends or ends[-1]!=len(text):ends.append(len(text))
        groups=[];spans=[];start=0
        for end in ends:
            value=text[start:end-1] if text[end-1:end]=='\n' else text[start:end]
            if re.search(r'[^\s\ufeff]',value):groups.append(value);spans.append((start,start+len(value)))
            start=end
        if not 2<=len(groups)<=64:raise ValueError('拆分需要2至64组非空文字')
        if params['layout']=='lines':
            offsets=[0]
            for char in text:offsets.append(offsets[-1]+units(char))
            mapped=[];ends=[];end=0
            for index,(group,(start,stop)) in enumerate(zip(groups,spans)):
                mapped.extend(origins[offsets[start]:offsets[stop]])
                if index<len(groups)-1:mapped.append(origins[offsets[stop]])
                end+=len(group)+(index<len(groups)-1);ends.append(end)
            mapped.append(mapped[-1]);origins=mapped;text='\n'.join(groups)
        elif any(end<len(text) and text[end-1]!='\n' for end in ends):
            raise ValueError('保留位置仅支持原有换行边界；不换行拆分请选择“逐行排列”')
        breaks=[units(text[:end]) for end in ends]
        preview='\n\n'.join(str(index+1)+':\n'+group for index,group in enumerate(groups));count=len(groups)
    value=text.replace('\n','\r')
    if not value or units(value)>20000:raise ValueError('结果不能为空或超过20000字符')
    changed=value!=source
    if operation not in ('replace','split','list'):count=int(changed)
    result={'value':value,'origins':origins,'count':count,'changed':changed,
            'kind':'split' if operation=='split' else 'replace','characters':len(text),
            'preview':preview if preview is not None else text,'matches':matches}
    if draft_origins is not None:result['draft_origins']=draft_origins
    if breaks is not None:result.update(breaks=breaks,layout=params['layout'])
    return result


class _Evaluator:
    def __init__(self):
        import win32con,win32event,win32job,win32process
        self.folder=tempfile.TemporaryDirectory(prefix='photoshop-text-')
        self.path=Path(self.folder.name);self.process=self.job=self.request=self.response=None
        session=secrets.token_hex(16)
        (self.path/'session.json').write_text(json.dumps({'session':session}),'utf-8')
        thread=None
        try:
            self.request=win32event.CreateEvent(None,False,False,'Local\\PhotoshopMenuText-'+session+'-request')
            self.response=win32event.CreateEvent(None,False,False,'Local\\PhotoshopMenuText-'+session+'-response')
            self.job=win32job.CreateJobObject(None,'')
            limits=win32job.QueryInformationJobObject(self.job,win32job.JobObjectExtendedLimitInformation)
            limits['BasicLimitInformation']['LimitFlags']|=win32job.JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
            win32job.SetInformationJobObject(self.job,win32job.JobObjectExtendedLimitInformation,limits)
            command=[sys.executable,'--text-plan-worker',str(self.path)] if getattr(sys,'frozen',False) else [sys.executable,'-m','backend.text_plan',str(self.path)]
            startup=win32process.STARTUPINFO();startup.dwFlags=win32con.STARTF_USESHOWWINDOW;startup.wShowWindow=win32con.SW_HIDE
            self.process,thread,_,_=win32process.CreateProcess(sys.executable,subprocess.list2cmdline(command),None,None,False,
                subprocess.CREATE_NO_WINDOW|win32process.CREATE_SUSPENDED|win32process.BELOW_NORMAL_PRIORITY_CLASS,None,str(Path(__file__).resolve().parents[1]),startup)
            win32job.AssignProcessToJobObject(self.job,self.process);win32process.ResumeThread(thread)
        except BaseException:
            self.close();raise
        finally:
            if thread:thread.Close()

    def close(self):
        import win32api,win32con,win32event,win32process
        # This process and its bootloader children only compute local text, never PS.
        if self.process and win32process.GetExitCodeProcess(self.process)==win32con.STILL_ACTIVE:
            if self.job:
                import win32job
                win32job.TerminateJobObject(self.job,1)
            if win32process.GetExitCodeProcess(self.process)==win32con.STILL_ACTIVE:win32api.TerminateProcess(self.process,1)
            win32event.WaitForSingleObject(self.process,1000)
        for name in ('process','job','request','response'):
            handle=getattr(self,name,None)
            if handle:handle.Close();setattr(self,name,None)
        self.folder.cleanup()


_evaluator=None
_evaluation_lock=threading.Lock()


def _close_evaluator():
    global _evaluator
    if _evaluator:_evaluator.close();_evaluator=None


atexit.register(_close_evaluator)


def evaluate(source,params):
    global _evaluator
    import win32con,win32event,win32process
    params=parameters(params)
    if not isinstance(source,str) or units(source)>20000 or '\0' in source:raise ValueError('原文无效或超过处理上限')
    if not _evaluation_lock.acquire(blocking=False):raise ValueError('文字预览计算中，本次未接受')
    try:
        if _evaluator and win32process.GetExitCodeProcess(_evaluator.process)!=win32con.STILL_ACTIVE:_close_evaluator()
        if _evaluator is None:_evaluator=_Evaluator()
        worker=_evaluator;identity=secrets.token_hex(16)
        try:
            path=worker.path/'response.json';path.unlink(missing_ok=True)
            (worker.path/'request.json').write_text(json.dumps({'id':identity,'source':source,'params':params},ensure_ascii=False),'utf-8')
            win32event.SetEvent(worker.request)
            waited=win32event.WaitForMultipleObjects([worker.response,worker.process],False,2000)
            if waited==win32con.WAIT_TIMEOUT:raise ValueError('计算预览超时，已停止独立计算进程；请简化正则或文字')
            if waited!=win32con.WAIT_OBJECT_0 or not path.exists() or path.stat().st_size>1024*1024:raise ValueError('文字预览计算未完成')
            result=json.loads(path.read_text('utf-8'))
            if result.get('id')!=identity:raise ValueError('文字预览身份不一致')
            path.unlink();(worker.path/'request.json').unlink()
        except BaseException:
            _close_evaluator();raise
        if result.get('error'):raise ValueError(result['error'])
        return result['result']
    finally:_evaluation_lock.release()


def cli():
    import win32con,win32event
    folder=Path(sys.argv[1]);request=folder/'request.json';response=folder/'response.json'
    if len(sys.argv)!=2:raise ValueError('Invalid preview envelope')
    session=json.loads((folder/'session.json').read_text('utf-8'))['session']
    if not isinstance(session,str) or not re.fullmatch('[0-9a-f]{32}',session):raise ValueError('Invalid preview session')
    incoming=win32event.OpenEvent(win32con.SYNCHRONIZE,False,'Local\\PhotoshopMenuText-'+session+'-request')
    outgoing=win32event.OpenEvent(win32con.EVENT_MODIFY_STATE,False,'Local\\PhotoshopMenuText-'+session+'-response')
    while win32event.WaitForSingleObject(incoming,win32event.INFINITE)==win32con.WAIT_OBJECT_0:
        if request.stat().st_size>200000:raise ValueError('Invalid preview envelope')
        value=json.loads(request.read_text('utf-8'));result={'id':value['id']}
        try:result['result']=compute(value['source'],value['params'])
        except (ValueError,re.error,OverflowError,IndexError) as error:result['error']=str(error)[:300]
        response.write_text(json.dumps(result,ensure_ascii=False),'utf-8')
        win32event.SetEvent(outgoing)


if __name__=='__main__':cli()
