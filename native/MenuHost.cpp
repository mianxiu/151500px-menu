#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <imm.h>
#ifndef WS_EX_NOREDIRECTIONBITMAP
#define WS_EX_NOREDIRECTIONBITMAP 0x00200000L
#endif
#include <d3d11.h>
#include <dxgi1_3.h>
#include <dcomp.h>
#include <dwmapi.h>
#include <wincodec.h>
#include <shellapi.h>
#include <array>
#include <string>
#include <thread>
#include <iostream>
#include <memory>
#include <algorithm>
#include <cmath>
#include <vector>
#include <sstream>
#include <regex>
#include <fstream>
#include <filesystem>
#include <chrono>
#include "vendor/json/json.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include "MenuGlyphs.h"
#include "FigmaMenuIcons.h"
#include "StyledMenuIcons.h"
#include "MenuUiMetrics.h"
#include "MenuShadow.h"
static constexpr int pieRightInset=int(menuQuickGroupOffset+menuQuickGroupMaxCount*figmaDisplayIconWidth+menuShadow::extent);
static std::string uiFontSize="small";
#include "MenuTypography.h"
static void requireText(const std::string& text);
#include "MenuControls.h"
#include "ActivityIndicator.h"

using Json=nlohmann::json;
#include "DiagnosticHint.h"
#include "PreviewPendingHint.h"
static HWND window;
static void (*platformShowWindow)(ImGuiViewport*)=nullptr;
static void showPopupWindow(ImGuiViewport* viewport) {
    if (IsWindowVisible(window)) platformShowWindow(viewport);
}
static DWORD backendParent=0;
static ID3D11Device* device;
static ID3D11DeviceContext* context;
static std::string renderMode="auto",activeRenderMode="",renderAdapter,benchmarkRenderMode="auto";
static bool rendererDirty=false;
static bool benchmarkRenderCompletion=false,benchmarkMenuOnly=false;
static Json graphicsInfo(){return Json{{"requested",renderMode},{"active",activeRenderMode},{"adapter",renderAdapter}};}
static IDXGISwapChain* swapChain;
static HANDLE frameLatency=nullptr;
static bool frameGranted=false;
static IDCompositionDevice* composition;
static IDCompositionTarget* compositionTarget;
static IDCompositionVisual* compositionVisual;
static IDCompositionEffectGroup* compositionEffect;
static float compositionOpacity=-1;
static ID3D11RenderTargetView* target;
static bool running=true,opened=false,contextMode=false,focusSearch=false;
static bool allowed=false,capable=false,rightHide=false;
static std::string document="连接中…",notice;
static int renderedHit=-1,pendingHit=-1;
static bool stabilityInspection=false,stabilityObserveBusy=false;
static int lastVisibleVertices=0;
static bool taskBusy=false,taskFailed=false,taskHasResult=false;
static bool maskPreviewStarted=false,shapeDraftOnly=false;
static bool busyFeedback=false;
// Only a Caps menu explicitly opened while a task was running requests wakeup.
// Ordinary submitted-action feedback keeps its existing close behavior.
static bool busyMenuWake=false;
static bool hoverReturnConfirmed=false;
static ULONGLONG feedbackStarted=0;
static ULONGLONG completionStarted=0;
static ULONGLONG taskStarted=0;
static std::string taskName,taskError,taskRecovery;
static int taskQueueCount=0;
struct PreviewProgress {
    bool active=false;std::string stage,phase,operation;int done=0,total=0,frame=0;float displayed=0;
    ULONGLONG changedAt=0;
    void clear(){*this=PreviewProgress{};}
    void update(const Json& v){
        const auto next=v.value("stage",std::string());const int f=v.value("frame",0),n=v.value("total",0);
        if(stage!=next || frame!=f || total!=n)displayed=0;
        active=true;stage=next;phase=v.value("phase",std::string());operation=v.value("operation",std::string());frame=f;total=n;done=v.value("done",0);changedAt=GetTickCount64();
    }
    std::string label() const {
        std::string text=stage=="read"?"读取图层":stage=="write"?"写入调整":stage=="verify"?"核验图层":
            stage=="redraw"?"正在刷新画布":stage=="restore"?"正在恢复原值":stage=="restore-verify"?"核验恢复结果":
            stage=="restore-redraw"?"正在刷新恢复后的画布":stage=="cancel-wait"?"已请求取消 · 等待当前步骤返回":
            stage=="move"?"正在移动图层":stage=="latest"?"正在应用最新调整":stage=="commit"?"正在确认调整":stage=="ready"?"当前预览已更新":"正在准备预览";
        if(total>0)text+=" · "+std::to_string(done)+"/"+std::to_string(total);
        if(stage!="ready" && GetTickCount64()-changedAt>3000)text+=" · 等待 Photoshop";
        return operation.empty()?text:operation+" · "+text;
    }
    float advance(float dt){
        float actual=total>0?std::min(1.f,float(done)/total):0.f;
        displayed=std::min(actual,displayed+(actual-displayed)*std::min(1.f,dt/0.09f));return displayed;
    }
};
static PreviewProgress previewProgress;
static std::string taskStatus() {
    if(previewProgress.active && taskRecovery.empty())return previewProgress.label();
    if(maskPreviewStarted && !taskBusy)return "预览中：填充／描边／蒙版";
    if (!taskRecovery.empty()) return taskRecovery;
    if (taskBusy) return "执行中："+(taskName.empty()?std::string("Photoshop 操作"):taskName)+
        " · "+std::to_string((GetTickCount64()-taskStarted)/1000)+" 秒"+
        (taskQueueCount>0?" · 等待 "+std::to_string(taskQueueCount)+" 项":"");
    if(shapeDraftOnly)return "本地草稿 · 确认后生效（兼容模式）";
    if (taskHasResult && taskFailed && taskError.find("背景图层")!=std::string::npos)
        return "背景图层请先解锁，再设置标签";
    if (taskHasResult && taskFailed) return std::string("操作失败：")+
        (taskName.empty()?std::string("Photoshop 操作"):taskName);
    return "";
}
static void updateBusy(bool busy,bool queueReady=false,int queueCount=0) {
    if(stabilityInspection && !stabilityObserveBusy)busy=false;
    if (busy && !taskBusy) taskStarted=GetTickCount64();
    if (busy && window && IsWindowVisible(window)) busyFeedback=true;
    if(!busy && taskBusy && taskHasResult && !taskFailed)completionStarted=GetTickCount64();
    taskBusy=busy;taskQueueCount=0;allowed=capable&&!busy;
}
static std::array<std::string,4> shortcuts={"CapsLock + A","CapsLock + C","CapsLock + W","CapsLock + S"};
static const char* actions[]={"select","color","moveToTop","moveToBottom"};
static const char* labels[]={"选择组内图层","橙色图层标签","移到顶部","移到底部"};
static const char* keywords[]={"select group layers","orange color tag","move top","move bottom"};
static char search[256]{};
static int selection=0;
static bool holdMode=false;
static bool holdFinished=false;
static bool completionReady() {
    return busyFeedback && !taskBusy && taskHasResult && !taskFailed && taskRecovery.empty() &&
        completionStarted && GetTickCount64()-completionStarted>=450;
}
static std::array<RECT,4> holdRows{};
static POINT holdOrigin{};
static bool holdMoved=false;
static ImVec2 pieCenter{};
static int pieAnchorX=-1;
static bool shapePanelRight=false;
static bool pieReady=false;
static bool pinned=false;
static LPARAM menuEpoch=0;
static ULONGLONG pieStarted=0;
static bool pieAnimating=false;
// Presentation-only timing: never participates in hover hits or dispatch.
struct HoverTipState {
    int shown=-1,group=-1,pending=-1;
    ULONGLONG pendingSince=0,lastSeen=0;
    void reset() {*this=HoverTipState{};}
    int update(int action,int family,ULONGLONG now,bool inGap,bool enabled) {
        if(!enabled) {reset();return -1;}
        if(action<0) {
            pending=-1;
            if(inGap && shown>=0 && now-lastSeen<=90)return shown;
            reset();return -1;
        }
        if(group!=family) {reset();group=family;}
        lastSeen=now;
        if(action==shown) {pending=action;pendingSince=now;return shown;}
        if(pending!=action) {pending=action;pendingSince=now;}
        if(now-pendingSince>=ULONGLONG(shown<0?150:60))shown=action;
        return shown;
    }
};
static HoverTipState hoverTip;
// Continuous backdrop and crossfaded text; no widgets or hit rectangles.
struct TipVisual {
    bool ready=false;int family=-1;
    ImVec4 bounds{},from{},to{};
    std::string text,previous;
    ULONGLONG began=0;
    float blend=1;
    void reset() {*this=TipVisual{};}
    void sample(ULONGLONG now) {
        if(!ready)return;
        const float t=std::clamp(float(now-began)/120.f,0.f,1.f);
        blend=1.f-(1.f-t)*(1.f-t)*(1.f-t);
        bounds={from.x+(to.x-from.x)*blend,from.y+(to.y-from.y)*blend,
            from.z+(to.z-from.z)*blend,from.w+(to.w-from.w)*blend};
        if(t>=1)previous.clear();
    }
    void update(const std::string& next,ImVec4 target,int nextFamily,ULONGLONG now) {
        sample(now);
        if(!ready || family!=nextFamily) {
            ready=true;family=nextFamily;bounds=from=to=target;
            text=next;previous.clear();began=now>=120?now-120:0;blend=1;
        } else if(next!=text || target.x!=to.x || target.y!=to.y || target.z!=to.z || target.w!=to.w) {
            from=bounds;to=target;previous=text;text=next;began=now;blend=0;
        }
    }
};
static TipVisual tipVisual;


static float ringAngle=0;
static bool ringAngleReady=false;
static float followAngle(float current,float desired,float seconds) {
    constexpr float tau=6.28318530718f;
    return current+std::remainder(desired-current,tau)*(1.f-std::exp(-std::max(0.f,seconds)/.032f));
}
static float previewAnimationMs=-1;
static float pieAnimation(float elapsedMs,bool reduced=false) {
    return reduced?1.f:std::clamp(elapsedMs/60.f,0.f,1.f);
}
struct LabelColor {const char* id;const char* name;ImU32 color;ImU32 disabled=0;};
static const std::array<LabelColor,12> defaultLabelColors={{{"none","无颜色 / 清除标签",IM_COL32(105,109,117,255)},
    {"red","红色",IM_COL32(224,91,88,255)},{"orange","橙色",IM_COL32(229,151,63,255)},
    {"yellowColor","黄色",IM_COL32(226,201,83,255)},{"grain","绿色",IM_COL32(110,176,99,255)},
    {"blue","蓝色",IM_COL32(88,144,213,255)},{"violet","紫色",IM_COL32(169,125,207,255)},
    {"gray","灰色",IM_COL32(169,172,180,255)},
    {"seafoam","海沫色",IM_COL32(93,171,164,255)},{"indigo","靛蓝",IM_COL32(112,111,188,255)},
    {"magenta","洋红",IM_COL32(189,91,157,255)},{"fuchsia","紫红",IM_COL32(211,120,184,255)}}};
static auto labelColors=defaultLabelColors;
static bool applyLabelColors(const Json& values) {
    labelColors=defaultLabelColors;
    if(!values.is_array() || (values.size()!=8 && values.size()!=12))return false;
    auto next=defaultLabelColors;
    for(size_t i=0;i<values.size();++i) {
        const auto& value=values[i];
        if(!value.is_object() || !value.contains("id") || !value["id"].is_string() || value["id"]!=next[i].id)return false;
        for(const char* field:{"rgb","disabled_rgb"}) {
            if(!value.contains(field)){if(std::string(field)=="disabled_rgb")continue;return false;}
            const auto& rgb=value[field];
            if(!rgb.is_array() || rgb.size()!=3)return false;
            for(const auto& channel:rgb)if(!channel.is_number_integer() || channel.get<int64_t>()<0 || channel.get<int64_t>()>255)return false;
            auto color=IM_COL32(rgb[0].get<int>(),rgb[1].get<int>(),rgb[2].get<int>(),255);
            if(std::string(field)=="rgb")next[i].color=color;else next[i].disabled=color;
        }
    }
    labelColors=next;return true;
}
static std::array<RECT,12> labelRows{};
static int labelCount=8;
static int labelColumns(){return 4;}
static int labelRowCount(){return labelCount/labelColumns();}
static int supportedLabelCount(const Json& ps) {
    int host=0,hostMinor=0,major=0,minor=0,patch=0;
    std::sscanf(ps.value("photoshop_version",std::string()).c_str(),"%d.%d",&host,&hostMinor);
    std::sscanf(ps.value("uxp_version",std::string()).c_str(),"%d.%d.%d",&major,&minor,&patch);
    return (host>25 || host==25 && hostMinor>=2) && ps.value("uxp_ready",false) && ps.value("extended_label_colors",false) && (major>0 || minor>3 || minor==3 && patch>=79)?12:8;
}
static constexpr int recentActionBase=16;
struct RecentPreset {std::string id,name,size,detail;bool available=true;};
static std::vector<RecentPreset> recentPresets,shownPresets;
static std::vector<RECT> recentRows;
static std::vector<std::string> recentTips;
static RECT recentPanelBounds{};
static std::string recentMessage="等待读取最近预设";
static bool recentReady=false;

// Backend resolves editor identity outside the hook and sends it synchronously
// before showing the popup: 0 generic, 1 canvas, 2 layers, 3 text input, 4 Home.
static int panelKind=0;
// Home is authorized by the fresh Win32 workspace/context check at popup
// preparation. COM can reject status reads while New Document is closing;
// that cached connection result must not disable the native Ctrl+N entry.
static bool homepageCapable=true;
static bool layerAware=false,layerBackground=false,layerLocked=false,layerCrossParents=false;
static unsigned layerMask=15;
static int layerKind=0,layerCount=0;
static unsigned quickMask=0;
static const std::array<const char*,7> quickActions={"layer.new","layer.solid","layer.group","adjustment.curves","adjustment.hueSaturation","adjustment.colorBalance","adjustment.blackWhite"};
static const std::array<const char*,7> quickLabels={"新建空白图层","新建纯色填充图层","选中图层打组","新建曲线调整图层","新建色相饱和度调整图层","新建色彩平衡调整图层","新建黑白调整图层"};
static std::array<RECT,7> quickRows{};
static std::array<RECT,11> capsRows{};
static const char* capsActions[]={"copyStyle","pasteStyle","editStyle","selectAbove","selectBelow","moveUp","moveDown","moveDocumentTop","moveDocumentBottom","convertSmartObjectsIndividually","convertSmartObject"};
static const char* capsKeys[]={"c","v","f","w","s","上移","下移","文档顶","文档底","多层智能对象","单层智能对象"};
static const char* capsLabels[]={"复制图层样式","粘贴图层样式","打开图层样式","单选上一个图层（Shift扩展多选）","单选下一个图层（Shift扩展多选）","上移一层","下移一层","移到文档顶部（移出当前组）","移到文档底部（移出当前组）","各自转为智能对象（一次撤销整批）","转换为一个智能对象（多选合并）"};
static bool capsReady=false;
static bool documentSortReady=false;
static bool individualSmartReady=false;
static bool layerSearchReady=false,textSplitReady=false,layerPreviewReady=false;
static RECT toolRow{};
static bool menuToolsVisible();
static int capsCount() {return panelKind==2?9:5;}
static int capsBottomCount() {return 5;}

static constexpr float quickGroupGap=4;
static float quickGroupY(bool lower);
static const char* rowAction(int i) {return panelKind==4?"document.new":actions[i];}
static const char* rowLabel(int i) {return panelKind==4?"新建文档":i==2?"置顶":i==3?"置底":i==0 && layerAware && layerKind!=3?"选择相邻图层":labels[i];}
static const char* rowShortcut(int i) {return panelKind==4?"Ctrl + N":shortcuts[i].c_str();}
static bool inputFocused=false;
static bool returnInputsIdle() {
    return !inputFocused && panelKind==2 && !maskPreviewStarted &&
        !(GetAsyncKeyState(VK_CAPITAL)&0x8000) && !(GetAsyncKeyState(VK_LBUTTON)&0x8000) &&
        !(GetAsyncKeyState(VK_RBUTTON)&0x8000) && !(GetAsyncKeyState(VK_MBUTTON)&0x8000);
}
static bool canExecute() {return !menuToolsVisible() && !maskPreviewStarted && (panelKind==4?homepageCapable && !taskBusy:allowed) && !busyFeedback && !inputFocused && panelKind!=3;}
static bool toolEnabled(int i) {return canExecute() && panelKind!=4 && (i==0?layerSearchReady:textSplitReady && layerAware && (layerKind==4 || layerKind==5) && layerCount==1 && !layerLocked);}
static bool capsEnabled(int i) {
    if(i==9 || i==10)return (i!=9 || layerCount>1) && (i==9 || !layerCrossParents) && individualSmartReady && capsReady && canExecute() && panelKind!=3 && panelKind!=4 && layerAware && layerCount && layerKind && !layerLocked && !layerBackground;
    if(i>=capsCount() || i>=7 && !documentSortReady)return false;
    if(!capsReady || !canExecute() || panelKind==4 || !layerAware || !layerCount || !layerKind)return false;
    if((i==0 || i==2) && layerCount!=1)return false;
    return !(i==1 || i==2 || i>=5) || (!layerLocked && !layerBackground && !layerCrossParents);
}

static bool rowEnabled(int i) {return !((panelKind==0 || panelKind==1) && i>=2) && canExecute() && (panelKind==4 || !layerAware || (layerMask & (1u<<i)));}
static bool quickEnabled(int i) {return layerAware && panelKind!=4 && canExecute() && (quickMask & (1u<<i));}
static std::string layerTitle() {
    const char* names[]={"图层识别待更新","背景图层","像素图层","图层组","文字图层","智能对象","形状／填充图层","调整图层","混合类型","其他图层"};
    std::string name=names[std::clamp(layerKind,0,9)];
    if(layerCount>1)name="多选 "+std::to_string(layerCount)+" · "+name;
    if(layerLocked)name+="（位置锁定）";
    return name;
}
static const char* panelTitle() {
    return panelKind==4?"Photoshop 首页":panelKind==1?"画布操作":panelKind==2?"图层操作":panelKind==3?"输入控件":"通用操作";
}
static bool rowVisible(int i) {return panelKind==4?i==0:panelKind!=3 && ((panelKind!=1 && panelKind!=0) || i<2);}
static float scale=1;
#include "PerceptualPicker.h"
#include "GradientPanel.h"
static bool gradientWindow=false;
static bool gradientLaunching=false;
static GradientPanel gradientPanel;
static HWND gradientAnchor=nullptr;
static constexpr UINT_PTR gradientOwnerTimer=1;
static struct GradientOwner {
    HWND hwnd=nullptr;
    HANDLE process=nullptr;
    DWORD pid=0;
    bool hidden=false,closeRequested=false;

    ~GradientOwner(){if(process)CloseHandle(process);}
    bool bind(){
        if(!hwnd)return true;
        wchar_t name[32]{};
        if(!GetClassNameW(hwnd,name,32) || std::wstring_view(name)!=L"Photoshop")return false;
        GetWindowThreadProcessId(hwnd,&pid);
        return pid && (process=OpenProcess(SYNCHRONIZE,FALSE,pid));
    }
    bool alive() const{
        DWORD current=0;GetWindowThreadProcessId(hwnd,&current);
        return current==pid && WaitForSingleObject(process,0)==WAIT_TIMEOUT;
    }
    bool usable() const{return !IsIconic(hwnd) && IsWindowVisible(hwnd) && IsWindowEnabled(hwnd);}
    bool foreground(HWND foreground) const{
        DWORD foregroundPid=0;GetWindowThreadProcessId(foreground,&foregroundPid);
        if(foregroundPid==GetCurrentProcessId())return true;
        if(foregroundPid!=pid)return false;
        const HWND top=GetAncestor(foreground,GA_ROOT);
        if(top==hwnd)return true;
        wchar_t name[32]{};
        GetClassNameW(top,name,32);
        const std::wstring_view kind(name);
        return (kind==L"OWL.DocumentWindow" || kind==L"OWL.Dock") && GetAncestor(top,GA_ROOTOWNER)==hwnd;
    }
    void update(HWND editor){
        if(!alive()){
            if(IsWindowVisible(editor))ShowWindow(editor,SW_HIDE);
            if(!closeRequested && !gradientPanel.terminal){
                closeRequested=true;gradientPanel.finish(false);
            }
            return;
        }
        if(!usable() || !foreground(GetForegroundWindow())){
            if(IsWindowVisible(editor) && !IsIconic(editor)){
                hidden=true;ShowWindow(editor,SW_HIDE);
            }
            // The backend saves this draft before checking its foreground write guard.
            gradientPanel.flush();
        }else if(hidden && !gradientPanel.terminal){
            hidden=false;
            SetWindowPos(editor,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
        }
    }
} gradientOwner;
static bool capsDividerAt(int i,POINT point) {
    if(i>=9)return layerCount>1 && (i==9?point.y<capsRows[i].top+scale:point.y>=capsRows[i].bottom-scale);
    const int first=i>=9?9:i>=7?7:panelKind==2 && i>=5?5:0;
    const int last=i>=9?10:i>=7?8:panelKind==2 && i>=5?6:capsBottomCount()-1;
    return ((i>first || panelKind==2 && i==5) && point.x<capsRows[i].left+scale) ||
           (i<last && point.x>=capsRows[i].right-scale);
}

static std::string uiTheme="dark";
static bool fontDirty=false;
static ImFontGlyphRangesBuilder requiredGlyphs;

static unsigned fontBuilds=0;
static void requireText(const std::string& text) {
    const char* p=text.c_str();
    while (*p) {
        unsigned int c=0;int n=ImTextCharFromUtf8(&c,p,nullptr);
        if (n<=0) break;p+=n;
        if (c<=IM_UNICODE_CODEPOINT_MAX && !requiredGlyphs.GetBit(c)) {
            requiredGlyphs.AddChar(static_cast<ImWchar>(c));fontDirty=true;
        }
    }
}
static void theme();
static ImU32 tint(ImGuiCol role) {return ImGui::GetColorU32(role);}
static bool knownTheme(const std::string& value) {
    return value=="light" || value=="light-gray" || value=="dark-gray" || value=="dark";
}
static unsigned renderedFrames=0;
static std::wstring preview;
static std::string previewKind="menu";
static int previewRecentHover=-1;
static int previewQuickHover=-1,previewActionHover=-1,previewCapsHover=-1,previewShapeHover=-1;
static bool previewOwningEditor=false,previewOrdinary=false,previewExitEditor=false;
static float previewShapeHeight=0;
static bool previewLifecycle=false;
static bool previewLive=false;
static bool previewShape=false;
static std::string previewTool;
static int previewTextTab=0;
static std::string previewProgressStage;
static std::wstring shapeBenchmarkOutput;
static bool selfTest=false;
static std::vector<std::string> testedActions;
static Json testedGradientOpen;
static size_t labelTestActionCount=0;
static ImVec2 bottomRow;
static void send(const char* action,const Json& params=Json()) {
    if(stabilityInspection)return;
    for(int i:{9,10})if(std::string(action)==capsActions[i] && !capsEnabled(i))return;
    if((panelKind==0 || panelKind==1) && (std::string(action)=="moveToTop" || std::string(action)=="moveToBottom" ||
                       std::string(action)=="moveUp" || std::string(action)=="moveDown" || std::string(action)=="moveDocumentTop" || std::string(action)=="moveDocumentBottom"))return;
    for(int i=0;i<7;++i)if(std::string(action)==quickActions[i] && !quickEnabled(i))return;
    if(layerAware && panelKind!=4) {
        int row=std::string(action).rfind("label.",0)==0?1:-1;
        for(int i=0;i<4;++i)if(std::string(action)==actions[i])row=i;
        if(row>=0 && !rowEnabled(row))return;
    }
    if (selfTest) {testedActions.emplace_back(action);if(!params.is_null())testedGradientOpen=params;return;}
    // The foreground menu grants its own backend permission to return focus
    // to PS when it hides. Do not loosen foreground restrictions globally.
    if (backendParent) AllowSetForegroundWindow(backendParent);
    Json event={{"type","action"},{"action",action},{"session",menuEpoch}};
    if(!params.is_null())event["params"]=params;
    std::cout<<event.dump()<<std::endl;
}
static std::string normalized(std::string value) {
    std::string result;
    for (unsigned char c:value) if (!isspace(c)) result+=static_cast<char>(tolower(c));
    return result;
}
static ImVec2 basicLabelExtent(int index);
static float basicTextOffset();
#include "ShapeColorEditor.h"
static void sendMenuTool(const char* action,const Json& params) {
    if(stabilityInspection)return;
    if(selfTest){testedActions.emplace_back(action);return;}
    if(backendParent)AllowSetForegroundWindow(backendParent);
    std::cout<<Json{{"type","action"},{"action",action},{"session",menuEpoch},{"params",params}}.dump()<<std::endl;
}
#include "MenuToolsPanel.h"
static bool menuToolsVisible(){return menuTools.active || menuTools.opening;}
static bool matches(int i,const std::string& query) {
    std::string haystack=normalized(std::string(rowLabel(i))+" "+(panelKind==4?"新建 文档 new document create":keywords[i])+" "+rowShortcut(i));
    std::istringstream words(query);std::string word;
    while (words>>word) if (haystack.find(normalized(word))==std::string::npos) return false;
    return true;
}
static void releaseTarget() { if (target) { target->Release();target=nullptr; } }
static bool createTarget() {
    ID3D11Texture2D* buffer=nullptr;
    if (FAILED(swapChain->GetBuffer(0,IID_PPV_ARGS(&buffer)))) return false;
    HRESULT result=device->CreateRenderTargetView(buffer,nullptr,&target); buffer->Release();
    return SUCCEEDED(result);
}
static void setOpacity(float opacity) {
    if (compositionVisual && opacity!=compositionOpacity) {
        compositionEffect->SetOpacity(opacity);composition->Commit();compositionOpacity=opacity;
    }
}
static bool createDevice(HWND hwnd) {
    D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_0},level;
    HRESULT result=renderMode=="cpu"?E_FAIL:D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        levels,2,D3D11_SDK_VERSION,&device,&level,&context);
    const bool hardware=SUCCEEDED(result);
    if (FAILED(result)) result=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        levels,2,D3D11_SDK_VERSION,&device,&level,&context);
    if (FAILED(result)) return false;
    activeRenderMode=hardware?"gpu":"cpu";
    IDXGIDevice* dxgi=nullptr;IDXGIAdapter* adapter=nullptr;IDXGIFactory2* factory=nullptr;IDXGISwapChain1* chain=nullptr;
    result=device->QueryInterface(IID_PPV_ARGS(&dxgi));
    if (SUCCEEDED(result)) result=dxgi->GetAdapter(&adapter);
    if (SUCCEEDED(result)) {
        DXGI_ADAPTER_DESC info{};
        if(SUCCEEDED(adapter->GetDesc(&info))) {
            std::wstring name(info.Description);std::string ascii(name.begin(),name.end());
            renderAdapter=ascii;
            std::cerr<<"DX11 renderer: "<<(hardware?"hardware GPU":"software WARP")<<", adapter="<<ascii<<"\n";
        }
    }
    if (SUCCEEDED(result)) result=adapter->GetParent(IID_PPV_ARGS(&factory));
    RECT bounds{};GetClientRect(hwnd,&bounds);
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width=std::max<LONG>(1,bounds.right);desc.Height=std::max<LONG>(1,bounds.bottom);
    desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;
    desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;
    desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;desc.AlphaMode=DXGI_ALPHA_MODE_PREMULTIPLIED;
    desc.Flags=DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    if (SUCCEEDED(result)) result=factory->CreateSwapChainForComposition(device,&desc,nullptr,&chain);
    if (SUCCEEDED(result)) {
        IDXGISwapChain2* lowLatency=nullptr;
        result=chain->QueryInterface(IID_PPV_ARGS(&lowLatency));
        if (SUCCEEDED(result)) {
            result=lowLatency->SetMaximumFrameLatency(1);
            if (SUCCEEDED(result)) frameLatency=lowLatency->GetFrameLatencyWaitableObject();
            lowLatency->Release();
        }
        if (SUCCEEDED(result) && !frameLatency) result=E_FAIL;
        if (FAILED(result)) {chain->Release();chain=nullptr;}
    }
    if (SUCCEEDED(result)) {swapChain=chain;result=DCompositionCreateDevice(dxgi,__uuidof(IDCompositionDevice),reinterpret_cast<void**>(&composition));}
    if (SUCCEEDED(result)) result=composition->CreateTargetForHwnd(hwnd,TRUE,&compositionTarget);
    if (SUCCEEDED(result)) result=composition->CreateVisual(&compositionVisual);
    if (SUCCEEDED(result)) result=composition->CreateEffectGroup(&compositionEffect);
    if (SUCCEEDED(result)) result=compositionVisual->SetEffect(compositionEffect);
    if (SUCCEEDED(result)) result=compositionVisual->SetContent(swapChain);
    if (SUCCEEDED(result)) result=compositionTarget->SetRoot(compositionVisual);
    if (SUCCEEDED(result)) result=composition->Commit();
    if (factory) factory->Release();if (adapter) adapter->Release();if (dxgi) dxgi->Release();
    compositionOpacity=-1;
    frameGranted=false;
    if(FAILED(result) || !createTarget())return false;
    if(!menuShadow::create(device))std::cerr<<"Shadow atlas unavailable; drawing menu without shadows\n";
    return true;
}
static void releaseDevice() {
    menuShadow::release();
    releaseTarget();
    if (frameLatency) {CloseHandle(frameLatency);frameLatency=nullptr;}
    frameGranted=false;
    if (compositionEffect) {compositionEffect->Release();compositionEffect=nullptr;}
    if (compositionVisual) {compositionVisual->Release();compositionVisual=nullptr;}
    if (compositionTarget) {compositionTarget->Release();compositionTarget=nullptr;}
    if (composition) {composition->Release();composition=nullptr;}
    if (swapChain) {swapChain->Release();swapChain=nullptr;}
    if (context) {context->Release();context=nullptr;}
    menuTools.clearPreview();
    if (device) {device->Release();device=nullptr;}
}
static bool prepareRendererResources() {
    if(!ImGui_ImplDX11_CreateDeviceObjects())return false;
    // Register early uploads so backend shutdown releases them before device changes.
    auto* atlas=ImGui::GetIO().Fonts;
    auto& textures=ImGui::GetPlatformIO().Textures;
    for(auto* texture:atlas->TexList) {
        texture->RefCount=static_cast<unsigned short>(atlas->RefCount);
        if(!textures.contains(texture))textures.push_back(texture);
        if(texture->Status==ImTextureStatus_Destroyed && texture->Pixels && !texture->WantDestroyNextFrame)
            texture->SetStatus(ImTextureStatus_WantCreate);
        if(texture->Status==ImTextureStatus_WantCreate || texture->Status==ImTextureStatus_WantUpdates) {
            ImGui_ImplDX11_UpdateTexture(texture);
            if(texture->Status!=ImTextureStatus_OK)return false;
        }
    }
    return true;
}
static bool recoverRenderer() {
    // A Photoshop/GPU restart can invalidate this process's D3D device too.
    // Keep the CPU font atlas and rebuild the renderer on a fresh device.
    auto& platform=ImGui::GetPlatformIO();
    const auto clipboardGet=platform.Platform_GetClipboardTextFn;
    const auto clipboardSet=platform.Platform_SetClipboardTextFn;
    const auto clipboardData=platform.Platform_ClipboardUserData;
    const auto ime=platform.Platform_SetImeDataFn;
    const auto imeData=platform.Platform_ImeUserData;
    const auto shell=platform.Platform_OpenInShellFn;
    const auto shellData=platform.Platform_OpenInShellUserData;
    ImGui_ImplDX11_Shutdown();
    // DX11 shutdown destroys platform data for ALL viewports, including the
    // main one. Reinitialize Win32 too, while retaining our owned HWND.
    ImGui_ImplWin32_Shutdown();releaseDevice();
    if (!createDevice(window)) return false;
    if (!ImGui_ImplWin32_Init(window)) return false;
    // Backend shutdown also clears core-owned OS callbacks in ImGui 1.92.
    platform.Platform_GetClipboardTextFn=clipboardGet;
    platform.Platform_SetClipboardTextFn=clipboardSet;
    platform.Platform_ClipboardUserData=clipboardData;
    platform.Platform_SetImeDataFn=ime;
    platform.Platform_ImeUserData=imeData;
    platform.Platform_OpenInShellFn=shell;
    platform.Platform_OpenInShellUserData=shellData;
    platformShowWindow=ImGui::GetPlatformIO().Platform_ShowWindow;
    ImGui::GetPlatformIO().Platform_ShowWindow=showPopupWindow;
    if (!ImGui_ImplDX11_Init(device,context)) return false;
    return prepareRendererResources();
}
static void resetMenu() {
    hoverTip.reset();tipVisual.reset();
    busyFeedback=taskBusy;
    feedbackStarted=0;
    renderedHit=pendingHit=-1;
    search[0]=0;selection=0;focusSearch=!contextMode;
    capsRows={};
    holdRows={};
    labelRows={};
    quickRows={};
    toolRow={};menuTools.reset();
    recentRows.clear();
    pieReady=false;
    if (ImGui::GetCurrentContext()) { ImGui::GetCurrentContext()->OpenPopupStack.clear(); }
}
static void closePopups() {
    hoverTip.reset();tipVisual.reset();
    if (!ImGui::GetCurrentContext()) return;
    ImGui::GetCurrentContext()->OpenPopupStack.clear();
    // Destroy only secondary viewports. Their handles must be recreated when
    // the same submenu is opened again; preserve the main Win32 backend data.
    auto& platform=ImGui::GetPlatformIO();
    for (int i=1;i<platform.Viewports.Size;++i) {
        ImGui::DestroyPlatformWindow(static_cast<ImGuiViewportP*>(platform.Viewports[i]));
    }
}
static void finishPlatformFrame() {
    // This is mandatory for EVERY ended frame, even if ShowWindow hid the
    // main HWND during Present. Skipping it asserts on the next NewFrame.
    ImGui::UpdatePlatformWindows();
    // Keep the preheated PS sizes alive under the upstream baked-font cache.
    for(auto* cached:menuFonts)if(cached)cached->GetFontBaked(cached->LegacySize);
    if (IsWindowVisible(window)) ImGui::RenderPlatformWindowsDefault();
    if (!IsWindowVisible(window)) closePopups();
}
static std::string shortcut(const std::string& raw) {
    std::string result;size_t start=0;
    while (start<raw.size()) {
        size_t end=raw.find('+',start); std::string word=raw.substr(start,end==std::string::npos?end:end-start);
        if (word=="capslock") word="CapsLock";
        else if (word=="ctrl") word="Ctrl";
        else if (word=="shift") word="Shift";
        else if (word=="alt") word="Alt";
        else if (!word.empty()) word[0]=static_cast<char>(toupper(static_cast<unsigned char>(word[0])));
        if (!result.empty()) result+=" + "; result+=word;
        if (end==std::string::npos) break;start=end+1;
    }
    return result;
}
static void accept(const Json& event) {
    std::string type=event.value("type","");
    if(gradientWindow){
        if(type=="gradient.open"){
            if(gradientOwner.hwnd){
                const HWND foreground=GetForegroundWindow();
                if(!gradientOwner.alive() || !gradientOwner.usable() ||
                   (foreground!=gradientAnchor && !gradientOwner.foreground(foreground)) ||
                   !SetTimer(window,gradientOwnerTimer,80,nullptr)){
                    DestroyWindow(window);return;
                }
            }
            gradientPanel.open(event);
            const auto nextTheme=event.value("theme",uiTheme),nextFont=event.value("font",uiFontSize);
            if(knownTheme(nextTheme))uiTheme=nextTheme;
            if(nextFont=="small" || nextFont=="medium" || nextFont=="large")uiFontSize=nextFont;
            theme();setOpacity(1.f);
            ShowWindow(window,SW_SHOWNORMAL);SetForegroundWindow(window);
        }else if(type=="gradient.state"){
            gradientPanel.receive(event);
            if(gradientPanel.phase=="closed")DestroyWindow(window);
        }
        return;
    }
    if(type=="gradient.open.failed"){
        if(event.value("epoch",LPARAM(0))==menuEpoch){gradientLaunching=false;notice=event.value("message","");}
        return;
    }
    if(type=="menu.tools.opened" || type=="menu.tools.result" || type=="menu.tools.preview.result"){
        const bool pending=menuTools.previewPending;menuTools.receive(event);
        if(type=="menu.tools.preview.result" && pending && !menuTools.previewPending && !taskBusy)busyFeedback=false;
        return;
    }
    if(type=="preview.pending") {
        PreviewPendingHint::update(event,window,ImGui::GetStyle(),int(menuFontEms[menuFontIndex(MenuTextRole::Secondary)]));return;
    }
    if(type=="preview.progress") {
        if(event.value("epoch",LPARAM(-1))!=menuEpoch)return;
        previewProgress.update(event);requireText(previewProgress.label());
        if(event.value("compact",false) && IsWindowVisible(window)) {maskPreviewStarted=false;updateBusy(true);holdFinished=false;holdMode=true;pinned=true;pieReady=false;hoverTip.reset();tipVisual.reset();}
        return;
    }
    if(type=="preview.menu.confirmed") {
        if(shapeReturning && event.value("epoch",LPARAM(-1))==menuEpoch){shapeEditor.prepare(Json{{"available",false}});shapeReturning=false;hoverReturnConfirmed=true;}
        return;
    }
    if(type=="mask.preview") {maskPreviewStarted=event.value("active",false);if(!maskPreviewStarted)previewProgress.clear();return;}
    if (type=="diagnostic.progress") {DiagnosticHint::update(event);return;}
    if (type=="document") {
        if (event.contains("name") && event["name"].is_string()) document=event["name"].get<std::string>();
    } else if (type=="status") {
        const auto& data=event.at("data"); const auto& ps=data.at("photoshop");
        labelCount=supportedLabelCount(ps);
        applyLabelColors(data.value("label_colors",Json()));
        {int major=0,minor=0,patch=0;std::sscanf(ps.value("uxp_version",std::string()).c_str(),"%d.%d.%d",&major,&minor,&patch);documentSortReady=ps.value("uxp_ready",false) && (major>0 || minor>3 || minor==3 && patch>=64);individualSmartReady=ps.value("uxp_ready",false) && (major>0 || minor>3 || minor==3 && patch>=65);capsReady=ps.value("uxp_ready",false) && (major>0 || minor>3 || minor==3 && patch>=16);}
        {int major=0,minor=0,patch=0;std::sscanf(ps.value("uxp_version",std::string()).c_str(),"%d.%d.%d",&major,&minor,&patch);layerSearchReady=ps.value("uxp_ready",false) && (major>0 || minor>3 || minor==3 && patch>=57);textSplitReady=ps.value("uxp_ready",false) && (major>0 || minor>3 || minor==3 && patch>=59);}
        recentReady=ps.value("uxp_ready",false) && ps.value("uxp_version",std::string())!="0.3.1";
        {int major=0,minor=0,patch=0;std::sscanf(ps.value("uxp_version",std::string()).c_str(),"%d.%d.%d",&major,&minor,&patch);shapeClipboardReady=ps.value("uxp_ready",false) && (major>0 || minor>3 || minor==3 && patch>=41);}
        shapeClipboardAvailable=ps.value("shape_clipboard_available",false);
        layerPreviewReady=ps.value("layer_preview_available",false);
        if (data.contains("recent_documents")) {
            recentPresets.clear();const auto& recent=data["recent_documents"];
            recentMessage=recent.value("message","");
            for (const auto& item:recent.value("items",Json::array())) {
                recentPresets.push_back({item.value("id",""),item.value("name",""),item.value("size",""),item.value("detail",""),item.value("available",true)});
                requireText(recentPresets.back().name);requireText(recentPresets.back().size);requireText(recentPresets.back().detail);
                if (recentPresets.size()>=9) break;
            }
        }
        const std::string nextTheme=ps.value("ui_theme",uiTheme);
        if (knownTheme(nextTheme) && nextTheme!=uiTheme) {uiTheme=nextTheme;theme();}
        const std::string nextFont=ps.value("ui_font_size",uiFontSize);
        if ((nextFont=="small" || nextFont=="medium" || nextFont=="large") && nextFont!=uiFontSize) {
            uiFontSize=nextFont;
        }
        capable=ps.value("connected",capable) && data.at("settings").value("enabled",true);
        homepageCapable=data.at("settings").value("enabled",true);
        if (ps.contains("document")) capable=capable && ps["document"].is_string();
        updateBusy(ps.value("busy",false),ps.value("queue_ready",false),ps.value("queue_count",0));
        taskRecovery=ps.contains("uxp_recovery")?ps["uxp_recovery"].value("message",std::string()):std::string();
        if (!taskRecovery.empty()) {taskError.clear();taskFailed=false;requireText(taskRecovery);}
        document=ps.contains("document") && ps["document"].is_string()?ps["document"].get<std::string>():ps.value("message","Photoshop 未连接");
        const auto& settings=data.at("settings");rightHide=settings.value("right_click_hide",false);
        const std::string nextMode=settings.value("render_mode",std::string("auto"));
        if((nextMode=="auto" || nextMode=="gpu" || nextMode=="cpu") && nextMode!=renderMode){renderMode=nextMode;rendererDirty=true;}
        for (int i=0;i<4;++i) shortcuts[i]=shortcut(settings.at("shortcuts").value(actions[i],""));
    } else if (type=="operation") {
        if (event.value("busy",false) && event.contains("action")) {
            std::string action=event.value("action","");taskName=action;
            for (int i=0;i<4;++i) if (action==actions[i]) taskName=labels[i];
            if (action=="document.recent")taskName="按最近预设新建文档";
            if (action=="shape.paint")taskName="调整形状填充与描边";
            if (action=="shape.copyPaint")taskName="复制形状填充与描边";
            if (action=="shape.pastePaint")taskName="粘贴形状填充与描边";
            for (const auto& label:labelColors) if (action==std::string("label.")+label.id) taskName=std::string(label.name)+"标签";
            for(int i=0;i<7;++i)if(action==quickActions[i])taskName=quickLabels[i];
            for(int i=0;i<int(capsRows.size());++i)if(action==capsActions[i])taskName=capsLabels[i];
            if(action=="extendAbove")taskName="向上扩展图层多选";
            if(action=="extendBelow")taskName="向下扩展图层多选";
            if(action=="convertSmartObject")taskName="转换为智能对象";
            notice.clear();taskError.clear();taskRecovery.clear();taskFailed=false;taskHasResult=false;completionStarted=0;
        }
        updateBusy(event.value("busy",false),event.value("queue_ready",false),event.value("queue_count",0));
    }
    else if (type=="notice") {
        notice=event.value("message","");
        if (event.value("scope","")=="operation") {
            previewProgress.clear();
            taskFailed=!event.value("ok",true);taskHasResult=true;
            taskError=taskFailed?notice:"";
            completionStarted=taskFailed?0:GetTickCount64();
        }
    }
    else if (type=="popup.prepare") { previewProgress.clear();contextMode=event.value("mode","quick")=="context";resetMenu(); }
}
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
static bool quickDividerAt(int i,POINT point) {
    return (i!=0 && i!=3 && point.x<quickRows[i].left+scale) ||
        (i!=2 && i!=6 && point.x>=quickRows[i].right-scale);
}
static int holdActionAt(POINT point) {
    if(menuToolsVisible())return -1;
    if (shapeEyedropActive || !holdMode || !pieReady || holdFinished || pieAnimating || (!canExecute() && !shapeEditor.ready())) return -1;
    float dx=point.x-pieCenter.x,dy=point.y-pieCenter.y;
    float ox=static_cast<float>(point.x-holdOrigin.x),oy=static_cast<float>(point.y-holdOrigin.y);
    // Edge clamping may put the invocation point on top of a swatch. Protect
    // an untouched press, but don't leave an invisible hole after a gesture.
    if (ox*ox+oy*oy>=12*12*scale*scale) holdMoved=true;
    if (dx*dx+dy*dy<12*12*scale*scale || (!pinned && !holdMoved)) return -1;
    RECT bounds{};GetWindowRect(window,&bounds);
    if (!PtInRect(&bounds,point)) return -1;
    if(toolEnabled(1) && PtInRect(&toolRow,point))return 71;
    for(int i=0;i<int(capsRows.size());++i)if(capsEnabled(i) && PtInRect(&capsRows[i],point)){
        if(capsDividerAt(i,point))return -1;
        return 40+i;
    }
    if(shapeEditor.ready() && shapeEditor.hit(point))return 100;
    for(int i=0;i<7;++i)if(quickEnabled(i) && PtInRect(&quickRows[i],point)) {
        if(quickDividerAt(i,point))return -1;
        return 32+i;
    }
    for (int i=0;i<labelCount;++i) if (!pieAnimating && rowVisible(1) && rowEnabled(1) && PtInRect(&labelRows[i],point)) return 4+i;
    if (panelKind==4 && recentReady && !pieAnimating) for (size_t i=0;i<recentRows.size();++i)
        if(shownPresets[i].available && PtInRect(&recentRows[i],point))return recentActionBase+static_cast<int>(i);
    for (int i=0;i<4;++i) if (rowVisible(i) && rowEnabled(i) && PtInRect(&holdRows[i],point)) {
        if(i==2 && point.x>=holdRows[2].right-scale || i==3 && (point.x<holdRows[3].left+scale || panelKind==2 && capsReady && layerAware && point.x>=holdRows[3].right-scale))return -1;
        return i==1?-1:i;
    }
    // Only visible cards/swatches commit. Blank radial sectors are neutral.
    return -1;
}
static void setHoldMode(bool enabled) {
    gradientLaunching=false;
    busyMenuWake=enabled && taskBusy && panelKind==2 && !inputFocused;
    holdMode=enabled;holdFinished=false;pinned=false;holdMoved=false;resetMenu();focusSearch=false;
    pieStarted=0;pieAnimating=enabled;
    stopShapeEyedrop();
    ringAngleReady=false;shapeReturning=false;hoverReturnConfirmed=false;shapeHoverReady=false;shapeHoverBounds={};shapePanelOpen=false;shapeModeBounds={};shapeModeRows={};
    shownPresets=recentPresets;
    // The compositor retains the last presented surface while hidden. Never
    // expose that old, fully expanded pie before the new zero-alpha frame.
    setOpacity(enabled || busyFeedback?0.f:1.f);
    if (enabled) GetCursorPos(&holdOrigin);
}
static void finishHold(POINT point,bool requireHighlight=false,const POINT* currentOverride=nullptr) {
    if (!holdMode || holdFinished || pinned) return;
    int chosen=holdActionAt(point);
    const int frozenHit=chosen;
    if (requireHighlight && chosen!=renderedHit) chosen=-1;
    POINT current=point;int currentHit=-1;
    if (requireHighlight) {
        bool sampled=currentOverride!=nullptr;
        if (sampled) current=*currentOverride;
        else sampled=GetCursorPos(&current)!=0;
        if (sampled) currentHit=holdActionAt(current);
        // A late position may veto a frozen release, never select a new item.
        // Moving out while the release travels through IPC cancels immediately.
        if (!sampled || currentHit!=chosen) chosen=-1;
    }
    if (requireHighlight && !selfTest) std::cout<<Json({{"type","selection"},{"session",menuEpoch},
        {"cursor",{point.x,point.y}},{"frozen_hit",frozenHit},{"current_cursor",{current.x,current.y}},
        {"current_hit",currentHit},{"highlight",renderedHit},{"chosen",chosen}}).dump()<<std::endl;
    if(shapeEyedropActive){stopShapeEyedrop();chosen=-1;}
    if(chosen==71){pinned=true;pieReady=false;menuTools.open("split");return;}
    if(chosen==100){commitShapeDraft();holdFinished=true;pieReady=false;setOpacity(0.f);return;}
    holdFinished=true;pieReady=false;
    setOpacity(0.f);
    if(chosen>=40 && chosen<40+int(capsRows.size()))send(capsActions[chosen-40]);
    else if (chosen>=32 && chosen<39)send(quickActions[chosen-32]);
    else if (chosen>=recentActionBase) send((std::string("document.recent.")+shownPresets.at(chosen-recentActionBase).id).c_str());
    else if (chosen>=4) send((std::string("label.")+labelColors[chosen-4].id).c_str());
    else send(chosen<0?"hide":rowAction(chosen));
}
static bool focusRequestCurrent(LPARAM packet,LPARAM epoch,DWORD now) {
    const auto value=static_cast<uint64_t>(packet);const DWORD remaining=DWORD(value)-now;
    return packet>0 && (value>>32)==static_cast<uint64_t>(epoch) && remaining>0 && remaining<=500;
}
static constexpr DWORD gradientStyle=WS_POPUP|WS_THICKFRAME|WS_SYSMENU;
static constexpr DWORD gradientExStyle=WS_EX_TOOLWINDOW|WS_EX_NOREDIRECTIONBITMAP;
static SIZE gradientMinimum(UINT dpi){
    return {MulDiv(GradientPanel::minWidth,dpi,96),MulDiv(GradientPanel::minHeight,dpi,96)};
}
static RECT gradientWindowBounds(RECT work,UINT dpi) {
    const LONG inset=MulDiv(int(2*menuPanelGap),dpi,96);
    const auto minimum=gradientMinimum(dpi);
    const LONG width=std::max(minimum.cx,std::min<LONG>(MulDiv(604,dpi,96),work.right-work.left-2*inset));
    const LONG height=minimum.cy;
    const LONG x=work.left+std::max(0L,(work.right-work.left-width)/2),y=work.top+std::max(0L,(work.bottom-work.top-height)/2);
    return {x,y,x+width,y+height};
}
static LRESULT gradientFrameHit(POINT point,RECT client,LONG edge,RECT dragArea){
    if(!PtInRect(&client,point))return HTNOWHERE;
    const bool left=point.x<edge,right=point.x>=client.right-edge,top=point.y<edge,bottom=point.y>=client.bottom-edge;
    if(top)return left?HTTOPLEFT:right?HTTOPRIGHT:HTTOP;
    if(bottom)return left?HTBOTTOMLEFT:right?HTBOTTOMRIGHT:HTBOTTOM;
    if(left || right)return left?HTLEFT:HTRIGHT;
    return PtInRect(&dragArea,point)?HTCAPTION:HTCLIENT;
}
static LRESULT CALLBACK procedure(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    if(gradientWindow){
        if(msg==WM_TIMER && wp==gradientOwnerTimer){gradientOwner.update(hwnd);return 0;}
        if(msg==WM_CLOSE && gradientPanel.ready()){gradientPanel.finish(false);return 0;}
        if(msg==WM_APP+32){DestroyWindow(hwnd);return 0;}
        if(msg==WM_NCCALCSIZE || msg==WM_NCPAINT)return 0;
        if(msg==WM_NCACTIVATE)return DefWindowProcW(hwnd,msg,wp,-1);
        if(msg==WM_NCHITTEST){
            POINT point={short(LOWORD(lp)),short(HIWORD(lp))};
            RECT client{};
            if(ScreenToClient(hwnd,&point) && GetClientRect(hwnd,&client))
                return gradientFrameHit(point,client,std::lround(menuCompactGap*scale),gradientPanel.dragArea);
        }
        if(msg==WM_GETMINMAXINFO){
            auto* limits=reinterpret_cast<MINMAXINFO*>(lp);
            const auto minimum=gradientMinimum(UINT(std::lround(96*ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd))));
            limits->ptMinTrackSize={minimum.cx,minimum.cy};
            return 0;
        }
    }
    if(msg==WM_APP+31){
        if(IsWindowVisible(hwnd) && GetForegroundWindow()==hwnd && lp==menuEpoch)return AllowSetForegroundWindow(static_cast<DWORD>(wp));
        return FALSE;
    }
    // Own both halves even when cancellation hides the window before the up.
    // The global hook normally handles this, but client blank space must also
    // work when the hook is being renewed or the menu receives input directly.
    static bool rightCancelOwned=false;
    if(stabilityInspection && ((msg>=WM_MOUSEFIRST && msg<=WM_MOUSELAST) || (msg>=WM_KEYFIRST && msg<=WM_KEYLAST)))return 0;
    if(msg==WM_RBUTTONUP && rightCancelOwned){rightCancelOwned=false;return 0;}
    if(ImGui::GetCurrentContext() && shapeEyedropMessage(msg,wp,lp)){
        if(msg==WM_RBUTTONDOWN)rightCancelOwned=true;
        return 0;
    }
    if(msg==WM_RBUTTONDOWN && IsWindowVisible(hwnd) && (holdMode || taskBusy || busyFeedback)){
        rightCancelOwned=true;
        // Invalidate release hits immediately: a later Caps up cannot commit
        // while the asynchronous hide/cancel event travels to the backend.
        holdFinished=true;pieReady=false;renderedHit=pendingHit=-1;
        hoverTip.reset();tipVisual.reset();
        if(GetCapture()==hwnd)ReleaseCapture();
        if(ImGui::GetCurrentContext())ImGui::ClearActiveID();
        setOpacity(0.f);send("hide");return 0;
    }
    if(msg==WM_WINDOWPOSCHANGED && (reinterpret_cast<WINDOWPOS*>(lp)->flags&SWP_HIDEWINDOW))stopShapeEyedrop();
    if(msg==WM_APP+23){holdOrigin={static_cast<LONG>(wp),static_cast<LONG>(lp)};holdMoved=false;return 0;}
    if(msg==WM_APP+26){stabilityInspection=wp!=0;stabilityObserveBusy=wp==2;if(stabilityInspection && !stabilityObserveBusy){taskBusy=busyFeedback=taskHasResult=false;taskRecovery.clear();allowed=capable=true;}return 0;}
    if(msg==WM_APP+27){
        if(!stabilityInspection || static_cast<LPARAM>(wp)!=menuEpoch)return 0;
        return (layerMask&15)|((quickMask&127)<<4)|(canExecute()?1<<11:0)|
            (lastVisibleVertices>0?1<<12:0)|(compositionOpacity>.05f?1<<13:0)|(panelKind==2?1<<14:0)|(rowEnabled(1)?1<<15:0);
    }
    if(msg==WM_COPYDATA){
        auto* data=reinterpret_cast<COPYDATASTRUCT*>(lp);
        // 128 pattern names of 128 characters may exceed 64KiB after JSON
        // escaping. Keep a bound that covers the validated editor schema.
        if(!data || (data->dwData!=0x53485045 && data->dwData!=0x4d435458) || !data->lpData || data->cbData<2 || data->cbData>256*1024)return FALSE;
        try {auto* text=static_cast<const char*>(data->lpData);const auto value=Json::parse(text,text+data->cbData-1);
            if(data->dwData==0x4d435458){
                const bool resume=value.value("resume",false);
                const bool hoverReturn=value.value("hover_return",false);
                const bool busyWake=value.value("busy_wake",false);
                if(!IsWindowVisible(hwnd) || value.value("epoch",0)!=menuEpoch || taskBusy)return FALSE;
                if(busyWake && (hoverReturn || !busyMenuWake || !completionReady() || !returnInputsIdle() ||
                    value.value("next_epoch",LPARAM(0))!=menuEpoch+1))return FALSE;
                if(hoverReturn && (!(shapeReturning || hoverReturnConfirmed) || !completionReady() || inputFocused || panelKind!=2 ||
                    maskPreviewStarted || (GetAsyncKeyState(VK_CAPITAL)&0x8000) || (GetAsyncKeyState(VK_LBUTTON)&0x8000) ||
                    (GetAsyncKeyState(VK_RBUTTON)&0x8000) || value.value("next_epoch",LPARAM(0))!=menuEpoch+1))return FALSE;
                // Success feedback now closes; old restore packets cannot reopen it.
                if(resume || (!hoverReturn && !busyWake && (shapeEditor.dirty || holdFinished || busyFeedback)))return FALSE;
                const auto flags=value.value("flags",uint64_t(0));
                layerAware=true;layerMask=(flags>>8)&15;layerKind=(flags>>12)&15;
                layerBackground=(flags&(1u<<16))!=0;layerCount=(flags>>17)&255;
                layerLocked=(flags&(1u<<25))!=0;layerCrossParents=(flags&(1u<<26))!=0;quickMask=(flags>>32)&127;
                shapeEditor.prepare(value.at("shape"));hoverTip.reset();tipVisual.reset();renderedHit=pendingHit=-1;pieReady=false;
                if(hoverReturn || busyWake){menuEpoch=value.at("next_epoch").get<LPARAM>();shapeReturning=hoverReturnConfirmed=busyMenuWake=false;
                    busyFeedback=taskHasResult=taskFailed=false;completionStarted=0;notice.clear();taskError.clear();previewProgress.clear();
                    shapePanelOpen=false;shapeHoverReady=false;holdFinished=false;pinned=true;allowed=capable&&!taskBusy;}
            }else shapeEditor.prepare(value);
        }
        catch(...) {shapeEditor.prepare(Json{{"available",false}});}
        return TRUE;
    }
    if(msg==WM_APP+22){
        if(IsWindowVisible(hwnd) && !holdFinished){commitShapeDraft();holdFinished=true;pieReady=false;setOpacity(0.f);}
        return 0;
    }
    if (msg==WM_APP+21) {
        if (IsWindowVisible(hwnd)) finishHold({static_cast<LONG>(wp),static_cast<LONG>(lp)},true);
        return 0;
    }
    if (msg==WM_APP+11) return renderedFrames;
    // Read-only diagnostic used by the real SetWindowPos lifecycle regression.
    if (msg==WM_APP+18) return static_cast<LRESULT>(std::lround(compositionOpacity*1000));
    if (msg==WM_APP+19) return (busyFeedback?1:0)|(taskBusy?2:0);
    // Read-only success feedback expiry. The backend waits for preview/guard
    // release, then hides this window without querying Photoshop or replaying actions.
    if (msg==WM_APP+24) return IsWindowVisible(hwnd) && !gradientLaunching && completionReady() && (!busyMenuWake || returnInputsIdle());
    if (msg==WM_APP+28) return (shapeReturning || hoverReturnConfirmed)?2:busyMenuWake?1:0;
    if(msg==WM_APP+29)return (menuTools.active?1:0)|(menuTools.ready?2:0)|(menuTools.pending?4:0)|
        (menuTools.mode=="split"?8:0)|(!menuTools.revision.empty()?16:0)|(!menuTools.error.empty()?32:0)|(menuTools.split.count<<8);
    if (msg==WM_APP+25) {
        // The foreground menu process can hand activation back to its owner;
        // the backend process often cannot, especially after a native script.
        HWND target=reinterpret_cast<HWND>(wp),owner=GetWindow(hwnd,GW_OWNER),fg=GetForegroundWindow();
        if(lp && (!focusRequestCurrent(lp,menuEpoch,GetTickCount()) || !menuTools.active || !IsWindowVisible(hwnd)))return FALSE;
        DWORD pid=0;GetWindowThreadProcessId(fg,&pid);wchar_t name[64]{};GetClassNameW(target,name,64);
        if(target!=owner || !IsWindow(target) || !IsWindowEnabled(target) || IsIconic(target) ||
           wcscmp(name,L"Photoshop")!=0 || pid!=GetCurrentProcessId())return FALSE;
        SetForegroundWindow(target);
        return GetForegroundWindow()==target;
    }
    // One synchronous transition: no intermediate layout or repeated round trips.
    if (msg==WM_APP+17) {
        pieAnchorX=static_cast<int>((wp>>40)&0xffff)-1;shapePanelRight=(wp&(1ull<<56))!=0;
        panelKind=(wp&32)?4:static_cast<int>(wp&3);inputFocused=(wp&4)!=0;
        layerAware=(wp&64)!=0;layerMask=(wp>>8)&15;layerKind=(wp>>12)&15;
        layerBackground=(wp&(1u<<16))!=0;layerCount=(wp>>17)&255;
        layerLocked=(wp&(1u<<25))!=0;layerCrossParents=(wp&(1u<<26))!=0;
        quickMask=static_cast<unsigned>((wp>>32)&127);
        contextMode=(wp&8)!=0;menuEpoch=lp;setHoldMode((wp&16)!=0);
        return 0;
    }
    if (msg==WM_APP+13) {pieAnchorX=-1;shapePanelRight=false;menuEpoch=lp;setHoldMode(wp!=0);return 0;}
    if (msg==WM_APP+16) {pinned=wp!=0;return 0;}
    if (msg==WM_APP+14) {
        POINT point{};
        if (IsWindowVisible(hwnd) && GetCursorPos(&point)) finishHold(point);
        return 0;
    }
    if (msg==WM_APP+12) {panelKind=wp<=4?static_cast<int>(wp):0;inputFocused=lp!=0;resetMenu();return 0;}
    if (msg==WM_APP) {
        std::unique_ptr<Json> event(reinterpret_cast<Json*>(lp));
        try { accept(*event); } catch (const std::exception& error) { notice=error.what(); }
        return 0;
    }
    if (msg==WM_APP+10) { contextMode=wp!=0;resetMenu();return 0; }
    if (msg==WM_SHOWWINDOW) {
        opened=wp!=0;
        if(gradientWindow){
            setOpacity(opened?1.f:0.f);
            if(!opened){
                gradientPanel.gesture=GradientPanel::none;
                closePopups();
                if(GetCapture()==hwnd)ReleaseCapture();
            }
        }
        else if (opened) {
            resetMenu();pieStarted=0;setOpacity(holdMode?0.f:1.f);
        } else {
            stopShapeEyedrop();
            busyMenuWake=false;
            // Keep the composition surface attached while the HWND hides.
            // The next frame selects transparent pie or opaque regular mode.
            holdFinished=true;pieReady=false;holdRows={};labelRows={};quickRows={};closePopups();
            // Closing during a slider drag must not leave an invisible window
            // owning mouse capture. WM_CAPTURECHANGED clears the Win32 backend's
            // tracked buttons; no input is synthesized or sent to Photoshop.
            if(GetCapture()==hwnd)ReleaseCapture();
            if(ImGui::GetCurrentContext())ImGui::ClearActiveID();
            setOpacity(0.f);
        }
    }
    if (ImGui_ImplWin32_WndProcHandler(hwnd,msg,wp,lp)) return true;
    switch (msg) {
    case WM_SIZE:
        if (device && wp!=SIZE_MINIMIZED) {
            DXGI_SWAP_CHAIN_DESC desc{};
            if (SUCCEEDED(swapChain->GetDesc(&desc)) &&
                desc.BufferDesc.Width==LOWORD(lp) && desc.BufferDesc.Height==HIWORD(lp)) return 0;
            context->OMSetRenderTargets(0,nullptr,nullptr);
            releaseTarget();
            if (SUCCEEDED(swapChain->ResizeBuffers(0,LOWORD(lp),HIWORD(lp),DXGI_FORMAT_UNKNOWN,
                DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT))) createTarget();
        }
        return 0;
    case WM_DPICHANGED: {
        const auto* rect=reinterpret_cast<const RECT*>(lp);
        const auto minimum=gradientWindow?gradientMinimum(LOWORD(wp)):SIZE{};
        SetWindowPos(hwnd,nullptr,rect->left,rect->top,std::max(rect->right-rect->left,minimum.cx),
            std::max(rect->bottom-rect->top,minimum.cy),SWP_NOZORDER|SWP_NOACTIVATE); return 0;
    }
    case WM_SYSCOMMAND: if ((wp&0xfff0)==SC_KEYMENU) return 0;break;
    case WM_CLOSE: DestroyWindow(hwnd);return 0;
    case WM_DESTROY: running=false;PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
static void theme() {
    // Photoshop's four brightness presets, lightest to darkest. Widget tones
    // match the shipped UIColors.txt; label colors arrive through cached status.
    int i=uiTheme=="light"?0:uiTheme=="light-gray"?1:uiTheme=="dark-gray"?2:3;
    auto& style=ImGui::GetStyle();style=ImGuiStyle();
    if (i<2) ImGui::StyleColorsLight();else ImGui::StyleColorsDark();
    auto gray=[](int v){float f=v/255.f;return ImVec4(f,f,f,1);};
    const int background[]={240,184,83,50},fill[]={252,209,69,41};
    const int text[]={41,15,240,225},muted[]={84,56,214,173},disabled[]={190,150,112,90};
    const int border[]={204,148,102,71},divider[]={217,153,62,38};
    // Less saturated blue on light themes; subdued blue on dark themes.
    // Keep the theme's normal text readable on every hover surface.
    const ImVec4 hover[]={ImVec4(220/255.f,238/255.f,1,1),ImVec4(168/255.f,199/255.f,226/255.f,1),
        ImVec4(53/255.f,79/255.f,108/255.f,1),ImVec4(46/255.f,64/255.f,84/255.f,1)};
    const ImVec4 active[]={ImVec4(195/255.f,221/255.f,248/255.f,1),ImVec4(146/255.f,183/255.f,216/255.f,1),
        ImVec4(44/255.f,68/255.f,96/255.f,1),ImVec4(39/255.f,56/255.f,76/255.f,1)};
    auto& c=style.Colors;
    c[ImGuiCol_WindowBg]=c[ImGuiCol_PopupBg]=gray(background[i]);
    c[ImGuiCol_Text]=gray(text[i]);c[ImGuiCol_TextDisabled]=gray(disabled[i]);
    c[ImGuiCol_Border]=gray(border[i]);c[ImGuiCol_Separator]=gray(divider[i]);
    c[ImGuiCol_FrameBg]=c[ImGuiCol_Button]=gray(fill[i]);
    c[ImGuiCol_FrameBgHovered]=c[ImGuiCol_ButtonHovered]=c[ImGuiCol_HeaderHovered]=hover[i];
    c[ImGuiCol_FrameBgActive]=c[ImGuiCol_ButtonActive]=c[ImGuiCol_HeaderActive]=active[i];
    c[ImGuiCol_Header]=c[ImGuiCol_TextSelectedBg]=hover[i];
    // Reuse PlotLines for secondary labels; all hand-drawn UI uses this palette.
    c[ImGuiCol_PlotLines]=gray(muted[i]);
    c[ImGuiCol_CheckMark]=c[ImGuiCol_NavHighlight]=i<2?ImVec4(.08f,.45f,.90f,1):ImVec4(.30f,.65f,1,1);
    c[ImGuiCol_SliderGrab]=c[ImGuiCol_SliderGrabActive]=c[ImGuiCol_CheckMark];
    style.WindowPadding={figmaPadding,figmaPadding};style.FramePadding={figmaPadding,figmaPadding};style.ItemSpacing={figmaGap,figmaGap};
    style.WindowRounding=style.PopupRounding=style.ChildRounding=style.FrameRounding=
        style.TabRounding=style.GrabRounding=style.ScrollbarRounding=figmaCornerRadius;
    style.WindowBorderSize=1;
    style.ScaleAllSizes(scale);
}

static float basicLabelSize() {return menuFont(MenuTextRole::Control)->LegacySize;}
static ImVec2 basicLabelExtent(int index) {return menuFont(MenuTextRole::Control)->CalcTextSizeA(basicLabelSize(),FLT_MAX,0,rowLabel(index));}
static float basicTextOffset() {return (figmaBasicLeftPadding+figmaBasicIconSize+figmaBasicIconGap)*scale;}
static void icon(int index,ImVec2 at,bool active=false) {
    auto* draw=ImGui::GetWindowDrawList();const ImU32 ink=tint(rowEnabled(index)?ImGuiCol_Text:ImGuiCol_TextDisabled);
    const ImU32 surface=tint(active?ImGuiCol_HeaderHovered:ImGuiCol_Button);
    const float size=figmaBasicIconSize*scale;
    if(index==2 || index==3)drawFigmaSortIcon(draw,index,at,size,ink);
    else if(index==0 && panelKind==4)drawStyledMenuIcon(draw,8,at,size,ink,surface);
    else if(index==0 && layerAware && layerKind==3)drawStyledMenuIcon(draw,7,at,size,ink,surface);
    else if(index==0)drawFigmaMenuIcon(draw,0,at,size,ink,surface);
    else drawStyledMenuIcon(draw,1,at,size,ink,surface);
}
static void layoutLabels(ImVec2 pos) {
    for (int i=0;i<labelCount;++i) {
        float x=pos.x+(i%labelColumns())*36*scale,y=pos.y+(i/labelColumns())*36*scale;
        labelRows[i]={static_cast<LONG>(x),static_cast<LONG>(y),
            static_cast<LONG>(pos.x+(i%labelColumns()+1)*36*scale),static_cast<LONG>(pos.y+(i/labelColumns()+1)*36*scale)};
    }
}
static void commitLabel(int index) {
    if (index<0 || index>=labelCount || !rowEnabled(1) || (holdMode && holdFinished)) return;
    if (holdMode) {holdFinished=true;pieReady=false;}
    send((std::string("label.")+labelColors[index].id).c_str());
}
static void drawLabels() {
    auto* draw=ImGui::GetWindowDrawList();POINT cursor{};GetCursorPos(&cursor);
    ImVec2 saved=ImGui::GetCursorScreenPos();
    ImGui::PushItemFlag(ImGuiItemFlags_NoNav,true);
    ImGui::BeginDisabled(!rowEnabled(1) || (holdMode && (holdFinished || pieAnimating)));
    for (int i=0;i<labelCount;++i) {
        auto r=labelRows[i];ImVec2 a={static_cast<float>(r.left),static_cast<float>(r.top)},b={static_cast<float>(r.right),static_cast<float>(r.bottom)};
        ImGui::PushID(100+i);ImGui::SetCursorScreenPos(a);
        bool active=rowEnabled(1) && (holdMode?holdActionAt(cursor)==4+i:PtInRect(&r,cursor));
        if (ImGui::InvisibleButton("##label",{b.x-a.x,b.y-a.y})) commitLabel(i);
        ImU32 color=labelColors[i].color;
        if (!rowEnabled(1)) color=labelColors[i].disabled?labelColors[i].disabled:(color&0x00ffffff)|0x66000000;
        draw->AddRectFilled(a,b,color);
        if (active) draw->AddRect({a.x+scale,a.y+scale},{b.x-scale,b.y-scale},tint(ImGuiCol_CheckMark),0,0,2*scale);
        if (i==0) {
            float inset=(b.x-a.x)*.2f;
            draw->AddLine({a.x+inset,b.y-inset},{b.x-inset,a.y+inset},tint(rowEnabled(1)?ImGuiCol_Text:ImGuiCol_TextDisabled),1.5f*scale);
        }
        // Swatches are icon-only, including hover; no detached color tooltip.
        ImGui::PopID();
    }
    ImGui::EndDisabled();ImGui::PopItemFlag();ImGui::SetCursorScreenPos(saved);
}
static void colorGrid() {
    layoutLabels(ImGui::GetCursorScreenPos());drawLabels();ImGui::Dummy({float(labelColumns()*36)*scale,float(labelRowCount()*36)*scale});
}
static void contextItems() {
    ImGui::TextDisabled("%s",panelTitle());ImGui::Separator();
    if (panelKind==3) {
        ImGui::TextWrapped("正在编辑输入框，请退出输入后执行操作。");
        if (ImGui::MenuItem("后台管理…")) send("control");
        if (ImGui::MenuItem("关闭菜单")) send("hide");
        return;
    }
    if (panelKind==4) {
        if (ImGui::MenuItem("新建文档",nullptr,false,canExecute())) send("document.new");
        if (ImGui::MenuItem("关闭菜单")) send("hide");
        return;
    }
    if (panelKind==1) {
        colorGrid();ImGui::Separator();
        if (ImGui::BeginMenu("图层操作")) {
            for (int i=0;i<4;++i) if (ImGui::MenuItem(rowLabel(i),nullptr,false,rowEnabled(i))) send(rowAction(i));
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("后台管理…")) send("control");
        if (ImGui::MenuItem("关闭菜单")) send("hide");
        return;
    }
    if (ImGui::MenuItem(rowLabel(0),nullptr,false,rowEnabled(0))) send(actions[0]);
    colorGrid();
    if ((rowVisible(2) || rowVisible(3)) && ImGui::BeginMenu("图层排序")) {
        for (int i=2;i<4;++i) if (ImGui::MenuItem(rowLabel(i),nullptr,false,rowEnabled(i))) send(rowAction(i));
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("后台管理…")) send("control");
    if (ImGui::MenuItem("关闭菜单")) send("hide");
}
static void drawRecentPresets() {
    const size_t count=std::min(size_t(9),shownPresets.size());
    recentRows.assign(count,{});
    if(panelKind!=4)return;
    recentPanelBounds={};
    recentTips.assign(count,"");
    auto* draw=ImGui::GetWindowDrawList();
    ImFont* dimensionFont=menuFont(MenuTextRole::PresetDimension);
    ImFont* badgeFont=menuFont(MenuTextRole::PresetTag);
    ImFont* suffixFont=menuFont(MenuTextRole::PresetUnit);
    struct CardText {std::string dimensions,unit,resolution,mode;};
    auto content=[](const RecentPreset& item) {
        CardText value;
        const auto split=item.size.rfind(' ');
        value.dimensions=item.size.substr(0,split);
        value.dimensions.erase(std::remove(value.dimensions.begin(),value.dimensions.end(),' '),value.dimensions.end());
        value.unit=split==std::string::npos?"":item.size.substr(split+1);
        std::string detail=item.detail;
        const auto gray=detail.find("grayscale");
        if(gray!=std::string::npos)detail.replace(gray,9,"灰度");
        const auto separator=detail.find(" · ");
        value.resolution=detail.substr(0,separator);
        value.mode=separator==std::string::npos?"":detail.substr(separator+4);
        return value;
    };
    auto measured=[](ImFont* font,const std::string& text) {return font->CalcTextSizeA(font->LegacySize,FLT_MAX,0,text.c_str()).x;};
    const float padding=7*scale,textOffset=34*scale,columnGap=4*scale,panelPadding=8*scale;
    const float lineGap=3*scale,badgeHeight=badgeFont->LegacySize+2*scale;
    const float rowHeight=std::ceil(std::max(36*scale,10*scale+dimensionFont->LegacySize+lineGap+badgeHeight));
    auto naturalWidth=[&](const RecentPreset& item) {
        const auto value=content(item);
        const auto suffix=value.unit+(value.resolution.empty()?"":" @ "+value.resolution);
        const float first=measured(dimensionFont,value.dimensions)+3*scale+measured(suffixFont,suffix);
        const float second=measured(badgeFont,item.name)+6*scale+(value.mode.empty()?0:4*scale+measured(badgeFont,value.mode)+6*scale);
        return std::ceil(std::clamp(textOffset+std::max(first,second)+padding,154*scale,228*scale));
    };
    std::array<float,2> columnWidths={154*scale,154*scale};
    size_t gridCount=0;
    for(size_t i=0;i<count;++i)if(shownPresets[i].name!="剪贴板") {
        const auto column=gridCount++%2;
        columnWidths[column]=std::max(columnWidths[column],naturalWidth(shownPresets[i]));
    }
    const float width=2*panelPadding+columnWidths[0]+columnGap+columnWidths[1];
    const size_t rows=std::max(size_t(1),(gridCount+1)/2);
    const float height=2*panelPadding+rows*rowHeight+(rows-1)*columnGap;
    // No persistent heading: the first card stays level with the ring.
    const ImVec2 a={pieCenter.x+65*scale,pieCenter.y-panelPadding-rowHeight*.5f};
    const ImVec2 b={a.x+width,a.y+height};
    recentPanelBounds={LONG(a.x),LONG(a.y),LONG(b.x),LONG(b.y)};
    menuShadow::draw(draw,a,b,ImGui::GetStyle().FrameRounding,scale);
    draw->AddRectFilled(a,b,tint(ImGuiCol_WindowBg),ImGui::GetStyle().FrameRounding);
    draw->AddRect(a,b,tint(ImGuiCol_Border),ImGui::GetStyle().FrameRounding,0,scale);
    if(!gridCount) {
        ImGui::SetCursorScreenPos({a.x+panelPadding,a.y+panelPadding});
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+width-2*panelPadding);
        ImGui::TextWrapped("%s",recentMessage.c_str());ImGui::PopTextWrapPos();
    }
    POINT cursor{};GetCursorPos(&cursor);
    auto text=[&](ImFont* font,ImVec2 at,ImU32 color,std::string value,float maxWidth) {
        auto extent=[&](const std::string& s){return font->CalcTextSizeA(font->LegacySize,FLT_MAX,0,s.c_str()).x;};
        if(extent(value)>maxWidth) {
            do {
                size_t last=value.size()-1;
                while(last && (static_cast<unsigned char>(value[last])&0xc0)==0x80)--last;
                value.resize(last);
            } while(!value.empty() && extent(value+"…")>maxWidth);
            value+="…";
        }
        draw->AddText(font,font->LegacySize,{roundf(at.x),roundf(at.y)},color,value.c_str());
        return extent(value);
    };
    size_t gridIndex=0;
    for(size_t i=0;i<count;++i) {
        const auto& item=shownPresets[i];
        const bool clipboard=item.name=="剪贴板";
        // Keep the unavailable clipboard out of both drawing and hit testing.
        if(clipboard && !item.available)continue;
        const float rowWidth=clipboard?naturalWidth(item):columnWidths[gridIndex%2];
        ImVec2 at;
        if(clipboard)at={pieCenter.x-rowWidth*.5f,pieCenter.y-158*scale};
        else {
            at={a.x+panelPadding+(gridIndex%2?columnWidths[0]+columnGap:0),a.y+panelPadding+(rowHeight+columnGap)*(gridIndex/2)};
            ++gridIndex;
        }
        RECT r={LONG(at.x),LONG(at.y),LONG(at.x+rowWidth),LONG(at.y+rowHeight)};
        recentRows[i]=r;
        bool active=item.available && canExecute() && recentReady && !pieAnimating &&
            ((!preview.empty() && previewRecentHover>=0)?previewRecentHover==int(i):PtInRect(&r,cursor));
        ImVec4 fill=ImGui::GetStyle().Colors[active?ImGuiCol_HeaderHovered:ImGuiCol_Button];
        menuShadow::draw(draw,at,{at.x+rowWidth,at.y+rowHeight},ImGui::GetStyle().FrameRounding,scale);
        draw->AddRectFilled(at,{at.x+rowWidth,at.y+rowHeight},ImGui::ColorConvertFloat4ToU32(fill),ImGui::GetStyle().FrameRounding);
        draw->AddRect(at,{at.x+rowWidth,at.y+rowHeight},tint(ImGuiCol_Border),ImGui::GetStyle().FrameRounding,0,scale);
        ImGui::SetCursorScreenPos(at);
        ImGui::PushID(int(i)+400);
        bool clicked=ImGui::InvisibleButton("##preset",{rowWidth,rowHeight});
        if(clicked && pinned && active && !holdFinished) {
            holdFinished=true;send(("document.recent."+item.id).c_str());
        }
        const ImU32 color=tint(canExecute() && recentReady && (item.available || clipboard)?ImGuiCol_Text:ImGuiCol_TextDisabled);
        const auto value=content(item);
        const auto& dimensions=value.dimensions;const auto& unit=value.unit;
        const auto& resolution=value.resolution;const auto& mode=value.mode;
        recentTips[i]=clipboard?"按剪贴板尺寸新建文档":"最近预设："+item.name;
        // Draw a vector outline from the actual preset ratio, without loading images.
        float iconWidth=18*scale,iconHeight=18*scale;
        try {
            const auto cross=dimensions.find("×");
            const double w=std::stod(dimensions.substr(0,cross));
            const double h=std::stod(dimensions.substr(cross+2));
            if(w>0 && h>0 && std::isfinite(w) && std::isfinite(h)) {
                const double factor=std::min(18*scale/w,18*scale/h);
                iconWidth=std::max(float(w*factor),scale);
                iconHeight=std::max(float(h*factor),scale);
            }
        } catch(...) {}
        const ImVec2 iconAt={roundf(at.x+5*scale+(22*scale-iconWidth)*.5f),roundf(at.y+(rowHeight-iconHeight)*.5f)};
        drawStyledPresetIcon(draw,iconAt,iconWidth,iconHeight,color,ImGui::ColorConvertFloat4ToU32(fill),clipboard);
        const float textTop=at.y+(rowHeight-dimensionFont->LegacySize-lineGap-badgeHeight)*.5f;
        const ImVec2 dimensionAt={at.x+textOffset,textTop};
        if(item.available) {
            // Dimension numbers lead; suffix and individual metadata badges are smaller.
            const std::string suffix=unit+(resolution.empty()?"":" @ "+resolution);
            const float suffixWidth=suffixFont->CalcTextSizeA(suffixFont->LegacySize,FLT_MAX,0,suffix.c_str()).x;
            const float gap=3*scale;
            const float numberWidth=text(dimensionFont,dimensionAt,color,dimensions,
                std::max(0.f,rowWidth-textOffset-padding-suffixWidth-gap));
            text(suffixFont,{dimensionAt.x+numberWidth+gap,dimensionAt.y+dimensionFont->GetFontBaked(dimensionFont->LegacySize)->Ascent-suffixFont->GetFontBaked(suffixFont->LegacySize)->Ascent},
                color,suffix,suffixWidth);
        } else text(menuFont(MenuTextRole::Secondary),dimensionAt,color,"复制图像后可用",rowWidth-textOffset-padding);
        // Each metadata value gets a compact badge; unused row space keeps the card fill.
        const bool light=ImGui::GetStyle().Colors[ImGuiCol_WindowBg].x>.5f;
        const bool metric=unit=="mm";
        const ImVec4 badge=light?(metric?ImVec4(.68f,.82f,.61f,1):ImVec4(.66f,.75f,.90f,1)):
            (metric?ImVec4(.18f,.29f,.16f,1):ImVec4(.19f,.25f,.37f,1));
        const ImVec4 ink=light?ImVec4(.10f,.13f,.16f,1):ImVec4(.94f,.96f,.99f,1);
        const ImVec2 infoAt={roundf(at.x+textOffset),roundf(textTop+dimensionFont->LegacySize+lineGap)};
        ImFont* titleFont=badgeFont;
        const float padding=3*scale,gap=4*scale;
        const float modeWidth=mode.empty()?0:titleFont->CalcTextSizeA(titleFont->LegacySize,FLT_MAX,0,mode.c_str()).x+2*padding;
        const float availableWidth=rowWidth-textOffset-padding;
        // Reserve space for the mode badge before truncating a long preset name.
        const float nameLimit=std::max(0.f,availableWidth-modeWidth-(mode.empty()?0:gap)-2*padding);
        auto badgeAt=infoAt;
        auto drawBadge=[&](const std::string& value,float maxTextWidth) {
            const float measured=titleFont->CalcTextSizeA(titleFont->LegacySize,FLT_MAX,0,value.c_str()).x;
            const float width=std::min(measured,maxTextWidth)+2*padding;
            draw->AddRectFilled(badgeAt,{badgeAt.x+width,badgeAt.y+titleFont->LegacySize+2*scale},
                ImGui::ColorConvertFloat4ToU32(badge),ImGui::GetStyle().FrameRounding);
            text(titleFont,{badgeAt.x+padding,badgeAt.y+scale},ImGui::ColorConvertFloat4ToU32(ink),value,maxTextWidth);
            badgeAt.x+=width+gap;
        };
        drawBadge(item.name,nameLimit);
        if(!mode.empty())drawBadge(mode,std::max(0.f,availableWidth-(badgeAt.x-infoAt.x)-2*padding));
        ImGui::PopID();
    }
}
// Read-only footer: never registers a widget or expands a gesture hit region.
static ImVec4 footerBounds{};
// Secondary icons keep the confirmed compact right column beside the ring.
static float quickGroupY(bool lower) {return 45.f+(lower?figmaDisplayWidgetHeight+quickGroupGap:0.f);}
static const char* actionTipText(int action) {
    if(action==200+shapeModeTipBase)return shapePanelOpen?(shapeEditor.batchMask?"羽化／密度":"颜色／羽化"):shapeEditor.batchMask?"展开羽化／密度":"展开颜色／羽化";
    if(action==205)return shapeEditor.channel?"无描边":"无填充";
    if(action==222){const auto method=shapeEditor.current().value("method",std::string("classic"));return method=="perceptual"?"感知渐变；点击切换线性":method=="linear"?"线性渐变；点击切换经典":"经典渐变；点击切换感知";}
    if(action>=200 && action<200+shapeTipCount)return action==207 && shapeEditor.copiedAt && GetTickCount64()-shapeEditor.copiedAt<900?"已复制Hex颜色":shapeTipNames[action-200];
    if(action>=recentActionBase && action<recentActionBase+int(recentTips.size()) && panelKind==4)return recentTips[action-recentActionBase].c_str();
    if(action>=40 && action<40+int(capsRows.size()))return capsLabels[action-40];
    if(action>=32 && action<39)return quickLabels[action-32];
    if(action>=4 && action<4+labelCount)return labelColors[action-4].name;
    if(action==0)return panelKind==4?"打开新建文档窗口":layerAware && layerKind!=3?"选择同一父组中的相邻图层":"选择当前组内的图层";
    if(action==2)return "将所选图层移到顶部";
    if(action==3)return "将所选图层移到底部";
    return nullptr;
}
static bool tipOverlaps(ImVec4 tip,RECT r,float gap=0) {
    return r.right>r.left && r.bottom>r.top && tip.x<r.right+gap && tip.z>r.left-gap && tip.y<r.bottom+gap && tip.w>r.top-gap;
}
static int tipFamily(int action) {
    if(action==49 || action==50)return 49;
    if(action>=200+shapeModeTipBase)return 200+shapeModeTipBase;
    if(action>=200)return action==213 || action==214?213:200;
    if(panelKind==2 && action>=45 && action<47)return 2;
    if(panelKind==2 && (action==2 || action==3))return 2;
    if(action>=40 && action<49)return 40;
    if(action>=32)return 32;
    if(action>=recentActionBase && action<recentActionBase+int(shownPresets.size()))return shownPresets[action-recentActionBase].name=="剪贴板"?80+action:12;
    return action;
}
static RECT tipGroupRect(int action) {
    if(action==49 || action==50)return {capsRows[10].left,capsRows[10].top,capsRows[10].right,layerCount>1?capsRows[9].bottom:capsRows[10].bottom};
    if(action>=200+shapeModeTipBase)return shapeModeBounds;
    if(action>=200)return action==213 || action==214?shapeEditor.maskBounds:shapeEditor.bounds;
    if(panelKind==2 && capsReady && layerAware && tipFamily(action)==2)return {holdRows[2].left,holdRows[2].top,capsRows[6].right?capsRows[6].right:holdRows[3].right,holdRows[3].bottom};
    if(action>=47 && action<49)return {capsRows[7].left,capsRows[7].top,capsRows[8].right,capsRows[8].bottom};
    if(action>=40 && action<49){
        const int first=panelKind==2 && action>=45?5:0;
        const int last=first==5?6:capsBottomCount()-1;
        return {capsRows[first].left,capsRows[first].top,capsRows[last].right,capsRows[last].bottom};
    }
    if(action>=32)return {quickRows[0].left,quickRows[0].top,quickRows[6].right,quickRows[6].bottom};
    if(tipFamily(action)==12)return recentPanelBounds;
    if(action>=recentActionBase)return recentRows[action-recentActionBase];
    if(action>=4)return holdRows[1];
    return holdRows[action];
}
static bool tipGapAt(int action,POINT cursor) {
    if(action<0 || (tipFamily(action)!=12 && tipFamily(action)!=32 && tipFamily(action)!=40 && tipFamily(action)!=45 && tipFamily(action)!=49 && tipFamily(action)!=2))return false;
    const RECT group=tipGroupRect(action);
    if(!PtInRect(&group,cursor))return false;
    // Retain through spacing only, never through disabled buttons or centre.
    for(int i=0;i<int(capsRows.size());++i)if(PtInRect(&capsRows[i],cursor))return capsEnabled(i) && capsDividerAt(i,cursor);
    for(int i=0;i<7;++i)if(PtInRect(&quickRows[i],cursor))return quickEnabled(i) && quickDividerAt(i,cursor);
    for(int i:{2,3})if(PtInRect(&holdRows[i],cursor))return rowEnabled(i) && std::abs(cursor.x-pieCenter.x)<scale;
    for(auto r:recentRows)if(PtInRect(&r,cursor))return false;
    return true;
}
// Read-only tooltips never create an ImGui widget or a gesture hit region.
static ImVec4 actionTipBounds(int action) {
    const char* text=actionTipText(action);if(!text)return {};
    auto* font=menuFont(MenuTextRole::Secondary);
    const float padding=6*scale,gap=6*scale;
    auto extent=font->CalcTextSizeA(font->LegacySize,FLT_MAX,0,text);
    const float width=extent.x+2*padding,height=extent.y+2*padding;
    const auto pos=ImGui::GetMainViewport()->Pos,size=ImGui::GetMainViewport()->Size;
    if(action>=200 && action<200+shapeTipCount){
        const RECT group=tipGroupRect(action),anchor=shapeTipRows[action-200];
        const auto heading=menuFont(MenuTextRole::Body)->CalcTextSizeA(menuFont(MenuTextRole::Body)->LegacySize,FLT_MAX,0,panelTitle());
        const RECT centre={LONG(pieCenter.x-std::max(19*scale,heading.x*.5f)),LONG(pieCenter.y-40*scale),
                           LONG(pieCenter.x+std::max(19*scale,heading.x*.5f)),LONG(pieCenter.y+19*scale)};
        const float x=std::clamp((anchor.left+anchor.right-width)*.5f,pos.x+8*scale,std::max(pos.x+8*scale,pos.x+size.x-width-8*scale));
        const ImVec4 above={x,group.top-gap-height,x+width,group.top-gap};
        auto valid=[&](ImVec4 tip){
            if(tip.x<pos.x+8*scale || tip.y<pos.y+8*scale || tip.z>pos.x+size.x-8*scale || tip.w>pos.y+size.y-8*scale)return false;
            if(tipOverlaps(tip,shapeEditor.bounds,2*scale) || tipOverlaps(tip,shapeEditor.maskBounds,2*scale) || tipOverlaps(tip,shapeModeBounds,2*scale))return false;
            if(tipOverlaps(tip,centre,2*scale))return false;
            for(const auto& row:holdRows)if(tipOverlaps(tip,row,2*scale))return false;
            for(const auto& row:quickRows)if(tipOverlaps(tip,row,2*scale))return false;
            for(const auto& row:capsRows)if(tipOverlaps(tip,row,2*scale))return false;
            if(tipOverlaps(tip,{LONG(footerBounds.x),LONG(footerBounds.y),LONG(footerBounds.z),LONG(footerBounds.w)},2*scale))return false;
            return true;
        };
        const bool pickerSideTip=action==223 || action>=225 && action<200+shapeModeTipBase;
        if(pickerSideTip){
            const float nearX=group.right+gap;
            const float nearY=std::clamp((anchor.top+anchor.bottom-height)*.5f,pos.y+8*scale,std::max(pos.y+8*scale,pos.y+size.y-height-8*scale));
            const ImVec4 nearby={nearX,nearY,nearX+width,nearY+height};
            if(valid(nearby))return nearby;
        }
        const float left=group.left-gap-width;
        const float y=std::clamp(float(anchor.top),pos.y+8*scale,std::max(pos.y+8*scale,pos.y+size.y-height-8*scale));
        if(!pickerSideTip){
            if(valid(above))return above;
            for(const auto& candidate:std::array<ImVec4,3>{{{left,y,left+width,y+height},{group.right+gap,y,group.right+gap+width,y+height},{x,group.bottom+gap,x+width,group.bottom+gap+height}}})if(valid(candidate))return candidate;
        }
        // A tall panel can touch the viewport edge. Search free side corridors
        // instead of silently suppressing the tip when all four anchors fail.
        std::vector<float> rows={float(group.top),float(anchor.top),pos.y+8*scale,pos.y+size.y-height-8*scale};
        auto obstacle=[&](RECT r){if(r.right>r.left && r.bottom>r.top){rows.push_back(r.top-gap-height);rows.push_back(r.bottom+gap);}};
        obstacle(centre);obstacle(shapeEditor.bounds);obstacle(shapeEditor.maskBounds);obstacle(shapeModeBounds);
        obstacle({LONG(footerBounds.x),LONG(footerBounds.y),LONG(footerBounds.z),LONG(footerBounds.w)});
        for(const auto& r:holdRows)obstacle(r);
        for(const auto& r:quickRows)obstacle(r);
        for(const auto& r:capsRows)obstacle(r);
        ImVec4 best{};float score=FLT_MAX;
        for(float px:{float(group.right)+gap,left,pos.x+8*scale,pos.x+size.x-width-8*scale})for(float py:rows){
            const ImVec4 candidate={roundf(px),roundf(py),roundf(px+width),roundf(py+height)};
            if(!valid(candidate))continue;
            const float dx=(candidate.x+candidate.z-anchor.left-anchor.right)*.5f,dy=(candidate.y+candidate.w-anchor.top-anchor.bottom)*.5f;
            const float distance=dx*dx+dy*dy;
            if(distance<score){score=distance;best=candidate;}
        }
        return best;
    }
    RECT anchor=action>=40?capsRows[action-40]:action>=32?quickRows[action-32]:action>=recentActionBase?recentRows[action-recentActionBase]:action>=4?labelRows[action-4]:holdRows[action];
    RECT group=anchor;
    if(action>=40)group=tipGroupRect(action);
    else if(action>=32)group={quickRows[0].left,quickRows[0].top,quickRows[6].right,quickRows[6].bottom};
    if(action>=4 && action<4+labelCount)group=holdRows[1];
    if(action>=recentActionBase && action<32 && shownPresets[action-recentActionBase].name!="剪贴板")group=recentPanelBounds;
    if(tipFamily(action)==2){group=tipGroupRect(action);anchor=group;}
    if(tipFamily(action)==12 || tipFamily(action)==32)anchor=group;
    float basicRight=pieCenter.x+19*scale;
    for(int i:{0,2,3})if(rowVisible(i))basicRight=std::max(basicRight,float(holdRows[i].right));
    const auto heading=menuFont(MenuTextRole::Body)->CalcTextSizeA(menuFont(MenuTextRole::Body)->LegacySize,FLT_MAX,0,panelTitle());
    const RECT centre={LONG(pieCenter.x-std::max(19*scale,heading.x*.5f)),LONG(pieCenter.y-40*scale),LONG(pieCenter.x+std::max(19*scale,heading.x*.5f)),LONG(pieCenter.y+19*scale)};
    auto valid=[&](ImVec4 tip) {
        for(int i=0;i<4;++i)if(rowVisible(i) && tipOverlaps(tip,holdRows[i],2*scale))return false;
        for(auto r:capsRows)if(tipOverlaps(tip,r,2*scale))return false;
        for(auto r:quickRows)if(tipOverlaps(tip,r,2*scale))return false;
        if(tipOverlaps(tip,toolRow,2*scale))return false;
        for(auto r:recentRows)if(tipOverlaps(tip,r,2*scale))return false;
        if(tipOverlaps(tip,shapeModeBounds,2*scale))return false;
        if(panelKind==4 && tipOverlaps(tip,recentPanelBounds,2*scale))return false;
        if(tipOverlaps(tip,centre,2*scale))return false;
        if(footerBounds.z>footerBounds.x && tipOverlaps(tip,{LONG(footerBounds.x),LONG(footerBounds.y),LONG(footerBounds.z),LONG(footerBounds.w)},2*scale))return false;
        return true;
    };
    const float middle=(anchor.left+anchor.right)*.5f;
    const std::array<ImVec2,9> candidates={{{middle-width*.5f,anchor.top-gap-height},
        {middle<pieCenter.x?std::min(middle-width*.5f,centre.left-gap-width):std::max(middle-width*.5f,centre.right+gap),anchor.top-gap-height},
        {middle-width*.5f,group.top-gap-height},
        {middle<pieCenter.x?std::min(middle-width*.5f,centre.left-gap-width):std::max(float(anchor.left),basicRight+gap),group.top-gap-height},
        {group.right+gap,(anchor.top+anchor.bottom-height)*.5f},
        {group.left-gap-width,(anchor.top+anchor.bottom-height)*.5f},
        {middle-width*.5f,anchor.bottom+gap},
        {middle-width*.5f,group.bottom+gap},
        {std::max(float(anchor.left),basicRight+gap),group.bottom+gap}}};
    for(auto at:candidates) {
        float x=std::clamp(at.x,pos.x+8*scale,pos.x+size.x-width-8*scale);
        float y=std::clamp(at.y,pos.y+8*scale,pos.y+size.y-height-8*scale);
        ImVec4 tip={roundf(x),roundf(y),roundf(x+width),roundf(y+height)};
        if(valid(tip))return tip;
    }
    return {}; // Never cover a control when an unusually small viewport has no room.
}
static ImVec4 quickTipBounds(int index) {return actionTipBounds(32+index);}
static void drawActionTip(int action) {
    const auto target=actionTipBounds(action);if(target.z<=target.x) {tipVisual.reset();return;}
    auto* draw=ImGui::GetWindowDrawList();
    auto* font=menuFont(MenuTextRole::Secondary);
    if(!preview.empty())tipVisual.reset(); // Static preview captures final geometry.
    tipVisual.update(actionTipText(action),target,tipFamily(action),GetTickCount64());
    const auto r=tipVisual.bounds;
    draw->AddRectFilled({r.x,r.y},{r.z,r.w},menuTipBackground,ImGui::GetStyle().FrameRounding);
    draw->AddRect({r.x,r.y},{r.z,r.w},menuTipBorder,ImGui::GetStyle().FrameRounding,0,scale);
    // Equal padding at rest; clip only the visual transition, never hit tests.
    const float padding=6*scale;
    draw->PushClipRect({r.x+padding,r.y+padding},{r.z-padding,r.w-padding},true);
    auto label=[&](const std::string& value,float alpha) {
        if(value.empty() || alpha<=0)return;
        const auto extent=font->CalcTextSizeA(font->LegacySize,FLT_MAX,0,value.c_str());
        draw->AddText(font,font->LegacySize,{roundf((r.x+r.z-extent.x)*.5f),roundf((r.y+r.w-extent.y)*.5f)},
            IM_COL32(245,245,245,int(255*alpha)),value.c_str());
    };
    label(tipVisual.previous,1-tipVisual.blend);
    label(tipVisual.text,tipVisual.blend);
    draw->PopClipRect();
}
static void drawFooter(const std::vector<std::string>& messages,float y,bool avoidHomeGrid=false) {
    auto* draw=ImGui::GetWindowDrawList();
    ImFont* font=menuFont(MenuTextRole::Secondary);
    const auto pos=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();
    const float padding=figmaPadding*scale,gap=3*scale;
    const float right=avoidHomeGrid?pieCenter.x+55*scale:pos.x+size.x-8*scale;
    const float maxWidth=std::min((avoidHomeGrid?300.f:540.f)*scale,right-pos.x-16*scale)-2*padding;
    std::vector<std::string> lines;
    float width=0,height=0;
    for(size_t index=0;index<messages.size();++index) {
        const auto& message=messages[index];
        if(message.empty())continue;
        // Metadata stays on one line; task/result may use two. This reserves
        // the space below the bottom action even for long backend diagnostics.
        std::string text=message;
        auto measure=[&](const std::string& value){return font->CalcTextSizeA(font->LegacySize,FLT_MAX,maxWidth,value.c_str());};
        const float maxLines=index+1==messages.size()?2.f:1.f;
        bool trimmed=false;
        while(!text.empty() && measure(text+(trimmed?"…":"")).y>maxLines*font->LegacySize+.5f) {
            size_t last=text.size()-1;
            while(last && (static_cast<unsigned char>(text[last])&0xc0)==0x80)--last;
            text.resize(last);trimmed=true;
        }
        if(trimmed)text+="…";
        auto extent=measure(text);width=std::max(width,extent.x);
        height+=extent.y+(lines.empty()?0:gap);lines.push_back(text);
    }
    if(lines.empty()){footerBounds={};return;}
    // The centre owns the one running animation; progress remains truthful text.
    width+=2*padding;height+=2*padding;
    float x=std::min(pieCenter.x-width*.5f,right-width);
    x=std::max(pos.x+8*scale,x);
    y=std::min(y,pos.y+size.y-height-8*scale);
    const ImVec2 a={roundf(x),roundf(y)},b={roundf(x+width),roundf(y+height)};
    footerBounds={a.x,a.y,b.x,b.y};
    auto bg=ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);bg.w=.92f;
    menuShadow::draw(draw,a,b,ImGui::GetStyle().FrameRounding,scale);
    draw->AddRectFilled(a,b,ImGui::ColorConvertFloat4ToU32(bg),ImGui::GetStyle().FrameRounding);
    auto border=ImGui::GetStyleColorVec4(ImGuiCol_Border);border.w=.65f;
    draw->AddRect(a,b,ImGui::ColorConvertFloat4ToU32(border),ImGui::GetStyle().FrameRounding,0,scale);
    float textY=a.y+padding;
    for(const auto& line:lines) {
        draw->AddText(font,font->LegacySize,{a.x+padding,roundf(textY)},tint(ImGuiCol_Text),line.c_str(),nullptr,maxWidth);
        textY+=font->CalcTextSizeA(font->LegacySize,FLT_MAX,maxWidth,line.c_str()).y+gap;
    }

}
static void quickIcon(int index,ImVec2 at,bool enabled,ImU32 surface) {
    auto* d=ImGui::GetWindowDrawList();const auto color=tint(enabled?ImGuiCol_Text:ImGuiCol_TextDisabled);
    if(index==4) {
        drawFigmaMenuIcon(d,1,at,figmaDisplayIconSize*scale,color,surface);
    } else {
        drawStyledMenuIcon(d,index,at,figmaDisplayIconSize*scale,color,surface);
    }
}
static void drawQuickGroups(int selected,float fac) {
    if(panelKind==4 || panelKind==3 || !layerAware)return;
    auto* d=ImGui::GetWindowDrawList();
    for(int group=0;group<2;++group) {
        const int first=group?3:0,count=group?menuQuickGroupMaxCount:3;
        const float cell=figmaDisplayIconWidth*scale,height=figmaDisplayWidgetHeight*scale;
        const ImVec2 a={pieCenter.x+menuQuickGroupOffset*scale,pieCenter.y+quickGroupY(group)*scale};
        const ImVec2 b={a.x+count*cell,a.y+height};
        const int begin=d->VtxBuffer.Size;
        menuShadow::draw(d,a,b,ImGui::GetStyle().FrameRounding,scale);
        d->AddRectFilled(a,b,tint(ImGuiCol_Button),ImGui::GetStyle().FrameRounding);
        for(int n=0;n<count;++n) {
            const int i=first+n;const ImVec2 cellA={a.x+n*cell,a.y},cellB={cellA.x+cell,b.y};
            quickRows[i]={LONG(cellA.x),LONG(cellA.y),LONG(cellB.x),LONG(cellB.y)};
            if(selected==32+i)d->AddRectFilled(cellA,cellB,tint(ImGuiCol_HeaderHovered),ImGui::GetStyle().FrameRounding,n==0?ImDrawFlags_RoundCornersLeft:n==count-1?ImDrawFlags_RoundCornersRight:ImDrawFlags_RoundCornersNone);
            if(n)d->AddLine(cellA,{cellA.x,b.y},tint(ImGuiCol_Border),scale);
            quickIcon(i,{cellA.x+(cell-figmaDisplayIconSize*scale)*.5f,cellA.y+(height-figmaDisplayIconSize*scale)*.5f},quickEnabled(i),tint(selected==32+i?ImGuiCol_HeaderHovered:ImGuiCol_Button));
            if(pinned && !pieAnimating && quickEnabled(i) && !holdFinished) {
                ImGui::SetCursorScreenPos(cellA);ImGui::PushID(400+i);
                if(ImGui::InvisibleButton("##quick-layer",{cell,height})){holdFinished=true;send(quickActions[i]);}
                ImGui::PopID();
            }
        }
        d->AddRect(a,b,tint(ImGuiCol_Border),ImGui::GetStyle().FrameRounding,0,scale);
        const ImVec2 offset={((a.x+b.x)*.5f-pieCenter.x)*(fac-1),((a.y+b.y)*.5f-pieCenter.y)*(fac-1)};
        for(int v=begin;v<d->VtxBuffer.Size;++v){d->VtxBuffer[v].pos.x+=roundf(offset.x);d->VtxBuffer[v].pos.y+=roundf(offset.y);}
    }
}
static void drawCapsBar() {
    capsRows={};
    if(!capsReady || panelKind==4 || !layerAware)return;
    const ImVec2 savedCursor=ImGui::GetCursorScreenPos();
    auto* d=ImGui::GetWindowDrawList();
    const float h=figmaWidgetHeight*scale;
    POINT cursor{};GetCursorPos(&cursor);
    auto drawGroup=[&](int first,int last,float x,float y,bool smart=false) {
        const bool compact=first==0;
        auto* font=menuFont(compact?MenuTextRole::Shortcut:MenuTextRole::Control);
        const float h=(compact?menuShortcutHeight:figmaWidgetHeight)*scale;
        const float minimum=(compact?menuShortcutMinWidth:figmaWidgetHeight)*scale,inset=(compact?2*menuShortcutTextInset:2*figmaBasicLeftPadding)*scale;
        std::array<float,11> widths{};float total=0;
        for(int j=first;j<=last;++j){const int i=smart?19-j:j;const char* label=capsKeys[i];widths[i]=std::max(minimum,font->CalcTextSizeA(font->LegacySize,FLT_MAX,0,label).x+inset);total=smart?std::max(total,widths[i]):total+widths[i];}
        if(first==0 || first==7)x=pieCenter.x-total*.5f;
        if(smart)x-=total;
        const float left=x,height=h*(smart?last-first+1:1);
        MenuControls::surface(d,{x,y},{x+total,y+height},scale);
        for(int j=first;j<=last;++j){
            const int i=smart?19-j:j;const char* label=capsKeys[i];
            const float width=smart?total:widths[i];
            const ImVec2 a={x,y+(smart?(j-first)*h:0)},b={x+width,a.y+h};capsRows[i]={LONG(a.x),LONG(a.y),LONG(b.x),LONG(b.y)};
            const bool enabled=capsEnabled(i),seam=capsDividerAt(i,cursor);
            const bool hover=enabled && ((!preview.empty() && previewCapsHover>=0)?previewCapsHover==i:!seam && PtInRect(&capsRows[i],cursor));
            MenuControls::segment(d,a,b,j==first,j==last,hover,scale,smart);
            const auto extent=font->CalcTextSizeA(font->LegacySize,FLT_MAX,0,label);
            d->AddText(font,font->LegacySize,{a.x+(width-extent.x)*.5f,a.y+(h-extent.y)*.5f},tint(enabled?ImGuiCol_Text:ImGuiCol_TextDisabled),label);
            if(pinned && !pieAnimating && enabled && !holdFinished){
                const float leading=j>first?scale:0,trailing=j<last?scale:0;
                ImGui::SetCursorScreenPos({a.x+(smart?0:leading),a.y+(smart?leading:0)});ImGui::PushID(450+i);
                if(ImGui::InvisibleButton("##caps",{width-(smart?0:leading+trailing),h-(smart?leading+trailing:0)})){holdFinished=true;send(capsActions[i]);}
                ImGui::PopID();
            }
            if(!smart)x=b.x;
        }
        MenuControls::outline(d,{left,y},{left+total,y+height},scale);
    };
    drawGroup(0,capsBottomCount()-1,0,pieCenter.y+(107+(rowVisible(1)?(labelRowCount()-2)*21.f:0))*scale);
    if(individualSmartReady && panelKind!=3 && layerCount && layerKind)
        drawGroup(9,layerCount>1?10:9,float(holdRows[1].left)-quickGroupGap*scale,float(holdRows[1].top),true);
    if(panelKind==2 && rowVisible(3)){
        float x=float(holdRows[3].right);
        for(int i:{5,6}){
            const float width=menuFont(MenuTextRole::Control)->CalcTextSizeA(basicLabelSize(),FLT_MAX,0,capsKeys[i]).x+basicTextOffset()+figmaBasicRightPadding*scale;
            capsRows[i]={LONG(std::lround(x)),holdRows[2].top,LONG(std::lround(x+width)),holdRows[2].bottom};x+=width;
        }
        const LONG shift=LONG(std::lround((holdRows[2].left+capsRows[6].right)*.5f-pieCenter.x));
        for(auto* row:{&holdRows[2],&holdRows[3],&capsRows[5],&capsRows[6]}){row->left-=shift;row->right-=shift;}
    }
    if(panelKind==2 && documentSortReady)drawGroup(7,8,0,holdRows[2].top-h-menuControlPadding*scale);
    ImGui::SetCursorScreenPos(savedCursor);
}
static void drawToolBar() {
    toolRow={};
    if(panelKind==3 || panelKind==4 || !layerAware || (layerKind!=4 && layerKind!=5) || layerCount!=1)return;
    const char* label=layerKind==5?"文字源":"文本工具";
    const float h=menuControlHeight*scale,y=pieCenter.y+(layerKind==5?15.f:-15.f)*scale;
    const auto extent=menuFont(MenuTextRole::Control)->CalcTextSizeA(basicLabelSize(),FLT_MAX,0,label);
    const float width=extent.x+2*figmaBasicLeftPadding*scale,x=pieCenter.x+65*scale;
    auto* draw=ImGui::GetWindowDrawList();POINT cursor{};GetCursorPos(&cursor);
    const ImVec2 a={x,y};
    toolRow={LONG(x),LONG(y),LONG(x+width),LONG(y+h)};
    const bool enabled=toolEnabled(1),hover=enabled && !pieAnimating && PtInRect(&toolRow,cursor);
    MenuControls::surface(draw,a,{x+width,y+h},scale);
    MenuControls::segment(draw,a,{x+width,y+h},true,true,hover,scale);
    draw->AddText(menuFont(MenuTextRole::Control),basicLabelSize(),{x+(width-extent.x)*.5f,y+(h-extent.y)*.5f},tint(enabled?ImGuiCol_Text:ImGuiCol_TextDisabled),label);
    if(pinned && !pieAnimating && enabled && !holdFinished){
        ImGui::SetCursorScreenPos(a);
        if(ImGui::InvisibleButton("##split-tool",{width,h}))menuTools.open("split");
    }
    MenuControls::outline(draw,a,{x+width,y+h},scale);
}
static void shapeEditorKeys() {
    // The hook leaves letters with the editor so focused RGB/Hex fields keep
    // normal typing. Only an unfocused editor frame handles these shortcuts.
    if(!menuToolsVisible() && shapeEditor.ready() && !shapeEyedropActive && !ImGui::IsAnyItemActive() && !ImGui::GetIO().WantTextInput && !holdFinished && !pieAnimating){
        const ImGuiKey keys[]={ImGuiKey_C,ImGuiKey_V,ImGuiKey_F,ImGuiKey_W,ImGuiKey_S};
        if(!ImGui::GetIO().KeyAlt && !ImGui::GetIO().KeySuper)
            for(int i=0;i<5;++i)if(ImGui::IsKeyPressed(keys[i],false)){
                const int action=ImGui::GetIO().KeyCtrl || i==2 && !(GetAsyncKeyState(VK_CAPITAL)&0x8000)?-1:i;
                if(action>=0 && capsEnabled(action) && (!ImGui::GetIO().KeyShift || i>=3)){
                    holdFinished=true;send(ImGui::GetIO().KeyShift?(i==3?"extendAbove":"extendBelow"):capsActions[action]);break;
                }
            }
    }
}
static void drawPie() {
    auto* draw=ImGui::GetWindowDrawList();
    ImVec2 pos=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();
    pieCenter={pos.x+size.x*.5f,pos.y+size.y*.5f};
    if((panelKind==2 || menuToolsVisible()) && size.x>700*scale)pieCenter.x=pos.x+std::max(size.x*.5f,std::min(650*scale,size.x-270*scale));
    if(panelKind==4)pieCenter.x=pos.x+270*scale;
    if(pieAnchorX>=0)pieCenter.x=pos.x+pieAnchorX;
    if (!pieStarted) pieStarted=GetTickCount64();
    float elapsed=previewAnimationMs>=0?previewAnimationMs:selfTest?60.f:float(GetTickCount64()-pieStarted);
    const float fac=pieAnimation(elapsed);
    pieAnimating=fac<1;
    // A sent hide/release may arrive during rendering. A late draw must not
    // restore opacity after that transition while the backend closes HWND.
    // SetWindowPos(SWP_SHOWWINDOW), our production entry, need not deliver
    // WM_SHOWWINDOW. Query the HWND instead of relying on that message's cache.
    setOpacity(IsWindowVisible(window) && !holdFinished?fac:0.f);
    if(menuToolsVisible()) {
        holdRows={};labelRows={};capsRows={};quickRows={};toolRow={};pieReady=false;
        menuTools.draw(scale,pieCenter,allowed && !taskBusy && !busyFeedback && !inputFocused && !maskPreviewStarted);
        drawFooter({document,menuTools.status(),menuTools.error},pos.y+size.y-60*scale);
        return;
    }
    if(toolEnabled(0) && !shapePanelVisible() && !shapeEyedropActive && !ImGui::IsAnyItemActive() && !ImGui::GetIO().WantTextInput &&
        ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyAlt && !ImGui::GetIO().KeyShift && !ImGui::GetIO().KeySuper &&
        !(GetAsyncKeyState(VK_CAPITAL)&0x8000) && ImGui::IsKeyPressed(ImGuiKey_F,false))menuTools.open("search");
    // Blender 5.2 defaults: 20-unit widgets, pie rows 1.5 widgets high,
    // 100-unit radius to the inner edge, roundness .4 of a widget.
    const float rowHeight=figmaWidgetHeight*scale,padding=figmaPadding*scale,rounding=ImGui::GetStyle().FrameRounding;
    const ImGuiKey numberKeys[]={ImGuiKey_4,ImGuiKey_None,ImGuiKey_8,ImGuiKey_2};
    const ImGuiKey keypadKeys[]={ImGuiKey_Keypad4,ImGuiKey_None,ImGuiKey_Keypad8,ImGuiKey_Keypad2};
    // Direction accelerators are local to an open pie, never global PS keys.
    if (canExecute() && !shapePanelVisible() && !holdFinished && !ImGui::GetIO().KeyCtrl &&
        !ImGui::GetIO().KeyAlt && !ImGui::GetIO().KeyShift && !ImGui::GetIO().KeySuper) {
        for (int i:{0,2,3}) if (rowVisible(i) && rowEnabled(i) &&
            (ImGui::IsKeyPressed(numberKeys[i],false) || ImGui::IsKeyPressed(keypadKeys[i],false))) {
            holdFinished=true;send(rowAction(i));break;
        }
    }
    std::array<ImVec2,4> offsets{};
    holdRows={};labelRows={};quickRows={};
    for (int i=0;i<4;++i) {
        if (!rowVisible(i)) continue;
        float width=i==1?(14+labelColumns()*21)*scale:basicLabelExtent(i).x+basicTextOffset()+figmaBasicRightPadding*scale;
        float height=i==1?(14+labelRowCount()*21)*scale:rowHeight;
        // Palette below the centre; both sort buttons form one row above it.
        // A closer palette leaves space for the read-only footer underneath.
        float radius=(i==1?45.f:65.f)*scale;
        if (i==0) radius=std::min(radius,std::max(20*scale,pieCenter.x-pos.x-width-8*scale));
        offsets[i]=i==0?ImVec2(-radius-width*.5f,0):i==1?ImVec2(0,radius+height*.5f):
            ImVec2((i==2?-1.f:1.f)*width*.5f,-radius-height*.5f);
        ImVec2 a={pieCenter.x+offsets[i].x-width*.5f,pieCenter.y+offsets[i].y-height*.5f};
        holdRows[i]={LONG(std::lround(a.x)),LONG(std::lround(a.y)),LONG(std::lround(a.x+width)),LONG(std::lround(a.y+height))};
        // Both sort segments share one pixel seam even with fractional glyph widths.
        if(i==2)holdRows[i].right=LONG(std::lround(pieCenter.x));
        if(i==3)holdRows[i].left=LONG(std::lround(pieCenter.x));
    }
    if (rowVisible(1)) {
        auto r=holdRows[1];
        for (int i=0;i<labelCount;++i) labelRows[i]={LONG(r.left+(7+(i%labelColumns())*21)*scale),LONG(r.top+(7+(i/labelColumns())*21)*scale),
            LONG(r.left+(7+(i%labelColumns()+1)*21)*scale),LONG(r.top+(7+(i/labelColumns()+1)*21)*scale)};
    }
    pieReady=!holdFinished;
    drawShapeEditor();
    const bool editing=shapePanelVisible() || shapeReturning;
    if(editing){holdRows={};labelRows={};quickRows={};capsRows={};}
    drawRecentPresets();
    // Build final hit rectangles before sampling, independent of animation.
    if(!editing && panelKind!=4 && panelKind!=3 && layerAware)for(int i=0;i<7;++i){int group=i>=3,n=group?i-3:i;
        float x=pieCenter.x+(menuQuickGroupOffset+figmaDisplayIconWidth*n)*scale,y=pieCenter.y+quickGroupY(group)*scale;
        quickRows[i]={LONG(x),LONG(y),LONG(x+figmaDisplayIconWidth*scale),LONG(y+figmaDisplayWidgetHeight*scale)};}
    if(!editing){drawCapsBar();drawToolBar();}else toolRow={};
    shapeEditorKeys();
    POINT cursor{};GetCursorPos(&cursor);int selected=holdActionAt(cursor);
    if(!preview.empty() && previewCapsHover>=0 && previewCapsHover<int(capsRows.size()) && capsEnabled(previewCapsHover))selected=40+previewCapsHover;
    if(!preview.empty() && previewQuickHover>=0 && previewQuickHover<7 && quickEnabled(previewQuickHover))selected=32+previewQuickHover;
    if(!preview.empty() && previewRecentHover>=0 && previewRecentHover<int(recentRows.size()) && shownPresets[previewRecentHover].available)selected=recentActionBase+previewRecentHover;
    if(!preview.empty() && previewActionHover>=0 && previewActionHover<4 && rowVisible(previewActionHover) && rowEnabled(previewActionHover))selected=previewActionHover;
    pendingHit=selected;
    auto captionX=[&](float textWidth) {
        const float centered=pieCenter.x-textWidth*.5f;
        return panelKind==4?std::min(centered,pieCenter.x+55*scale-textWidth):centered;
    };
    auto caption=[&](const char* text,float y) {
        auto extent=ImGui::CalcTextSize(text);
        ImVec2 at={roundf(captionX(extent.x)),roundf(y)};
        // A theme-colored second copy looks embossed on a transparent HWND.
        // Draw once with the same contrast as action labels.
        draw->AddText(at,tint(ImGuiCol_Text),text);
    };
    if(!editing)caption(panelTitle(),pieCenter.y-40*scale);
    // Blender's centre is a 12..19-unit annulus with a direction segment.
    const bool previewWorking=previewProgress.active && previewProgress.stage!="ready" && taskRecovery.empty();
    if(!editing || previewWorking)draw->AddCircle(pieCenter,15.5f*scale,tint(ImGuiCol_WindowBg),40,7*scale);
    float cursorX=float(cursor.x)-pieCenter.x,cursorY=float(cursor.y)-pieCenter.y;
    if(previewWorking)activity::draw(draw,pieCenter,15.5f*scale,scale,tint(ImGuiCol_Border),tint(ImGuiCol_CheckMark),previewAnimationMs>=0?ULONGLONG(previewAnimationMs):GetTickCount64());
    else if (!editing && cursorX*cursorX+cursorY*cursorY>=12*12*scale*scale && !holdFinished) {
        float desired=atan2f(cursorY,cursorX);
        ringAngle=ringAngleReady?followAngle(ringAngle,desired,ImGui::GetIO().DeltaTime):desired;
        ringAngleReady=true;
        draw->PathArcTo(pieCenter,15.5f*scale,ringAngle-.785398f,ringAngle+.785398f,10);
        draw->PathStroke(tint(selected>=0?ImGuiCol_CheckMark:ImGuiCol_PlotLines),0,7*scale);
    }
    if(!editing || previewWorking){menuShadow::draw(draw,{pieCenter.x-19*scale,pieCenter.y-19*scale},{pieCenter.x+19*scale,pieCenter.y+19*scale},19*scale,scale);draw->AddCircle(pieCenter,12*scale,tint(ImGuiCol_Border),40,scale);
        draw->AddCircle(pieCenter,19*scale,tint(ImGuiCol_Border),40,scale);}
    for (int i=0;i<4;++i) {
        if (editing || !rowVisible(i)) continue;
        if(i>=2 && panelKind==2 && capsReady && layerAware)continue;
        int vertexStart=draw->VtxBuffer.Size;
        ImVec2 a={float(holdRows[i].left),float(holdRows[i].top)},b={float(holdRows[i].right),float(holdRows[i].bottom)};
        bool active=i==selected || (i==1 && selected>=4 && selected<4+labelCount);
        if (pinned && i!=1 && !pieAnimating && rowEnabled(i) && !holdFinished) {
            ImGui::SetCursorScreenPos(a);ImGui::PushID(200+i);
            if (ImGui::InvisibleButton("##pinned-action",{b.x-a.x,b.y-a.y})) {holdFinished=true;send(rowAction(i));}
            ImGui::PopID();
        }
        if(i<2){menuShadow::draw(draw,a,b,rounding,scale);draw->AddRectFilled(a,b,active?tint(ImGuiCol_HeaderHovered):tint(i==1?ImGuiCol_WindowBg:ImGuiCol_Button),rounding);draw->AddRect(a,b,tint(ImGuiCol_Border),rounding,0,scale);}
        else {
            draw->AddRectFilled(a,b,active?tint(ImGuiCol_HeaderHovered):tint(ImGuiCol_Button),rounding,i==2?ImDrawFlags_RoundCornersLeft:ImDrawFlags_RoundCornersRight);
            if(i==2)draw->AddLine({b.x,a.y},{b.x,b.y},tint(ImGuiCol_Border),scale);
        }
        if (i==1) {
            drawLabels();
        } else {
            icon(i,{a.x+figmaBasicLeftPadding*scale,a.y+(rowHeight-figmaBasicIconSize*scale)*.5f},active);
            float textY=roundf(a.y+(rowHeight-basicLabelSize())*.5f);
            draw->AddText(menuFont(MenuTextRole::Control),basicLabelSize(),{roundf(a.x+basicTextOffset()),textY},rowEnabled(i)?tint(ImGuiCol_Text):tint(ImGuiCol_TextDisabled),rowLabel(i));
        }
        // Keep hit regions at their final positions so fast gestures don't
        // accidentally select a different card while visual cards overlap.
        for (int v=vertexStart;v<draw->VtxBuffer.Size;++v) {
            draw->VtxBuffer[v].pos.x+=i>=2?0:roundf(offsets[i].x*(fac-1));
            draw->VtxBuffer[v].pos.y+=roundf(offsets[i].y*(fac-1));
        }
    }
    if(!editing && panelKind==2 && capsReady && layerAware && rowVisible(2)){
        auto drawSortGroup=[&](int first,int last,RECT group){
        MenuControls::surface(draw,{float(group.left),float(group.top)},{float(group.right),float(group.bottom)},scale);
        for(int n=first;n<=last;++n){
            const int action=n<2?n+2:n+43;
            const RECT r=n<2?holdRows[n+2]:capsRows[n+3];
            const bool enabled=n<2?rowEnabled(n+2):capsEnabled(n+3),active=enabled && selected==action;
            const ImVec2 a={float(r.left),float(r.top)},b={float(r.right),float(r.bottom)};
            MenuControls::segment(draw,a,b,n==first,n==last,active,scale);
            const ImU32 ink=tint(enabled?ImGuiCol_Text:ImGuiCol_TextDisabled);
            const float height=b.y-a.y;
            const ImVec2 at={a.x+figmaBasicLeftPadding*scale,a.y+(height-figmaBasicIconSize*scale)*.5f};
            if(n<2)drawFigmaSortIcon(draw,n+2,at,figmaBasicIconSize*scale,ink);
            else drawStyledMenuIcon(draw,n+7,at,figmaBasicIconSize*scale,ink,tint(active?ImGuiCol_HeaderHovered:ImGuiCol_Button));
            draw->AddText(menuFont(MenuTextRole::Control),basicLabelSize(),{a.x+basicTextOffset(),roundf(a.y+(height-basicLabelSize())*.5f)},ink,n<2?rowLabel(n+2):capsKeys[n+3]);
            if(pinned && !pieAnimating && enabled && !holdFinished){
                ImGui::SetCursorScreenPos({a.x+(n>first?scale:0),a.y});ImGui::PushID(460+n);
                if(ImGui::InvisibleButton("##reorder",{b.x-a.x-(n>first?scale:0)-(n<last?scale:0),b.y-a.y})){
                    holdFinished=true;send(n<2?rowAction(n+2):capsActions[n+3]);
                }
                ImGui::PopID();
            }
        }
        MenuControls::outline(draw,{float(group.left),float(group.top)},{float(group.right),float(group.bottom)},scale);
        };
        drawSortGroup(0,3,tipGroupRect(2));
    }
    else if(!editing && rowVisible(2) && rowVisible(3)) {
        const auto left=holdRows[2],right=holdRows[3];
        ImVec2 a={float(left.left),float(left.top)},b={float(right.right),float(right.bottom)};
        a.y+=roundf(offsets[2].y*(fac-1));b.y+=roundf(offsets[3].y*(fac-1));
        // Both outer ends expand from their own centre; outline joins them at rest.
        draw->AddRect(a,b,tint(ImGuiCol_Border),rounding,0,scale);
    }
    if(!editing)drawQuickGroups(selected,fac);
    // Only show exceptional state; the normal pie has no tutorial banner.
    const char* hint=panelKind==3 || inputFocused?"输入框编辑中":!canExecute() && !previewProgress.active?"当前不可执行":nullptr;
    std::vector<std::string> messages;
    if(panelKind!=4 && !document.empty())messages.push_back(document);
    if(layerAware && panelKind!=4) {
        const char* restriction=layerKind==0?"请重新打开菜单":layerBackground?"标签和排序需先解锁":
            layerCrossParents?"跨父组多选，排序不可用":layerLocked?"排序不可用":"";
        messages.push_back(layerTitle()+(restriction[0]?std::string(" · ")+restriction:""));
    }
    if(hint)messages.push_back(hint);
    if(shapeEditor.ready() && shapeEditor.dirty && !previewProgress.active && !maskPreviewStarted)messages.push_back("调整待应用");
    if(shapeEyedropActive)messages.push_back("屏幕取色：点击颜色；右键取消取色");
    if(shapeEditor.ready() && shapeEditor.copyFailed)messages.push_back("剪贴板暂时不可用，未复制颜色");
    messages.push_back(taskStatus()+(taskFailed && !taskError.empty()?"\n"+taskError:""));
    drawFooter(messages,pieCenter.y+((!editing && capsReady && panelKind!=4?143.f:116.f)+(!editing && rowVisible(1)?(labelRowCount()-2)*21.f:0))*scale,panelKind==4);
    const int tipAction=shapeEditor.ready() && shapeTipCandidate>=200?shapeTipCandidate:actionTipText(selected)?selected:-1;
    const bool ownEditorTip=tipAction>=200 && tipAction<200+shapeTipCount && shapeEditor.ready();
    const bool showTip=!pieAnimating && !holdFinished && !inputFocused && !ImGui::IsAnyItemActive() && (ownEditorTip || canExecute());
    const int displayed=!preview.empty()?(showTip?tipAction:-1):hoverTip.update(tipAction,tipFamily(tipAction),GetTickCount64(),tipGapAt(hoverTip.shown,cursor),showTip);
    if(displayed>=0)drawActionTip(displayed);else tipVisual.reset();
}
static void drawBusyFeedback() {
    hoverTip.reset();tipVisual.reset();
    busyFeedback=true;quickRows={};capsRows={};
    pieReady=false;pieAnimating=false;holdRows={};labelRows={};renderedHit=pendingHit=-1;
    auto* draw=ImGui::GetWindowDrawList();
    const auto pos=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();
    pieCenter={pos.x+size.x*.5f,pos.y+size.y*.5f};
    if(panelKind==2 && size.x>700*scale)pieCenter.x=pos.x+std::max(size.x*.5f,std::min(650*scale,size.x-270*scale));
    if(pieAnchorX>=0)pieCenter.x=pos.x+pieAnchorX;
    if (!feedbackStarted) feedbackStarted=GetTickCount64();
    float fade=selfTest?1.f:pieAnimation(float(GetTickCount64()-feedbackStarted));
    setOpacity(IsWindowVisible(window) && !holdFinished?fade:0.f);
    draw->AddCircle(pieCenter,15.5f*scale,tint(ImGuiCol_WindowBg),40,7*scale);
    if (taskBusy) {
        activity::draw(draw,pieCenter,15.5f*scale,scale,tint(ImGuiCol_Border),tint(ImGuiCol_CheckMark),
            previewAnimationMs>=0?ULONGLONG(previewAnimationMs):GetTickCount64());
    } else if (taskHasResult) {
        if (taskFailed) {
            draw->AddLine({pieCenter.x-5*scale,pieCenter.y-5*scale},{pieCenter.x+5*scale,pieCenter.y+5*scale},tint(ImGuiCol_Text),2*scale);
            draw->AddLine({pieCenter.x-5*scale,pieCenter.y+5*scale},{pieCenter.x+5*scale,pieCenter.y-5*scale},tint(ImGuiCol_Text),2*scale);
        } else {
            draw->AddLine({pieCenter.x-6*scale,pieCenter.y},{pieCenter.x-2*scale,pieCenter.y+4*scale},tint(ImGuiCol_CheckMark),2*scale);
            draw->AddLine({pieCenter.x-2*scale,pieCenter.y+4*scale},{pieCenter.x+6*scale,pieCenter.y-5*scale},tint(ImGuiCol_CheckMark),2*scale);
        }
    }
    draw->AddCircle(pieCenter,12*scale,tint(ImGuiCol_Border),40,scale);
    draw->AddCircle(pieCenter,19*scale,tint(ImGuiCol_Border),40,scale);
    drawFooter({panelKind==4?"":document,taskStatus()+(taskFailed && !taskError.empty()?"\n"+taskError:"")},pieCenter.y+32*scale);
}
static void drawMenu() {
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos);
    ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    const bool feedback=taskBusy || busyFeedback;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,holdMode || feedback?0.f:scale);
    ImGui::Begin("##menu",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|((holdMode || feedback)?ImGuiWindowFlags_NoBackground:0));
    ImGui::PopStyleVar();
    if (feedback) {drawBusyFeedback();ImGui::End();return;}
    if (holdMode) {drawPie();ImGui::End();return;}
    {MenuTextStyle text(MenuTextRole::Control);
    ImGui::TextWrapped("%s",taskStatus().c_str());
    if (contextMode) {
        contextItems();
    } else {
        ImGui::TextDisabled("%s",panelTitle());ImGui::SameLine();ImGui::TextDisabled("QUICK MENU");
        if (panelKind!=3 && !holdMode) {
            ImGui::SetNextItemWidth(-1);
            if (focusSearch) {ImGui::SetKeyboardFocusHere();focusSearch=false;}
            if (ImGui::InputTextWithHint("##search","搜索操作…",search,sizeof(search))) selection=0;
        }
        ImGui::Separator();std::array<int,4> results{};int count=0;
        for (int i=0;i<4;++i) {
            if (rowVisible(i) && matches(i,search)) results[count++]=i;
        }
        if (count && !holdMode) {
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) selection=(selection+1)%count;
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) selection=(selection+count-1)%count;
            selection=std::clamp(selection,0,count-1);
        }
        bool popupOpen=ImGui::IsPopupOpen("context");
        if (!holdMode && count && rowEnabled(results[selection]) && !popupOpen && ImGui::IsKeyPressed(ImGuiKey_Enter,false)) send(rowAction(results[selection]));
        // Quick-menu keyboard navigation is explicit. Letting Selectable also
        // receive ImGui nav activation can dispatch a second, different row.
        ImGui::PushItemFlag(ImGuiItemFlags_NoNav,true);
        ImGui::BeginDisabled(!canExecute());
        holdRows={};
        for (int r=0;r<count;++r) {
            int i=results[r];ImVec2 pos=ImGui::GetCursorScreenPos();
            ImGui::PushID(i);ImGui::BeginDisabled(!rowEnabled(i));
            POINT cursor{};GetCursorPos(&cursor);
            RECT row={static_cast<LONG>(pos.x),static_cast<LONG>(pos.y),
                static_cast<LONG>(pos.x+ImGui::GetContentRegionAvail().x),static_cast<LONG>(pos.y+27*scale)};
            bool highlighted=holdMode?PtInRect(&row,cursor):selection==r;
            if (ImGui::Selectable("##row",highlighted,0,{0,27*scale}) && !holdMode) send(rowAction(i));
            ImVec2 minimum=ImGui::GetItemRectMin(),maximum=ImGui::GetItemRectMax();
            holdRows[i]={static_cast<LONG>(minimum.x),static_cast<LONG>(minimum.y),static_cast<LONG>(maximum.x),static_cast<LONG>(maximum.y)};
            if (i==3) bottomRow={pos.x+70*scale,pos.y+13*scale};
            if (!holdMode && ImGui::IsItemHovered() && (ImGui::GetIO().MouseDelta.x!=0 || ImGui::GetIO().MouseDelta.y!=0)) selection=r;
            icon(i,{pos.x+figmaBasicLeftPadding*scale,pos.y+(27-figmaBasicIconSize)*scale*.5f},highlighted);auto* draw=ImGui::GetWindowDrawList();
            draw->AddText(menuFont(MenuTextRole::Control),basicLabelSize(),{pos.x+basicTextOffset(),pos.y+(27*scale-basicLabelSize())*.5f},tint(rowEnabled(i)?ImGuiCol_Text:ImGuiCol_TextDisabled),rowLabel(i));
            ImGui::EndDisabled();ImGui::PopID();
        }
        ImGui::EndDisabled();
        if (!count) ImGui::TextDisabled(panelKind==3?"输入框编辑中，图层操作已暂停":"没有匹配的操作");
        if (inputFocused && panelKind!=3) ImGui::TextDisabled("输入框仍有焦点，图层操作已暂停");
        ImGui::Separator();if (!holdMode && ImGui::MenuItem("后台管理…")) send("control");
        ImGui::PopItemFlag();
        ImGui::TextDisabled("%s",document.c_str());
        if (!notice.empty()) { ImGui::PushTextWrapPos();ImGui::TextColored(ImGui::GetStyle().Colors[ImGuiCol_Text],"%s",notice.c_str());ImGui::PopTextWrapPos(); }
        if (!holdMode && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            if (rightHide || !ImGui::IsAnyItemHovered()) send("hide");else ImGui::OpenPopup("context");
        }
        if (ImGui::BeginPopup("context")) {contextItems();ImGui::EndPopup();}
    }
    if (contextMode && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) send("hide");
    }
    ImGui::End();
}
// Long diagnostic loops must service the same queue as the normal renderer.
static bool pumpMessages() {
    MSG msg;
    while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) {
        TranslateMessage(&msg);DispatchMessageW(&msg);
        if(msg.message==WM_QUIT)running=false;
    }
    return running;
}
static bool waitMessages(DWORD milliseconds) {
    const auto until=GetTickCount64()+milliseconds;
    while(pumpMessages()) {
        const auto now=GetTickCount64();if(now>=until)return true;
        if(MsgWaitForMultipleObjectsEx(0,nullptr,DWORD(until-now),QS_ALLINPUT,MWMO_INPUTAVAILABLE)==WAIT_FAILED)
            throw std::runtime_error("Diagnostic message wait failed");
    }
    return false;
}
static void benchmarkShapeEditor(const Json& fixture) {
    if(shapeBenchmarkOutput.empty())return;
    Json report={{"scope",benchmarkRenderCompletion?"warm CPU build/submission plus DX11 EVENT completion (includes queue/driver/query wait); no Present/vsync or physical key latency":"warm native CPU frame build + DX11 command submission; no Present/vsync, physical key latency or PS IPC"},{"graphics",graphicsInfo()},{"samples_per_case",200},{"font","large"},{"dpi_scale",scale},{"cases",Json::array()}};
    ID3D11Query* completion=nullptr;
    if(benchmarkRenderCompletion){D3D11_QUERY_DESC desc={D3D11_QUERY_EVENT,0};if(FAILED(device->CreateQuery(&desc,&completion)))throw std::runtime_error("Unable to create render completion query");}
    const auto releaseQuery=[](ID3D11Query* p){if(p)p->Release();};
    std::unique_ptr<ID3D11Query,decltype(releaseQuery)> queryOwner(completion,releaseQuery);
    auto& io=ImGui::GetIO();uiFontSize="large";io.FontDefault=menuFont(MenuTextRole::Body);layerAware=true;layerKind=6;layerCount=1;layerMask=15;quickMask=127;panelKind=2;capsReady=documentSortReady=true;
    const auto actionsBefore=testedActions.size();
    std::cout<<Json{{"type","benchmark.started"},{"hwnd",reinterpret_cast<uintptr_t>(window)}}.dump()<<std::endl;
    for(const char* themeName:{"light","light-gray","dark-gray","dark"}){
        if(benchmarkMenuOnly && std::string(themeName)!="light-gray")continue;
        uiTheme=themeName;theme();
        for(const char* kind:{"plain","solid","gradient","pattern"}){
            if(benchmarkMenuOnly && std::string(kind)!="plain")continue;
            shapeEditor.prepare(fixture);setHoldMode(true);shapePanelOpen=true;
            if(std::string(kind)=="plain")shapeEditor.available=false;
            else if(std::string(kind)=="gradient")shapeEditor.mode(1);
            else if(std::string(kind)=="pattern")shapeEditor.mode(2);
            std::vector<double> values,completed;int vertices=0,commands=0;
            for(int frame=0;frame<221;++frame){
                if(!pumpMessages() || frame==20 && !waitMessages(250))return;
                ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();io.DeltaTime=1.f/60;
                const auto start=std::chrono::steady_clock::now();
                ImGui::NewFrame();
            if (!io.Fonts->IsBuilt()) throw std::runtime_error("Font atlas unavailable after core NewFrame");
            drawMenu();ImGui::Render();
            lastVisibleVertices=ImGui::GetDrawData()->TotalVtxCount;
                context->OMSetRenderTargets(1,&target,nullptr);const float clear[]={0,0,0,0};context->ClearRenderTargetView(target,clear);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
                const double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                if(completion){
                    context->End(completion);context->Flush();HRESULT done;
                    while((done=context->GetData(completion,nullptr,0,D3D11_ASYNC_GETDATA_DONOTFLUSH))==S_FALSE){
                        if(std::chrono::steady_clock::now()-start>std::chrono::seconds(2))throw std::runtime_error("Render completion timed out");
                        if(!pumpMessages())return;
                        SwitchToThread();
                    }
                    if(FAILED(done))throw std::runtime_error("Render device failed during completion benchmark");
                    if(frame>=21)completed.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
                }
                vertices=ImGui::GetDrawData()->TotalVtxCount;commands=0;for(auto* list:ImGui::GetDrawData()->CmdLists)commands+=list->CmdBuffer.Size;finishPlatformFrame();
                if(frame>=21)values.push_back(elapsed);
                if(frame==0 || frame==20)report["render_transitions"].push_back({{"theme",themeName},{"kind",kind},{"phase",frame==0?"initial-render":"idle-250ms-render"},{"samples",Json::array({elapsed})}});
            }
            const auto raw=values,rawCompleted=completed;
            std::sort(values.begin(),values.end());
            report["cases"].push_back({{"theme",themeName},{"kind",kind},{"phase","warm-render"},{"samples",raw},{"maximum_ms",values.back()},{"median_ms",values[100]},{"p95_ms",values[189]},{"vertices",vertices},{"draw_commands",commands}});
            if(completion){std::sort(completed.begin(),completed.end());report["cases"].back()["completion_median_ms"]=completed[100];report["cases"].back()["completion_p95_ms"]=completed[189];report["cases"].back()["completion_maximum_ms"]=completed.back();report["cases"].back()["completion_samples"]=rawCompleted;}
        }
    }
    report["shadow_enabled"]=menuShadow::enabled;
    if(testedActions.size()!=actionsBefore)throw std::runtime_error("Rendering benchmark submitted an operation");
    std::ofstream output{std::filesystem::path(shapeBenchmarkOutput)};output<<report.dump(2);
}
static void screenshot(const std::wstring& path) {
    ID3D11Texture2D *buffer=nullptr,*staging=nullptr;
    // Capture the texture actually bound for this frame. A flip-chain buffer
    // queried separately can expose a different alias from the rendered RTV.
    ID3D11Resource* rendered=nullptr;
    if(target)target->GetResource(&rendered);
    HRESULT sourceResult=rendered?rendered->QueryInterface(IID_PPV_ARGS(&buffer)):E_FAIL;
    if(rendered)rendered->Release();
    if (FAILED(sourceResult)) throw std::runtime_error("Preview render texture unavailable");
    D3D11_TEXTURE2D_DESC desc;buffer->GetDesc(&desc);
    // Re-render the completed draw data into a dedicated non-flip texture.
    // Flip-chain aliases can return partially updated contents during capture;
    // this preview-only pass does not change the production display path.
    D3D11_TEXTURE2D_DESC captureDesc=desc;
    captureDesc.Usage=D3D11_USAGE_DEFAULT;captureDesc.BindFlags=D3D11_BIND_RENDER_TARGET;
    captureDesc.CPUAccessFlags=0;captureDesc.MiscFlags=0;
    ID3D11Texture2D* capture=nullptr;ID3D11RenderTargetView* captureTarget=nullptr;
    HRESULT captureResult=device->CreateTexture2D(&captureDesc,nullptr,&capture);
    if(SUCCEEDED(captureResult))captureResult=device->CreateRenderTargetView(capture,nullptr,&captureTarget);
    if(FAILED(captureResult)) {if(capture)capture->Release();buffer->Release();throw std::runtime_error("Preview offscreen target failed");}
    const float transparent[]={0,0,0,0};
    context->OMSetRenderTargets(1,&captureTarget,nullptr);
    context->ClearRenderTargetView(captureTarget,transparent);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    context->OMSetRenderTargets(1,&target,nullptr);
    captureTarget->Release();buffer->Release();buffer=capture;
    desc.Usage=D3D11_USAGE_STAGING;
    desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
    HRESULT result=device->CreateTexture2D(&desc,nullptr,&staging);
    if (FAILED(result)) {buffer->Release();throw std::runtime_error("Preview staging failed");}
    context->CopyResource(staging,buffer);buffer->Release();D3D11_MAPPED_SUBRESOURCE pixels;
    if (FAILED(context->Map(staging,0,D3D11_MAP_READ,0,&pixels))) {staging->Release();throw std::runtime_error("Preview read failed");}
    IWICImagingFactory* factory=nullptr;IWICStream* stream=nullptr;IWICBitmapEncoder* encoder=nullptr;IWICBitmapFrameEncode* frame=nullptr;
    result=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));
    if (SUCCEEDED(result)) result=factory->CreateStream(&stream);
    if (SUCCEEDED(result)) result=stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE);
    if (SUCCEEDED(result)) result=factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder);
    if (SUCCEEDED(result)) result=encoder->Initialize(stream,WICBitmapEncoderNoCache);
    if (SUCCEEDED(result)) result=encoder->CreateNewFrame(&frame,nullptr);
    if (SUCCEEDED(result)) result=frame->Initialize(nullptr);
    if (SUCCEEDED(result)) result=frame->SetSize(desc.Width,desc.Height);
    WICPixelFormatGUID format=GUID_WICPixelFormat32bppRGBA;
    if (SUCCEEDED(result)) result=frame->SetPixelFormat(&format);
    std::vector<BYTE> converted;
    UINT stride=pixels.RowPitch;BYTE* data=static_cast<BYTE*>(pixels.pData);
    if (SUCCEEDED(result)) {
        stride=desc.Width*4;converted.resize(stride*desc.Height);
        for (UINT y=0;y<desc.Height;++y) for (UINT x=0;x<desc.Width;++x) {
            BYTE* source=data+y*pixels.RowPitch+x*4;BYTE* output=converted.data()+y*stride+x*4;
            bool bgra=IsEqualGUID(format,GUID_WICPixelFormat32bppBGRA);
            // DComposition buffers are premultiplied BGRA; PNG stores straight alpha.
            const int alpha=source[3];
            auto straight=[&](int channel)->BYTE {return alpha?BYTE(std::min(255,(int(source[channel])*255+alpha/2)/alpha)):0;};
            output[0]=straight(bgra?0:2);output[1]=straight(1);output[2]=straight(bgra?2:0);
            output[3]=BYTE(alpha);
        }
        data=converted.data();
    } else if (SUCCEEDED(result) && !IsEqualGUID(format,GUID_WICPixelFormat32bppRGBA)) result=E_FAIL;
    if (SUCCEEDED(result)) result=frame->WritePixels(desc.Height,stride,stride*desc.Height,data);
    if (SUCCEEDED(result)) result=frame->Commit();
    if (SUCCEEDED(result)) result=encoder->Commit();
    if (frame) frame->Release();if (encoder) encoder->Release();if (stream) stream->Release();if (factory) factory->Release();
    context->Unmap(staging,0);staging->Release();
    if (FAILED(result)) throw std::runtime_error("Preview PNG encoding failed");
}
static void readEvents() {
    try {
        std::string line;
        while (std::getline(std::cin,line)) {
            auto event=std::make_unique<Json>(Json::parse(line));
            if (!PostMessageW(window,WM_APP,0,reinterpret_cast<LPARAM>(event.get()))) break;
            event.release();
        }
    } catch (...) {}
    PostMessageW(window,gradientWindow?WM_APP+32:WM_CLOSE,0,0);
}
int main() {
    // Report CRT assertions to overlay.log rather than a blocking OS dialog.
    _set_error_mode(_OUT_TO_STDERR);
    int argc;LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    std::wstring iconPath;DWORD parent=0;POINT testOrigin{100,100};
    for (int i=1;i<argc;++i) {
        std::wstring arg=argv[i];
        if (arg==L"--gradient-window") gradientWindow=true;
        else if (i+1<argc && arg==L"--gradient-anchor") gradientAnchor=reinterpret_cast<HWND>(std::stoull(argv[++i]));
        else if (i+1<argc && arg==L"--gradient-owner") gradientOwner.hwnd=reinterpret_cast<HWND>(std::stoull(argv[++i]));
        else if (arg==L"--preview-lifecycle") previewLifecycle=true;
        else if (arg==L"--preview-live") previewLive=true;
        else if (arg==L"--self-test") selfTest=true;
        else if (i+2<argc && arg==L"--self-test-origin") {
            testOrigin.x=std::stol(argv[++i]);testOrigin.y=std::stol(argv[++i]);
        }
        else if (arg==L"--benchmark-menu-only") benchmarkMenuOnly=true;
        else if (arg==L"--benchmark-no-menu-shadow") menuShadow::enabled=false;
        else if (arg==L"--benchmark-render-completion") benchmarkRenderCompletion=true;
        else if (i+1<argc && arg==L"--render-mode") {
            std::wstring value=argv[++i];std::string mode(value.begin(),value.end());
            if(mode=="auto" || mode=="gpu" || mode=="cpu")renderMode=benchmarkRenderMode=mode;
        }
        else if (i+1<argc && arg==L"--benchmark-shape-output") {shapeBenchmarkOutput=argv[++i];selfTest=true;}
        else if (i+1<argc && arg==L"--preview-output") preview=argv[++i];
        else if (i+1<argc && arg==L"--preview-kind") {
            std::wstring kind=argv[++i];previewKind=kind==L"context"?"context":kind==L"hold"?"hold":kind==L"busy"?"busy":"menu";
        }
        else if(i+1<argc && arg==L"--preview-text-tab") {std::wstring value=argv[++i];previewTextTab=value==L"replace"?1:value==L"tidy"?2:value==L"split"?3:value==L"list"?4:0;}
        else if(i+1<argc && arg==L"--preview-tool") {std::wstring value=argv[++i];if(value==L"search" || value==L"split")previewTool=std::string(value.begin(),value.end());}
        else if (i+1<argc && arg==L"--preview-theme") {
            std::wstring value=argv[++i];std::string name(value.begin(),value.end());
            if (knownTheme(name)) uiTheme=name;
        }
        else if (i+1<argc && arg==L"--preview-animation-ms") previewAnimationMs=std::stof(argv[++i]);
        else if (i+1<argc && arg==L"--preview-recent-hover") previewRecentHover=std::stoi(argv[++i]);
        else if (i+1<argc && arg==L"--preview-action-hover") previewActionHover=std::stoi(argv[++i]);
        else if (i+1<argc && arg==L"--preview-caps-hover") previewCapsHover=std::stoi(argv[++i]);
        else if (i+1<argc && arg==L"--preview-quick-hover") previewQuickHover=std::stoi(argv[++i]);
        else if (i+1<argc && arg==L"--preview-progress") previewProgressStage=std::filesystem::path(argv[++i]).u8string();
        else if (i+1<argc && arg==L"--preview-shape-hover") previewShapeHover=std::stoi(argv[++i]);
        else if (arg==L"--preview-owning-editor") previewOwningEditor=true;
        else if (arg==L"--preview-ordinary") previewOrdinary=true;
        else if (arg==L"--preview-exit-editor") previewExitEditor=true;
        else if (i+1<argc && arg==L"--preview-shape-height") previewShapeHeight=std::clamp(std::stof(argv[++i]),440.f,620.f);
        else if (i+1<argc && arg==L"--preview-shape") {
            std::ifstream source(std::filesystem::path(argv[++i]));Json data;source>>data;
            shapeEditor.prepare(data);previewShape=true;previewKind="hold";
            shapeClipboardReady=data.value("clipboard_ready",false);shapeClipboardAvailable=data.value("clipboard_available",false);
            layerAware=true;layerKind=6;layerCount=shapeEditor.batchMask?shapeEditor.batchCount:1;layerMask=15;quickMask=127;panelKind=2;
        }
        else if (i+1<argc && arg==L"--preview-layer-context") {
            std::ifstream source(std::filesystem::path(argv[++i]));Json data;source>>data;
            if(data.contains("photoshop"))labelCount=supportedLabelCount(data.at("photoshop"));
            if(data.contains("label_colors"))applyLabelColors(data["label_colors"]);
            capsReady=data.value("caps_ready",false);
            individualSmartReady=data.value("individual_smart_ready",false);
            layerSearchReady=textSplitReady=data.value("tools_ready",false);
            layerAware=true;layerMask=data.value("mask",0);
            quickMask=data.value("quick_mask",0);
            const std::string kind=data.value("kind","unknown");
            const char* names[]={"unknown","background","pixel","group","text","smart","shape-fill","adjustment","mixed","other"};
            layerKind=0;for(int n=0;n<10;++n)if(kind==names[n])layerKind=n;
            layerCount=int(data.value("layer_ids",Json::array()).size());
            layerBackground=data.value("background",false);layerLocked=data.value("locked",false);
            layerCrossParents=!data.value("same_parent",true);
        }
        else if (i+1<argc && arg==L"--preview-font-size") {
            std::wstring value=argv[++i];
            if (value==L"small" || value==L"medium" || value==L"large") uiFontSize=std::string(value.begin(),value.end());
        }
        else if (i+1<argc && arg==L"--preview-panel") {
            std::wstring kind=argv[++i];panelKind=kind==L"canvas"?1:kind==L"layers"?2:kind==L"text"?3:kind==L"home"?4:0;
        }
        else if (i+1<argc && arg==L"--preview-recents") {
            std::ifstream source(std::filesystem::path(argv[++i]));Json data;source>>data;
            for(const auto& item:data.at("items")) {
                recentPresets.push_back({item.at("id"),item.at("name"),item.at("size"),item.at("detail")});
                requireText(recentPresets.back().name);requireText(recentPresets.back().size);requireText(recentPresets.back().detail);
            }
            recentReady=true;
        }
        else if (i+1<argc && arg==L"--icon") iconPath=argv[++i];
        else if (i+1<argc && arg==L"--parent") parent=std::stoul(argv[++i]);
    }
    LocalFree(argv);CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    backendParent=parent;
    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};wc.style=CS_CLASSDC;wc.lpfnWndProc=procedure;wc.hInstance=GetModuleHandleW(nullptr);
    wc.lpszClassName=gradientWindow?L"PhotoshopGradientImGui":L"PhotoshopMenuImGui";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    HICON iconHandle=iconPath.empty()?nullptr:static_cast<HICON>(LoadImageW(nullptr,iconPath.c_str(),IMAGE_ICON,32,32,LR_LOADFROMFILE));
    wc.hIcon=wc.hIconSm=iconHandle;RegisterClassExW(&wc);
    contextMode=previewKind=="context";
    RECT bounds={100,100,100+(contextMode?260:360),100+(contextMode?240:300)};
    if(gradientWindow){
        if(!gradientOwner.bind()){std::cerr<<"Photoshop gradient owner unavailable\n";return 1;}
        POINT cursor{};GetCursorPos(&cursor);
        const auto monitor=gradientAnchor?MonitorFromWindow(gradientAnchor,MONITOR_DEFAULTTONEAREST):MonitorFromPoint(cursor,MONITOR_DEFAULTTONEAREST);
        MONITORINFO info{sizeof(info)};
        if(!GetMonitorInfoW(monitor,&info))return 1;
        bounds=gradientWindowBounds(info.rcWork,UINT(std::lround(96*ImGui_ImplWin32_GetDpiScaleForMonitor(monitor))));
    }
    window=CreateWindowExW(gradientWindow?gradientExStyle:WS_EX_TOOLWINDOW|WS_EX_NOREDIRECTIONBITMAP,wc.lpszClassName,gradientWindow?L"151500px-menu — 渐变编辑":preview.empty()?L"151500px-menu - Actions":L"151500px-menu - Preview",
        gradientWindow?gradientStyle:WS_POPUP,bounds.left,bounds.top,bounds.right-bounds.left,bounds.bottom-bounds.top,nullptr,nullptr,wc.hInstance,nullptr);
    if(window && gradientWindow){
        const COLORREF border=DWMWA_COLOR_NONE;
        const DWM_WINDOW_CORNER_PREFERENCE corners=DWMWCP_DONOTROUND;
        DwmSetWindowAttribute(window,DWMWA_BORDER_COLOR,&border,sizeof(border));
        DwmSetWindowAttribute(window,DWMWA_WINDOW_CORNER_PREFERENCE,&corners,sizeof(corners));
        SetWindowPos(window,nullptr,0,0,0,0,SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
    }
    if (!window || !createDevice(window)) {std::cerr<<"DirectX 11 initialization failed\n";return 1;}
    IMGUI_CHECKVERSION();ImGui::CreateContext();auto& io=ImGui::GetIO();
    io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard|ImGuiConfigFlags_ViewportsEnable;
    io.IniFilename=nullptr;io.LogFilename=nullptr;
    // Photoshop owns its tool cursor. The visible overlay must never replace
    // the desktop cursor during renderer frames/status updates. Our window
    // class supplies its arrow on ordinary client WM_SETCURSOR messages.
    // Keep this policy identical in production and automated checks.
    io.ConfigFlags|=ImGuiConfigFlags_NoMouseCursorChange;
    scale=ImGui_ImplWin32_GetDpiScaleForHwnd(window);theme();
    if (!preview.empty() || selfTest) SetWindowPos(window,nullptr,testOrigin.x,testOrigin.y,
        static_cast<int>((previewShape?920:contextMode?260:360)*scale),static_cast<int>((previewShape?menuShapeWindowHeight:contextMode?240:300)*scale),SWP_NOZORDER|SWP_NOACTIVATE);
    io.ConfigViewportsNoTaskBarIcon=true;
    std::wstring fontPath; wchar_t windowsPath[MAX_PATH];GetWindowsDirectoryW(windowsPath,MAX_PATH);fontPath=std::wstring(windowsPath)+L"/Fonts/msyh.ttc";
    int size=WideCharToMultiByte(CP_UTF8,0,fontPath.c_str(),-1,nullptr,0,nullptr,nullptr);std::string font(size,0);WideCharToMultiByte(CP_UTF8,0,fontPath.c_str(),-1,font.data(),size,nullptr,nullptr);
    requiredGlyphs.AddRanges(io.Fonts->GetGlyphRangesDefault());
    for (ImWchar c:menuStaticGlyphs) requiredGlyphs.AddChar(c);
    ImVector<ImWchar> glyphs;
    wchar_t executable[MAX_PATH]{};GetModuleFileNameW(nullptr,executable,MAX_PATH);
    auto fontFolder=std::filesystem::path(executable).parent_path()/L"fonts";
    auto utf8=[](const std::filesystem::path& path) {return path.u8string();};
    auto loadFont=[&]{
        const ULONGLONG fontStart=GetTickCount64();
        io.FontDefault=nullptr;
        io.Fonts->Clear();
        glyphs.clear();requiredGlyphs.BuildRanges(&glyphs);
        auto inter=utf8(fontFolder/L"Inter.ttf"),cjk=utf8(fontFolder/L"NotoSansCJK.otf");
        for (int index=0;index<int(std::size(menuFontEms));++index) {
        const float em=menuFontEms[index];
        if (std::filesystem::exists(fontFolder/L"Inter.ttf") && std::filesystem::exists(fontFolder/L"NotoSansCJK.otf")) {
            // stb_truetype sizes by ascent-descent; Blender/FreeType sizes by
            // em. Convert hhea metrics; apply monitor DPI exactly once.
            menuFonts[index]=io.Fonts->AddFontFromFileTTF(inter.c_str(),std::floor(em*scale*(2478.f/2048.f)));
            ImFontConfig merge;merge.MergeMode=true;
            io.Fonts->AddFontFromFileTTF(cjk.c_str(),std::floor(em*scale*1.448f),&merge,glyphs.Data);
        } else {
            menuFonts[index]=io.Fonts->AddFontFromFileTTF(font.c_str(),std::floor(em*scale),nullptr,glyphs.Data);
            if (!menuFonts[index]) menuFonts[index]=io.Fonts->AddFontDefault();
        }
        }
        // Preload the same bounded glyph set before opening, including all PS sizes.
        for(auto* cached:menuFonts) {
            auto* baked=cached->GetFontBaked(cached->LegacySize);
            for(const ImWchar* range=glyphs.Data;range[0];range+=2)
                for(unsigned c=range[0];c<=range[1];++c)baked->FindGlyph(static_cast<ImWchar>(c));
        }
        // Build CPU glyph data explicitly. GPU creation can fail before its
        // implicit font build (e.g. a removed device); NewFrame must never
        // receive the cleared, unbuilt atlas in that case.
        if (!io.Fonts->Build() || !io.Fonts->IsBuilt())
            throw std::runtime_error("Unable to build menu font atlas");
        fontDirty=false;
        ++fontBuilds;io.FontDefault=menuFont(MenuTextRole::Body);
        std::cerr<<"Font atlas prepared: semantic cached sizes, "<<(GetTickCount64()-fontStart)<<" ms, "
                 <<io.Fonts->TexData->Width<<"x"<<io.Fonts->TexData->Height<<", builds="<<fontBuilds<<"\n";
    };
    ImGui_ImplWin32_Init(window);ImGui_ImplDX11_Init(device,context);
    loadFont();
    platformShowWindow=ImGui::GetPlatformIO().Platform_ShowWindow;
    ImGui::GetPlatformIO().Platform_ShowWindow=showPopupWindow;
    if (!prepareRendererResources()) {
        std::cerr<<"Unable to initialize menu renderer resources\n";return 1;
    }
    if (preview.empty() && !selfTest) {
        std::cout<<Json({{"type","ready"},{"hwnd",reinterpret_cast<uintptr_t>(window)},{"pid",GetCurrentProcessId()},{"graphics",graphicsInfo()},{"pie_right_inset",pieRightInset}}).dump()<<std::endl;
        std::thread(readEvents).detach();
        if (parent) std::thread([parent]{HANDLE process=OpenProcess(SYNCHRONIZE,FALSE,parent);if (process){WaitForSingleObject(process,INFINITE);CloseHandle(process);}PostMessageW(window,gradientWindow?WM_APP+32:WM_CLOSE,0,0);}).detach();
    } else {
        allowed=true;document="示例文档.psd";
        if (selfTest) {
            const auto focusPacket=[](uint32_t epoch,DWORD expires){return LPARAM((uint64_t(epoch)<<32)|expires);};
            if(!focusRequestCurrent(focusPacket(7,1500),7,1000) || !focusRequestCurrent(focusPacket(7,1001),7,1000) ||
               !focusRequestCurrent(focusPacket(7,32),7,0xfffffff0) || focusRequestCurrent(focusPacket(7,1000),7,1000) ||
               focusRequestCurrent(focusPacket(7,999),7,1000) || focusRequestCurrent(focusPacket(7,1501),7,1000) ||
               focusRequestCurrent(focusPacket(7,1500),8,1000) || focusRequestCurrent(0,7,1000))
                throw std::runtime_error("Focus request epoch/expiry/wrap validation failed");
            if(!menuShadow::texture)throw std::runtime_error("Prewarmed shadow atlas missing");
            for(double radius:{double(figmaCornerRadius),19.}){
                if(menuShadow::alpha(0,0,radius) || menuShadow::alpha(0,radius+1+menuShadow::extent,radius) || !menuShadow::alpha(0,radius+2,radius))
                    throw std::runtime_error("Shadow atlas lost transparent centre, fade or lower shadow");
                auto previous=menuShadow::alpha(0,radius+2,radius);
                for(double d=2.125;d<=1+menuShadow::extent;d+=.125){
                    const auto next=menuShadow::alpha(0,radius+d,radius);
                    if(next>previous || previous-next>2)throw std::runtime_error("Shadow profile contains contour bands");
                    previous=next;
                }
            }
            DiagnosticHint::selfTest();
            for(int themeIndex=0;themeIndex<4;++themeIndex) {
                Json palette=Json::array();
                for(const auto& color:defaultLabelColors)
                    palette.push_back({{"id",color.id},{"rgb",{11+themeIndex,31+themeIndex,51}},
                                       {"disabled_rgb",{41+themeIndex,61+themeIndex,81}}});
                if(!applyLabelColors(palette) || labelColors[1].color!=IM_COL32(11+themeIndex,31+themeIndex,51,255) ||
                   labelColors[1].disabled!=IM_COL32(41+themeIndex,61+themeIndex,81,255))
                    throw std::runtime_error("Host theme label colors lost normal/disabled state");
                palette[1]["rgb"][0]=256;
                if(applyLabelColors(palette) || labelColors[0].color!=defaultLabelColors[0].color)
                    throw std::runtime_error("Invalid host label colors partly applied");
                palette[1]["id"]=1;
                if(applyLabelColors(palette))throw std::runtime_error("Invalid label identity accepted");
            }
            applyLabelColors(Json());
            std::cout<<"Host label palette passed: four themes, normal/disabled, enum order and atomic invalid fallback\n";
            for(const char* name:{"light","light-gray","dark-gray","dark"}) {
                uiTheme=name;theme();PreviewPendingHint::selfTest(name);
            }
            SetWindowPos(window,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);
            SetCapture(window);
            if(GetCapture()!=window)throw std::runtime_error("Capture-close regression did not acquire test capture");
            ShowWindow(window,SW_HIDE);
            if(GetCapture()==window)throw std::runtime_error("Hidden menu retained mouse capture");
            if (!(io.ConfigFlags&ImGuiConfigFlags_NoMouseCursorChange))
                throw std::runtime_error("Menu renderer must preserve the Photoshop tool cursor");
            for (int kind=0;kind<5;++kind) for (int input=0;input<2;++input) {
                procedure(window,WM_APP+17,(kind==4?32:kind)|(input<<2)|16,73);
                if (panelKind!=kind || inputFocused!=bool(input) || !holdMode || contextMode || menuEpoch!=73)
                    throw std::runtime_error("Atomic pie preparation lost context/input/session");
            }
            // Start with a stale fully opaque surface, as on a repeated tap.
            // Each prepare/show must stay transparent until its new animation.
            for (int cycle=0;cycle<100;++cycle) {
                setOpacity(1.f);
                procedure(window,WM_APP+17,16,cycle);
                if (compositionOpacity!=0.f) throw std::runtime_error("Pie prepare exposed previous frame");
                procedure(window,WM_SHOWWINDOW,TRUE,0);
                if (compositionOpacity!=0.f) throw std::runtime_error("Pie show exposed previous frame");
                procedure(window,WM_SHOWWINDOW,FALSE,0);
                if (compositionOpacity!=0.f || !holdFinished)
                    throw std::runtime_error("Pie hide retained visible content");
            }
            procedure(window,WM_APP+17,8,74);
            if (holdMode || !contextMode || inputFocused || panelKind || menuEpoch!=74)
                throw std::runtime_error("Atomic regular preparation retained pie state");
            procedure(window,WM_APP+17,0,0);
            // Capabilities share the existing prepare message, keeping slots fixed.
            procedure(window,WM_APP+17,16|2|64|(1u<<8)|(1u<<12)|(1u<<16)|(1u<<17),75);
            if(!layerAware || layerKind!=1 || !rowEnabled(0) || rowEnabled(1) || rowEnabled(2) || rowEnabled(3) || !rowVisible(3))
                throw std::runtime_error("Background capabilities removed slots or enabled writes");
            auto disabledCount=testedActions.size();send("label.red");send("moveToTop");
            if(testedActions.size()!=disabledCount)throw std::runtime_error("Disabled layer action dispatched");
            procedure(window,WM_APP+17,16|2|64|(15u<<8)|(4u<<12)|(1u<<17),76);
            if(layerTitle()!="文字图层" || !rowEnabled(1) || !rowEnabled(3) || std::string(rowLabel(0))!="选择相邻图层")
                throw std::runtime_error("Text type/common capabilities lost");
            procedure(window,WM_APP+17,16|2|64,77);
            if(rowEnabled(0) || rowEnabled(1) || rowEnabled(2) || rowEnabled(3))
                throw std::runtime_error("Unknown layer metadata enabled actions");
            procedure(window,WM_APP+17,16|32|64,78);
            if(!rowEnabled(0))throw std::runtime_error("Layer metadata disabled Home New");
            procedure(window,WM_APP+17,0,0);
            if (pieAnimation(0)!=0 || pieAnimation(30)!=.5f || pieAnimation(60)!=1 || pieAnimation(120)!=1 || pieAnimation(0,true)!=1)
                throw std::runtime_error("Blender pie animation timing failed");
            const float savedScale=scale;const float padding=ImGui::GetStyle().WindowPadding.x;
            for (const char* name:{"light","light-gray","dark-gray","dark"}) {
                Json testSettings={{"enabled",true},{"shortcuts",Json::object()}};
                accept({{"type","status"},{"data",{{"photoshop",{{"ui_theme",name}}},{"settings",testSettings}}}});
                const auto& c=ImGui::GetStyle().Colors;
                auto luminance=[](const ImVec4& color) {
                    auto linear=[](float channel){return channel<=.04045f?channel/12.92f:powf((channel+.055f)/1.055f,2.4f);};
                    return .2126f*linear(color.x)+.7152f*linear(color.y)+.0722f*linear(color.z);
                };
                auto contrast=[&](const ImVec4& foreground,const ImVec4& background) {
                    float a=luminance(foreground),b=luminance(background);
                    return (std::max(a,b)+.05f)/(std::min(a,b)+.05f);
                };
                if(contrast(c[ImGuiCol_Text],c[ImGuiCol_Header])<4.5f ||
                   contrast(c[ImGuiCol_Text],c[ImGuiCol_HeaderActive])<4.5f ||
                   contrast(c[ImGuiCol_Text],c[ImGuiCol_ButtonActive])<4.5f ||
                   contrast(c[ImGuiCol_Text],c[ImGuiCol_WindowBg])<4.5f ||
                   contrast(c[ImGuiCol_Text],c[ImGuiCol_HeaderHovered])<4.5f ||
                   contrast(c[ImGuiCol_PlotLines],c[ImGuiCol_Button])<4.5f)
                    throw std::runtime_error("Theme preset text contrast failed");
                bool light=uiTheme=="light" || uiTheme=="light-gray";
                if (uiTheme!=name || (c[ImGuiCol_Text].x<c[ImGuiCol_WindowBg].x)!=light ||
                    scale!=savedScale || ImGui::GetStyle().WindowPadding.x!=padding)
                    throw std::runtime_error("Theme update lost contrast or accumulated DPI scale");
            }
            Json settings={{"enabled",true},{"shortcuts",Json::object()}};
            settings["shortcuts"]={{"select","capslock+a"},{"color","capslock+c"},{"moveToTop","capslock+w"},{"moveToBottom","capslock+s"}};
            accept({{"type","status"},{"data",{{"photoshop",{{"connected",true},{"document","test.psd"}}},{"settings",settings}}}});
            accept({{"type","status"},{"data",{{"photoshop",{{"busy",true}}},{"settings",settings}}}});
            accept({{"type","operation"},{"busy",false}});
            resetMenu();
            if (!canExecute()) throw std::runtime_error("Menu readiness lost after busy status");
            accept({{"type","operation"},{"busy",true},{"action","select"}});
            auto started=taskStarted;resetMenu();
            accept({{"type","status"},{"data",{{"photoshop",{{"busy",true}}},{"settings",settings}}}});
            { PreviewProgress progress;progress.update({{"stage","write"},{"done",12},{"total",32},{"frame",1}});
              for(int n=0;n<40;n++){float x=progress.advance(.016f);if(x<0 || x>12.f/32)throw std::runtime_error("Progress invented unverified work");}
              if(progress.displayed<.37f)throw std::runtime_error("Progress failed to approach evidence");
              progress.update({{"stage","verify"},{"done",0},{"total",32},{"frame",1}});
              if(progress.displayed!=0)throw std::runtime_error("Progress phase was not reset");
              progress.update({{"stage","restore"},{"done",0},{"total",0},{"frame",1}});
              if(progress.label().find("恢复")==std::string::npos)throw std::runtime_error("Missing restoration status");
              accept({{"type","preview.progress"},{"epoch",menuEpoch+1},{"stage","write"}});
              if(previewProgress.active)throw std::runtime_error("Stale menu progress accepted"); }
            if (canExecute() || taskStarted!=started || taskStatus().find("选择组内图层")==std::string::npos)
                throw std::runtime_error("Running task status lost on reopen/status refresh");
            accept({{"type","operation"},{"busy",true},{"queue_ready",true},{"queue_count",2}});
            if (canExecute())
                throw std::runtime_error("Busy actions accepted despite deterministic policy");
            accept({{"type","operation"},{"busy",true},{"queue_ready",false},{"queue_count",0}});
            if (canExecute()) throw std::runtime_error("Unknown target bypassed busy protection");
            accept({{"type","notice"},{"scope","operation"},{"ok",false},{"message","Operation timed out"}});
            if (taskStatus().find("执行中")==std::string::npos)
                throw std::runtime_error("Timeout must not imply COM task stopped");
            accept({{"type","operation"},{"busy",false}});resetMenu();
            if (taskStatus().find("失败")==std::string::npos || notice.empty())
                throw std::runtime_error("Failure status lost after reopening");
            accept({{"type","operation"},{"busy",true},{"action","select"}});
            accept({{"type","notice"},{"scope","operation"},{"ok",true},{"message","Completed"}});
            accept({{"type","operation"},{"busy",false}});resetMenu();
            if (!canExecute() || !taskStatus().empty() || !taskHasResult || taskFailed)
                throw std::runtime_error("Completion state failed");
            notice.clear();taskFailed=false;taskHasResult=false;
            accept({{"type","notice"},{"scope","interaction"},{"ok",false},{"message","Focus changed"}});
            if (taskStatus().find("失败")!=std::string::npos || taskHasResult)
                throw std::runtime_error("Menu interaction error must not become a task failure");
            accept({{"type","operation"},{"busy",true},{"action","label.gray"}});
            accept({{"type","notice"},{"scope","operation"},{"ok",false},{"message","Verification failed"}});
            accept({{"type","operation"},{"busy",false}});resetMenu();
            if (taskStatus().find("操作失败：灰色标签")==std::string::npos)
                throw std::runtime_error("Historical failure must identify its action");
            auto previousResult=taskStatus();
            accept({{"type","notice"},{"scope","interaction"},{"ok",false},{"message","Menu cancelled"}});
            if (taskStatus()!=previousResult) throw std::runtime_error("Interaction overwrote task result");
            accept({{"type","notice"},{"scope","operation"},{"ok",false},{"message","背景图层不支持颜色标签"}});
            if (taskStatus().find("请先解锁")==std::string::npos)
                throw std::runtime_error("Background rejection must have an actionable menu message");
            previousResult=taskStatus();
            accept({{"type","notice"},{"scope","interaction"},{"ok",false},{"message","Menu cancelled"}});
            if (taskStatus()!=previousResult) throw std::runtime_error("Background rejection overwritten by cancel");
            notice.clear();taskFailed=false;taskHasResult=false;
            panelKind=1;if (!rowVisible(0) || rowVisible(2)) throw std::runtime_error("Canvas panel routing failed");
            panelKind=3;if (rowVisible(0)) throw std::runtime_error("Text panel routing failed");
            panelKind=2;if (!rowVisible(3)) throw std::runtime_error("Layer panel routing failed");
            panelKind=2;
            {
                const auto p=menuSplitPreview("猫😀\r第二\r末","1,2,3");
                if(!p.error.empty() || p.breaks!=Json({4,7,8}) || p.count!=3 || p.text.find("猫😀")==std::string::npos)
                    throw std::runtime_error("Caps text split lost UTF-16 offsets");
                const auto blank=menuSplitPreview("A\r\rB\r","1,2,3,4");
                if(!blank.error.empty() || blank.count!=2 || blank.breaks!=Json({2,3,5}))throw std::runtime_error("Caps split blank-line preservation failed");
                for(const char* invalid:{"", "1,3", "1,2,", "2,3", "1-4", "1,1,2", "999999999999999999999"})
                    if(menuSplitPreview("a\rb\rc",invalid).error.empty())throw std::runtime_error("Invalid Caps split grouping accepted");
                const auto comma=menuSplitPreview("a\rb\rc","1-2，3");
                if(!comma.error.empty() || comma.breaks!=Json({4,5}) || comma.count!=2)throw std::runtime_error("Caps split merged lines failed");
                std::string many="a";for(int i=1;i<200;++i)many+="\ra";
                const auto bounded=menuSplitPreview(many,menuDefaultGroups(many));
                if(!bounded.error.empty() || bounded.count>64)throw std::runtime_error("Caps split default exceeded group limit");
                const auto nested=menuIndentLines("甲\n乙😀\n丙",4,15,15,1);
                if(!nested.error.empty() || nested.text!="甲\n\t乙😀\n\t丙" || nested.start!=5 || nested.end!=17)
                    throw std::runtime_error("List indentation lost selected UTF-8 lines or selection");
                const auto reduced=menuIndentLines(nested.text,nested.end,nested.start,nested.start,-1);
                if(reduced.text!="甲\n乙😀\n丙")throw std::runtime_error("Reverse list selection did not outdent the same lines");
                const auto boundary=menuIndentLines("a\nb\nc",0,4,4,1);
                if(boundary.text!="\ta\n\tb\nc")throw std::runtime_error("List indentation included the unselected next line");
                const auto maximum=menuIndentLines("a\n\t\t\t\t\tb",0,8,8,1);
                if(maximum.error.empty() || maximum.text!="a\n\t\t\t\t\tb")throw std::runtime_error("List depth limit partly changed selected text");
            }
            if (!matches(2,"CapsLock+w") || !matches(0,"组内") || !matches(1,"orange color") || matches(3,"top") || matches(0,"missing"))
                throw std::runtime_error("Search self-test failed");
        }
        if (previewLifecycle) {
            ShowWindow(window,SW_MINIMIZE);ShowWindow(window,SW_HIDE);
            for (int i=0;i<3;++i) {ShowWindow(window,SW_RESTORE);ShowWindow(window,SW_HIDE);}
        }
    if (!selfTest) {ShowWindow(window,preview.empty()?SW_SHOWNORMAL:SW_SHOWNOACTIVATE);UpdateWindow(window);}
        resetMenu();
        if (previewKind=="hold" || previewKind=="busy") {
            SetWindowPos(window,nullptr,testOrigin.x,testOrigin.y,static_cast<int>((!previewTool.empty()?(previewTool=="search"?432:632):previewShape?920:panelKind==4?820:620)*scale),static_cast<int>((!previewTool.empty()?(previewTool=="search"?304:780):previewShape?(previewShapeHeight>0?previewShapeHeight:menuShapeWindowHeight):500)*scale),SWP_NOZORDER|SWP_NOACTIVATE);
            setHoldMode(true);shapePanelOpen=previewShape && !previewOrdinary;
            if(!previewTool.empty()) {
                layerSearchReady=textSplitReady=true;holdFinished=false;pinned=true;capable=allowed=true;
                menuTools.receive(Json{{"type","menu.tools.opened"},{"epoch",menuEpoch},{"mode",previewTool}});
                if(previewTool=="split")menuTools.receive(Json{{"type","menu.tools.result"},{"epoch",menuEpoch},{"request",menuTools.request},{"action","text.read"},{"ok",true},
                    {"result",{{"name","标题与说明"},{"text","作品标题\r第一段说明文字。这是一段用于核对自动折行与中文显示的长文字，不需要先手动换行，修改后自动预览。Lorem ipsum dolor sit amet, consectetur adipiscing elit.\r第二段说明文字"},{"revision","preview"}}}});
                if(previewTool=="split"){
                    menuTools.textTab=previewTextTab;std::snprintf(menuTools.find,sizeof(menuTools.find),"文字");std::snprintf(menuTools.replacement,sizeof(menuTools.replacement),"内容");
                    if(previewTextTab==1){
                        Json matches=Json::array();std::string output=menuTools.sourceDisplay;size_t offset=0;
                        while((offset=output.find("文字",offset))!=std::string::npos){matches.push_back(Json::array({offset,offset+strlen("文字")}));output.replace(offset,strlen("文字"),"内容");offset+=strlen("内容");}
                        menuTools.textDirty=false;menuTools.submit("text.preview",menuTools.textParams());
                        menuTools.receive(Json{{"type","menu.tools.result"},{"epoch",menuEpoch},{"request",menuTools.request},{"action","text.preview"},{"ok",true},{"result",{{"plan_id","preview-only"},{"value",output},{"preview",output},{"kind","replace"},{"count",matches.size()},{"matches",matches}}}});
                    }
                    if(previewTextTab==4){
                        std::string output="• "+menuTools.sourceDisplay;size_t offset=0;
                        while((offset=output.find('\n',offset))!=std::string::npos){output.insert(++offset,"• ");offset+=strlen("• ");}
                        menuTools.textDirty=false;menuTools.submit("text.preview",menuTools.textParams());
                        menuTools.receive(Json{{"type","menu.tools.result"},{"epoch",menuEpoch},{"request",menuTools.request},{"action","text.preview"},{"ok",true},{"result",{{"plan_id","preview-only"},{"value",output},{"preview",output},{"kind","replace"},{"count",1},{"matches",Json::array()}}}});
                    }
                }
                else {menuTools.searchDirty=false;menuTools.submit("layers.search",Json::object());menuTools.receive(Json{{"type","menu.tools.result"},{"epoch",menuEpoch},{"request",menuTools.request},{"action","layers.search"},{"ok",true},
                    {"result",{{"search_id","preview"},{"scanned",24},{"items",Json::array({Json{{"name","产品标题"},{"type","text"},{"document_name","商品详情.psd"}},Json{{"name","产品说明"},{"type","smart-object"},{"document_name","商品详情.psd"}}})}}}});}
            }
            if (previewKind=="busy") accept({{"type","operation"},{"busy",true},{"action","select"}});
            if(!previewProgressStage.empty())accept({{"type","preview.progress"},{"epoch",menuEpoch},{"phase","preview"},{"stage",previewProgressStage},{"done",12},{"total",previewProgressStage=="redraw"?0:32},{"frame",2}});
        }
    }
    int frames=0;
    try {
        while (running) {
            // Wait BEFORE sampling input/building the next frame. Messages wake
            // this wait, so release/cancel/prepare do not queue behind v-sync.
            if (!selfTest && IsWindowVisible(window) && frameLatency && !frameGranted) {
                DWORD result=MsgWaitForMultipleObjectsEx(1,&frameLatency,100,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
                if (result==WAIT_FAILED) throw std::runtime_error("Frame wait failed");
                frameGranted=result==WAIT_OBJECT_0;
            }
            if(!pumpMessages())break;
            if ((selfTest || previewLifecycle) && frames==14) {
                RECT rect;GetWindowRect(window,&rect);
                SetWindowPos(window,nullptr,0,0,rect.right-rect.left+40,rect.bottom-rect.top+40,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
                SetWindowPos(window,nullptr,0,0,rect.right-rect.left,rect.bottom-rect.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
                if (previewLifecycle) {ShowWindow(window,SW_MINIMIZE);ShowWindow(window,SW_HIDE);ShowWindow(window,SW_RESTORE);}
            }
            float dpi=ImGui_ImplWin32_GetDpiScaleForHwnd(window);
            requireText(document);requireText(notice);requireText(taskName);requireText(search);
            if(gradientWindow){requireText(gradientPanel.title);requireText(gradientPanel.message);}
            for (const auto& text:shortcuts) requireText(text);
            if (selfTest && frames>=1 && frames<=3) {
                uiFontSize=frames==1?"large":frames==2?"medium":"small";
                if (frames==2) requireText("\xE9\xBE\x98"); // Rare document-name glyph U+9F98.
            }
            if (fabsf(dpi-scale)>.01f || fontDirty) {
                ImGui_ImplDX11_InvalidateDeviceObjects();scale=dpi;theme();loadFont();
                // Inject an early GPU creation failure into the native test.
                bool uploaded=!(selfTest && frames==2) && prepareRendererResources();
                if (!uploaded && !recoverRenderer())
                    throw std::runtime_error("Unable to restore renderer after font update");
            }
            if (io.FontDefault!=menuFont(MenuTextRole::Body)) {
                io.FontDefault=menuFont(MenuTextRole::Body);
                std::cerr<<"Font selection ready: "<<uiFontSize<<", cached\n";
            }
            if (selfTest && frames>=1 && frames<=3) {
                if (fontBuilds!=(frames==1?1u:2u))
                    throw std::runtime_error("Cached size switch unexpectedly rebuilt fonts");
                if (frames>=2) for (auto* cached:menuFonts)
                    if (!cached->GetFontBaked(cached->LegacySize)->IsGlyphLoaded(0x9F98))
                        throw std::runtime_error("Dynamic Chinese glyph missing from a cached size");
            }
            // Prepare pending fonts while hidden, on receipt of PS status.
            if(rendererDirty && !IsWindowVisible(window)) {
                if(!recoverRenderer())throw std::runtime_error("Unable to switch menu graphics device");
                rendererDirty=false;
                std::cout<<Json{{"type","graphics"},{"graphics",graphicsInfo()}}.dump()<<std::endl;
            }
            // Deferring this until ShowWindow blocked the first CapsLock frame.
            // Preserve an acquired slot if a hide arrived during the wait.
            // Dropping it without Present can strand the next reopen.
            if ((!IsWindowVisible(window) || gradientWindow && IsIconic(window)) && !selfTest) {WaitMessage();continue;}
            if (FAILED(device->GetDeviceRemovedReason()) || !target) {
                if (!recoverRenderer()) throw std::runtime_error("Unable to recover removed graphics device");
                continue;
            }
            if (!selfTest && frameLatency && !frameGranted) continue;
            frameGranted=false;
            ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();
            if (io.Fonts->Fonts.empty() || !io.Fonts->TexData || io.Fonts->TexData->Width<=0)
                throw std::runtime_error("Prepared CPU font data unavailable");
            if (selfTest) {
                io.AddFocusEvent(true);
                if (frames==4 || frames==5) io.AddKeyEvent(ImGuiKey_DownArrow,frames==4);
                if (frames==6 || frames==7) io.AddKeyEvent(ImGuiKey_Enter,frames==6);
                if (frames>=10 && frames<=12) {
                    io.AddMouseViewportEvent(ImGui::GetMainViewport()->ID);
                    io.AddMousePosEvent(bottomRow.x,bottomRow.y);
                    if (frames==10 || frames==11) io.AddMouseButtonEvent(ImGuiMouseButton_Left,frames==10);
                }
                if (frames==22 || frames==23) {
                    auto r=labelRows[1];
                    io.AddMouseViewportEvent(ImGui::GetMainViewport()->ID);
                    io.AddMousePosEvent((r.left+r.right)*.5f,(r.top+r.bottom)*.5f);
                    io.AddMouseButtonEvent(ImGuiMouseButton_Left,frames==22);
                }
            }
            if(!preview.empty() && previewShapeHover>=0 && previewShapeHover<shapeTipCount && frames>=2){
                const auto r=shapeTipRows[previewShapeHover];
                const ImVec2 point=previewShapeHover>=25 && previewShapeHover<shapeModeTipBase?perceptualPicker.points[previewShapeHover-25]:ImVec2{(r.left+r.right)*.5f,(r.top+r.bottom)*.5f};
                io.AddMouseViewportEvent(ImGui::GetMainViewport()->ID);io.AddMousePosEvent(point.x,point.y);io.AddMouseButtonEvent(ImGuiMouseButton_Left,false);
            }
            if(!preview.empty() && previewShape && frames>=2 && (previewExitEditor || previewShapeHover<0 && !previewOrdinary)){
                const auto r=shapeEditor.batchMask?shapeEditor.maskBounds:shapeEditor.bounds;
                io.AddMouseViewportEvent(ImGui::GetMainViewport()->ID);
                io.AddMousePosEvent(previewExitEditor?pieCenter.x:float(r.left+3*scale),previewExitEditor?pieCenter.y:float(r.top+3*scale));
            }
            if(!preview.empty() && previewOwningEditor && frames>=2)maskPreviewStarted=true;
            ImGui::NewFrame();if(gradientWindow)gradientPanel.draw();else drawMenu();ImGui::Render();
            lastVisibleVertices=ImGui::GetDrawData()->TotalVtxCount;
            ++renderedFrames;
            if (!target) throw std::runtime_error("DX11 render target lost after window resize");
            auto bg=ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
            const bool transparent=holdMode || gradientWindow;
            float clear[]={transparent?0.f:bg.x,transparent?0.f:bg.y,transparent?0.f:bg.z,transparent?0.f:1.f};context->OMSetRenderTargets(1,&target,nullptr);context->ClearRenderTargetView(target,clear);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            ++frames;
            if (selfTest && frames==20) {
                if (testedActions!=std::vector<std::string>{"color","moveToBottom"}) throw std::runtime_error("ImGui keyboard/click dispatch self-test failed: "+Json(testedActions).dump());
                SetWindowPos(window,nullptr,testOrigin.x,testOrigin.y,static_cast<int>(620*scale),static_cast<int>(440*scale),SWP_NOZORDER|SWP_NOACTIVATE);
                setHoldMode(true);
            }
            if (selfTest && frames==21) {
                holdOrigin={static_cast<LONG>(pieCenter.x),static_cast<LONG>(pieCenter.y)};
                auto point=[&](float x,float y){return POINT{static_cast<LONG>(pieCenter.x+x*scale),static_cast<LONG>(pieCenter.y+y*scale)};};
                if(footerBounds.y<=std::max(holdRows[1].bottom,holdRows[3].bottom) || footerBounds.w>ImGui::GetMainViewport()->Pos.y+ImGui::GetMainViewport()->Size.y ||
                   holdActionAt({LONG((footerBounds.x+footerBounds.z)*.5f),LONG((footerBounds.y+footerBounds.w)*.5f)})!=-1)
                    throw std::runtime_error("Footer overlaps actions, clips or commits a gesture");
                if(std::abs((holdRows[1].left+holdRows[1].right)*.5f-pieCenter.x)>scale || holdRows[1].top<=pieCenter.y+19*scale ||
                   holdRows[2].top!=holdRows[3].top || holdRows[2].right!=holdRows[3].left || holdRows[3].bottom>=pieCenter.y-19*scale ||
                   std::abs((holdRows[2].left+(panelKind==2 && capsReady && layerAware?capsRows[6].right:holdRows[3].right))*.5f-pieCenter.x)>scale)
                    throw std::runtime_error("Palette below ring; two distinct sort buttons aligned above");
                for (const auto& entry:std::array<std::pair<POINT,int>,11>{{{point(-110,0),0},{point(45,0),-1},{point(110,0),-1},{point(-50,-80),2},{point(50,-80),3},{point(0,-80),-1},{point(0,80),10},{point(0,0),-1},{point(20,10),-1},{point(150,150),-1},{{-100,-100},-1}}})
                    if (holdActionAt(entry.first)!=entry.second) throw std::runtime_error("Pie direction/dead-zone hit test failed");
                for (int i=0;i<4;++i) {
                    if (i==1) continue; // The palette commits only an explicit swatch.
                    POINT middle={(holdRows[i].left+holdRows[i].right)/2,(holdRows[i].top+holdRows[i].bottom)/2};
                    if (holdActionAt(middle)!=i) throw std::runtime_error("Pie card hit test failed");
                }
                holdOrigin=point(-110,0);holdMoved=false;
                if (holdActionAt(holdOrigin)!=-1) throw std::runtime_error("Clamped invocation must cancel");
                holdOrigin=point(0,0);
                if (holdActionAt(point(0,11))!=-1 || holdActionAt(point(0,20))!=-1)
                    throw std::runtime_error("Blender 12-unit centre threshold failed");
                panelKind=1;
                if (std::string(panelTitle())!="画布操作" || !rowVisible(0) || !rowVisible(1) || rowVisible(2) || rowVisible(3) ||
                    holdActionAt(point(-50,-80))!=-1 || holdActionAt(point(50,-80))!=-1 || holdActionAt(point(0,80))!=10)
                    throw std::runtime_error("Canvas pie leaked hidden layer-sort actions");
                panelKind=2;
                if (std::string(panelTitle())!="图层操作" || holdActionAt(point(-50,-80))!=2 || holdActionAt(point(50,-80))!=3 || holdActionAt(point(0,80))!=10)
                    throw std::runtime_error("Layer pie lost sorting actions");
                panelKind=3;
                if (rowVisible(0) || rowVisible(1) || holdActionAt(point(-110,0))!=-1)
                    throw std::runtime_error("Input context exposed pie actions");
                panelKind=4;
                const bool previousAllowed=allowed,previousCapable=capable;
                const std::string previousDocument=document;
                // A real native dialog rejects COM reads. Returning to a
                // verified Home must work on the first frame, without waiting
                // for the next 1-second status read / 3-second broadcast.
                Json homeSettings={{"enabled",true},{"shortcuts",Json::object()}};
                accept({{"type","status"},{"data",{{"photoshop",{{"connected",false},{"document",nullptr}}},{"settings",homeSettings}}}});
                if (!canExecute()) throw std::runtime_error("Home New entry waits for COM reconnect");
                panelKind=0;
                if (canExecute()) throw std::runtime_error("Disconnected layer operations became executable");
                panelKind=4;inputFocused=true;
                if (canExecute()) throw std::runtime_error("Home input protection lost");
                inputFocused=false;homeSettings["enabled"]=false;
                accept({{"type","status"},{"data",{{"photoshop",Json::object()},{"settings",homeSettings}}}});
                if (canExecute()) throw std::runtime_error("Disabled Home entry became executable");
                homepageCapable=true;allowed=previousAllowed;capable=previousCapable;document=previousDocument;
                if (!rowVisible(0) || rowVisible(1) || rowVisible(2) || rowVisible(3) ||
                    std::string(rowAction(0))!="document.new" || !matches(0,"new document") ||
                    holdActionAt(point(0,-80))!=-1 || holdActionAt(point(68,0))!=-1)
                    throw std::runtime_error("Home menu leaked layer actions");
                auto homeActionCount=testedActions.size();
                finishHold(point(-110,0));
                if (testedActions.size()!=homeActionCount+1 || testedActions.back()!="document.new")
                    throw std::runtime_error("Home release did not request the New dialog");
                testedActions.pop_back();holdFinished=false;pieReady=true;
                shownPresets={{"test-preset","剪贴板","690 × 929 px","72 ppi · RGB"}};
                recentReady=true;
                for(int i=1;i<9;++i)shownPresets.push_back({"test-"+std::to_string(i),"自定",i%2?"181.42 × 181.42 px":"64 × 32 px","72 ppi · RGB"});
                // Clipboard above the ring, eight presets in two columns and four rows.
                const auto previousLabelRows=labelRows;
                const auto previousHoldRows=holdRows;
                const auto previousPieCenter=pieCenter;
                finishPlatformFrame();
                SetWindowPos(window,nullptr,testOrigin.x,testOrigin.y,static_cast<int>(820*scale),static_cast<int>(440*scale),SWP_NOZORDER|SWP_NOACTIVATE);
                ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();drawMenu();ImGui::Render();
                if(recentRows.size()!=9)throw std::runtime_error("Recent preset grid count failed");
                HoverTipState timed;
                if(timed.update(13,12,1000,false,true)!=-1 || timed.update(14,12,1080,false,true)!=-1 ||
                    timed.update(14,12,1229,false,true)!=-1 || timed.update(14,12,1230,false,true)!=14)
                    throw std::runtime_error("Tooltip initial hover must settle before showing");
                if(timed.update(-1,-1,1260,true,true)!=14 || timed.update(15,12,1270,false,true)!=14 ||
                    timed.update(15,12,1329,false,true)!=14 || timed.update(15,12,1330,false,true)!=15)
                    throw std::runtime_error("Tooltip gap / sibling switch flickered");
                if(timed.update(-1,-1,1421,true,true)!=-1 || timed.update(15,12,1500,false,true)!=-1 ||
                    timed.update(15,12,1650,false,true)!=15 || timed.update(-1,-1,1651,false,true)!=-1)
                    throw std::runtime_error("Tooltip gap expiry / centre exit did not hide");
                timed.update(13,12,2000,false,true);timed.update(13,12,2150,false,true);
                if(timed.update(32,32,2151,false,true)!=-1 || timed.update(32,32,2301,false,true)!=32 ||
                    timed.update(32,32,2302,false,false)!=-1)
                    throw std::runtime_error("Tooltip crossed groups or survived input/task disable");
                if(tipFamily(recentActionBase)==tipFamily(recentActionBase+1))throw std::runtime_error("Clipboard tooltip shares the preset family");
                TipVisual morph;
                const ImVec4 shortBox={100,100,200,130},longBox={0,100,300,130};
                morph.update("short",shortBox,32,3000);
                morph.update("long",longBox,32,3010);
                if(morph.bounds.x!=shortBox.x || morph.bounds.z!=shortBox.z || morph.blend!=0)
                    throw std::runtime_error("Tooltip width jumped on retarget");
                morph.sample(3070);
                if(morph.bounds.z<=shortBox.z || morph.bounds.z>=longBox.z || morph.blend<=0 || morph.blend>=1)
                    throw std::runtime_error("Tooltip width / text did not ease continuously");
                const auto interrupted=morph.bounds;
                morph.update("short",shortBox,32,3070);
                if(morph.bounds.x!=interrupted.x || morph.bounds.z!=interrupted.z)
                    throw std::runtime_error("Rapid tooltip reversal jumped");
                morph.sample(3190);
                if(morph.bounds.x!=shortBox.x || morph.bounds.z!=shortBox.z || !morph.previous.empty())
                    throw std::runtime_error("Tooltip did not settle to exact padded width");
                const auto stableTip=actionTipBounds(recentActionBase+1);
                for(int action=recentActionBase+2;action<recentActionBase+9;++action) {
                    const auto bounds=actionTipBounds(action);
                    if(std::abs((bounds.x+bounds.z)-(stableTip.x+stableTip.z))>scale || bounds.y!=stableTip.y)
                        throw std::runtime_error("Sibling preset tooltip anchor changed");
                }

                auto gridWindow=ImGui::GetMainViewport();
                if(std::abs((recentRows[1].top+recentRows[1].bottom)*.5f-pieCenter.y)>scale)
                    throw std::runtime_error("First preset row must align to ring, not centre the panel");
                if(footerBounds.z>=recentRows[1].left || footerBounds.w>gridWindow->Pos.y+gridWindow->Size.y)
                    throw std::runtime_error("Home footer overlaps presets or clips");
                for(size_t i=0;i<recentRows.size();++i) {
                    const auto r=recentRows[i];POINT middle={(r.left+r.right)/2,(r.top+r.bottom)/2};
                    if(r.right<=r.left || r.bottom<=r.top || r.left<gridWindow->Pos.x || r.top<gridWindow->Pos.y ||
                       r.right>gridWindow->Pos.x+gridWindow->Size.x || r.bottom>gridWindow->Pos.y+gridWindow->Size.y ||
                       holdActionAt(middle)!=recentActionBase+int(i))throw std::runtime_error("Recent grid button clipped or unreachable");
                    if(i>1 && (i-1)%2 && (r.top!=recentRows[i-1].top || r.left<=recentRows[i-1].right))
                        throw std::runtime_error("Recent grid column order failed");
                    if(i>=3 && r.top<=recentRows[i-2].bottom)throw std::runtime_error("Recent grid row order failed");
                    if(i==0 && (r.bottom>recentRows[1].top-32*scale || std::abs(middle.x-pieCenter.x)>scale))
                        throw std::runtime_error("Clipboard was not centered above the menu");
                }
                if(recentRows[1].right-recentRows[1].left<=recentRows[2].right-recentRows[2].left ||
                    recentRows[0].right-recentRows[0].left>=recentRows[1].right-recentRows[1].left)
                    throw std::runtime_error("Preset columns / clipboard did not fit their own content");
                if(recentPanelBounds.bottom-recentPanelBounds.top>=224*scale)
                    throw std::runtime_error("Preset panel retained the old title / excess row spacing");
                if(menuFont(MenuTextRole::PresetDimension)->LegacySize<=menuFont(MenuTextRole::PresetTag)->LegacySize)
                    throw std::runtime_error("Preset dimensions must lead the metadata badges");
                for(int action=recentActionBase;action<recentActionBase+9;++action) {
                    const auto tip=actionTipBounds(action);
                    if(tip.z<=tip.x || holdActionAt({LONG((tip.x+tip.z)*.5f),LONG((tip.y+tip.w)*.5f)})!=-1)
                        throw std::runtime_error("Preset tooltip missing or has a gesture hit");
                    for(auto r:recentRows)if(tipOverlaps(tip,r))throw std::runtime_error("Preset tooltip overlaps a card");
                    if(action>recentActionBase && tip.w>recentPanelBounds.top)throw std::runtime_error("Preset tooltip must prefer above the grid");
                }
                auto recentRect=recentRows[0];POINT recentPoint={(recentRect.left+recentRect.right)/2,(recentRect.top+recentRect.bottom)/2};
                shownPresets[0].available=false;
                if(holdActionAt(recentPoint)!=-1)throw std::runtime_error("Unavailable clipboard preset became selectable");
                auto redrawPresets=[&] {
                    finishPlatformFrame();ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();
                    ImGui::NewFrame();drawMenu();ImGui::Render();
                };
                redrawPresets();
                if(!IsRectEmpty(&recentRows[0]) || holdActionAt(recentPoint)!=-1)
                    throw std::runtime_error("Unavailable clipboard still drawn or selectable");
                for(size_t i=1;i<recentRows.size();++i) {
                    const auto r=recentRows[i];POINT middle={(r.left+r.right)/2,(r.top+r.bottom)/2};
                    if(holdActionAt(middle)!=recentActionBase+int(i))throw std::runtime_error("Clipboard visibility moved recent actions");
                }
                shownPresets[0].available=true;redrawPresets();
                POINT neutral={LONG(pieCenter.x),LONG(pieCenter.y)};
                holdFinished=false;pieReady=true;renderedHit=recentActionBase;
                finishHold(recentPoint,true,&neutral);
                if(testedActions.back()!="hide")throw std::runtime_error("Recent preset exit committed");
                testedActions.pop_back();holdFinished=false;pieReady=true;renderedHit=recentActionBase;
                finishHold(recentPoint,true,&recentPoint);
                if(testedActions.back()!="document.recent.test-preset")throw std::runtime_error("Recent preset stable release failed");
                auto submitted=testedActions.size();finishHold(recentPoint,true,&recentPoint);
                if(testedActions.size()!=submitted)throw std::runtime_error("Recent preset release replayed");
                testedActions.pop_back();shownPresets.clear();recentRows.clear();recentReady=false;
                labelRows=previousLabelRows;holdRows=previousHoldRows;
                pieCenter=previousPieCenter;
                SetWindowPos(window,nullptr,testOrigin.x,testOrigin.y,static_cast<int>(620*scale),static_cast<int>(440*scale),SWP_NOZORDER|SWP_NOACTIVATE);
                holdFinished=false;pieReady=true;renderedHit=-1;
                panelKind=2;
                for(const auto& host: {"24.0.1","25.0","25.1"})
                    if(supportedLabelCount(Json{{"photoshop_version",host},{"uxp_version","0.3.79"},{"uxp_ready",true},{"extended_label_colors",true}})!=8)
                        throw std::runtime_error("Old host enabled extended labels");
                for(const auto& host: {"25.2","25.10","26.0","27.0"}) {
                    Json state={{"photoshop_version",host},{"uxp_version","0.3.79"},{"uxp_ready",true},{"extended_label_colors",true}};
                    if(supportedLabelCount(state)!=12)throw std::runtime_error("Confirmed label capability hidden");
                    state.erase("extended_label_colors");if(supportedLabelCount(state)!=8)throw std::runtime_error("Missing label capability enabled controls");
                    state["extended_label_colors"]=false;if(supportedLabelCount(state)!=8)throw std::runtime_error("Rejected label capability enabled controls");
                    state["extended_label_colors"]=true;state["uxp_version"]="0.3.78";
                    if(supportedLabelCount(state)!=8)throw std::runtime_error("Old executor enabled extended labels");
                }
                {
                    const bool oldCaps=capsReady,oldAware=layerAware;capsReady=layerAware=true;
                    labelCount=8;redrawPresets();const auto legacy=holdRows[1],bar=capsRows[0];
                    labelCount=12;redrawPresets();
                    if(holdRows[1].left!=legacy.left || holdRows[1].right!=legacy.right || holdRows[1].top!=legacy.top ||
                       labelRows[8].left!=labelRows[0].left || labelRows[8].top!=labelRows[4].bottom || labelRows[11].right!=labelRows[3].right ||
                       capsRows[0].top<=bar.top || capsRows[0].top<holdRows[1].bottom+menuControlPadding*scale-1 || footerBounds.y<=capsRows[0].bottom ||
                       holdRows[1].right+menuControlPadding*scale>quickRows[0].left)
                        throw std::runtime_error("Three-row palette overlaps controls or moves its existing anchor");
                    capsReady=oldCaps;layerAware=oldAware;
                }
                labelCount=12;redrawPresets();pinned=true;
                for(int i=0;i<labelCount;++i) {
                    holdFinished=false;pieReady=true;
                    const auto r=labelRows[i];POINT middle={(r.left+r.right)/2,(r.top+r.bottom)/2};
                    if(holdActionAt(middle)!=4+i || r.right<=r.left || r.bottom<=r.top || r.left<holdRows[1].left || r.right>holdRows[1].right || r.bottom>holdRows[1].bottom)
                        throw std::runtime_error("Modern label swatch not visible/hittable inside palette: index="+std::to_string(i)+", hit="+std::to_string(holdActionAt(middle)));
                    holdFinished=false;pieReady=true;commitLabel(i);
                    if(testedActions.back()!=std::string("label.")+labelColors[i].id)throw std::runtime_error("Modern label enum dispatch mismatch");
                    testedActions.pop_back();
                }
                labelCount=8;holdFinished=false;pinned=false;redrawPresets();
                for (int i=0;i<8;++i) {
                    auto r=labelRows[i];POINT middle={(r.left+r.right)/2,(r.top+r.bottom)/2};
                    if (holdActionAt(middle)!=4+i) throw std::runtime_error("Label swatch hit test failed");
                }
                for (int origin=0;origin<8;++origin) {
                    auto r=labelRows[origin];
                    holdOrigin={(r.left+r.right)/2,(r.top+r.bottom)/2};holdMoved=false;
                    if (holdActionAt(holdOrigin)!=-1)
                        throw std::runtime_error("Unmoved clamped press must cancel");
                    holdActionAt(point(0,0)); // Deliberately leave the invocation area.
                    for (int i=0;i<8;++i) {
                        auto cell=labelRows[i];
                        for (LONG y=cell.top;y<cell.bottom;++y)
                            for (LONG x=cell.left;x<cell.right;++x)
                                if (holdActionAt({x,y})!=4+i)
                                    throw std::runtime_error("Invocation dead zone masks a swatch pixel");
                    }
                    holdMoved=false;pinned=true;
                    if (holdActionAt(holdOrigin)!=4+origin)
                        throw std::runtime_error("Pinned swatch must ignore invocation dead zone");
                    pinned=false;
                }
                holdOrigin=point(0,0);holdMoved=false;
                POINT rowPoint=point(50,-80);
                pinned=true;auto beforePin=testedActions.size();finishHold(rowPoint);
                if (testedActions.size()!=beforePin || holdFinished)
                    throw std::runtime_error("Pinned menu must ignore key release");
                pinned=false;
                pieAnimating=true;
                auto animLabel=labelRows[1];
                if (holdActionAt({(animLabel.left+animLabel.right)/2,(animLabel.top+animLabel.bottom)/2})!=-1 ||
                    holdActionAt(point(-110,0))!=-1)
                    throw std::runtime_error("Animation must cancel card and palette releases");
                pieAnimating=false;
                allowed=false;
                if (holdActionAt(rowPoint)!=-1) throw std::runtime_error("Disabled hold action executed");
                allowed=true;finishHold(rowPoint);finishHold(rowPoint);
                if (testedActions.back()!="moveToBottom" || testedActions.size()!=3) throw std::runtime_error("Hold release must execute exactly once");
                holdFinished=false;pieReady=true;finishHold(point(0,0));
                if (testedActions.back()!="hide") throw std::runtime_error("Empty hold release must cancel");
                holdFinished=false;pieReady=false;finishHold(rowPoint);
                if (testedActions.back()!="hide") throw std::runtime_error("Release before first render must cancel");
                for (int i=0;i<8;++i) {
                    holdFinished=false;pieReady=true;auto r=labelRows[i];
                    POINT middle={(r.left+r.right)/2,(r.top+r.bottom)/2};
                    auto before=testedActions.size();finishHold(middle);finishHold(middle);
                    if (testedActions.size()!=before+1 || testedActions.back()!=std::string("label.")+labelColors[i].id)
                        throw std::runtime_error("Label release did not dispatch exactly once");
                    holdFinished=false;pieReady=true;before=testedActions.size();commitLabel(i);finishHold(middle);
                    if (testedActions.size()!=before+1) throw std::runtime_error("Click plus release dispatched twice");
                }
                auto swatch=labelRows[1];POINT swatchPoint={(swatch.left+swatch.right)/2,(swatch.top+swatch.bottom)/2};
                // Production releases use a frozen key-up point and require
                // the same candidate to have appeared in a submitted frame.
                holdFinished=false;pieReady=true;renderedHit=5;
                auto blankPoint=point(0,0);
                finishHold(blankPoint,true,&blankPoint);
                if (testedActions.back()!="hide") throw std::runtime_error("Blank release selected a later swatch");
                holdFinished=false;pieReady=true;renderedHit=-1;
                finishHold(swatchPoint,true,&swatchPoint);
                if (testedActions.back()!="hide") throw std::runtime_error("Unhighlighted release executed");
                holdFinished=false;pieReady=true;renderedHit=4;
                finishHold(swatchPoint,true,&swatchPoint);
                if (testedActions.back()!="hide") throw std::runtime_error("Different highlighted swatch executed");
                holdFinished=false;pieReady=true;renderedHit=5;
                finishHold(swatchPoint,true,&swatchPoint);
                if (testedActions.back()!="label.red") throw std::runtime_error("Highlighted frozen swatch failed");
                for (int i=0;i<8;++i) {
                    auto cell=labelRows[i];POINT from={(cell.left+cell.right)/2,(cell.top+cell.bottom)/2};
                    auto next=labelRows[(i+1)%8];POINT other={(next.left+next.right)/2,(next.top+next.bottom)/2};
                    for (POINT now:{blankPoint,other}) {
                        holdFinished=false;pieReady=true;renderedHit=4+i;
                        finishHold(from,true,&now);
                        if (testedActions.back()!="hide") throw std::runtime_error("Fast swatch exit committed");
                    }
                    holdFinished=false;pieReady=true;renderedHit=4+i;
                    finishHold(blankPoint,true,&from);
                    if (testedActions.back()!="hide") throw std::runtime_error("Late cursor selected a new swatch");
                    holdFinished=false;pieReady=true;renderedHit=4+i;
                    finishHold(from,true,&from);auto before=testedActions.size();finishHold(from,true,&from);
                    if (testedActions.back()!=std::string("label.")+labelColors[i].id || testedActions.size()!=before)
                        throw std::runtime_error("Stable release did not commit exactly once");
                }
                labelTestActionCount=testedActions.size();holdFinished=false;pieReady=true;
            }
            if (selfTest && frames==25) {
                if (testedActions.size()!=labelTestActionCount+1 || testedActions.back()!="label.red")
                    throw std::runtime_error("ImGui swatch click did not dispatch immediately");
                auto r=labelRows[1];finishHold({(r.left+r.right)/2,(r.top+r.bottom)/2});
                if (testedActions.size()!=labelTestActionCount+1) throw std::runtime_error("Swatch click repeated on CapsLock release");
                auto composed=GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_NOREDIRECTIONBITMAP;
                procedure(window,WM_SHOWWINDOW,FALSE,0);
                if (!composed || !(GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_NOREDIRECTIONBITMAP) || !holdMode || pieReady)
                    throw std::runtime_error("Hiding a pie must keep transparency and cancel hit testing");
                setHoldMode(false);
                if (!(GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_NOREDIRECTIONBITMAP) || compositionOpacity!=1.f)
                    throw std::runtime_error("Regular menu must retain compositor and restore opacity");
                ShowWindow(window,SW_HIDE);
                finishPlatformFrame();
                for (int iteration=0;iteration<100;++iteration) {
                    ImGui::NewFrame();drawMenu();ImGui::Render();finishPlatformFrame();
                    if (ImGui::GetCurrentContext()->FrameCountPlatformEnded!=ImGui::GetFrameCount())
                        throw std::runtime_error("Hidden frame skipped platform lifecycle");
                }
                setHoldMode(true);panelKind=2;allowed=true;
                auto numberFrame=[&](ImGuiKey key,bool down) {
                    // Synthetic input frames need a fixed cadence, not the last
                    // platform delta (which can include atlas rebuilds).
                    io.DeltaTime=1.f/60.f;
                    io.AddKeyEvent(key,down);
                    ImGui::NewFrame();drawMenu();ImGui::Render();finishPlatformFrame();
                };
                {
                    for(const auto& example:{std::pair{RECT{0,0,1920,1040},96u},std::pair{RECT{-1360,0,0,1040},192u}}){
                        const auto work=example.first,fit=gradientWindowBounds(work,example.second);
                        const auto minimum=gradientMinimum(example.second);
                        if(fit.left<=work.left || fit.top<=work.top || fit.right>=work.right || fit.bottom>=work.bottom ||
                           fit.right-fit.left<minimum.cx || fit.bottom-fit.top!=minimum.cy)
                            throw std::runtime_error("The gradient window did not fit inside the monitor work area");
                        const RECT client={0,0,minimum.cx,minimum.cy},drag={0,0,minimum.cx,30};
                        if(gradientFrameHit({1,1},client,4,drag)!=HTTOPLEFT ||
                           gradientFrameHit({minimum.cx-1,minimum.cy-1},client,4,drag)!=HTBOTTOMRIGHT ||
                           gradientFrameHit({minimum.cx/2,10},client,4,drag)!=HTCAPTION ||
                           gradientFrameHit({minimum.cx/2,60},client,4,drag)!=HTCLIENT)
                            throw std::runtime_error("Gradient resize edges overlapped the move or control areas");
                    }
                    const auto compactWork=gradientWindowBounds({-1000,0,0,840},192);
                    const auto minimum=gradientMinimum(192);
                    if(compactWork.left!=-1000 || compactWork.top!=0 || compactWork.right-compactWork.left!=minimum.cx ||
                       compactWork.bottom-compactWork.top!=minimum.cy)
                        throw std::runtime_error("A small work area squeezed the gradient below its usable minimum");
                    GradientPanel panel;
                    panel.open(Json::parse(R"({"phase":"editing","draft":{"fill":{"kind":"gradient","stops":[{"location":0,"rgb":[0,0,0]},{"location":4096,"rgb":[200,100,50]}],"opacity_stops":[{"location":0,"opacity":100},{"location":4096,"opacity":50}]}}})"));
                    const auto& colors=panel.track(1);const auto& opacity=panel.track(0);
                    for(int p=0;p<=GradientPanel::end;++p){
                        const auto expected=GradientPanel::swatch(GradientPanel::sample(colors,1,p),0,
                            std::lround(GradientPanel::sample(opacity,0,p).at("opacity").get<float>()*2.55f));
                        if(GradientPanel::rampColor(colors,opacity,p)!=expected)
                            throw std::runtime_error("Allocation-free gradient preview changed the sampled colors");
                    }
                    if(GradientPanel::rulerStep(4096,560,48)!=10 || GradientPanel::rulerStep(4096,220,48)!=25 ||
                       GradientPanel::rulerStep(1024,560,48)!=5 || GradientPanel::angleAt({0,0},{5,-3},true)!=45)
                        throw std::runtime_error("Gradient ruler spacing or direction snapping was invalid");
                    enum {tracks,picker,angle,viewport,type} part=tracks;
                    ImVec2 from{},to{};float radialX=0;
                    std::vector<ImDrawVert> overviewVertices;
                    auto frame=[&](bool focus=false){
                        ImGui::NewFrame();const auto origin=ImGui::GetMainViewport()->Pos;
                        ImGui::SetNextWindowPos(origin);ImGui::SetNextWindowSize({340*scale,220*scale});
                        if(focus)ImGui::SetNextWindowFocus();
                        ImGui::Begin("##test-gradient",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings);
                        {
                            MenuControls::PanelStyle style(scale);
                            switch(part){
                                case tracks:panel.timeline(scale);break;
                                case picker:panel.colorPicker(menuPanelWidth*scale);break;
                                case angle:panel.direction();break;
                                case viewport:{
                                    const auto& vertices=ImGui::GetWindowDrawList()->VtxBuffer;
                                    const int first=vertices.Size;
                                    panel.overview();
                                    overviewVertices.assign(vertices.begin()+first,vertices.end());
                                    break;
                                }
                                case type:panel.types();radialX=1.5f*(ImGui::CalcTextSize("径向").x+2*menuControlTextInset*scale);break;
                            }
                            from=ImGui::GetItemRectMin();to=ImGui::GetItemRectMax();
                        }
                        ImGui::End();ImGui::Render();finishPlatformFrame();
                    };
                    frame(true);frame();
                    const ImVec2 middle={(from.x+to.x)*.5f,from.y+3.5f*menuControlHeight*scale};
                    io.AddMouseViewportEvent(ImGui::GetMainViewport()->ID);io.AddMousePosEvent(middle.x,middle.y);frame();
                    io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();
                    const auto& inserted=panel.track(1).at(1);
                    if(panel.track(1).size()!=3 || std::abs(inserted.at("location").get<int>()-2048)>1 || inserted.at("rgb")!=Json({100,50,25}))
                        throw std::runtime_error("A single native timeline click did not insert the sampled color stop");
                    const int insertedPosition=inserted.at("location");
                    panel.select(0,0,true,false);const auto prior=panel.paint();const int shift=panel.shift(prior,512);panel.moveBy(prior,shift);
                    if(shift!=512 || panel.track(0).at(0).at("location")!=512 || panel.track(1).at(1).at("location")!=insertedPosition+512)
                        throw std::runtime_error("Grouped gradient movement lost relative spacing");
                    const auto moved=panel.draft;panel.zoom(2,.25f);
                    if(panel.start!=512 || panel.span!=2048 || moved!=panel.draft)
                        throw std::runtime_error("Timeline zoom changed Photoshop draft values");
                    part=picker;panel.select(1,2,true,false);frame(true);frame();
                    if(panel.picker.last!=PerceptualPickerState::color(panel.track(1).at(2).at("rgb")) || panel.draft!=moved)
                        throw std::runtime_error("The shared gradient picker did not follow the clicked color stop");
                    const RECT square=panel.picker.squareBounds;
                    io.AddMousePosEvent(square.left+(square.right-square.left)*.75f,square.top+(square.bottom-square.top)*.25f);frame();
                    io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();
                    if(panel.track(1)[1]["rgb"]==moved["fill"]["stops"][1]["rgb"] || panel.track(1)[1]["rgb"]!=panel.track(1)[2]["rgb"] ||
                       panel.track(1)[0]!=moved["fill"]["stops"][0] || panel.track(0)!=moved["fill"]["opacity_stops"])
                        throw std::runtime_error("The shared picker did not edit only selected gradient colors");
                    const auto recolored=panel.draft;panel.selection[1]=0;frame();
                    io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();
                    if(panel.draft!=recolored)throw std::runtime_error("Unselected gradient colors were editable");
                    auto pressAt=[&](float x,float y){
                        io.AddMousePosEvent(x,y);frame();io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();
                    };
                    panel.paint()["options"]={{"style","linear"},{"angle",0}};part=type;frame(true);frame();
                    pressAt(from.x+radialX,(from.y+to.y)*.5f);
                    if(panel.paint()["options"]["style"]!="radial")throw std::runtime_error("Gradient type segment selected the wrong style");
                    part=angle;frame();
                    pressAt((from.x+to.x)*.5f,from.y+(to.y-from.y)*.25f);
                    if(panel.paint()["options"]["angle"]!=90)throw std::runtime_error("Gradient direction click failed to write the angle");
                    panel.track(1)[0]["rgb"]={19,123,57};
                    panel.track(1)[1]["rgb"]={216,31,92};
                    panel.track(1)[2]["rgb"]={73,114,224};
                    part=viewport;panel.start=0;panel.span=2048;const auto beforePan=panel.draft;frame();
                    auto checkOverview=[&]{
                        const float inset=menuGradientHandleSize*scale,width=to.x-from.x-2*inset;
                        for(const auto& stop:panel.track(1)){
                            const auto color=GradientPanel::swatch(stop,0);
                            float left=FLT_MAX,right=-FLT_MAX;
                            for(const auto& vertex:overviewVertices)if(vertex.col==color){
                                left=std::min(left,vertex.pos.x);right=std::max(right,vertex.pos.x);
                            }
                            const float expected=from.x+inset+stop.at("location").get<int>()*width/GradientPanel::end;
                            if(left>right || std::abs((left+right)*.5f-expected)>.5f*scale)
                                throw std::runtime_error("Gradient overview markers did not show the full-range color stop positions");
                        }
                    };
                    checkOverview();
                    pressAt(from.x+(to.x-from.x)*.8f,(from.y+to.y)*.5f);
                    if(panel.start!=2048 || panel.draft!=beforePan)throw std::runtime_error("Timeline overview did not pan independently of the gradient");
                    panel.zoom(2,.5f);frame();checkOverview();
                    std::cout<<"Gradient overview color stops passed before and after pan and zoom\n";
                    std::cout<<"Gradient type, direction and adaptive timeline passed\n";
                    std::cout<<"Native gradient shared color picker passed\n";
                    std::cout<<"Native gradient click, group movement and zoom passed\n";
                    io.AddMousePosEvent(-1000,-1000);
                }
                {
                    RECT before{};GetWindowRect(window,&before);
                    const auto minimum=gradientMinimum(UINT(std::lround(scale*96)));
                    SetWindowPos(window,nullptr,0,0,minimum.cx,minimum.cy,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
                    const auto previousFont=uiFontSize;
                    const auto gradient=Json::parse(R"({"kind":"gradient","method":"perceptual","stops":[{"location":0,"midpoint":30,"rgb":[24,60,92]},{"location":4096,"midpoint":50,"rgb":[255,255,255]}],"opacity_stops":[{"location":0,"midpoint":40,"opacity":90},{"location":4096,"midpoint":50,"opacity":100}],"options":{"style":"linear","angle":90,"scale":100,"smoothness":4096,"reverse":false,"dither":true,"align":true}})");
                    for(const char* font:{"small","medium","large"}){
                        uiFontSize=font;
                        for(const char* model:{"rgb","cmyk","lab","gray"}){
                            GradientPanel layout;auto paint=gradient;
                            if(std::string(model)!="rgb")for(auto& stop:paint["stops"]){
                                stop.erase("rgb");
                                stop["native"]={{"model",model},{"values",std::string(model)=="cmyk"?Json({1,2,3,4}):std::string(model)=="lab"?Json({50,0,0}):Json({40})}};
                            }
                            layout.open({{"phase","editing"},{"title",std::string(350,'W')},{"message",std::string(550,'W')+"\n完整错误"},
                                {"draft",{{"fill",paint},{"stroke",paint}}}});
                            layout.span=512;layout.start=768;
                            for(const bool selected:{false,true}){
                                layout.selection={selected?3u:0u,selected?3u:0u};
                                for(int n=0;n<2;++n){
                                    ImGui_ImplWin32_NewFrame();ImGui::NewFrame();layout.draw(true);ImGui::Render();finishPlatformFrame();
                                }
                            }
                        }
                    }
                    GradientPanel closable;
                    closable.open({{"phase","editing"},{"title","渐变关闭测试"},{"draft",{{"fill",gradient},{"stroke",gradient}}}});
                    const auto original=closable.draft;
                    auto closeFrame=[&]{
                        ImGui::NewFrame();ImGui::SetNextWindowFocus();closable.draw(true);ImGui::Render();finishPlatformFrame();
                    };
                    closeFrame();
                    const auto origin=ImGui::GetMainViewport()->Pos;
                    const POINT click={closable.dragArea.right+LONG(menuControlHeight*.5f*scale),LONG((menuControlPadding+menuControlHeight*.5f)*scale)};
                    const LONG edge=std::lround(menuCompactGap*scale);
                    const RECT client={0,0,minimum.cx,minimum.cy};
                    if(gradientFrameHit(click,client,edge,closable.dragArea)!=HTCLIENT ||
                        gradientFrameHit({click.x-LONG(menuControlHeight*scale),click.y},client,edge,closable.dragArea)!=HTCAPTION)
                        throw std::runtime_error("The gradient close button did not leave the document header draggable");
                    io.AddMouseViewportEvent(ImGui::GetMainViewport()->ID);
                    io.AddMousePosEvent(origin.x+click.x,origin.y+click.y);
                    closeFrame();
                    for(int n=0;n<2;++n){
                        io.AddMouseButtonEvent(0,true);closeFrame();
                        io.AddMouseButtonEvent(0,false);closeFrame();
                    }
                    if(!closable.terminal || !closable.pending || closable.draft!=original)
                        throw std::runtime_error("Gradient close icon did not preserve the draft and request cancellation");
                    io.AddMousePosEvent(-1000,-1000);
                    std::cout<<"Gradient close icon and draggable header passed\n";
                    uiFontSize=previousFont;
                    SetWindowPos(window,nullptr,0,0,before.right-before.left,before.bottom-before.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
                    ImGui_ImplWin32_NewFrame();
                    std::cout<<"Gradient minimum layout passed: all font roles and color modes, both tracks, zoomed, no scroll\n";
                }
                {
                    {
                        char value[4096]="Long editable paragraph with words and 中文内容, automatic visual wrapping keeps original text intact. Long editable paragraph with words and 中文内容, automatic visual wrapping keeps original text intact.";
                        const std::string original=value;bool focus=true;ImGuiID editorId=0;
                        auto wrapFrame=[&](float width){
                            ImGui::NewFrame();const auto origin=ImGui::GetMainViewport()->Pos;
                            ImGui::SetNextWindowPos({origin.x+16*scale,origin.y+16*scale});ImGui::SetNextWindowSize({width*scale,220*scale});
                            ImGui::Begin("##wrap-test",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings);
                            {MenuControls::PanelStyle panel(scale);if(focus){ImGui::SetKeyboardFocusHere();focus=false;}
                             editorId=ImGui::GetID("##wrap");MenuControls::multiline("##wrap",value,sizeof(value),150*scale);}
                            ImGui::End();ImGui::Render();finishPlatformFrame();
                        };
                        auto key=[&](ImGuiKey code){io.AddKeyEvent(code,true);wrapFrame(220);io.AddKeyEvent(code,false);wrapFrame(220);};
                        auto chord=[&](ImGuiKey code){io.AddKeyEvent(ImGuiMod_Ctrl,true);key(code);io.AddKeyEvent(ImGuiMod_Ctrl,false);wrapFrame(220);};
                        wrapFrame(500);wrapFrame(500);wrapFrame(220);wrapFrame(220);
                        auto* edit=ImGui::GetInputTextState(editorId);
                        if(!edit || edit->WrapWidth<=0 || edit->LineCount<2 || edit->Scroll.x!=0 || std::string(value)!=original)
                            throw std::runtime_error("Soft wrapping failed or changed underlying text");
                        chord(ImGuiKey_Home);key(ImGuiKey_DownArrow);edit=ImGui::GetInputTextState(editorId);
                        if(edit->GetCursorPos()<=0 || edit->GetCursorPos()>=edit->TextLen || (static_cast<unsigned char>(value[edit->GetCursorPos()])&0xC0)==0x80)
                            throw std::runtime_error("Wrapped cursor navigation lost UTF-8 boundary");
                        auto& platform=ImGui::GetPlatformIO();const auto oldGet=platform.Platform_GetClipboardTextFn;const auto oldSet=platform.Platform_SetClipboardTextFn;const auto oldData=platform.Platform_ClipboardUserData;
                        std::string clipboard;platform.Platform_ClipboardUserData=&clipboard;
                        platform.Platform_GetClipboardTextFn=[](ImGuiContext* ctx){return static_cast<std::string*>(ctx->PlatformIO.Platform_ClipboardUserData)->c_str();};
                        platform.Platform_SetClipboardTextFn=[](ImGuiContext* ctx,const char* text){*static_cast<std::string*>(ctx->PlatformIO.Platform_ClipboardUserData)=text;};
                        chord(ImGuiKey_A);chord(ImGuiKey_C);
                        if(clipboard!=original)throw std::runtime_error("Wrapped copy inserted display line breaks");
                        const int selected=ImGui::GetInputTextState(editorId)->GetSelectionEnd();wrapFrame(400);wrapFrame(220);
                        if(ImGui::GetInputTextState(editorId)->GetSelectionEnd()!=selected || std::string(value)!=original)
                            throw std::runtime_error("Resizing wrapped editor changed selection or text");
                        io.AddInputCharactersUTF8("修改中文");wrapFrame(220);
                        if(std::string(value)!="修改中文")throw std::runtime_error("Wrapped UTF-8 replacement failed");
                        // Selection deletion and four typed code points have separate undo entries.
                        for(int i=0;i<5;++i)chord(ImGuiKey_Z);
                        if(std::string(value)!=original)throw std::runtime_error("Wrapped edit undo failed");
                        clipboard="第一段 Chinese and English words that wrap naturally across the editor width.\n第二段 keeps one real newline.";
                        chord(ImGuiKey_A);chord(ImGuiKey_V);
                        if(std::string(value)!=clipboard || std::count(clipboard.begin(),clipboard.end(),'\n')!=1)
                            throw std::runtime_error("Wrapped paste changed real line breaks");
                        platform.Platform_GetClipboardTextFn=oldGet;platform.Platform_SetClipboardTextFn=oldSet;platform.Platform_ClipboardUserData=oldData;
                        std::cout<<"Soft-wrap editor passed: resize, UTF-8 cursor, exact copy/paste, Chinese edit, undo, no synthetic newlines or horizontal scroll\n";
                    }
                    {
                        ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({220*scale,180*scale});
                        ImGui::Begin("##match-test",nullptr,ImGuiWindowFlags_NoSavedSettings);
                        {MenuControls::PanelStyle controls(scale);MenuControls::textPreview("##match-colors","前猫 alpha beta gamma\n猫后",100*scale,{{3,6},{24,27}});}
                        int accented=0;const auto color=ImGui::GetColorU32(ImGuiCol_CheckMark);
                        for(auto* child:ImGui::GetCurrentWindow()->DC.ChildWindows)for(const auto& vertex:child->DrawList->VtxBuffer)if(vertex.col==color)++accented;
                        if(accented<8)throw std::runtime_error("Matched text did not render accent glyphs");
                        {
                            MenuTextStyle text(MenuTextRole::Body);
                            const std::string list="• "+std::string(80,'x')+"\n    1.1. "+std::string(80,'y');
                            const auto rows=MenuControls::previewLines(list.c_str(),180*scale,true);
                            const float rootIndent=ImGui::CalcTextSize("• ").x,childIndent=ImGui::CalcTextSize("    1.1. ").x;
                            bool child=false,sawRootWrap=false,sawChildWrap=false;std::string rebuilt;
                            for(const auto& row:rows){
                                if(row.begin>list.c_str() && row.begin[-1]=='\n')child=true;
                                if(row.x>0){if(std::abs(row.x-(child?childIndent:rootIndent))>.01f)throw std::runtime_error("List continuation lost its body indent");(child?sawChildWrap:sawRootWrap)=true;}
                                else if(row.begin!=list.c_str() && row.begin[-1]!='\n')throw std::runtime_error("Wrapped list unexpectedly returned to the left edge");
                                rebuilt.append(row.begin,row.end);if(*row.end=='\n')rebuilt+='\n';
                            }
                            if(!sawRootWrap || !sawChildWrap || rebuilt!=list)throw std::runtime_error("List wrapping changed text or omitted a level");
                            MenuControls::textPreview("##list-wrap",list.c_str(),100*scale,{},true);
                        }
                        ImGui::End();ImGui::Render();finishPlatformFrame();
                    }
                    {
                        MenuToolsPanel editor;editor.active=editor.ready=true;editor.mode="split";editor.revision="r";
                        editor.pending=true;editor.pendingAction="text.preview";std::snprintf(editor.draft,sizeof(editor.draft),"original");
                        auto editorFrame=[&](){
                            io.DeltaTime=1.f/60.f;ImGui::NewFrame();ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);const auto origin=ImGui::GetMainViewport()->Pos;
                            ImGui::SetNextWindowPos(origin);ImGui::SetNextWindowSize({700*scale,850*scale});
                            ImGui::Begin("##text-pending-test",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings);
                            editor.draw(scale,{0,0},true);ImGui::End();ImGui::Render();finishPlatformFrame();
                        };
                        editorFrame();editorFrame();ImGuiWindow* draftWindow=nullptr;
                        for(auto* child:GImGui->Windows)if(strstr(child->Name,"##text-pending-test/") && strstr(child->Name,"##draft"))draftWindow=child;
                        if(!draftWindow)throw std::runtime_error("Pending text editor has no input window");
                        io.AddMouseViewportEvent(ImGui::GetMainViewport()->ID);io.AddMousePosEvent(draftWindow->Pos.x+12*scale,draftWindow->Pos.y+12*scale);
                        editorFrame();
                        io.AddMouseButtonEvent(ImGuiMouseButton_Left,true);editorFrame();io.AddMouseButtonEvent(ImGuiMouseButton_Left,false);editorFrame();
                        io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_A,true);editorFrame();
                        io.AddKeyEvent(ImGuiKey_A,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);editorFrame();
                        io.AddInputCharactersUTF8("继续中文编辑");editorFrame();
                        if(std::string(editor.draft)!="继续中文编辑" || !editor.pending || !editor.textDirty || !editor.planId.empty())
                            throw std::runtime_error("Pending preview blocked editable text or retained an applicable plan: "+Json{{"draft",editor.draft},{"pending",editor.pending},{"dirty",editor.textDirty},{"active",ImGui::GetActiveID()},{"child",draftWindow->Name},{"hover",GImGui->HoveredWindow?GImGui->HoveredWindow->Name:"none"},{"mouse",{io.MousePos.x,io.MousePos.y}},{"pos",{draftWindow->Pos.x,draftWindow->Pos.y}},{"plan",editor.planId}}.dump());
                        ImGui::ClearActiveID();std::cout<<"Text panel remains clickable/editable during a pending preview; stale plan cleared\n";
                        editor.pending=false;editor.textDirty=false;editor.textTab=3;editor.error="invalid grouping";
                        std::snprintf(editor.grouping,sizeof(editor.grouping),"1");editorFrame();
                        auto* body=draftWindow->ParentWindow;const auto actionCount=testedActions.size();
                        ImGui::ActivateItemByID(body->GetID("拆成多个图层"));editorFrame();
                        if(editor.pending || testedActions.size()!=actionCount)throw std::runtime_error("Invalid split action was enabled");
                        ImGui::ActivateItemByID(body->GetID("写回当前文字"));editorFrame();
                        if(!editor.pending || editor.pendingAction!="text.write" || testedActions.size()!=actionCount+1 || editor.textStatus()!="正在写回文字…")
                            throw std::runtime_error("Invalid grouping blocked the actual write button or mislabeled its phase");
                        std::cout<<"Independent text actions passed: invalid split disabled, draft write button activates once\n";
                        editor.pending=false;editor.textTab=4;editor.error.clear();editor.textDirty=false;ImGui::ClearActiveID();
                        std::string largeList;
                        for(int i=0;i<40;++i){if(i)largeList+='\n';largeList+=std::string(150,'x')+" 中文😀";}
                        editor.setDraft(largeList);
                        editor.selectionStart=0;editor.selectionEnd=editor.cursorPos=int(largeList.size());editor.restoreDraftFocus=true;editorFrame();editorFrame();editorFrame();
                        int increase=1;const auto indentId=ImHashStr("增加缩进",0,ImHashData(&increase,sizeof(increase),body->GetID("##list-indent")));
                        ImGui::ActivateItemByID(indentId);editorFrame();editorFrame();editorFrame();
                        std::string indented="\t"+largeList;
                        for(size_t at=0;(at=indented.find('\n',at))!=std::string::npos;at+=2)indented.insert(at+1,"\t");
                        if(std::string(editor.draft)!=indented)throw std::runtime_error("List indent toolbar did not preserve selected text: "+Json{{"size",strlen(editor.draft)},{"expected",indented.size()},{"tabs",std::count(editor.draft,editor.draft+strlen(editor.draft),'\t')},{"selection",{editor.selectionStart,editor.selectionEnd}},{"cursor",editor.cursorPos},{"command",editor.indentCommand},{"focus",editor.restoreDraftFocus},{"pending",editor.pending},{"active",ImGui::GetActiveID()},{"toolbar",indentId},{"prefix",std::string(editor.draft).substr(0,40)}}.dump());
                        editorFrame();editorFrame();
                        io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_Z,true);editorFrame();
                        io.AddKeyEvent(ImGuiKey_Z,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);editorFrame();
                        if(std::string(editor.draft)!=largeList)throw std::runtime_error("Whole-list indentation was not one undo: "+Json{{"size",strlen(editor.draft)},{"tabs",std::count(editor.draft,editor.draft+strlen(editor.draft),'\t')},{"active",ImGui::GetActiveID()},{"input",GImGui->InputTextState.ID},{"text_len",GImGui->InputTextState.TextLen},{"focus",editor.restoreDraftFocus},{"selection",{editor.selectionStart,editor.selectionEnd}}}.dump());
                        io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_A,true);editorFrame();
                        io.AddKeyEvent(ImGuiKey_A,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);editorFrame();
                        io.AddKeyEvent(ImGuiKey_Tab,true);editorFrame();io.AddKeyEvent(ImGuiKey_Tab,false);editorFrame();
                        if(std::string(editor.draft)!=indented)throw std::runtime_error("List Tab replaced the selected text instead of indenting it");
                        io.AddKeyEvent(ImGuiMod_Shift,true);io.AddKeyEvent(ImGuiKey_Tab,true);editorFrame();
                        io.AddKeyEvent(ImGuiKey_Tab,false);io.AddKeyEvent(ImGuiMod_Shift,false);editorFrame();
                        if(std::string(editor.draft)!=largeList)throw std::runtime_error("List Shift+Tab did not outdent selected text");
                        auto& platform=ImGui::GetPlatformIO();const auto oldGet=platform.Platform_GetClipboardTextFn;const auto oldData=platform.Platform_ClipboardUserData;
                        std::string clipboard="\t第一项\n\t\t子项";platform.Platform_ClipboardUserData=&clipboard;
                        platform.Platform_GetClipboardTextFn=[](ImGuiContext* ctx){return static_cast<std::string*>(ctx->PlatformIO.Platform_ClipboardUserData)->c_str();};
                        io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_A,true);editorFrame();io.AddKeyEvent(ImGuiKey_A,false);editorFrame();
                        io.AddKeyEvent(ImGuiKey_V,true);editorFrame();io.AddKeyEvent(ImGuiKey_V,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);editorFrame();
                        platform.Platform_GetClipboardTextFn=oldGet;platform.Platform_ClipboardUserData=oldData;
                        if(std::string(editor.draft)!=clipboard)throw std::runtime_error("Pasted list tabs were mistaken for indentation shortcuts: "+Json{{"draft",std::string(editor.draft).substr(0,60)},{"size",strlen(editor.draft)},{"expected",clipboard},{"active",ImGui::GetActiveID()},{"input",GImGui->InputTextState.ID},{"tab_down",ImGui::IsKeyDown(ImGuiKey_Tab)},{"shift",io.KeyShift}}.dump());
                        io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_Z,true);editorFrame();io.AddKeyEvent(ImGuiKey_Z,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);editorFrame();
                        // Upstream paste records selection deletion and insertion separately.
                        if(std::string(editor.draft)!="")throw std::runtime_error("List paste insertion undo failed");
                        io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_Z,true);editorFrame();io.AddKeyEvent(ImGuiKey_Z,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);editorFrame();
                        if(std::string(editor.draft)!=largeList)throw std::runtime_error("List paste selection undo failed");
                        ImGui::ClearActiveID();std::cout<<"List editor passed: toolbar, multi-line Tab/Shift+Tab, exact indented paste, UTF-8 selection and large single undo\n";
                    }
                    int choice=0,iconChoice=0,rowClicks=0,previewClicks=0;char query[16]{};POINT second{},seam{},iconSecond{},iconSeam{},rowMain{},rowPreview{};bool focusQuery=false,rowPreviewHovered=false,showRowPreview=true;
                    auto controlsFrame=[&](bool disabled){
                        const auto padding=ImGui::GetStyle().FramePadding;
                        const auto border=ImGui::GetStyle().FrameBorderSize;
                        ImGui::NewFrame();ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
                        const auto oldFont=ImGui::GetFont();
                        const auto origin=ImGui::GetMainViewport()->Pos;
                        ImGui::SetNextWindowPos({origin.x+16*scale,origin.y+16*scale});ImGui::SetNextWindowSize({360*scale,240*scale});
                        ImGui::Begin("##shared-controls-test",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings);
                        {
                            MenuControls::PanelStyle controls(scale);
                            if(focusQuery){ImGui::SetKeyboardFocusHere();focusQuery=false;}
                            MenuControls::input("##query","搜索图层…",query,sizeof(query),MenuControls::Emphasis::Neutral);
                            const float input=ImGui::GetItemRectSize().y;
                            MenuControls::button("定位",scale);
                            if(std::abs(input-menuControlHeight*scale)>.1f || std::abs(ImGui::GetItemRectSize().y-input)>.1f ||
                               ImGui::GetStyle().FrameBorderSize!=menuControlBorder*scale || ImGui::GetFont()!=menuFont(MenuTextRole::Control))
                                throw std::runtime_error("Shared Caps/tool input and button metrics drifted");
                            const char* names[]={"全部","像素","文字","组","形状","智能对象","调整","填充","其他"};const auto at=ImGui::GetCursorScreenPos();
                            const auto first=ImGui::CalcTextSize(names[0]).x+2*menuControlTextInset*scale;
                            const auto next=ImGui::CalcTextSize(names[1]).x+2*menuControlTextInset*scale;
                            second={LONG(at.x+first+next*.5f),LONG(at.y+input*.5f)};
                            seam={LONG(at.x+first+next),second.y};
                            const auto expectedHeight=MenuControls::segmentsHeight(names,ImGui::GetContentRegionAvail().x,scale);
                            ImGui::BeginDisabled(disabled);MenuControls::segments("##types",names,choice,scale);ImGui::EndDisabled();
                            if(std::abs(ImGui::GetItemRectSize().y-expectedHeight)>.1f)throw std::runtime_error("Wrapped text filters lost measured height");
                            const char* iconNames[]={"全部","文字","智能对象"};const int iconIds[]={7,36,38};const auto bar=ImGui::GetCursorScreenPos();
                            iconSecond={LONG(bar.x+figmaDisplayIconWidth*scale*1.5f),LONG(bar.y+figmaDisplayWidgetHeight*scale*.5f)};
                            iconSeam={LONG(bar.x+figmaDisplayIconWidth*scale*2),iconSecond.y};
                            ImGui::BeginDisabled(disabled);MenuControls::iconSegments("##kind-icons",iconNames,iconIds,iconChoice,scale);
                            const auto row=ImGui::GetCursorScreenPos();const float rowWidth=ImGui::GetContentRegionAvail().x;ImVec2 anchor{};
                            rowMain={LONG(row.x+12*scale),LONG(row.y+menuSearchRowHeight*scale*.5f)};
                            rowPreview={LONG(row.x+rowWidth-menuSearchRowHeight*scale*.5f),rowMain.y};
                            bool previewClick=false;
                            if(MenuControls::resultRow("##compact-result",36,"文字图层长名称","示例.psd",scale,rowPreviewHovered,anchor,&previewClick,showRowPreview))rowClicks++;
                            if(previewClick)previewClicks++;
                            if(std::abs(ImGui::GetItemRectSize().y-menuSearchRowHeight*scale)>.1f)throw std::runtime_error("Compact result height drifted");
                            ImGui::EndDisabled();
                        }
                        if(ImGui::GetStyle().FramePadding.x!=padding.x || ImGui::GetStyle().FramePadding.y!=padding.y ||
                           ImGui::GetStyle().FrameBorderSize!=border || ImGui::GetFont()!=oldFont)
                            throw std::runtime_error("Shared panel style leaked into Caps");
                        ImGui::End();ImGui::Render();finishPlatformFrame();
                    };
                    const auto previousTextSize=uiFontSize;
                    float previousBody=0,previousControl=0,previousSecondary=0,previousShortcut=0;
                    for(const char* size:{"small","medium","large"}) {
                        uiFontSize=size;io.FontDefault=menuFont(MenuTextRole::Body);
                        const auto body=menuFont(MenuTextRole::Body)->LegacySize,control=menuFont(MenuTextRole::Control)->LegacySize,secondary=menuFont(MenuTextRole::Secondary)->LegacySize;
                        if(!(body>control && control>=secondary && body>previousBody && control>previousControl && secondary>previousSecondary))
                            throw std::runtime_error("Semantic typography lost hierarchy / PS size scaling");
                        const auto shortcut=menuFont(MenuTextRole::Shortcut)->LegacySize;
                        if(!(shortcut<secondary && shortcut>previousShortcut && shortcut<menuShortcutHeight*scale))
                            throw std::runtime_error("Compact shortcut typography lost hierarchy / PS size scaling");
                        // ImGui truncates atlas sizes to whole pixels.
                        if(std::abs(control-body*figmaBasicTextScale)>1.f || basicLabelSize()!=control || control>=menuControlHeight*scale)
                            throw std::runtime_error("Caps and panel control typography drifted / overflowed");
                        controlsFrame(false);previousBody=body;previousControl=control;previousSecondary=secondary;previousShortcut=shortcut;
                    }
                    uiFontSize=previousTextSize;io.FontDefault=menuFont(MenuTextRole::Body);
                    controlsFrame(false);io.AddMousePosEvent(float(second.x),float(second.y));controlsFrame(false);
                    io.AddMouseButtonEvent(0,true);controlsFrame(false);io.AddMouseButtonEvent(0,false);controlsFrame(false);
                    if(choice!=1)throw std::runtime_error("Shared type segment click failed");
                    io.AddMousePosEvent(float(seam.x),float(seam.y));controlsFrame(false);
                    io.AddMouseButtonEvent(0,true);controlsFrame(false);io.AddMouseButtonEvent(0,false);controlsFrame(false);
                    if(choice!=1)throw std::runtime_error("Shared segment divider gained a hit");
                    choice=0;io.AddMousePosEvent(float(second.x),float(second.y));controlsFrame(true);
                    io.AddMouseButtonEvent(0,true);controlsFrame(true);io.AddMouseButtonEvent(0,false);controlsFrame(true);
                    if(choice!=0)throw std::runtime_error("Disabled shared segment changed filter");
                    focusQuery=true;controlsFrame(false);controlsFrame(false);
                    procedure(window,WM_CHAR,0x9F97,0);controlsFrame(false);
                    if(std::string(query)!="\xE9\xBE\x97" || !requiredGlyphs.GetBit(0x9F97) || !fontDirty)
                        throw std::runtime_error("Chinese input did not reach shared input/font cache");
                    ImGui_ImplDX11_InvalidateDeviceObjects();loadFont();
                    if(!prepareRendererResources())throw std::runtime_error("Input glyph upload failed");
                    controlsFrame(false);
                    if(std::string(query)!="\xE9\xBE\x97" || !menuFont(MenuTextRole::Control)->GetFontBaked(menuFont(MenuTextRole::Control)->LegacySize)->IsGlyphLoaded(0x9F97))
                        throw std::runtime_error("Chinese input was lost after font update");
                    auto* viewport=ImGui::GetMainViewport();auto himc=ImmGetContext(window);
                    if(!himc || !ImGui::GetPlatformIO().Platform_SetImeDataFn)
                        throw std::runtime_error("Windows IME context/handler missing");
                    COMPOSITIONFORM previous{},position{};const bool restore=ImmGetCompositionWindow(himc,&previous)!=0;
                    ImGuiPlatformImeData ime;ime.WantVisible=true;ime.InputPos={viewport->Pos.x+37,viewport->Pos.y+59};ime.InputLineHeight=24*scale;
                    ImGui::GetPlatformIO().Platform_SetImeDataFn(ImGui::GetCurrentContext(),viewport,&ime);
                    const bool positioned=ImmGetCompositionWindow(himc,&position) && position.ptCurrentPos.x==37 && position.ptCurrentPos.y==59;
                    if(restore)ImmSetCompositionWindow(himc,&previous);ImmReleaseContext(window,himc);
                    if(!positioned)throw std::runtime_error("IME composition did not follow input caret");
                    io.AddMousePosEvent(float(iconSecond.x),float(iconSecond.y));controlsFrame(false);
                    io.AddMouseButtonEvent(0,true);controlsFrame(false);io.AddMouseButtonEvent(0,false);controlsFrame(false);
                    if(iconChoice!=1)throw std::runtime_error("Layer type icon click failed");
                    io.AddMousePosEvent(float(iconSeam.x),float(iconSeam.y));controlsFrame(false);
                    io.AddMouseButtonEvent(0,true);controlsFrame(false);io.AddMouseButtonEvent(0,false);controlsFrame(false);
                    if(iconChoice!=1)throw std::runtime_error("Layer icon divider gained a hit");
                    io.AddMousePosEvent(float(rowMain.x),float(rowMain.y));controlsFrame(false);
                    io.AddMouseButtonEvent(0,true);controlsFrame(false);io.AddMouseButtonEvent(0,false);controlsFrame(false);
                    if(rowClicks!=1)throw std::runtime_error("Compact row did not locate exactly once");
                    io.AddMousePosEvent(float(rowPreview.x),float(rowPreview.y));controlsFrame(false);
                    io.AddMouseButtonEvent(0,true);controlsFrame(false);io.AddMouseButtonEvent(0,false);controlsFrame(false);
                    if(rowClicks!=1 || !rowPreviewHovered || previewClicks!=1)throw std::runtime_error("Preview click did not stay separate from locate");
                    controlsFrame(false);controlsFrame(false);
                    if(previewClicks!=1)throw std::runtime_error("Preview hover repeated a click");
                    io.AddMouseButtonEvent(0,true);controlsFrame(true);io.AddMouseButtonEvent(0,false);controlsFrame(true);
                    if(previewClicks!=1)throw std::runtime_error("Disabled preview gained a click");
                    showRowPreview=false;controlsFrame(false);controlsFrame(false);
                    io.AddMouseButtonEvent(0,true);controlsFrame(false);io.AddMouseButtonEvent(0,false);controlsFrame(false);
                    if(previewClicks!=1 || rowPreviewHovered || rowClicks!=2)throw std::runtime_error("Unavailable preview left a hidden hit or took row selection");
                    io.AddMousePosEvent(-1000,-1000);ImGui::SetWindowFocus(nullptr);
                    std::cout<<"Shared Caps/tools UI passed: input/button/font/border parity, style restoration, segment click, divider cancel and disabled protection\n";
                    std::cout<<"Chinese input passed: own WM_CHAR to UTF-8 input, new glyph upload, preserved edit buffer and Windows IME caret position\n";
                }
                for (auto entry:std::array<std::pair<ImGuiKey,const char*>,3>{{
                    {ImGuiKey_4,"select"},{ImGuiKey_Keypad8,"moveToTop"},{ImGuiKey_2,"moveToBottom"}}}) {
                    holdFinished=false;auto before=testedActions.size();
                    numberFrame(entry.first,true);numberFrame(entry.first,false);
                    finishHold({LONG(pieCenter.x),LONG(pieCenter.y)});
                    if (testedActions.size()!=before+1 || testedActions.back()!=entry.second)
                        throw std::runtime_error("Pie numeric accelerator must execute once");
                }
                auto before=testedActions.size();holdFinished=false;panelKind=1;
                numberFrame(ImGuiKey_8,true);numberFrame(ImGuiKey_8,false);
                panelKind=3;numberFrame(ImGuiKey_4,true);numberFrame(ImGuiKey_4,false);
                panelKind=2;allowed=false;numberFrame(ImGuiKey_2,true);numberFrame(ImGuiKey_2,false);
                if (testedActions.size()!=before)
                    throw std::runtime_error("Pie accelerator bypassed panel/input/busy protection");
                allowed=true;panelKind=2;holdFinished=false;
                // Seven new segments use the same frozen-highlight release
                // fencing as swatches, including veto by the latest cursor.
                const auto quickCount=testedActions.size();
                const bool oldAware=layerAware;const unsigned oldMask=layerMask;
                procedure(window,WM_APP+17,WPARAM(2|16|64|(15<<8)|(2<<12)|(1<<17))|(WPARAM(127)<<32),menuEpoch);
                if(quickMask!=127)throw std::runtime_error("64-bit quick capabilities lost in prepare");
                numberFrame(ImGuiKey_None,false);
                if(quickRows[0].left!=quickRows[3].left || quickRows[2].top!=quickRows[0].top ||
                    quickRows[6].top!=quickRows[3].top || quickRows[2].bottom>=quickRows[3].top ||
                    std::abs(quickRows[3].top-quickRows[2].bottom-quickGroupGap*scale)>scale ||
                    quickRows[0].left<=pieCenter.x+19*scale ||
                    holdActionAt({LONG(pieCenter.x-140*scale),LONG(pieCenter.y-42*scale)})!=-1)
                    throw std::runtime_error("Quick groups must align right, remain separate and clear old left hits");
                if(holdActionAt({quickRows[0].left+LONG(figmaDisplayIconWidth*scale*.5f),(quickRows[0].bottom+quickRows[3].top)/2})!=-1)
                    throw std::runtime_error("Gap between quick rows must cancel");
                for(int i:{1,2,4,5}) {
                    POINT seam={quickRows[i].left,(quickRows[i].top+quickRows[i].bottom)/2};
                    if(holdActionAt(seam)!=-1 || !tipGapAt(32+i,seam))
                        throw std::runtime_error("Horizontal divider must retain tooltip but cancel gesture");
                    const auto before=testedActions.size();
                    const RECT dragWindow{-1000,100,-568,720},dragLimit{-1800,50,-100,1000};
                    const auto dragInside=MenuToolsPanel::dragPosition(dragWindow,dragLimit,{10,20});
                    const auto dragLow=MenuToolsPanel::dragPosition(dragWindow,dragLimit,{-5000,-5000});
                    const auto dragHigh=MenuToolsPanel::dragPosition(dragWindow,dragLimit,{5000,5000});
                    if(dragInside.x!=-990 || dragInside.y!=120 || dragLow.x!=-1800 || dragLow.y!=50 || dragHigh.x!=-532 || dragHigh.y!=380)
                        throw std::runtime_error("Tool drag escaped Photoshop/work-area bounds");
                    holdFinished=false;pieReady=true;renderedHit=32+i;
                    finishHold(seam,true,&seam);
                    if(testedActions.size()!=before+1 || testedActions.back()!="hide")
                        throw std::runtime_error("Retained divider tooltip submitted a command");
                    testedActions.pop_back();holdFinished=false;pieReady=true;
                }
                {const auto r=quickRows[6];const POINT shadow={r.right+LONG(4*scale),(r.top+r.bottom)/2};
                    if(holdActionAt(shadow)!=-1)throw std::runtime_error("Menu shadow acquired an action hit");}
                // Canvas and Layers keep the confirmed near-centre column.
                panelKind=1;numberFrame(ImGuiKey_None,false);
                if(std::abs(quickRows[0].top-(pieCenter.y+45*scale))>scale)throw std::runtime_error("Canvas creation group must align with label colors");
                for(int i=0;i<7;++i) {
                    auto tip=quickTipBounds(i);
                    if(tip.z<=tip.x)throw std::runtime_error("Canvas icon tooltip missing");
                    auto* tipFont=menuFont(MenuTextRole::Secondary);
                    const float labelWidth=tipFont->CalcTextSizeA(tipFont->LegacySize,FLT_MAX,0,quickLabels[i]).x;
                    if(std::abs(tip.z-tip.x-labelWidth-12*scale)>scale)
                        throw std::runtime_error("Tooltip padding contains reserved sibling text width");

                    if(tip.w>quickRows[0].top)throw std::runtime_error("Canvas tooltip must prefer above the icon group");
                    for(auto r:quickRows)if(tipOverlaps(tip,r))throw std::runtime_error("Canvas tooltip covers an icon");
                }
                panelKind=2;numberFrame(ImGuiKey_None,false);
                if(std::abs(quickRows[0].top-(pieCenter.y+45*scale))>scale)throw std::runtime_error("Layers creation group must align with label colors");
                for(int action:{0,2,3}) {
                    const auto tip=actionTipBounds(action);
                    if(tip.w>holdRows[action].top)throw std::runtime_error("Basic tooltip must prefer above its button");
                    if(tip.z<=tip.x || holdActionAt({LONG((tip.x+tip.z)*.5f),LONG((tip.y+tip.w)*.5f)})!=-1)
                        throw std::runtime_error("Basic action tooltip missing or has a gesture hit");
                    for(auto r:quickRows)if(tipOverlaps(tip,r))throw std::runtime_error("Basic tooltip covers an icon");
                    for(int j=0;j<4;++j)if(rowVisible(j) && tipOverlaps(tip,holdRows[j]))throw std::runtime_error("Basic tooltip covers an action");
                }
                for(int i=0;i<7;++i) {
                    const auto r=quickRows[i];POINT middle={(r.left+r.right)/2,(r.top+r.bottom)/2};
                    holdFinished=false;pieReady=true;holdMoved=true;renderedHit=32+i;
                    const auto tip=quickTipBounds(i);
                    if(tip.w>quickRows[0].top)throw std::runtime_error("Layers tooltip must prefer above the icon group");
                    for(const auto& button:quickRows)if(tip.x<button.right && tip.z>button.left && tip.y<button.bottom && tip.w>button.top)
                        throw std::runtime_error("Quick tooltip overlaps either row");
                    if(tip.z<=tip.x || tipOverlaps(tip,r) || tip.x<ImGui::GetMainViewport()->Pos.x || tip.z>ImGui::GetMainViewport()->Pos.x+ImGui::GetMainViewport()->Size.x ||
                        holdActionAt({LONG((tip.x+tip.z)*.5f),LONG((tip.y+tip.w)*.5f)})!=-1)
                        throw std::runtime_error("Quick tooltip overlaps icon, clips or commits a gesture");
                    for(int action=0;action<4;++action)if(rowVisible(action)) {
                        const auto card=holdRows[action];
                        if(tip.x<card.right && tip.z>card.left && tip.y<card.bottom && tip.w>card.top)
                            throw std::runtime_error("Quick tooltip overlaps a primary action");
                    }
                    if(holdActionAt(middle)!=32+i)throw std::runtime_error("Quick segment not reachable");
                    const auto count=testedActions.size();finishHold(middle,true,&middle);finishHold(middle,true,&middle);
                    if(testedActions.size()!=count+1 || testedActions.back()!=quickActions[i])throw std::runtime_error("Quick segment replay or wrong command");
                    holdFinished=false;pieReady=true;renderedHit=32+i;
                    POINT blank={LONG(pieCenter.x),LONG(pieCenter.y)};
                    finishHold(middle,true,&blank);
                    if(testedActions.back()!="hide")throw std::runtime_error("Quick segment stale hover committed");
                    holdFinished=false;pieReady=true;quickMask=127 & ~(1u<<i);
                    const auto disabled=testedActions.size();send(quickActions[i]);
                    if(holdActionAt(middle)!=-1 || testedActions.size()!=disabled)throw std::runtime_error("Disabled quick segment executed");
                    quickMask=127;
                    pinned=true;holdFinished=false;
                    io.AddMouseViewportEvent(ImGui::GetMainViewport()->ID);io.AddMousePosEvent(float(middle.x),float(middle.y));
                    io.AddMouseButtonEvent(ImGuiMouseButton_Left,true);numberFrame(ImGuiKey_None,false);
                    io.AddMouseButtonEvent(ImGuiMouseButton_Left,false);numberFrame(ImGuiKey_None,false);
                    if(testedActions.back()!=quickActions[i] || !holdFinished)throw std::runtime_error("Pinned quick click failed");
                    pinned=false;
                }
                quickMask=127;inputFocused=true;holdFinished=false;numberFrame(ImGuiKey_None,false);
                if(quickEnabled(0))throw std::runtime_error("Quick action bypassed input protection");
                inputFocused=false;panelKind=4;numberFrame(ImGuiKey_None,false);
                for(const auto& r:quickRows)if(r.right || r.bottom)throw std::runtime_error("Home retained quick segments");
                testedActions.resize(quickCount);quickMask=0;layerAware=oldAware;layerMask=oldMask;
                panelKind=2;holdFinished=false;numberFrame(ImGuiKey_None,false);
                accept({{"type","operation"},{"busy",true},{"action","color"}});
                setHoldMode(true);
                SetWindowPos(window,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);
                numberFrame(ImGuiKey_4,true);numberFrame(ImGuiKey_4,false);
                if (!busyFeedback || pieReady || pendingHit!=-1 || renderedHit!=-1 || canExecute() ||
                    compositionOpacity!=1.f || !IsWindowVisible(window) || ImGui::GetDrawData()->TotalVtxCount<=0)
                    throw std::runtime_error("Busy feedback did not render alone through production show path");
                for (const auto& r:holdRows) if (r.right || r.bottom)
                    throw std::runtime_error("Busy feedback retained action hit regions");
                for (const auto& r:labelRows) if (r.right || r.bottom)
                    throw std::runtime_error("Busy feedback retained label hit regions");
                accept({{"type","notice"},{"scope","operation"},{"ok",true},{"message","Completed"}});
                accept({{"type","operation"},{"busy",false}});
                numberFrame(ImGuiKey_2,true);numberFrame(ImGuiKey_2,false);
                if (!busyFeedback || canExecute() || testedActions.size()!=before)
                    throw std::runtime_error("Completion expanded feedback or executed a hidden action");
                completionStarted=GetTickCount64()-500;
                if(SendMessageW(window,WM_APP+24,0,0)!=1)
                    throw std::runtime_error("Confirmed completion did not become refresh-ready");
                const auto oldEpoch=menuEpoch;
                Json resume={{"epoch",oldEpoch},{"next_epoch",oldEpoch+1},{"resume",true},
                    {"flags",uint64_t(64)|(uint64_t(15)<<8)|(uint64_t(2)<<12)|(uint64_t(1)<<17)|(uint64_t(127)<<32)},
                    {"shape",{{"available",false}}}};
                auto applyResume=[&](const Json& value){const auto payload=value.dump();
                    COPYDATASTRUCT packet{0x4d435458,DWORD(payload.size()+1),const_cast<char*>(payload.c_str())};
                    return SendMessageW(window,WM_COPYDATA,0,reinterpret_cast<LPARAM>(&packet));};
                auto staleResume=resume;staleResume["epoch"]=oldEpoch-1;
                if(applyResume(staleResume))
                    throw std::runtime_error("Old completion epoch resumed the menu");
                taskBusy=true;
                if(applyResume(resume))throw std::runtime_error("Busy completion resumed actions");
                taskBusy=false;taskFailed=true;
                if(applyResume(resume))throw std::runtime_error("Failed completion resumed actions");
                taskFailed=false;taskRecovery="Result unknown";
                if(applyResume(resume))throw std::runtime_error("Unknown result resumed actions");
                taskRecovery.clear();inputFocused=true;
                if(applyResume(resume))throw std::runtime_error("Input protection allowed completion resume");
                inputFocused=false;
                if(applyResume(resume))throw std::runtime_error("Retired completion restore packet accepted");
                auto hoverReturn=resume;hoverReturn["resume"]=false;hoverReturn["hover_return"]=true;
                if(applyResume(hoverReturn))throw std::runtime_error("Unrequested hover return accepted");
                hoverReturnConfirmed=true;taskFailed=true;
                if(applyResume(hoverReturn))throw std::runtime_error("Failed hover return accepted");
                taskFailed=false;taskBusy=true;
                if(applyResume(hoverReturn))throw std::runtime_error("Busy hover return accepted");
                taskBusy=false;inputFocused=true;
                if(applyResume(hoverReturn))throw std::runtime_error("Input hover return accepted");
                inputFocused=false;panelKind=2;
                if(!applyResume(hoverReturn) || menuEpoch!=oldEpoch+1 || busyFeedback || !pinned || shapePanelOpen || hoverReturnConfirmed)
                    throw std::runtime_error("Verified hover confirmation failed: "+Json{{"epoch",menuEpoch},{"expected",oldEpoch+1},
                        {"busy",taskBusy},{"feedback",busyFeedback},{"ready",completionReady()},{"preview",maskPreviewStarted},
                        {"pinned",pinned},{"panel",shapePanelOpen},{"confirmed",hoverReturnConfirmed}}.dump());
                numberFrame(ImGuiKey_None,false);
                if(!canExecute() || ImGui::GetDrawData()->TotalVtxCount<=0 || applyResume(hoverReturn))
                    throw std::runtime_error("Hover return lost content or accepted duplicate epoch");
                std::cout<<"Hover return confirmed: fresh epoch and visible ordinary menu; unrequested/busy/failure/input/duplicate rejected\n";
                for(const char* host:{"23.3.0","24.0.1","24.2.0","25.5.1"}) {
                    procedure(window,WM_APP+26,2,0);
                    ShowWindow(window,SW_HIDE);panelKind=2;inputFocused=false;
                    Json wakeSettings={{"enabled",true},{"shortcuts",Json::object()}};
                    accept({{"type","status"},{"data",{{"photoshop",{{"connected",true},{"document","owned-test.psd"},
                        {"photoshop_version",host},{"uxp_version","0.3.51"},{"uxp_ready",true},{"busy",true}}},{"settings",wakeSettings}}}});
                    accept({{"type","operation"},{"busy",true},{"action","color"}});
                    setHoldMode(true);
                    SetWindowPos(window,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);
                    numberFrame(ImGuiKey_None,false);
                    if(!busyMenuWake || !busyFeedback || canExecute() || ImGui::GetDrawData()->TotalVtxCount<=0)
                        throw std::runtime_error("Caps busy wake did not start with visible exclusive running content");
                    accept({{"type","notice"},{"scope","operation"},{"ok",true},{"message","Completed"}});
                    accept({{"type","operation"},{"busy",false}});completionStarted=GetTickCount64()-500;
                    Json wake={{"epoch",menuEpoch},{"next_epoch",menuEpoch+1},{"busy_wake",true},
                        {"flags",uint64_t(64)|(uint64_t(15)<<8)|(uint64_t(2)<<12)|(uint64_t(1)<<17)|(uint64_t(127)<<32)},
                        {"shape",{{"available",false}}}};
                    if(SendMessageW(window,WM_APP+28,0,0)!=1 || SendMessageW(window,WM_APP+24,0,0)!=1)
                        throw std::runtime_error("Successful Caps busy menu never requested fresh wakeup");
                    busyMenuWake=false;if(applyResume(wake))throw std::runtime_error("Unrequested busy wake accepted");busyMenuWake=true;
                    taskBusy=true;if(applyResume(wake))throw std::runtime_error("Running busy wake accepted");taskBusy=false;
                    taskFailed=true;if(applyResume(wake))throw std::runtime_error("Failed busy wake accepted");taskFailed=false;
                    taskRecovery="unknown";if(applyResume(wake))throw std::runtime_error("Unknown busy wake accepted");taskRecovery.clear();
                    inputFocused=true;if(applyResume(wake) || SendMessageW(window,WM_APP+24,0,0))throw std::runtime_error("Input busy wake accepted");inputFocused=false;
                    maskPreviewStarted=true;if(applyResume(wake))throw std::runtime_error("Active preview busy wake accepted");maskPreviewStarted=false;
                    auto stale=wake;stale["epoch"]=menuEpoch-1;if(applyResume(stale))throw std::runtime_error("Stale busy wake accepted");
                    if(!applyResume(wake) || busyMenuWake || busyFeedback || !pinned || holdFinished)
                        throw std::runtime_error("Verified busy wake failed");
                    numberFrame(ImGuiKey_None,false);
                    if(!IsWindowVisible(window) || !canExecute() || ImGui::GetDrawData()->TotalVtxCount<=0 || applyResume(wake) ||
                        (SendMessageW(window,WM_APP+27,menuEpoch,0)&0xf800)!=0xf800)
                        throw std::runtime_error("Busy wake lost visible content or accepted duplicate");
                    ShowWindow(window,SW_HIDE);if(applyResume(wake))throw std::runtime_error("Hidden busy menu reopened");
                    setHoldMode(true);if(busyMenuWake)throw std::runtime_error("Idle reopen inherited busy wake intent");
                    SetWindowPos(window,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);
                }
                procedure(window,WM_APP+26,0,0);
                std::cout<<"Caps busy wake: 23.3/24.0/24.2/25.5 status contracts, fresh epoch + visible fixed ordinary content; failure/unknown/input/preview/stale/duplicate/hidden rejected\n";
                // All compact successes qualify, irrespective of layer/canvas/home scope or hold mode.
                for(int scope:{0,1,2,3,4})for(bool held:{false,true}){
                    panelKind=scope;setHoldMode(held);holdFinished=false;
                    accept({{"type","operation"},{"busy",true},{"action","color"}});
                    accept({{"type","notice"},{"scope","operation"},{"ok",true},{"message","Completed"}});
                    accept({{"type","operation"},{"busy",false}});
                    if(SendMessageW(window,WM_APP+24,0,0))throw std::runtime_error("Success closed before feedback interval");
                    completionStarted=GetTickCount64()-500;
                    if(SendMessageW(window,WM_APP+24,0,0)!=1)throw std::runtime_error("Completed scope never became close-ready");
                    taskBusy=true;if(completionReady())throw std::runtime_error("Running operation became close-ready");taskBusy=false;
                    taskFailed=true;if(completionReady())throw std::runtime_error("Failure became close-ready");taskFailed=false;
                    taskRecovery="unknown";if(completionReady())throw std::runtime_error("Unknown became close-ready");taskRecovery.clear();
                }
                ShowWindow(window,SW_HIDE);
                if(SendMessageW(window,WM_APP+24,0,0))throw std::runtime_error("Hidden feedback became close-ready");
                panelKind=2;setHoldMode(true);
                SetWindowPos(window,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);
                numberFrame(ImGuiKey_None,false);
                if (busyFeedback || !canExecute() || !pieReady || SendMessageW(window,WM_APP+24,0,0))
                    throw std::runtime_error("Explicit reopen inherited completed feedback");
                std::cout<<"Completion close readiness passed: all five scopes, held/pinned, 450ms delay, busy/failure/unknown/hidden guards, no restore packet, fresh reopen\n";
                const auto shapeCount=testedActions.size();
                for(int r=0;r<=255;r+=51)for(int g=0;g<=255;g+=51)for(int b=0;b<=255;b+=51){
                    perceptual::Color source={double(r),double(g),double(b)},back=perceptual::fromLab(perceptual::lab(source));
                    for(int i=0;i<3;++i)if(std::abs(back[i]-source[i])>.01)throw std::runtime_error("Oklab sRGB roundtrip failed");
                }
                const auto perceptualMid=perceptual::mix({255,0,0},{0,0,255},.5,"perceptual");
                if(std::abs(perceptualMid[0]-140.36)>1 || std::abs(perceptualMid[1]-83.03)>1 || std::abs(perceptualMid[2]-162.31)>1)
                    throw std::runtime_error("Oklab reference midpoint failed");
                for(int h=0;h<360;h+=10){const auto mapped=perceptual::fromLch(.7,.5,h),p=perceptual::lch(mapped);
                    if(std::abs(p[0]-.7)>.0001 || std::abs(perceptual::hue(p[2]-h+180)-180)>.01)
                        throw std::runtime_error("Perceptual gamut map changed hue or lightness");}
                {PerceptualPickerState standard;standard.cache();
                    const ImU32 stops[]={IM_COL32(255,0,0,255),IM_COL32(255,255,0,255),IM_COL32(0,255,0,255),IM_COL32(0,255,255,255),IM_COL32(0,0,255,255),IM_COL32(255,0,255,255),IM_COL32(255,0,0,255)};
                    for(int i=0;i<7;++i)if(standard.mesh[i*32]!=stops[i])throw std::runtime_error("Standard HSV hue stops differ from RGB picker");
                    for(double l:{.2,.5,.8})for(double c:{.02,.1,.25})for(int role=0;role<4;++role)for(int h=0;h<360;h+=30){
                        standard.base={l,c,40};standard.dragHue(h,role);
                        const double actual=PerceptualPickerState::stripHue(perceptual::wheelColor(standard.base,role),h);
                        if(std::abs(perceptual::hue(actual-h+180)-180)>.25 || standard.base[0]!=l || standard.base[1]!=c)
                            throw std::runtime_error("Standard hue drag changed L/C or marker alignment");
                    }
                }
                PerceptualPickerState harmony;harmony.base={.65,.12,40};harmony.dragPoint(220,.6,3);
                if(std::abs(harmony.base[2]-40)>.001)throw std::runtime_error("Complementary handle did not rotate linked base");
                harmony.cache();auto builds=harmony.meshBuilds;harmony.base[2]=80;harmony.cache();
                if(harmony.meshBuilds!=builds)throw std::runtime_error("Hue drag rebuilt unchanged wheel mesh");
                for(const auto& rgb:std::vector<perceptual::Color>{{0,0,0},{255,255,255},{128,128,128},{1,1,1},{254,254,254},{255,0,0},{0,255,0},{0,0,255},{48,216,111}})
                    for(float radius:{38.f,60.f,100.f})for(float uiScale:{1.f,2.f}){
                        harmony.base=perceptual::lch(rgb);harmony.layout({300,300},radius*uiScale,uiScale);
                        const auto layouts=harmony.layoutBuilds;harmony.layout({300,300},radius*uiScale,uiScale);
                        if(harmony.layoutBuilds!=layouts)throw std::runtime_error("Idle wheel rebuilt its handle layout");
                        for(int i=0;i<perceptual::wheelCount;++i){
                            if(harmony.hit(harmony.points[i],uiScale)!=i)throw std::runtime_error("Simultaneous wheel handles overlap or cannot be selected: "+std::to_string(i));
                            for(int j=0;j<i;++j)if(std::hypot(harmony.points[i].x-harmony.points[j].x,harmony.points[i].y-harmony.points[j].y)<(harmony.markerExtent(i)+harmony.markerExtent(j)+5.f)*uiScale)
                                throw std::runtime_error("Wheel handles lost visible separation");
                        }
                    }
                if(harmonyMarkerRoles[0]!=harmonyMarkerRoles[1] || harmonyMarkerRoles[1]!=harmonyMarkerRoles[2] ||
                   harmonyMarkerRoles[0]==harmonyMarkerRoles[3])throw std::runtime_error("Harmony role shapes lost their semantic grouping");
                // Polygon corners extend beyond the old circular hit boundary.
                for(int i=0;i<perceptual::wheelCount;++i){
                    const auto& g=PerceptualPickerState::geometry(i);const float u=PerceptualPickerState::markerRadius(i)/4.5f;
                    for(int n=0;n<g.count;++n){
                        ImVec2 vertex={100+(g.points[n].x-4.5f)*u,100+(g.points[n].y-4.5f)*u};
                        if(!PerceptualPickerState::markerHit(i,{100,100},vertex,1))throw std::runtime_error("Visible marker vertex has no hit");
                    }
                    if(PerceptualPickerState::markerHit(i,{100,100},{100+PerceptualPickerState::markerExtent(i)+3.f,100},1))
                        throw std::runtime_error("Marker hit extends beyond its visible padded bounds");
                }
                {const auto before=testedActions.size(),events=testedMaskEvents.size();
                    procedure(window,WM_APP+26,1,0);send("label.red");commitShapeDraft();emitMaskPreview();
                    if(testedActions.size()!=before || testedMaskEvents.size()!=events)throw std::runtime_error("Read-only menu inspection emitted an action");
                    if(procedure(window,WM_APP+27,menuEpoch+1,0))throw std::runtime_error("Old menu inspection accepted a different epoch");
                    procedure(window,WM_APP+26,0,0);}
                Json shapeFixture={{"available",true},{"live_preview",true},{"revision","test:1"},
                    {"mask",{{"feather",0},{"density",100}}},
                    {"fill",{{"kind","solid"},{"enabled",true},{"rgb",{12,34,56}}}},
                    {"stroke",{{"kind","solid"},{"enabled",false},{"rgb",{0,0,0}},{"width",1}}},
                    {"patterns",{"Test Pattern"}}};
                shapeEditor.prepare(shapeFixture);setHoldMode(true);shapePanelOpen=true;panelKind=2;layerAware=true;layerKind=6;layerCount=1;layerLocked=layerBackground=false;
                {ShapeColorEditor isolated;isolated.prepare(shapeFixture);isolated.mode(1);
                    isolated.current()["method"]="classic";isolated.methodExplicit[0]=true;
                    isolated.current()["stops"][0]["rgb"][0]=120;isolated.changed();
                    if(isolated.current()["method"]!="classic")throw std::runtime_error("Explicit interpolation choice was overwritten by color edit");
                    isolated.methodExplicit[0]=false;isolated.current()["stops"][0]["rgb"][0]=121;isolated.changed();
                    if(isolated.current()["method"]!="perceptual")throw std::runtime_error("Default gradient edit did not use perceptual method");}
                SetWindowPos(window,nullptr,0,0,int(920*scale),int(menuShapeWindowHeight*scale),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);
                ImGui_ImplWin32_NewFrame();
                const bool priorCaps=capsReady,priorSmart=individualSmartReady;
                const unsigned priorQuick=quickMask;capsReady=individualSmartReady=true;
                const auto rightDockLayout=2|16|64|(15<<8)|(6<<12)|(1<<17)|(127ull<<32)|((uint64_t(std::lround(200*scale))+1)<<40)|(1ull<<56);
                procedure(window,WM_APP+17,rightDockLayout,menuEpoch);io.AddMousePosEvent(-1000,-1000);numberFrame(ImGuiKey_None,false);
                const auto leftEdge=ImGui::GetMainViewport()->Pos.x;
                if(std::abs(pieCenter.x-leftEdge-200*scale)>1.f || holdRows[0].right>=pieCenter.x || holdRows[2].left<leftEdge ||
                   std::abs((holdRows[2].left+capsRows[6].right)*.5f-pieCenter.x)>1.f || capsRows[10].left<leftEdge ||
                   capsRows[10].right>=holdRows[1].left || quickRows[0].left<=holdRows[1].right)
                    throw std::runtime_error("Left-edge Caps changed the original control layout");
                shapePanelOpen=true;numberFrame(ImGuiKey_None,false);
                if(shapeEditor.bounds.left<=shapeModeBounds.right)
                    throw std::runtime_error("Left-dock Caps menu lost the right-hand editor or pointer anchor");
                const int rightAnchor=int(std::floor(ImGui::GetMainViewport()->Size.x-pieRightInset*scale));
                const auto rightEdgeLayout=(rightDockLayout&((1ull<<40)-1))|((uint64_t(rightAnchor)+1)<<40);
                procedure(window,WM_APP+17,rightEdgeLayout,menuEpoch);numberFrame(ImGuiKey_None,false);
                const auto rightEdge=leftEdge+ImGui::GetMainViewport()->Size.x;
                if(std::abs(quickRows[6].right+menuShadow::extent*scale-rightEdge)>2.f ||
                   holdRows[0].right>=pieCenter.x || std::abs((holdRows[2].left+capsRows[6].right)*.5f-pieCenter.x)>1.f)
                    throw std::runtime_error("Right-edge Caps clipped its controls or changed their layout");
                pieAnchorX=-1;shapePanelRight=false;capsReady=priorCaps;individualSmartReady=priorSmart;quickMask=priorQuick;
                numberFrame(ImGuiKey_None,false);holdMoved=true;
                // Exercise the real expander path before isolated control fixtures.
                shapePanelOpen=false;shapeHoverBounds={};io.AddMousePosEvent(-1000,-1000);numberFrame(ImGuiKey_None,false);
                const auto switchEvents=testedMaskEvents.size(),switchActions=testedActions.size();const auto switchDraft=shapeEditor.draft;
                auto pointer=[&](float x,float y){io.AddMouseViewportEvent(ImGui::GetMainViewport()->ID);io.AddMousePosEvent(x,y);numberFrame(ImGuiKey_None,false);};
                auto toggleEditor=[&]{const auto icon=shapeModeBounds;
                    pointer(pieCenter.x,pieCenter.y);
                    pointer((icon.left+icon.right)*.5f,(icon.top+icon.bottom)*.5f);};
                if(shapeEditor.bounds.right || !shapeModeBounds.right)throw std::runtime_error("New menu did not start ordinary");
                const auto arrow=shapeModeBounds;pointer((arrow.left+arrow.right)*.5f,(arrow.top+arrow.bottom)*.5f);
                const auto panel=shapeEditor.bounds;const auto mask=shapeEditor.maskBounds;
                if(!shapePanelOpen || panel.right<=panel.left || holdRows[0].right || capsRows[0].right || quickRows[0].right)
                    throw std::runtime_error("Expander failed to hide ordinary actions and show editor");
                if(std::abs(arrow.left-panel.right-menuPanelGap*scale)>1)throw std::runtime_error("Editor not anchored next to expander");
                const float cy=(arrow.top+arrow.bottom)*.5f;
                for(float x=(arrow.left+arrow.right)*.5f;x>panel.right-10*scale;x-=scale){pointer(x,cy);if(!shapePanelOpen)throw std::runtime_error("Moving from expander to panel collapsed editor");}
                // The entire red corridor keeps the editor open, but never confirms.
                for(float y=std::min(float(arrow.top),float(panel.top))+scale;y<float(mask.bottom)-scale;y+=3*scale){
                    pointer((panel.right+arrow.left)*.5f,y);
                    POINT p{LONG((panel.right+arrow.left)*.5f),LONG(y)};
                    if(!shapePanelOpen || holdActionAt(p)!=-1 || shapeEditor.hit(p))throw std::runtime_error("Full-height editor corridor collapsed or gained commit hit");
                }
                pointer((panel.left+panel.right)*.5f,(panel.top+panel.bottom)*.5f);
                for(int n=0;n<20;++n)numberFrame(ImGuiKey_None,false);
                if(!shapePanelOpen || shapeEditor.draft!=switchDraft || testedMaskEvents.size()!=switchEvents)throw std::runtime_error("Panel dwell altered state");
                if(mask.right){pointer(mask.left+10*scale,mask.top+5*scale);if(!shapePanelOpen)throw std::runtime_error("Moving to mask collapsed editor");}
                pointer(pieCenter.x,pieCenter.y);
                if(!shapePanelOpen || !shapeEditor.bounds.right || testedMaskEvents.size()!=switchEvents || testedActions.size()!=switchActions)
                    throw std::runtime_error("Leaving editor implicitly collapsed or wrote preview");
                // A drag across the icon cannot defer a collapse until release.
                io.AddMouseButtonEvent(0,true);numberFrame(ImGuiKey_None,false);
                pointer((arrow.left+arrow.right)*.5f,cy);io.AddMouseButtonEvent(0,false);numberFrame(ImGuiKey_None,false);
                if(!shapePanelOpen)throw std::runtime_error("Drag release over icon collapsed editor");
                toggleEditor();
                if(shapePanelOpen || shapeEditor.bounds.right || testedMaskEvents.size()!=switchEvents || testedActions.size()!=switchActions)
                    throw std::runtime_error("Clean icon collapse did not restore ordinary menu without writes");
                for(int n=0;n<6;++n)numberFrame(ImGuiKey_None,false);
                if(shapePanelOpen)throw std::runtime_error("Collapsed icon immediately reopened on hover");
                pointer(pieCenter.x,pieCenter.y);
                pointer((arrow.left+arrow.right)*.5f,cy);shapeEditor.current()["rgb"]={72,91,120};shapeEditor.changed();numberFrame(ImGuiKey_None,false);
                const auto ownedDraft=shapeEditor.draft;const auto ownedEvents=testedMaskEvents.size();pointer(pieCenter.x,pieCenter.y);
                if(!shapePanelOpen || shapeReturning || testedMaskEvents.size()!=ownedEvents)throw std::runtime_error("Dirty editor hover exit implicitly confirmed");
                toggleEditor();
                if(!shapeReturning || !taskBusy || testedMaskEvents.size()!=ownedEvents+1 || !testedMaskEvents.back().value("return_to_menu",false) || testedMaskEvents.back()["params"]!=ownedDraft)
                    throw std::runtime_error("Dirty icon collapse did not confirm once with protection");
                accept({{"type","preview.menu.confirmed"},{"epoch",menuEpoch+1}});if(!shapeReturning)throw std::runtime_error("Stale return accepted");
                accept({{"type","preview.menu.confirmed"},{"epoch",menuEpoch}});if(shapeEditor.available || !taskBusy)throw std::runtime_error("Fresh selection protection bypassed");
                taskBusy=busyFeedback=false;allowed=capable=true;shapeEditor.prepare(shapeFixture);setHoldMode(true);io.AddMousePosEvent(-1000,-1000);numberFrame(ImGuiKey_None,false);
                if(shapePanelOpen)throw std::runtime_error("New session inherited expanded editor");
                // Switching back to a shorter
                // fill panel must not move its header out from under the pointer.
                pointer((shapeModeBounds.left+shapeModeBounds.right)*.5f,(shapeModeBounds.top+shapeModeBounds.bottom)*.5f);
                const auto channelEvents=testedMaskEvents.size();
                auto hoverClick=[&](int tip){const auto rect=shapeTipRows[tip];
                    pointer((rect.left+rect.right)*.5f,(rect.top+rect.bottom)*.5f);
                    io.AddMouseButtonEvent(0,true);numberFrame(ImGuiKey_None,false);
                    io.AddMouseButtonEvent(0,false);numberFrame(ImGuiKey_None,false);
                    for(int n=0;n<5;++n)numberFrame(ImGuiKey_None,false);
                    if(!shapePanelOpen || shapeReturning)throw std::runtime_error("Fill/stroke switch collapsed hover editor: "+std::to_string(tip));};
                const auto stableTop=shapeEditor.bounds.top;
                for(int i=0;i<6;++i){hoverClick(1);hoverClick(0);}
                if(testedMaskEvents.size()!=channelEvents)throw std::runtime_error("Channel selection wrote or confirmed preview");
                for(int tip:{3,2,4,2,1,3,2,4,2,0}){
                    hoverClick(tip);
                    if(shapeEditor.bounds.top!=stableTop)throw std::runtime_error("Paint type moved editor header");
                    for(size_t n=channelEvents;n<testedMaskEvents.size();++n)
                        if(testedMaskEvents[n].value("action",std::string())!="mask.preview.update")throw std::runtime_error("Paint type switch implicitly confirmed preview");
                }
                pointer(pieCenter.x,pieCenter.y);
                if(!shapePanelOpen || shapeReturning)throw std::runtime_error("Paint switch hover exit implicitly confirmed");
                toggleEditor();
                if(!shapeReturning || !testedMaskEvents.back().value("return_to_menu",false))throw std::runtime_error("Icon after paint switch did not confirm once");
                taskBusy=busyFeedback=false;allowed=capable=true;shapeEditor.prepare(shapeFixture);setHoldMode(true);io.AddMousePosEvent(-1000,-1000);numberFrame(ImGuiKey_None,false);
                std::cout<<"Fill/stroke/type hover switching passed: 22 clicks, stable header, no implicit commit, icon hover confirms once\n";
                // Releasing a color drag outside must not implicitly commit.
                pointer((shapeModeBounds.left+shapeModeBounds.right)*.5f,(shapeModeBounds.top+shapeModeBounds.bottom)*.5f);
                auto dragSquare=perceptualPicker.squareBounds;pointer((dragSquare.left+dragSquare.right)*.5f,(dragSquare.top+dragSquare.bottom)*.5f);
                io.AddMouseButtonEvent(0,true);numberFrame(ImGuiKey_None,false);
                pointer(pieCenter.x,pieCenter.y);io.AddMouseButtonEvent(0,false);numberFrame(ImGuiKey_None,false);
                for(int n=0;n<6;++n)numberFrame(ImGuiKey_None,false);
                if(!shapePanelOpen || shapeReturning)throw std::runtime_error("Released color drag implicitly closed picker");
                pointer(shapeEditor.bounds.left+10*scale,shapeEditor.bounds.top+10*scale);
                // Sample through the production native message path. No OS input is injected.
                const auto beforeSample=testedMaskEvents.size();const auto sampleOriginal=shapeEditor.original;
                SetCapture(window);SetPropW(window,screenSampleProperty,window);
                if(!screenSampleCaptured(window,window) || screenSampleCaptured(window,nullptr))throw std::runtime_error("Sampler capture/foreground gate failed");
                ReleaseCapture();
                if(screenSampleCaptured(window,window))throw std::runtime_error("Stale screen sample marker accepted without capture");
                RemovePropW(window,screenSampleProperty);SetCapture(window);
                if(screenSampleCaptured(window,window))throw std::runtime_error("Ordinary editor capture accepted as screen sample");
                ReleaseCapture();
                if(!beginShapeEyedrop())throw std::runtime_error("Eyedropper did not arm");
                pointer(pieCenter.x,pieCenter.y);if(!shapePanelOpen)throw std::runtime_error("Sampling collapsed editor");
                // A memory DC is not used: read an actual visible pixel, then compare the consumed click.
                POINT pixelPoint{10,10};ClientToScreen(window,&pixelPoint);
                HDC sampleDC=GetDC(nullptr);const auto pixel=GetPixel(sampleDC,pixelPoint.x,pixelPoint.y);ReleaseDC(nullptr,sampleDC);
                if(pixel==CLR_INVALID)throw std::runtime_error("Desktop pixel not readable");
                if(!shapeEyedropMessage(WM_LBUTTONDOWN,0,MAKELPARAM(10,10)) || !shapeEyedropMessage(WM_LBUTTONUP,0,MAKELPARAM(10,10)))throw std::runtime_error("Sample click was not consumed");
                numberFrame(ImGuiKey_None,false);
                if(shapeEyedropActive || !shapePanelOpen || shapeReturning || shapeEditor.current()["rgb"]!=Json::array({GetRValue(pixel),GetGValue(pixel),GetBValue(pixel)}) || shapeEditor.original!=sampleOriginal)
                    throw std::runtime_error("Screen sample lost color, original, or expanded panel");
                for(size_t n=beforeSample;n<testedMaskEvents.size();++n)if(testedMaskEvents[n]["action"]!="mask.preview.update")throw std::runtime_error("Sample implicitly committed");
                const auto sampledDraft=shapeEditor.draft;beginShapeEyedrop();shapeEyedropMessage(WM_RBUTTONDOWN,0,0);
                if(shapeEyedropActive || shapeEditor.draft!=sampledDraft)throw std::runtime_error("Eyedrop cancellation changed paint");
                beginShapeEyedrop();if(shapeEyedropMessage(WM_KEYDOWN,VK_ESCAPE,0) || shapeEyedropActive || shapeEditor.draft!=sampledDraft)throw std::runtime_error("Eyedropper swallowed menu Esc rollback");
                beginShapeEyedrop();++menuEpoch;if(applyShapeSample(RGB(1,2,3)) || shapeEditor.draft!=sampledDraft)throw std::runtime_error("Stale sample session accepted");--menuEpoch;
                beginShapeEyedrop();shapeEditor.draft["revision"]="changed";if(applyShapeSample(RGB(1,2,3)))throw std::runtime_error("Stale sample revision accepted");shapeEditor.draft=sampledDraft;
                beginShapeEyedrop();if(applyShapeSample(CLR_INVALID) || shapeEditor.draft!=sampledDraft)throw std::runtime_error("Failed pixel read changed paint");
                beginShapeEyedrop();inputFocused=true;if(applyShapeSample(RGB(1,2,3)))throw std::runtime_error("Input protection accepted screen sample");inputFocused=false;
                beginShapeEyedrop();panelKind=0;if(applyShapeSample(RGB(1,2,3)))throw std::runtime_error("Canvas accepted shape screen sample");panelKind=2;
                beginShapeEyedrop();shapeEyedropMessage(WM_CAPTURECHANGED,0,0);if(shapeEyedropActive)throw std::runtime_error("Lost capture retained sampling");
                // Gradient sampling changes only the selected stop, preserving order and source rollback.
                shapeEditor.mode(1);shapeEditor.stop=1;const auto gradientBefore=shapeEditor.current();
                beginShapeEyedrop();if(!applyShapeSample(RGB(63,127,191)))throw std::runtime_error("Gradient sample rejected");
                if(shapeEditor.current()["stops"][0]!=gradientBefore["stops"][0] || shapeEditor.current()["stops"][1]["location"]!=gradientBefore["stops"][1]["location"] || shapeEditor.current()["stops"][1]["rgb"]!=Json({63,127,191}) || shapeEditor.original!=sampleOriginal)
                    throw std::runtime_error("Sampling changed gradient neighbours, order or rollback");
                shapeEditor.mode(0);numberFrame(ImGuiKey_None,false);
                const auto afterSamples=testedMaskEvents.size();
                for(int n=0;n<6;++n){pointer(shapeEditor.bounds.left+10*scale,shapeEditor.bounds.top+10*scale);pointer(pieCenter.x,pieCenter.y);pointer(-1000,-1000);}
                if(!shapePanelOpen || shapeReturning || testedMaskEvents.size()!=afterSamples)throw std::runtime_error("Returning and leaving after screen sample implicitly confirmed");
                // Sample exactly over the collapse icon via the real WndProc:
                // this click must be consumed, never becoming an icon click.
                beginShapeEyedrop();const auto sampleIcon=shapeModeBounds;
                pointer((sampleIcon.left+sampleIcon.right)*.5f,(sampleIcon.top+sampleIcon.bottom)*.5f);
                const auto samplePoint=MAKELPARAM((sampleIcon.left+sampleIcon.right)/2,(sampleIcon.top+sampleIcon.bottom)/2);
                procedure(window,WM_LBUTTONDOWN,MK_LBUTTON,samplePoint);procedure(window,WM_LBUTTONUP,0,samplePoint);numberFrame(ImGuiKey_None,false);
                if(!shapePanelOpen || shapeReturning || shapeEyedropActive)throw std::runtime_error("Screen sample leaked to collapse icon");
                toggleEditor();if(!shapeReturning)throw std::runtime_error("Explicit icon after screen sample did not confirm");
                // Compatibility mode has no live request: the same explicit
                // icon submits its local draft once rather than trapping it.
                taskBusy=busyFeedback=false;allowed=capable=true;
                auto localCollapse=shapeFixture;localCollapse["live_preview"]=false;
                shapeEditor.prepare(localCollapse);setHoldMode(true);shapePanelOpen=true;numberFrame(ImGuiKey_None,false);
                shapeEditor.current()["rgb"]={32,64,128};shapeEditor.changed();numberFrame(ImGuiKey_None,false);
                const auto localActions=testedActions.size(),localEvents=testedMaskEvents.size();const auto localDraft=shapeEditor.draft;
                pointer(pieCenter.x,pieCenter.y);
                if(!shapePanelOpen || shapeReturning || testedActions.size()!=localActions)throw std::runtime_error("Local draft hover exit submitted");
                toggleEditor();
                if(shapePanelOpen || !shapeReturning || !taskBusy || testedActions.size()!=localActions+1 || testedShapePaint!=localDraft || testedMaskEvents.size()!=localEvents)
                    throw std::runtime_error("Local draft icon collapse did not submit exactly once");
                taskBusy=busyFeedback=false;allowed=capable=true;shapeEditor.prepare(shapeFixture);setHoldMode(true);
                shapePanelOpen=true;numberFrame(ImGuiKey_None,false);
                std::cout<<"Screen eyedropper and hover regression passed: full-height neutral corridor, drag release hold, actual desktop pixel, WndProc consumed icon click, repeated outside/return retains picker, cancel/stale/failed read rejection\n";
                testedMaskEvents.resize(switchEvents);testedActions.resize(switchActions);
                std::cout<<"Hover editor path passed: anchored single expander, continuous travel, panel/mask dwell, hover-entry collapse without clicks, drag release and immediate reopen rejected, one dirty confirmation, stale terminal rejection\n";
                if(!shapeEditor.ready() || shapeEditor.bounds.right<=shapeEditor.bounds.left || ImGui::GetDrawData()->TotalVtxCount<=0)
                    throw std::runtime_error("Shape editor did not render through production window path");
                if(shapeTipRows[8].top!=shapeTipRows[10].top || shapeTipRows[10].top!=shapeTipRows[11].top ||
                   shapeTipRows[9].right>=shapeTipRows[10].left || shapeTipRows[10].right>=shapeTipRows[11].left ||
                   shapeTipRows[10].right-shapeTipRows[10].left>figmaDisplayIconWidth*scale+1)
                    throw std::runtime_error("Shape action toolbar lost compact non-overlapping groups: "+Json({
                        {shapeTipRows[8].left,shapeTipRows[8].top,shapeTipRows[8].right},
                        {shapeTipRows[9].left,shapeTipRows[9].top,shapeTipRows[9].right},
                        {shapeTipRows[10].left,shapeTipRows[10].top,shapeTipRows[10].right},
                        {shapeTipRows[11].left,shapeTipRows[11].top,shapeTipRows[11].right}}).dump()+" scale="+std::to_string(scale));
                for(int i=15;i<=21;++i)if(shapeTipRows[i].right || shapeTipRows[i].bottom)
                    throw std::runtime_error("Removed harmony mode retained a hidden hit target");
                if(PerceptualPickerState::markerRadius(0)<1.9f*PerceptualPickerState::markerRadius(1))
                    throw std::runtime_error("Main perceptual color marker lost its emphasis");
                const auto beforePaintEvents=testedMaskEvents.size();const auto beforePaintDraft=shapeEditor.draft;
                auto clickPaintChoice=[&](int tip){const auto r=shapeTipRows[tip];
                    io.AddMouseViewportEvent(ImGui::GetMainViewport()->ID);io.AddMousePosEvent((r.left+r.right)*.5f,(r.top+r.bottom)*.5f);
                    io.AddMouseButtonEvent(ImGuiMouseButton_Left,true);numberFrame(ImGuiKey_None,false);
                    io.AddMouseButtonEvent(ImGuiMouseButton_Left,false);numberFrame(ImGuiKey_None,false);};
                clickPaintChoice(5);
                if(shapeEditor.current().value("enabled",true) || shapeEditor.current()["rgb"]!=beforePaintDraft["fill"]["rgb"] ||
                   shapeEditor.draft["stroke"]!=beforePaintDraft["stroke"] || testedActions.size()!=shapeCount)
                    throw std::runtime_error("No-paint icon lost retained paint or submitted an action");
                clickPaintChoice(2);
                if(shapeEditor.draft!=beforePaintDraft || testedMaskEvents.size()!=beforePaintEvents+2)
                    throw std::runtime_error("Solid icon failed to restore retained paint in the owning preview");
                testedMaskEvents.resize(beforePaintEvents);shapeEditor.prepare(shapeFixture);numberFrame(ImGuiKey_None,false);
                if(shapeEditor.rgbBounds[0].top!=shapeEditor.rgbBounds[2].top || shapeEditor.rgbBounds[2].top!=shapeEditor.hexBounds.top ||
                   shapeEditor.rgbBounds[0].right!=shapeEditor.rgbBounds[1].left || shapeEditor.rgbBounds[1].right!=shapeEditor.rgbBounds[2].left ||
                   shapeEditor.rgbBounds[2].right>=shapeEditor.hexBounds.left || shapeEditor.copyBounds.right>=shapeEditor.hexBounds.right ||
                   shapeEditor.copyBounds.top<=shapeEditor.hexBounds.top)
                    throw std::runtime_error("RGB/Hex must share one row with inset copy inside Hex");
                const auto valueEvents=testedMaskEvents.size();const auto redRect=shapeEditor.rgbBounds[0];
                auto valueMouse=[&](const RECT& r,bool down){io.AddMouseViewportEvent(ImGui::GetMainViewport()->ID);
                    io.AddMousePosEvent((r.left+r.right)*.5f,(r.top+r.bottom)*.5f);io.AddMouseButtonEvent(ImGuiMouseButton_Left,down);numberFrame(ImGuiKey_None,false);};
                GImGui->Time+=io.MouseDoubleClickTime+.1f;valueMouse(redRect,false);valueMouse(redRect,true);valueMouse(redRect,false);valueMouse(redRect,true);
                if(!ImGui::TempInputIsActive(shapeEditor.rgbInputIDs[0]))throw std::runtime_error("RGB scalar did not enter numeric input: "+Json{
                    {"rect",{redRect.left,redRect.top,redRect.right,redRect.bottom}},
                    {"mouse",{io.MousePos.x,io.MousePos.y}},{"clicks",io.MouseClickedCount[0]},
                    {"active",ImGui::GetActiveID()},{"expected",shapeEditor.rgbInputIDs[0]},
                    {"hovered",GImGui->HoveredWindow?GImGui->HoveredWindow->Name:"none"},
                    {"font",uiFontSize},{"scale",scale},{"panel",{shapeEditor.bounds.left,shapeEditor.bounds.top,shapeEditor.bounds.right,shapeEditor.bounds.bottom}}}.dump());
                io.AddInputCharactersUTF8("73");numberFrame(ImGuiKey_None,false);numberFrame(ImGuiKey_Enter,true);numberFrame(ImGuiKey_Enter,false);valueMouse(redRect,false);
                if(shapeEditor.current()["rgb"]!=Json({73,34,56}) || testedActions.size()!=shapeCount)
                    throw std::runtime_error("RGB numeric entry changed other channels or submitted an action");
                auto enterHex=[&](const char* text){auto r=shapeEditor.hexBounds;r.right-=LONG(figmaDisplayIconWidth*scale);
                    GImGui->Time+=io.MouseDoubleClickTime+.1f;valueMouse(r,false);valueMouse(r,true);valueMouse(r,false);
                    io.AddKeyEvent(ImGuiMod_Ctrl,true);numberFrame(ImGuiKey_A,true);numberFrame(ImGuiKey_A,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);
                    io.AddInputCharactersUTF8(text);numberFrame(ImGuiKey_None,false);numberFrame(ImGuiKey_Enter,true);numberFrame(ImGuiKey_Enter,false);};
                enterHex("#30d86f");
                if(shapeEditor.current()["rgb"]!=Json({48,216,111}) || std::string(shapeEditor.hexInput)!="30D86F")
                    throw std::runtime_error("Hex entry failed to filter hash and uppercase digits");
                copyShapeHex(shapeEditor.current()["rgb"]);if(testedCopiedHex!="#30D86F")throw std::runtime_error("Hex copy lost canonical hash prefix");
                enterHex("112233");
                if(shapeEditor.current()["rgb"]!=Json({17,34,51}) || testedActions.size()!=shapeCount || testedMaskEvents.size()<=valueEvents)
                    throw std::runtime_error("Unprefixed Hex failed to update only the owning preview");
                for(const auto& model:std::vector<std::string>{"cmyk","lab","gray"}){
                    auto nativeFixture=shapeFixture;nativeFixture["live_preview"]=false;nativeFixture["native_model"]=model;
                    const Json values=model=="cmyk"?Json::array({12.345,20,30,4}):model=="lab"?Json::array({55.5,-12.75,27}):Json::array({62.5});
                    nativeFixture["fill"]=Json{{"kind","solid"},{"enabled",true},{"native",{{"model",model},{"values",values}}}};
                    shapeEditor.prepare(nativeFixture);setHoldMode(true);shapePanelOpen=true;numberFrame(ImGuiKey_None,false);numberFrame(ImGuiKey_None,false);
                    if(shapeEditor.hexBounds.right || shapeEditor.rgbBounds[values.size()-1].right<=shapeEditor.rgbBounds[0].left || beginShapeEyedrop())
                        throw std::runtime_error("Native color channels retained RGB conversion controls or lost inputs");
                    const int index=model=="lab"?1:0;const auto rect=shapeEditor.rgbBounds[index];
                    GImGui->Time+=io.MouseDoubleClickTime+.1f;valueMouse(rect,false);valueMouse(rect,true);valueMouse(rect,false);valueMouse(rect,true);
                    if(!ImGui::TempInputIsActive(shapeEditor.rgbInputIDs[index]))throw std::runtime_error("Native color scalar did not enter numeric input");
                    io.AddInputCharactersUTF8(model=="lab"?"-25.75":"37.25");numberFrame(ImGuiKey_None,false);numberFrame(ImGuiKey_Enter,true);numberFrame(ImGuiKey_Enter,false);valueMouse(rect,false);
                    if(std::abs(shapeEditor.current()["native"]["values"][index].get<double>()-(model=="lab"?-25.75:37.25))>.001 || !shapeEditor.dirty)
                        throw std::runtime_error("Native color scalar lost fractional or negative channel value");
                    shapeEditor.mode(1);numberFrame(ImGuiKey_None,false);
                    if(!shapeEditor.current()["stops"][0].contains("native") || !shapeEditor.current()["stops"][1].contains("native"))
                        throw std::runtime_error("Native gradient conversion changed the color model");
                }
                shapeEditor.prepare(shapeFixture);setHoldMode(true);shapePanelOpen=true;numberFrame(ImGuiKey_None,false);
                const auto idleDraft=shapeEditor.draft;const auto idleEvents=testedMaskEvents.size();
                for(int frame=0;frame<120;++frame)numberFrame(ImGuiKey_None,false);
                if(shapeEditor.draft!=idleDraft || testedMaskEvents.size()!=idleEvents)
                    throw std::runtime_error("Idle perceptual picker issued a preview without input");
                // Reproduce the live 60ms handoff, not only a final static tip.
                const auto tipDraft=shapeEditor.draft;const auto tipEvents=testedMaskEvents.size();
                hoverTip.reset();auto firstTipPoint=perceptualPicker.points[1];
                io.AddMousePosEvent(firstTipPoint.x,firstTipPoint.y);numberFrame(ImGuiKey_None,false);
                hoverTip.pendingSince=GetTickCount64()-200;numberFrame(ImGuiKey_None,false);
                if(hoverTip.shown!=226 || !tipVisual.ready)throw std::runtime_error("First adjacent tooltip missing");
                const auto heldTipTarget=tipVisual.to;auto nextTipPoint=perceptualPicker.points[2];
                io.AddMousePosEvent(nextTipPoint.x,nextTipPoint.y);numberFrame(ImGuiKey_None,false);
                if(shapeTipCandidate!=227 || hoverTip.shown!=226 || tipVisual.to.x!=heldTipTarget.x || tipVisual.to.y!=heldTipTarget.y ||
                   tipVisual.to.z!=heldTipTarget.z || tipVisual.to.w!=heldTipTarget.w)
                    throw std::runtime_error("Retained adjacent tooltip lost its anchor during the 60ms handoff");
                hoverTip.pendingSince=GetTickCount64()-80;numberFrame(ImGuiKey_None,false);
                if(hoverTip.shown!=227)throw std::runtime_error("Adjacent tooltip did not switch after stable hover");
                const auto hueTipRect=perceptualPicker.hueBounds,svTipRect=perceptualPicker.squareBounds;
                io.AddMousePosEvent((hueTipRect.left+hueTipRect.right)*.5f,(hueTipRect.top+hueTipRect.bottom)*.5f);numberFrame(ImGuiKey_None,false);
                if(shapeTipCandidate!=223 || shapeTipRows[23].left!=hueTipRect.left || shapeTipRows[23].right!=hueTipRect.right)
                    throw std::runtime_error("Hue tooltip uses the whole picker instead of its strip");
                io.AddMousePosEvent(hueTipRect.right+2*scale,nextTipPoint.y);numberFrame(ImGuiKey_None,false);
                if(shapeTipCandidate>=0 || hoverTip.shown!=-1 || tipVisual.ready)throw std::runtime_error("Neutral picker gap advertised a hue tooltip");
                io.AddMousePosEvent((svTipRect.left+svTipRect.right)*.5f,(svTipRect.top+svTipRect.bottom)*.5f);numberFrame(ImGuiKey_None,false);
                if(shapeTipCandidate!=224 || shapeTipRows[24].left!=svTipRect.left)throw std::runtime_error("SV tooltip shares the hue target");
                if(shapeEditor.draft!=tipDraft || testedMaskEvents.size()!=tipEvents)throw std::runtime_error("Tooltip handoff wrote the preview");
                std::cout<<"Picker tooltip handoff passed: retained adjacent anchor during delay, stable text change, independent hue/SV, neutral gap without false hue, no preview writes\n";
                for(int i=0;i<perceptual::wheelCount;++i){
                    const auto point=perceptualPicker.points[i];const auto base=perceptualPicker.base;
                    io.AddMousePosEvent(point.x,point.y);numberFrame(ImGuiKey_None,false);
                    if(shapeTipCandidate!=225+i)throw std::runtime_error("Wheel point tooltip did not identify its visible role");
                    io.AddMouseButtonEvent(0,true);numberFrame(ImGuiKey_None,false);
                    const auto heldDraft=shapeEditor.draft;const auto heldEvents=testedMaskEvents.size();
                    for(int frame=0;frame<30;++frame)numberFrame(ImGuiKey_None,false);
                    if(shapeEditor.draft!=heldDraft || testedMaskEvents.size()!=heldEvents)
                        throw std::runtime_error("Stationary wheel handle issued repeated previews");
                    io.AddMouseButtonEvent(0,false);numberFrame(ImGuiKey_None,false);
                    if(perceptualPicker.active!=i || perceptualPicker.base!=base || testedActions.size()!=shapeCount)
                        throw std::runtime_error("Wheel point pickup changed the base or dispatched a second task");
                }
                // A hovered, still-editable color control must retain its
                // read-only tip after the first write starts the shared preview.
                hoverTip.reset();const auto roleHover=perceptualPicker.points[3];
                io.AddMousePosEvent(roleHover.x,roleHover.y);numberFrame(ImGuiKey_None,false);
                hoverTip.pendingSince=GetTickCount64()-200;numberFrame(ImGuiKey_None,false);
                if(!maskPreviewStarted || canExecute() || !shapeEditor.ready() || hoverTip.shown!=228 || !tipVisual.ready)
                    throw std::runtime_error("Owning live preview recognized hover but suppressed its visible tooltip");
                const auto tipTestTheme=uiTheme,tipTestFont=uiFontSize;auto* tipTestDefault=io.FontDefault;
                for(const char* themeName:{"light","light-gray","dark-gray","dark"})for(const char* fontName:{"small","large"})for(float panelHeight:{500.f,620.f}){
                    uiTheme=themeName;theme();uiFontSize=fontName;io.FontDefault=menuFont(MenuTextRole::Body);
                    SetWindowPos(window,nullptr,0,0,int(920*scale),int(panelHeight*scale),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
                    ImGui_ImplWin32_NewFrame();numberFrame(ImGuiKey_None,false);
                    for(int i=0;i<perceptual::wheelCount;++i){
                        const auto p=perceptualPicker.points[i];hoverTip.reset();io.AddMousePosEvent(p.x,p.y);numberFrame(ImGuiKey_None,false);
                        hoverTip.pendingSince=GetTickCount64()-200;numberFrame(ImGuiKey_None,false);
                        if(hoverTip.shown!=225+i || !tipVisual.ready || tipVisual.bounds.z<=tipVisual.bounds.x ||
                           tipOverlaps(tipVisual.bounds,shapeEditor.bounds) || tipOverlaps(tipVisual.bounds,shapeEditor.maskBounds))
                            throw std::runtime_error("Live preview tooltip clipped or missing in theme/font/height matrix: "+std::to_string(i)+" "+themeName+" "+fontName+" "+std::to_string(panelHeight));
                        const auto marker=shapeTipRows[25+i];const auto tip=tipVisual.bounds;
                        const float tipDx=std::max({tip.x-marker.right,float(marker.left)-tip.z,0.f});
                        const float tipDy=std::max({tip.y-marker.bottom,float(marker.top)-tip.w,0.f});
                        if(std::hypot(tipDx,tipDy)>60*scale)throw std::runtime_error("Harmony tooltip drifted away from its marker");
                        if(canExecute() || capsEnabled(0) || holdActionAt({LONG((tipVisual.bounds.x+tipVisual.bounds.z)*.5f),LONG((tipVisual.bounds.y+tipVisual.bounds.w)*.5f)})!=-1)
                            throw std::runtime_error("Tooltip display changed input or operation permissions");
                    }
                }
                uiTheme=tipTestTheme;theme();uiFontSize=tipTestFont;io.FontDefault=tipTestDefault;
                SetWindowPos(window,nullptr,0,0,int(920*scale),int(menuShapeWindowHeight*scale),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
                ImGui_ImplWin32_NewFrame();numberFrame(ImGuiKey_None,false);
                inputFocused=true;numberFrame(ImGuiKey_None,false);
                if(tipVisual.ready || hoverTip.shown!=-1)throw std::runtime_error("Input protection retained a visible editing tooltip");
                inputFocused=false;numberFrame(ImGuiKey_None,false);
                const auto wheelDraft=shapeEditor.draft;const auto dot=perceptualPicker.points[3];
                io.AddMousePosEvent(dot.x,dot.y);numberFrame(ImGuiKey_None,false);io.AddMouseButtonEvent(0,true);numberFrame(ImGuiKey_None,false);
                const auto wb=perceptualPicker.hueBounds;
                io.AddMousePosEvent(dot.x,dot.y+(wb.bottom-wb.top)*.2f);numberFrame(ImGuiKey_None,false);
                io.AddMouseButtonEvent(0,false);numberFrame(ImGuiKey_None,false);
                if(shapeEditor.draft==wheelDraft || testedActions.size()!=shapeCount || !maskPreviewStarted || perceptualPicker.active!=3)
                    throw std::runtime_error("Perceptual companion drag did not update only its owning preview");
                const auto hb=perceptualPicker.hueBounds;
                if(hb.left<=perceptualPicker.squareBounds.right ||
                   perceptualPicker.points[0].x-PerceptualPickerState::markerExtent(0)*scale<=hb.right)
                    throw std::runtime_error("Hue, SV and harmony markers overlap");
                const auto hueBase=perceptualPicker.base;
                io.AddMousePosEvent((hb.left+hb.right)*.5f,hb.top+(hb.bottom-hb.top)*.2f);numberFrame(ImGuiKey_None,false);
                io.AddMouseButtonEvent(0,true);numberFrame(ImGuiKey_None,false);
                io.AddMousePosEvent((hb.left+hb.right)*.5f,hb.top+(hb.bottom-hb.top)*.7f);numberFrame(ImGuiKey_None,false);
                io.AddMouseButtonEvent(0,false);numberFrame(ImGuiKey_None,false);
                if(std::abs(perceptualPicker.base[0]-hueBase[0])>.0001 || std::abs(perceptualPicker.base[1]-hueBase[1])>.0001 ||
                   std::abs(perceptualPicker.base[2]-hueBase[2])<1 || perceptualPicker.active!=0 || testedActions.size()!=shapeCount)
                    throw std::runtime_error("Independent hue strip changed lightness/chroma or dispatched another task");
                const auto square=perceptualPicker.squareBounds;
                if(square.right-square.left<40*scale || perceptualPicker.lightnessBounds.right)
                    throw std::runtime_error("Large square or retired lightness hit bounds are wrong");
                io.AddMousePosEvent(square.left+(square.right-square.left)*.8f,square.top+(square.bottom-square.top)*.3f);
                numberFrame(ImGuiKey_None,false);io.AddMouseButtonEvent(0,true);numberFrame(ImGuiKey_None,false);
                const auto squareDraft=shapeEditor.draft;
                io.AddMousePosEvent(square.left+(square.right-square.left)*.3f,square.top+(square.bottom-square.top)*.6f);numberFrame(ImGuiKey_None,false);
                io.AddMouseButtonEvent(0,false);numberFrame(ImGuiKey_None,false);
                if(shapeEditor.draft==squareDraft || perceptualPicker.active!=0 || testedActions.size()!=shapeCount)
                    throw std::runtime_error("Square value drag failed or dispatched another task");
                const auto stillDraft=shapeEditor.draft;const auto stillEvents=testedMaskEvents.size();
                for(int frame=0;frame<30;++frame)numberFrame(ImGuiKey_None,false);
                if(shapeEditor.draft!=stillDraft || testedMaskEvents.size()!=stillEvents)
                    throw std::runtime_error("Idle square issued repeated previews");
                testedMaskEvents.resize(valueEvents);shapeEditor.prepare(shapeFixture);numberFrame(ImGuiKey_None,false);
                const auto featherRect=shapeEditor.featherBounds,densityRect=shapeEditor.densityBounds;
                RECT shapeWindow;GetWindowRect(window,&shapeWindow);
                if(featherRect.right<=featherRect.left || densityRect.right<=densityRect.left || densityRect.bottom>shapeEditor.maskBounds.bottom ||
                   shapeEditor.maskBounds.left!=shapeEditor.bounds.left || shapeEditor.maskBounds.right!=shapeEditor.bounds.right ||
                   shapeEditor.maskBounds.top-shapeEditor.bounds.bottom<LONG(menuPanelGap*scale)-1 ||
                   featherRect.top<shapeEditor.maskBounds.top || shapeEditor.maskBounds.bottom>shapeWindow.bottom)
                    throw std::runtime_error("Vector mask sliders are clipped or have no hit bounds");
                POINT panelGap={shapeEditor.bounds.left+LONG(10*scale),(shapeEditor.bounds.bottom+shapeEditor.maskBounds.top)/2};
                if(holdActionAt(panelGap)!=-1 || shapeEditor.hit(panelGap))throw std::runtime_error("Split panel gap must cancel, never confirm");
                auto maskMouse=[&](const RECT& r,float portion,bool down){io.AddMouseViewportEvent(ImGui::GetMainViewport()->ID);
                    io.AddMousePosEvent(r.left+(r.right-r.left)*portion,(r.top+r.bottom)*.5f);
                    io.AddMouseButtonEvent(ImGuiMouseButton_Left,down);numberFrame(ImGuiKey_None,false);};
                maskMouse(featherRect,.6f,false);maskMouse(featherRect,.6f,true);maskMouse(featherRect,.7f,true);maskMouse(featherRect,.7f,false);
                maskMouse(densityRect,.35f,false);maskMouse(densityRect,.35f,true);maskMouse(densityRect,.4f,true);maskMouse(densityRect,.4f,false);
                if(!shapeEditor.dirty || shapeEditor.draft["mask"]["feather"].get<float>()<=0 ||
                    shapeEditor.draft["mask"]["density"].get<float>()>=100 || testedActions.size()!=shapeCount)
                    throw std::runtime_error("Mask sliders must change the local draft without submitting");
                if(!maskPreviewStarted || !shapeEditor.ready() || canExecute() || capsEnabled(0))
                    throw std::runtime_error("Preview must preserve only its owning editor, rejecting other actions");
                GImGui->Time+=ImGui::GetIO().MouseDoubleClickTime+.1f;
                maskMouse(featherRect,.65f,false);maskMouse(featherRect,.65f,true);maskMouse(featherRect,.65f,false);maskMouse(featherRect,.65f,true);
                if(!ImGui::TempInputIsActive(shapeEditor.maskInputIDs[0]))throw std::runtime_error("Feather double click must open numeric input");
                io.AddInputCharactersUTF8("17.25");numberFrame(ImGuiKey_None,false);
                const auto inputPaintKind=shapeEditor.current().at("kind");numberFrame(ImGuiKey_Tab,true);numberFrame(ImGuiKey_Tab,false);
                if(shapeEditor.current().at("kind")!=inputPaintKind)throw std::runtime_error("Numeric input Tab switched paint type");
                numberFrame(ImGuiKey_Enter,true);numberFrame(ImGuiKey_Enter,false);
                maskMouse(featherRect,.65f,false);
                if(std::abs(shapeEditor.draft["mask"]["feather"].get<float>()-17.25f)>.01f)throw std::runtime_error("Numeric feather input did not update preview");
                POINT maskPoint={(densityRect.left+densityRect.right)/2,(densityRect.top+densityRect.bottom)/2};
                const auto maskDraft=shapeEditor.draft;const auto maskGrants=testedShapeGrants;renderedHit=100;finishHold(maskPoint,true,&maskPoint);const auto maskSubmitted=testedActions.size();finishHold(maskPoint);
                if(testedShapeGrants!=maskGrants+1)throw std::runtime_error("Preview confirmation must grant its backend focus exactly once");
                if(testedShapePaint!=maskDraft || testedActions.back()!="shape.paint" || testedActions.size()!=maskSubmitted)
                    throw std::runtime_error("Mask slider release must submit its exact draft once");
                if(testedMaskEvents.size()<3 || testedMaskEvents.front()["action"]!="mask.preview.update" ||
                   testedMaskEvents.back()["action"]!="mask.preview.commit" || testedMaskEvents.back()["params"]!=maskDraft)
                    throw std::runtime_error("Mask preview must stream updates then commit its exact final draft");
                const auto oldBatchTheme=uiTheme;
                Json batchFixture={{"available",true},{"mask_batch",true},{"revision","batch:1"},{"layer_ids",{30,31}},
                    {"active_layer_id",31},{"mask",{{"feather",1},{"density",80}}},{"mixed",{{"feather",true},{"density",true}}},{"live_preview",true}};
                for(const char* name:{"light","light-gray","dark-gray","dark"}) {
                    uiTheme=name;theme();shapeEditor.prepare(batchFixture);setHoldMode(true);shapePanelOpen=true;layerCount=2;layerKind=8;numberFrame(ImGuiKey_None,false);holdMoved=true;
                    if(!shapeEditor.ready() || shapeEditor.bounds.right || shapeEditor.maskBounds.right<=shapeEditor.maskBounds.left || shapeEditor.dirty)
                        throw std::runtime_error("Batch mask panel must show only sliders with untouched mixed values: "+Json{{"ready",shapeEditor.ready()},{"color_right",shapeEditor.bounds.right},{"mask_left",shapeEditor.maskBounds.left},{"mask_right",shapeEditor.maskBounds.right},{"dirty",shapeEditor.dirty},{"draft",shapeEditor.draft},{"want_input",ImGui::GetIO().WantTextInput},{"input_guard",inputFocused}}.dump());
                    const auto batchFeather=shapeEditor.featherBounds;
                    maskMouse(batchFeather,.5f,false);maskMouse(batchFeather,.5f,true);maskMouse(batchFeather,.7f,true);maskMouse(batchFeather,.7f,false);
                    if(!shapeEditor.dirty || !shapeEditor.draft["mask"].contains("feather") || shapeEditor.draft["mask"].contains("density") || shapeEditor.draft.contains("fill"))
                        throw std::runtime_error("Batch feather must not overwrite per-layer density or paint");
                    const auto batchDensity=shapeEditor.densityBounds;
                    maskMouse(batchDensity,.4f,false);maskMouse(batchDensity,.4f,true);maskMouse(batchDensity,.6f,true);maskMouse(batchDensity,.6f,false);
                    if(!shapeEditor.draft["mask"].contains("density") || testedMaskEvents.back()["params"]!=shapeEditor.draft)
                        throw std::runtime_error("Batch mask changes must share one latest preview");
                    inputFocused=true;if(shapeEditor.ready())throw std::runtime_error("Batch editor ignored input protection");inputFocused=false;
                    layerLocked=true;if(shapeEditor.ready())throw std::runtime_error("Batch editor ignored locked target");layerLocked=false;
                    panelKind=0;if(shapeEditor.ready())throw std::runtime_error("Batch editor leaked into canvas");panelKind=2;
                    layerCount=1;if(shapeEditor.ready())throw std::runtime_error("Batch editor accepted changed selection count");layerCount=2;
                    const auto finalBatch=shapeEditor.draft;POINT point={(batchDensity.left+batchDensity.right)/2,(batchDensity.top+batchDensity.bottom)/2};
                    renderedHit=100;finishHold(point,true,&point);const auto completed=testedActions.size();finishHold(point);
                    if(testedActions.size()!=completed || testedShapePaint!=finalBatch || testedMaskEvents.back()["action"]!="mask.preview.commit")
                        throw std::runtime_error("Batch confirmation must submit once");
                    testedActions.resize(shapeCount);
                }
                uiTheme=oldBatchTheme;theme();layerCount=1;layerKind=6;
                testedActions.resize(shapeCount);shapeEditor.prepare(shapeFixture);setHoldMode(true);shapePanelOpen=true;numberFrame(ImGuiKey_None,false);holdMoved=true;
                shapeEditor.mode(1);shapeEditor.mode(2);shapeEditor.mode(0);
                if(testedActions.size()!=shapeCount || shapeEditor.dirty)throw std::runtime_error("Local type switching wrote or lost original draft");
                Json parsed;
                if(!parseShapeHex("#3a8185",parsed) || parsed!=Json({58,129,133}) || shapeHex(parsed)!="#3A8185" ||
                    !parseShapeHex("FFFFFF",parsed) || parseShapeHex("#12345",parsed) || parseShapeHex("#XX8185",parsed))
                    throw std::runtime_error("Hex input validation failed");
                copyShapeHex(shapeEditor.current()["rgb"]);
                if(testedCopiedHex!="#0C2238" || testedActions.size()!=shapeCount || shapeEditor.dirty)
                    throw std::runtime_error("Hex copy changed the draft or submitted Photoshop action");
                for(int context:{0,1,3,4}){panelKind=context;numberFrame(ImGuiKey_None,false);
                    if(shapeEditor.ready() || shapeEditor.bounds.right || shapeEditor.maskBounds.right)throw std::runtime_error("Shape editor escaped Layers context");}
                panelKind=2;
                for(int kind:{0,1,2,3,4,5,7,8,9}){layerKind=kind;numberFrame(ImGuiKey_None,false);
                    if(shapeEditor.ready() || shapeEditor.bounds.right)throw std::runtime_error("Stale shape metadata enabled a nonshape layer");}
                layerKind=6;layerCount=2;numberFrame(ImGuiKey_None,false);
                if(shapeEditor.ready())throw std::runtime_error("Multiple shapes enabled color application");
                layerCount=1;layerLocked=true;numberFrame(ImGuiKey_None,false);
                if(shapeEditor.ready())throw std::runtime_error("Locked shape enabled color application");
                layerLocked=false;numberFrame(ImGuiKey_None,false);
                testedCopiedHex.clear();pinned=true;
                const auto copyRect=shapeEditor.copyBounds;
                io.AddMouseViewportEvent(ImGui::GetMainViewport()->ID);
                io.AddMousePosEvent((copyRect.left+copyRect.right)*.5f,(copyRect.top+copyRect.bottom)*.5f);
                numberFrame(ImGuiKey_None,false);
                io.AddMouseButtonEvent(ImGuiMouseButton_Left,true);numberFrame(ImGuiKey_None,false);
                io.AddMouseButtonEvent(ImGuiMouseButton_Left,false);numberFrame(ImGuiKey_None,false);
                if(testedCopiedHex!="#0C2238" || shapeEditor.dirty || testedActions.size()!=shapeCount)
                    throw std::runtime_error("Hex copy widget changed paint or failed its local click");
                pinned=false;
                shapeEditor.mode(1);shapeEditor.current()["stops"]=Json::array({
                    Json{{"rgb",{0,0,0}},{"location",0}},Json{{"rgb",{200,40,80}},{"location",2048}},Json{{"rgb",{255,255,255}},{"location",4096}}});
                shapeEditor.original=shapeEditor.draft;shapeEditor.changed();pinned=true;numberFrame(ImGuiKey_None,false);
                if(shapeEditor.gradientBounds.right<=shapeEditor.gradientBounds.left || gradientPosition(-10,0,100)!=0 || gradientPosition(200,0,100)!=4096)
                    throw std::runtime_error("Gradient bar bounds or edge positions invalid");
                auto gradientRect=shapeEditor.gradientBounds;
                const float marker=menuGradientHandleSize*scale,left=gradientRect.left+marker*.5f,width=gradientRect.right-gradientRect.left-marker;
                const float markerY=gradientRect.top+(menuGradientBarHeight+menuCompactGap+menuGradientHandleSize*.5f)*scale;
                auto gradientMouse=[&](float x,bool down){io.AddMouseViewportEvent(ImGui::GetMainViewport()->ID);io.AddMousePosEvent(x,markerY);io.AddMouseButtonEvent(ImGuiMouseButton_Left,down);numberFrame(ImGuiKey_None,false);};
                gradientMouse(left+width*.5f,false);gradientMouse(left+width*.5f,true);
                gradientMouse(left+width*.75f,true);gradientMouse(left+width*.75f,false);
                const float actualStopX=left+shapeEditor.current()["stops"][1]["location"].get<float>()/4096.f*width+shapeEditor.gradientDragOffset;
                if(shapeEditor.stop!=1 || std::abs(actualStopX-io.MousePos.x)>1.f ||
                   std::abs(shapeEditor.current()["stops"][1]["location"].get<int>()-3072)>std::ceil(4096.f/width) || !shapeEditor.dirty || testedActions.size()!=shapeCount)
                    throw std::runtime_error("Gradient marker drag did not update only its local position: stop="+std::to_string(shapeEditor.stop)+
                        " location="+shapeEditor.current()["stops"][1]["location"].dump()+" dirty="+std::to_string(shapeEditor.dirty));
                shapeEditor.stop=0;moveGradientStop(4096);
                if(shapeEditor.current()["stops"][0]["location"]!=shapeEditor.current()["stops"][1]["location"])
                    throw std::runtime_error("Gradient markers crossed their neighbours");
                shapeEditor.prepare(shapeFixture);pinned=false;numberFrame(ImGuiKey_None,false);
                POINT paintPoint={(shapeEditor.bounds.left+shapeEditor.bounds.right)/2,(shapeEditor.bounds.top+shapeEditor.bounds.bottom)/2};
                renderedHit=100;finishHold(paintPoint,true,&paintPoint);
                if(testedActions.back()!="hide")throw std::runtime_error("Unchanged paint submitted");
                auto resetShape=[&]{shapeEditor.prepare(shapeFixture);setHoldMode(true);shapePanelOpen=true;numberFrame(ImGuiKey_None,false);holdMoved=true;};
                resetShape();
                for(const bool live:{false,true}){
                    auto gradientShape=shapeFixture;gradientShape["live_preview"]=live;gradientShape["gradient_editor"]=true;
                    shapeEditor.prepare(gradientShape);setHoldMode(true);shapePanelOpen=true;
                    shapeEditor.channel=1;shapeEditor.mode(1);shapeEditor.current()["stops"][0]["rgb"]={160,70,95};shapeEditor.changed();
                    const auto draft=shapeEditor.draft;
                    const auto actions=testedActions.size();
                    numberFrame(ImGuiKey_None,false);
                    const RECT entry=shapeTipRows[shapeGradientConfirmTip];
                    io.AddMouseViewportEvent(ImGui::GetMainViewport()->ID);
                    io.AddMousePosEvent((entry.left+entry.right)*.5f,(entry.top+entry.bottom)*.5f);numberFrame(ImGuiKey_None,false);
                    io.AddMouseButtonEvent(0,true);numberFrame(ImGuiKey_None,false);
                    io.AddMouseButtonEvent(0,false);numberFrame(ImGuiKey_None,false);
                    const Json expected={{"channel","stroke"},{"draft",draft},{"preview",live}};
                    if(!gradientLaunching || testedActions.size()!=actions+1 || testedActions.back()!="gradient.open" || testedGradientOpen!=expected)
                        throw std::runtime_error("Adjusted Caps gradient did not request a single confirmed handoff: "+testedGradientOpen.dump());
                    accept({{"type","gradient.open.failed"},{"epoch",menuEpoch-1}});
                    if(!gradientLaunching)throw std::runtime_error("A stale launch failure unlocked the current gradient handoff");
                    accept({{"type","gradient.open.failed"},{"epoch",menuEpoch}});
                    if(gradientLaunching)throw std::runtime_error("Current gradient launch failure did not clear its pending state");
                    testedActions.resize(actions);
                }
                std::cout<<"Adjusted Caps gradient handoff passed for live and draft-only paths\n";
                resetShape();
                // The documented 128 patterns x 128 characters is valid
                // editor data. Python's ASCII JSON encoding can exceed 64KiB;
                // its completed snapshot may replace the brief basic result.
                {
                    Json largeShape=shapeFixture;largeShape["patterns"]=Json::array();
                    std::string longName;for(int n=0;n<64;++n)longName+=u8"图案";
                    for(int n=0;n<128;++n)largeShape["patterns"].push_back(longName);
                    Json update={{"epoch",menuEpoch},{"flags",uint64_t(64)|(uint64_t(15)<<8)|
                        (uint64_t(6)<<12)|(uint64_t(1)<<17)|(uint64_t(127)<<32)},{"shape",largeShape}};
                    const auto payload=update.dump(-1,' ',true);
                    if(payload.size()<=65536)throw std::runtime_error("Large layer-context fixture too small");
                    layerAware=true;layerMask=0;layerKind=0;quickMask=0;
                    COPYDATASTRUCT packet{0x4d435458,DWORD(payload.size()+1),const_cast<char*>(payload.c_str())};
                    if(!SendMessageW(window,WM_COPYDATA,0,reinterpret_cast<LPARAM>(&packet)) ||
                       layerMask!=15 || layerKind!=6 || quickMask!=127 || !shapeEditor.available)
                        throw std::runtime_error("Valid completed layer snapshot rejected; all layer actions remain disabled: bytes="+std::to_string(payload.size()+1));
                    const auto eventCount=testedActions.size(),previewCount=testedMaskEvents.size();
                    numberFrame(ImGuiKey_None,false);
                    if(ImGui::GetDrawData()->TotalVtxCount<=0 || !rowEnabled(0) ||
                       testedActions.size()!=eventCount || testedMaskEvents.size()!=previewCount)
                        throw std::runtime_error("Large snapshot did not enable visible controls without executing an action");
                    auto stale=update;stale["epoch"]=menuEpoch-1;const auto stalePayload=stale.dump(-1,' ',true);
                    COPYDATASTRUCT stalePacket{0x4d435458,DWORD(stalePayload.size()+1),const_cast<char*>(stalePayload.c_str())};
                    if(SendMessageW(window,WM_COPYDATA,0,reinterpret_cast<LPARAM>(&stalePacket)))
                        throw std::runtime_error("Large snapshot bypassed the current-menu epoch guard");
                    auto oversized=update;oversized["padding"]=std::string(256*1024,'x');const auto oversizedPayload=oversized.dump();
                    COPYDATASTRUCT oversizedPacket{0x4d435458,DWORD(oversizedPayload.size()+1),const_cast<char*>(oversizedPayload.c_str())};
                    if(SendMessageW(window,WM_COPYDATA,0,reinterpret_cast<LPARAM>(&oversizedPacket)))
                        throw std::runtime_error("Oversized snapshot bypassed the packet bound");
                    std::cout<<"Large completed layer snapshot passed: bytes="<<payload.size()+1<<", full pattern catalog, visible actions, zero execution\n";
                    resetShape();
                }
                // Old/missing host capability keeps the entire editor local.
                for(const bool missing:{false,true}) {
                    auto local=shapeFixture;
                    if(missing)local.erase("live_preview");else local["live_preview"]=false;
                    shapeEditor.prepare(local);setHoldMode(true);shapePanelOpen=true;numberFrame(ImGuiKey_None,false);holdMoved=true;
                    const auto events=testedMaskEvents.size(),actions=testedActions.size();
                    shapeEditor.current()["rgb"]={93,72,51};shapeEditor.draft["mask"]["feather"]=7.5;shapeEditor.changed();
                    numberFrame(ImGuiKey_None,false);
                    if(maskPreviewStarted || testedMaskEvents.size()!=events || !shapeDraftOnly || !shapeEditor.ready())
                        throw std::runtime_error("Draft-only host published a live preview or disabled editing");
                    renderedHit=100;finishHold(paintPoint,true,&paintPoint);finishHold(paintPoint);
                    if(testedActions.size()!=actions+1 || testedActions.back()!="shape.paint" || testedMaskEvents.size()!=events)
                        throw std::runtime_error("Draft-only confirmation did not submit exactly once");
                    shapeEditor.prepare(local);setHoldMode(true);shapePanelOpen=true;numberFrame(ImGuiKey_None,false);holdMoved=true;
                    shapeEditor.current()["rgb"]={42,62,82};shapeEditor.changed();
                    POINT centre={LONG(pieCenter.x),LONG(pieCenter.y)};renderedHit=100;finishHold(paintPoint,true,&centre);
                    if(testedActions.back()!="hide" || testedMaskEvents.size()!=events)
                        throw std::runtime_error("Draft-only centre cancellation submitted a change");
                    testedActions.resize(shapeCount);
                }
                resetShape();const auto colorEvents=testedMaskEvents.size();
                shapeEditor.current()["rgb"]={91,82,73};shapeEditor.changed();numberFrame(ImGuiKey_None,false);
                if(testedMaskEvents.size()!=colorEvents+1 || !maskPreviewStarted || !shapeEditor.ready() || canExecute() ||
                   testedMaskEvents.back()["params"]["fill"]["rgb"]!=Json({91,82,73}))
                    throw std::runtime_error("Fill color must preview through its owning editor");
                shapeEditor.channel=1;copyShapeHex(shapeEditor.current()["rgb"]);numberFrame(ImGuiKey_None,false);
                if(testedMaskEvents.size()!=colorEvents+1)throw std::runtime_error("Channel selection or copying wrote Photoshop");
                shapeEditor.current()["rgb"]={19,28,37};shapeEditor.current()["enabled"]=true;
                shapeEditor.draft["mask"]["density"]=61;shapeEditor.changed();numberFrame(ImGuiKey_None,false);
                if(testedMaskEvents.size()!=colorEvents+2 || testedMaskEvents.back()["params"]!=shapeEditor.draft)
                    throw std::runtime_error("Stroke and mask changes must coalesce to one frame payload");
                renderedHit=100;finishHold(paintPoint,true,&paintPoint);
                if(testedMaskEvents.back()["action"]!="mask.preview.commit" || testedShapePaint!=shapeEditor.draft)
                    throw std::runtime_error("Color preview confirmation lost its final draft");
                testedActions.resize(shapeCount);
                resetShape();shapeEditor.current()["rgb"]={200,30,10};shapeEditor.changed();
                renderedHit=100;finishHold(paintPoint,true,&paintPoint);auto paintSubmitted=testedActions.size();finishHold(paintPoint);
                if(testedActions.back()!="shape.paint" || testedShapePaint["fill"]["rgb"]!=Json({200,30,10}) || testedActions.size()!=paintSubmitted)
                    throw std::runtime_error("Paint release payload or single submission failed");
                resetShape();shapeEditor.current()["rgb"]={17,81,191};shapeEditor.changed();
                pieStarted=GetTickCount64()-120000;pieAnimating=false;
                const auto longDraft=shapeEditor.draft;
                const auto delayed=Json{{"epoch",menuEpoch},{"flags",uint64_t(0)},{"shape",Json{{"available",false}}}}.dump();
                COPYDATASTRUCT delayedPacket{0x4d435458,DWORD(delayed.size()+1),const_cast<char*>(delayed.c_str())};
                if(SendMessageW(window,WM_COPYDATA,0,reinterpret_cast<LPARAM>(&delayedPacket)) || shapeEditor.draft!=longDraft)
                    throw std::runtime_error("Delayed metadata replaced a dirty long-held paint draft");
                renderedHit=100;const auto longCount=testedActions.size();finishHold(paintPoint,true,&paintPoint);finishHold(paintPoint);
                if(testedActions.size()!=longCount+1 || testedActions.back()!="shape.paint" || testedShapePaint!=longDraft)
                    throw std::runtime_error("Long-held paint draft did not dispatch exactly once");
                resetShape();shapeEditor.current()["rgb"]={200,30,10};shapeEditor.changed();
                POINT neutral={LONG(pieCenter.x),LONG(pieCenter.y)};renderedHit=100;finishHold(paintPoint,true,&neutral);
                if(testedActions.back()!="hide")throw std::runtime_error("Paint stale cursor bypassed centre cancellation");
                resetShape();shapeEditor.current()["rgb"]={200,30,10};shapeEditor.changed();pinned=true;
                SendMessageW(window,WM_APP+22,0,0);auto pinnedSubmitted=testedActions.size();SendMessageW(window,WM_APP+22,0,0);
                if(testedActions.back()!="shape.paint" || testedActions.size()!=pinnedSubmitted)throw std::runtime_error("Pinned paint repeated");
                resetShape();inputFocused=true;numberFrame(ImGuiKey_None,false);
                if(shapeEditor.ready() || shapeEditor.bounds.right || shapeEditor.maskBounds.right)throw std::runtime_error("Input protection retained shape editor");
                inputFocused=false;shapeEditor.prepare(Json{{"available",false}});testedActions.resize(shapeCount);
                shapeFixture["vector"]=false;shapeEditor.prepare(shapeFixture);setHoldMode(true);shapePanelOpen=true;numberFrame(ImGuiKey_None,false);
                if(!shapeEditor.ready() || shapeEditor.vector || shapeEditor.channel!=0 || shapeEditor.bounds.right<=shapeEditor.bounds.left || shapeEditor.maskBounds.right)
                    throw std::runtime_error("Plain fill must expose only fill controls");
                shapeEditor.prepare(Json{{"available",false}});
                const auto previousRenderMode=renderMode;
                for(const char* mode:{"cpu","gpu","auto"}){
                    renderMode=mode;
                    if(!recoverRenderer() || !menuShadow::texture || (renderMode=="cpu" && activeRenderMode!="cpu"))
                        throw std::runtime_error("Graphics mode switch failed");
                    numberFrame(ImGuiKey_None,false);
                    if(!IsWindowVisible(window) || ImGui::GetDrawData()->TotalVtxCount<=0)
                        throw std::runtime_error("Graphics mode lost visible menu content");
                }
                renderMode=shapeBenchmarkOutput.empty()?previousRenderMode:benchmarkRenderMode;
                if(!recoverRenderer())throw std::runtime_error("Unable to restore requested benchmark graphics mode");
                shapeFixture["vector"]=true;
                benchmarkShapeEditor(shapeFixture);
                if(!running)break;
                capsReady=true;panelKind=2;layerAware=true;layerKind=2;layerMask=15;layerCount=1;layerLocked=layerBackground=layerCrossParents=false;
                shapeEditor.prepare(Json{{"available",false}});taskBusy=busyFeedback=false;
                setHoldMode(true);shapePanelOpen=true;holdMoved=true;numberFrame(ImGuiKey_None,false);
                auto* shortcutFont=menuFont(MenuTextRole::Shortcut);
                for(int i=0;i<7;++i){
                    const auto r=capsRows[i];POINT centre={(r.left+r.right)/2,(r.top+r.bottom)/2};
                    if(r.right<=r.left || holdActionAt(centre)!=40+i || !actionTipText(40+i))throw std::runtime_error("Caps bar geometry/hit/tooltip mismatch");
                    if(i!=0 && holdActionAt({r.left,centre.y})!=-1)throw std::runtime_error("Caps separator gained a hit");
                    if(i<5){
                        const auto textWidth=shortcutFont->CalcTextSizeA(shortcutFont->LegacySize,FLT_MAX,0,capsKeys[i]).x;
                        const auto width=std::max(menuShortcutMinWidth*scale,textWidth+2*menuShortcutTextInset*scale);
                        if(std::abs(r.right-r.left-width)>1.f)throw std::runtime_error("Bottom shortcut width does not match its text and inset");
                    }
                }
                if(capsRows[5].top!=holdRows[2].top || capsRows[5].bottom!=holdRows[2].bottom || capsRows[5].left!=holdRows[3].right || capsRows[6].left!=capsRows[5].right || capsRows[4].top<=capsRows[5].bottom ||
                   std::abs((capsRows[5].bottom-capsRows[5].top)-menuControlHeight*scale)>1)
                    throw std::runtime_error("Top/bottom and step controls must share one contiguous row");
                if(std::abs((capsRows[0].bottom-capsRows[0].top)-menuShortcutHeight*scale)>1 || capsRows[0].bottom-capsRows[0].top>=menuControlHeight*scale)
                    throw std::runtime_error("Bottom shortcut buttons lost their compact height");
                if(tipFamily(40)==tipFamily(45) || tipGroupRect(45).top!=capsRows[5].top || tipFamily(45)!=tipFamily(2) || tipGroupRect(45).right!=capsRows[6].right ||
                   tipGroupRect(40).right!=capsRows[4].right)
                    throw std::runtime_error("Sorting tooltip must share the four-segment row");
                for(int i:{5,6}){
                    const auto r=capsRows[i];const auto before=testedActions.size();
                    renderedHit=40+i;holdFinished=false;
                    finishHold({(r.left+r.right)/2,(r.top+r.bottom)/2});
                    if(testedActions.size()!=before+1 || testedActions.back()!=capsActions[i])
                        throw std::runtime_error("Upper step gesture must dispatch once");
                    setHoldMode(true);shapePanelOpen=true;holdMoved=true;numberFrame(ImGuiKey_None,false);
                }
                const auto capsRect=capsRows[0];const auto count=testedActions.size();
                renderedHit=40;holdFinished=false;finishHold({(capsRect.left+capsRect.right)/2,(capsRect.top+capsRect.bottom)/2});
                if(testedActions.size()!=count+1 || testedActions.back()!="copyStyle")throw std::runtime_error("Caps bar did not dispatch copy once");
                for(int scope:{0,1}){
                holdFinished=false;panelKind=scope;numberFrame(ImGuiKey_None,false);
                if(rowVisible(2) || rowVisible(3) || rowEnabled(2) || rowEnabled(3) || capsEnabled(5) || capsEnabled(6) ||
                   capsRows[5].right || capsRows[6].right || capsCount()!=5)
                    throw std::runtime_error("Generic menu retained layer reorder controls");
                const auto genericCount=testedActions.size();
                for(const char* action:{"moveToTop","moveToBottom","moveUp","moveDown","moveDocumentTop","moveDocumentBottom"})send(action);
                numberFrame(ImGuiKey_8,true);numberFrame(ImGuiKey_8,false);
                numberFrame(ImGuiKey_2,true);numberFrame(ImGuiKey_2,false);
                if(testedActions.size()!=genericCount)throw std::runtime_error("Generic menu reordered layers through hidden input");
                const RECT lastGeneric=capsRows[4];
                const RECT genericTipGroup=tipGroupRect(40);
                if(genericTipGroup.left!=capsRows[0].left || genericTipGroup.right!=lastGeneric.right ||
                   genericTipGroup.bottom!=lastGeneric.bottom || genericTipGroup.right<=genericTipGroup.left)
                    throw std::runtime_error("Compact generic Caps tooltip group retained removed segments");
                if(holdActionAt({lastGeneric.right-LONG(2*scale),(lastGeneric.top+lastGeneric.bottom)/2})!=44)
                    throw std::runtime_error("Compact generic Caps bar lost final segment hit");
                }
                panelKind=2;holdFinished=false;numberFrame(ImGuiKey_None,false);
                if(!rowVisible(2) || !rowVisible(3) || !capsEnabled(5) || !capsEnabled(6) || !capsRows[6].right)
                    throw std::runtime_error("Layers menu lost its reorder controls after generic transition");
                {
                    const auto count=testedActions.size();individualSmartReady=true;
                    for(int kind:{0,1,2}){
                        panelKind=kind;layerKind=8;layerCount=2;layerCrossParents=true;setHoldMode(true);holdMoved=true;numberFrame(ImGuiKey_None,false);
                        const auto r=capsRows[9];const POINT middle={(r.left+r.right)/2,(r.top+r.bottom)/2};
                        if(r.right<=r.left || std::abs(r.right-(holdRows[1].left-quickGroupGap*scale))>1 || capsRows[10].top!=holdRows[1].top ||
                           holdActionAt(middle)!=49 || !actionTipText(49) || tipGroupRect(49).left!=capsRows[10].left || tipFamily(49)==tipFamily(40))
                            throw std::runtime_error("Individual Smart Object button lost capability/hit/independent tooltip");
                        renderedHit=49;finishHold(middle);
                        if(testedActions.back()!=capsActions[9])throw std::runtime_error("Individual Smart Object gesture failed");
                        holdFinished=false;layerLocked=true;numberFrame(ImGuiKey_None,false);
                        const auto before=testedActions.size();send(capsActions[9]);
                        if(capsEnabled(9) || holdActionAt(middle)!=-1 || testedActions.size()!=before)throw std::runtime_error("Locked individual Smart Object dispatched");
                        layerLocked=false;inputFocused=true;if(capsEnabled(9))throw std::runtime_error("Input enabled individual Smart Object");inputFocused=false;
                    }
                    panelKind=2;layerKind=2;layerCount=2;layerCrossParents=true;holdFinished=false;numberFrame(ImGuiKey_None,false);
                    if(capsEnabled(10) || !capsEnabled(9))throw std::runtime_error("Cross-parent selection enabled combined Smart Object");
                    layerCrossParents=false;numberFrame(ImGuiKey_None,false);
                    const auto merged=capsRows[10];const POINT mergePoint={(merged.left+merged.right)/2,(merged.top+merged.bottom)/2};
                    const POINT divider={mergePoint.x,merged.bottom};
                    if(merged.left!=capsRows[9].left || merged.right!=capsRows[9].right || merged.bottom!=capsRows[9].top ||
                       holdActionAt(mergePoint)!=50 || holdActionAt(divider)!=-1 || holdActionAt({mergePoint.x,merged.bottom-1})!=-1 || tipFamily(50)!=tipFamily(49) || !actionTipText(50) || tipGroupRect(50).bottom!=capsRows[9].bottom)
                        throw std::runtime_error("Smart Object pair geometry/hit/divider/tooltip mismatch");
                    auto pairCount=testedActions.size();renderedHit=50;finishHold(mergePoint);
                    if(testedActions.size()!=pairCount+1 || testedActions.back()!="convertSmartObject")throw std::runtime_error("Combined Smart Object gesture did not use F5 action once");
                    setHoldMode(true);pinned=true;holdMoved=true;numberFrame(ImGuiKey_None,false);
                    pairCount=testedActions.size();io.AddMousePosEvent(float(mergePoint.x),float(mergePoint.y));numberFrame(ImGuiKey_None,false);
                    io.AddMouseButtonEvent(0,true);numberFrame(ImGuiKey_None,false);io.AddMouseButtonEvent(0,false);numberFrame(ImGuiKey_None,false);
                    if(testedActions.size()!=pairCount+1 || testedActions.back()!="convertSmartObject")throw std::runtime_error("Pinned combined Smart Object click failed");
                    holdFinished=false;layerLocked=true;numberFrame(ImGuiKey_None,false);pairCount=testedActions.size();send(capsActions[10]);
                    if(capsEnabled(10) || testedActions.size()!=pairCount)throw std::runtime_error("Locked combined Smart Object dispatched");
                    layerLocked=false;inputFocused=true;if(capsEnabled(10))throw std::runtime_error("Input enabled combined Smart Object");inputFocused=false;
                    panelKind=2;layerKind=4;layerCount=1;layerCrossParents=false;holdFinished=false;numberFrame(ImGuiKey_None,false);
                    if(capsRows[9].right || capsEnabled(9) || capsRows[10].top!=holdRows[1].top || std::abs(capsRows[10].right-(holdRows[1].left-quickGroupGap*scale))>1 ||
                       capsRows[10].right>=holdRows[1].left || capsRows[10].right>=toolRow.left)
                        throw std::runtime_error("Lower-left Smart Object overlaps selection, labels or text tool");
                    for(int kind:{3,4}){panelKind=kind;numberFrame(ImGuiKey_None,false);if(capsRows[9].right || capsRows[10].right || capsEnabled(9) || capsEnabled(10))throw std::runtime_error("Input/home retained Smart Object controls");}
                    panelKind=2;individualSmartReady=false;numberFrame(ImGuiKey_None,false);
                    const auto before=testedActions.size();send(capsActions[9]);send(capsActions[10]);
                    if(capsRows[9].right || capsRows[10].right || capsEnabled(9) || capsEnabled(10) || testedActions.size()!=before)throw std::runtime_error("Old UXP retained individual Smart Object");
                    testedActions.resize(count);layerKind=2;numberFrame(ImGuiKey_None,false);
                }
                holdFinished=false;inputFocused=true;numberFrame(ImGuiKey_None,false);
                if(capsEnabled(0))throw std::runtime_error("Input protection enabled Caps bar");
                inputFocused=false;capsReady=false;
                // Real client messages at empty space, independent of the hook.
                // Cancellation must never become a late release/paint commit.
                const auto rightActions=testedActions.size();
                for(int kind:{0,1,2,4}){
                    panelKind=kind;taskBusy=busyFeedback=false;setHoldMode(true);
                    SetWindowPos(window,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);
                    const auto before=testedActions.size();
                    procedure(window,WM_RBUTTONDOWN,MK_RBUTTON,MAKELPARAM(2,2));
                    procedure(window,WM_RBUTTONUP,0,MAKELPARAM(2,2));
                    finishHold({LONG(pieCenter.x),LONG(pieCenter.y)});
                    if(!holdFinished || pieReady || testedActions.size()!=before+1 || testedActions.back()!="hide" || compositionOpacity!=0.f)
                        throw std::runtime_error("Blank right-click failed cancellation or allowed a late release");
                }
                panelKind=2;layerAware=true;layerKind=6;layerCount=1;layerLocked=layerBackground=false;
                allowed=capable=true;maskPreviewStarted=false;
                shapeEditor.prepare(shapeFixture);setHoldMode(true);shapePanelOpen=true;
                const auto sampleCancelCount=testedActions.size();
                if(!beginShapeEyedrop())throw std::runtime_error("Right-cancel sampler did not arm");
                procedure(window,WM_RBUTTONDOWN,MK_RBUTTON,MAKELPARAM(2,2));
                procedure(window,WM_RBUTTONUP,0,MAKELPARAM(2,2));
                if(shapeEyedropActive || !shapePanelOpen || holdFinished || testedActions.size()!=sampleCancelCount)
                    throw std::runtime_error("Sampler right-click closed the menu or leaked its up");
                shapeEditor.current()["rgb"]={11,22,33};shapeEditor.changed();
                const auto dirtyEvents=testedMaskEvents.size();
                procedure(window,WM_RBUTTONDOWN,MK_RBUTTON,MAKELPARAM(2,2));
                procedure(window,WM_RBUTTONUP,0,MAKELPARAM(2,2));
                finishHold({LONG(shapeEditor.bounds.left+5),LONG(shapeEditor.bounds.top+5)});
                if(testedActions.size()!=sampleCancelCount+1 || testedActions.back()!="hide" || testedMaskEvents.size()!=dirtyEvents)
                    throw std::runtime_error("Dirty editor blank right-click confirmed instead of cancelling");
                taskBusy=true;busyFeedback=true;const auto busyRight=testedActions.size();
                procedure(window,WM_RBUTTONDOWN,MK_RBUTTON,MAKELPARAM(2,2));procedure(window,WM_RBUTTONUP,0,MAKELPARAM(2,2));
                if(!taskBusy || !busyFeedback || testedActions.size()!=busyRight+1 || testedActions.back()!="hide")
                    throw std::runtime_error("Feedback right-click cleared operation protection");
                testedActions.resize(rightActions);taskBusy=busyFeedback=false;
                {
                    const auto before=testedActions.size();
                    panelKind=2;layerAware=true;layerKind=4;layerCount=1;layerLocked=false;
                    layerSearchReady=textSplitReady=capsReady=capable=allowed=true;taskHasResult=false;taskRecovery.clear();
                    shapeEditor.prepare(Json{{"available",false}});setHoldMode(true);holdMoved=true;
                    ImGui::ClearActiveID();numberFrame(ImGuiKey_None,false);
                    numberFrame(ImGuiKey_F,true);
                    if(menuToolsVisible())throw std::runtime_error("Bare F must not open layer search");
                    numberFrame(ImGuiKey_F,false);io.AddKeyEvent(ImGuiMod_Ctrl,true);numberFrame(ImGuiKey_None,false);
                    numberFrame(ImGuiKey_F,true);
                    if(!menuToolsVisible() || menuTools.mode!="search")throw std::runtime_error("Ctrl+F must open layer search from the menu");
                    numberFrame(ImGuiKey_F,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);menuTools.reset();numberFrame(ImGuiKey_None,false);
                    setHoldMode(true);holdMoved=true;
                    for(int kind:{0,1,2,3,4}){
                        panelKind=kind;layerKind=2;numberFrame(ImGuiKey_None,false);
                        if(toolRow.right)throw std::runtime_error("Caps retained a search entry or a non-text split entry");
                        layerKind=4;numberFrame(ImGuiKey_None,false);
                        const bool textMenu=kind<3;
                        if(bool(toolRow.right)!=textMenu || toolEnabled(1)!=textMenu)
                            throw std::runtime_error("Text tool eligibility depends on mouse panel instead of selected layer");
                        layerCount=2;numberFrame(ImGuiKey_None,false);
                        if(toolRow.right || toolEnabled(1))throw std::runtime_error("Multi-selection enabled single-text tools");
                        layerCount=1;layerLocked=true;numberFrame(ImGuiKey_None,false);
                        if(toolEnabled(1))throw std::runtime_error("Locked text enabled split");
                        layerLocked=false;
                    }
                    panelKind=2;layerKind=4;
                    numberFrame(ImGuiKey_None,false);
                    if(toolRow.left!=LONG(pieCenter.x+65*scale) || toolRow.top!=LONG(pieCenter.y-15*scale))throw std::runtime_error("Split must replace original right-side creation position");
                    if(capsRows[5].top!=holdRows[2].top || capsRows[6].bottom>=toolRow.top)throw std::runtime_error("Move controls must share sorting row above the text tool");
                    for(int i=0;i<7;++i){const auto tip=quickTipBounds(i);if(tip.z>tip.x && tipOverlaps(tip,toolRow,2*scale))throw std::runtime_error("Creation tooltip covers relocated split");}
                    const auto r=toolRow;if(r.right<=r.left || !toolEnabled(1))throw std::runtime_error("Text Caps entry unavailable");
                    renderedHit=71;finishHold({(r.left+r.right)/2,(r.top+r.bottom)/2});
                    if(!menuTools.opening || !pinned || testedActions.back()!="menu.tools.open")throw std::runtime_error("Caps tool gesture did not pin an editor");
                    menuTools.receive(Json{{"type","menu.tools.opened"},{"epoch",menuEpoch},{"mode","split"}});
                    const int request=menuTools.request;
                    menuTools.receive(Json{{"type","menu.tools.result"},{"epoch",menuEpoch-1},{"request",request},{"action","text.read"},{"ok",true},{"result",Json::object()}});
                    if(!menuTools.pending)throw std::runtime_error("Stale Caps tool response accepted");
                    menuTools.receive(Json{{"type","menu.tools.result"},{"epoch",menuEpoch},{"request",request},{"action","text.read"},{"ok",true},
                        {"result",{{"text","a\rb"},{"name","source"},{"revision","r"}}}});
                    if(menuTools.pending || menuTools.split.breaks!=Json({2,3}) || canExecute())throw std::runtime_error("Caps text preview is not isolated");
                    menuTools.textTab=3;std::snprintf(menuTools.draft,sizeof(menuTools.draft),"one\ntwo\nthree\nfour");
                    if(menuTools.grouping[0] || menuTools.textParams().at("groups")!="1,2,3,4" || menuDefaultGroups("a\r\nb\nc\rd")!="1,2,3,4")
                        throw std::runtime_error("Automatic split groups did not follow actual draft line breaks");
                    std::snprintf(menuTools.grouping,sizeof(menuTools.grouping),"1");menuTools.error="invalid grouping";
                    if(!menuTools.canWriteText() || !menuTools.planId.empty() || menuTools.textStatus().find("仍可写回当前文字")==std::string::npos)
                        throw std::runtime_error("Invalid split disabled writing the current draft");
                    menuTools.pending=true;if(menuTools.canWriteText())throw std::runtime_error("Direct write overlapped pending work");menuTools.pending=false;
                    menuTools.error.clear();menuTools.grouping[0]=0;std::snprintf(menuTools.draft,sizeof(menuTools.draft),"a\nb");
                    menuTools.textTab=1;std::snprintf(menuTools.find,sizeof(menuTools.find),"a");std::snprintf(menuTools.replacement,sizeof(menuTools.replacement),"猫");
                    if(menuTools.textParams().at("operation")!="replace" || menuTools.textParams().at("draft")!="a\nb")throw std::runtime_error("Text editor parameters lost source or operation");
                    menuTools.changedText();const double textChanged=menuTools.textChanged;
                    if(menuTools.textDue(textChanged+.17) || !menuTools.textDue(textChanged+.19))throw std::runtime_error("Text live preview debounce failed");
                    menuTools.pending=true;if(menuTools.textDue(textChanged+1))throw std::runtime_error("Text live preview overlapped pending work");menuTools.pending=false;
                    menuTools.error="拆分需要2至64组非空文字";
                    if(menuTools.textStatus()!=menuTools.error)throw std::runtime_error("Disabled text plan hid its reason");
                    menuTools.error.clear();menuTools.pending=true;menuTools.pendingAction="text.preview";
                    if(menuTools.textStatus().find("可继续编辑")==std::string::npos)throw std::runtime_error("Preview editing status lost");
                    menuTools.pending=false;
                    for(int tab=0;tab<5;++tab){menuTools.textTab=tab;if(!menuTools.textDue(textChanged+.19))throw std::runtime_error("Text tab did not auto preview");}
                    menuTools.textTab=4;
                    const char* listKinds[]={"bullet","numbered","remove"};
                    for(int mode=0;mode<3;++mode){menuTools.listMode=mode;const auto params=menuTools.textParams();if(params.at("operation")!="list" || params.at("kind")!=listKinds[mode])throw std::runtime_error("List operation parameters lost");}
                    const Json savedList={{"kind","numbered"},{"marker","custom"},{"custom_marker","✓"},{"numbering","hierarchical"},{"start",7}};
                    menuTools.restoreList(savedList);
                    if(menuTools.textTab!=4 || menuTools.listMode!=1 || menuTools.listMarker!=6 || menuTools.listNumbering!=7 || std::string(menuTools.listStart)!="7" || std::string(menuTools.listCustom)!="✓")throw std::runtime_error("Saved list options were not restored");
                    menuTools.draftList=savedList;menuTools.draftListChanged=true;menuTools.textTab=0;
                    if(menuTools.textParams().at("list_options")!=savedList)throw std::runtime_error("Continuing a list draft dropped options");
                    menuTools.draftListChanged=false;
                    menuTools.textTab=3;menuTools.splitMode=1;
                    if(menuTools.textDue(textChanged+1))throw std::runtime_error("Empty split delimiter auto submitted");
                    menuTools.splitMode=0;menuTools.textTab=1;menuTools.find[0]=0;
                    if(menuTools.textDue(textChanged+1))throw std::runtime_error("Empty search auto submitted");
                    std::snprintf(menuTools.find,sizeof(menuTools.find),"a");
                    menuTools.submit("text.preview",menuTools.textParams());const auto previewRequest=menuTools.request;menuTools.changedText();
                    menuTools.receive(Json{{"type","menu.tools.result"},{"epoch",menuEpoch},{"request",previewRequest},{"action","text.preview"},{"ok",true},{"result",Json::object()}});
                    if(menuTools.pending || !menuTools.planId.empty())throw std::runtime_error("Late text plan accepted after an edit");
                    menuTools.submit("text.preview",menuTools.textParams());
                    menuTools.receive(Json{{"type","menu.tools.result"},{"epoch",menuEpoch},{"request",menuTools.request},{"action","text.preview"},{"ok",true},{"result",{{"plan_id","bounded"},{"value","猫\rb"},{"preview","猫\nb"},{"kind","replace"},{"count",1},{"matches",Json::array({Json::array({0,1})})}}}});
                    if(menuTools.textMatches!=std::vector<MenuControls::TextRange>{{0,1}})throw std::runtime_error("Text match range was not accepted");
                    if(menuTools.planId!="bounded" || menuTools.resultValue!="猫\nb")throw std::runtime_error("Text plan preview was not usable");
                    menuTools.changedText();if(!menuTools.textMatches.empty() || !menuTools.planId.empty())throw std::runtime_error("Text edit retained stale highlights or apply plan");
                    std::snprintf(menuTools.draft,sizeof(menuTools.draft),"猫a");menuTools.submit("text.preview",menuTools.textParams());
                    menuTools.receive(Json{{"type","menu.tools.result"},{"epoch",menuEpoch},{"request",menuTools.request},{"action","text.preview"},{"ok",true},{"result",{{"matches",Json::array({Json::array({1,3})})}}}});
                    if(!menuTools.planId.empty() || !menuTools.textMatches.empty() || menuTools.error.empty())throw std::runtime_error("Broken UTF-8 match accepted");
                    menuTools.changedText();menuTools.textTab=0;
                    numberFrame(ImGuiKey_None,false);
                    if(pieReady || holdRows[0].right || capsRows[0].right || quickRows[0].right || holdActionAt(POINT{LONG(pieCenter.x),LONG(pieCenter.y)})!=-1)
                        throw std::runtime_error("Tool editor retained radial command hits");
                    const auto count=testedActions.size();finishHold({(r.left+r.right)/2,(r.top+r.bottom)/2});
                    if(testedActions.size()!=count)throw std::runtime_error("Opening split submitted a write");
                    menuTools.reset();menuTools.active=menuTools.ready=true;menuTools.mode="search";
                    menuTools.changeSearch(1);
                    if(menuTools.searchDue(1.17) || !menuTools.searchDue(1.19))throw std::runtime_error("Search debounce failed");
                    menuTools.searchDirty=false;menuTools.submit("layers.search",Json::object());
                    menuTools.changeSearch(2);
                    if(menuTools.searchDue(3))throw std::runtime_error("Search overlapped a pending read");
                    menuTools.receive(Json{{"type","menu.tools.result"},{"epoch",menuEpoch},{"request",menuTools.request},{"action","layers.search"},{"ok",true},{"result",{{"items",Json::array()}}}});
                    if(menuTools.pending || !menuTools.results.is_null() || !menuTools.searchDue(3))throw std::runtime_error("Stale search became selectable or latest query lost");
                    menuTools.results=Json{{"search_id","current"},{"items",Json::array()}};
                    menuTools.previewPending=true;menuTools.previewQuery="current";menuTools.previewHovered=menuTools.previewTarget=0;menuTools.previewRequest=1;
                    const Json tile={{"available",true},{"width",2},{"height",1},{"pixels",Json({255,0,0,255,0,255,0,128})}};
                    accept(Json{{"type","operation"},{"busy",true},{"action","layer.thumbnail"}});
                    accept(Json{{"type","operation"},{"busy",false}});
                    accept(Json{{"type","menu.tools.preview.result"},{"epoch",menuEpoch-1},{"request",1},{"ok",true},{"result",tile}});
                    if(!menuTools.previewPending || menuTools.thumbnail || !busyFeedback)throw std::runtime_error("Stale image changed preview protection");
                    accept(Json{{"type","menu.tools.preview.result"},{"epoch",menuEpoch},{"request",1},{"ok",true},{"result",tile}});
                    if(menuTools.previewPending || !menuTools.thumbnail || !menuTools.previewVisible || menuTools.thumbnailSize.x!=2 || busyFeedback || taskBusy)
                        throw std::runtime_error("Completed image did not create a visible texture and release preview-only feedback");
                    menuTools.previewPending=true;menuTools.previewRequest=2;
                    accept(Json{{"type","operation"},{"busy",true},{"action","layer.thumbnail"}});
                    accept(Json{{"type","menu.tools.preview.result"},{"epoch",menuEpoch},{"request",2},{"ok",false},{"error","unknown"}});
                    if(!taskBusy || !busyFeedback)throw std::runtime_error("Unknown preview released busy protection");
                    accept(Json{{"type","operation"},{"busy",false}});busyFeedback=false;
                    menuTools.changeSearch(4);
                    if(menuTools.thumbnail || menuTools.previewTarget!=-1)throw std::runtime_error("Old search retained its image");
                    if(menuTools.loadPreview(Json{{"available",true},{"width",129},{"height",1},{"pixels",Json::array()}}))throw std::runtime_error("Unbounded thumbnail accepted");
                    menuTools.reset();testedActions.resize(before);layerSearchReady=textSplitReady=false;
                }
                std::cout<<"Blank right-click client cancellation passed: canvas/layers/home/general, late Caps release, dirty draft, sampler pair and busy protection\n";
                std::cout<<"Dear ImGui self-test passed: original radial/focus/input/cancel matrix; shape local draft, types, single release, stale cursor, pinned commit; production SHOWWINDOW content and 100 hidden frames\n";break;
            }
            if (!preview.empty() && frames==20) {
                screenshot(preview);
                if(previewShapeHover>=0){
                    Json draws=Json::array();auto* data=ImGui::GetDrawData();
                    for(int n=0;n<data->CmdListsCount;++n){const auto* list=data->CmdLists[n];Json commands=Json::array();
                        for(const auto& cmd:list->CmdBuffer)commands.push_back({{"elements",cmd.ElemCount},{"clip",{cmd.ClipRect.x,cmd.ClipRect.y,cmd.ClipRect.z,cmd.ClipRect.w}}});
                        Json vertices=Json::array();for(int j=0;j<list->VtxBuffer.Size;++j){const auto& v=list->VtxBuffer[j];if(j%40==0)vertices.push_back({v.pos.x,v.pos.y,v.uv.x,v.uv.y,double(v.col)});}
                        draws.push_back({{"vertices",list->VtxBuffer.Size},{"commands",commands},{"sample",vertices},{"display",{data->DisplayPos.x,data->DisplayPos.y,data->DisplaySize.x,data->DisplaySize.y}}});}
                    auto path=std::filesystem::path(preview);path+=L".draw.json";std::ofstream(path)<<draws.dump();
                }
                if(previewShapeHover>=0){auto report=std::filesystem::path(preview);report+=L".tooltip.json";
                    std::ofstream(report)<<Json{{"candidate",shapeTipCandidate},{"visible",tipVisual.ready},{"text",tipVisual.text},
                        {"bounds",{tipVisual.bounds.x,tipVisual.bounds.y,tipVisual.bounds.z,tipVisual.bounds.w}},
                        {"editor_visible",shapePanelVisible()},{"mode_bounds",{shapeModeBounds.left,shapeModeBounds.top,shapeModeBounds.right,shapeModeBounds.bottom}},{"preview_running",maskPreviewStarted},{"other_actions_enabled",canExecute()},{"preview_moves_enabled",false},
                        {"anchor",previewShapeHover<shapeTipCount?Json({shapeTipRows[previewShapeHover].left,shapeTipRows[previewShapeHover].top,shapeTipRows[previewShapeHover].right,shapeTipRows[previewShapeHover].bottom}):Json()},
                        {"hue_bounds",{perceptualPicker.hueBounds.left,perceptualPicker.hueBounds.top,perceptualPicker.hueBounds.right,perceptualPicker.hueBounds.bottom}},
                        {"square_bounds",{perceptualPicker.squareBounds.left,perceptualPicker.squareBounds.top,perceptualPicker.squareBounds.right,perceptualPicker.squareBounds.bottom}}}.dump();}
                if (!previewLive) break;
            }
            if (SUCCEEDED(swapChain->Present(1,0))) renderedHit=holdFinished?-1:pendingHit;
            finishPlatformFrame();
        }
    } catch (const std::exception& error) {std::cerr<<error.what()<<"\n";return 1;}
    ImGui_ImplDX11_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();releaseTarget();
    releaseDevice();
    DiagnosticHint::shutdown();
    PreviewPendingHint::shutdown();
    if (IsWindow(window)) DestroyWindow(window);UnregisterClassW(wc.lpszClassName,wc.hInstance);
    if (iconHandle) DestroyIcon(iconHandle);CoUninitialize();return 0;
}
