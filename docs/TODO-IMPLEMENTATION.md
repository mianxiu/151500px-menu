# 当前 TODO 实施方案

基线为 v0.4.444、UXP 0.3.137，本机验收宿主为 Photoshop 24.0.1。以下是拟实施的改动。每批以实际 Caps 入口完成一项可用功能，完成状态记回 [TODO](../TODO.md)。

## 现有技术栈与进程分工

版本取自仓库的清单、头文件和构建脚本。各批次优先复用以下组件，唯一单列的新技术验证是第 5 批的 Photoshop C++ SDK 插件。

| 运行位置 | 已有技术 | 在本计划中负责什么 |
| --- | --- | --- |
| Windows 桌面原生窗口 `MenuHost.exe` | C++17、MinGW g++、Win32、Dear ImGui 1.92.7、D3D11、DXGI、DirectComposition | 绘制 Caps、排版和颜色窗口，分发点击／拖动，管理本地草稿。窗口边缘用 Win32 客户区与屏幕坐标，界面控件沿用共享字体角色、尺寸和 DPI 规则。[原生构建](../tools/build-menu-host.ps1) · [ImGui 版本](../native/vendor/imgui/imgui.h) |
| 同一窗口的文字绘制 | DirectWrite、Direct2D、Windows Imaging Component（WIC） | DirectWrite 完成字体匹配、布局与命中，Direct2D 向 WIC 位图绘制本地字样，再上传 D3D11 显示。Photoshop 负责正式组版。字体列表慢与窗口缩放卡顿可在此分别计时。[文字渲染器](../native/FontSampleRaster.h) |
| 热键宿主与 PS 输入线程 | C++17、Win32 键鼠事件、TSF、IMM32 | `HotkeyHost.exe` 处理导航键与前台范围。`InputContext.dll` 在 PS 的输入上下文中核对焦点、临时禁用与恢复，`InputService.dll` 提供现有输入服务接入。第 2 批分别追踪这两条路径。[快捷键](../native/HotkeyHost.cpp) · [输入服务](../native/InputService.cpp) · [输入上下文](../native/InputContext.cpp) |
| Windows 本机后台 `151500px-menu.exe` | Python 3.11、asyncio、Tornado 6.x、pywin32／COM、PyInstaller | 调度工具请求、绑定 PS 会话与图层、保管可提交的操作计划、校验回执并交付像素。耗时的阻塞宿主调用沿用工作线程。菜单与后台通过子进程标准输入／输出交换 UTF-8 JSON 行。[后台依赖](../requirements.txt) · [进程通信](../backend/desktop.py) · [冻结构建](../build-exe.ps1) |
| Photoshop 内的 UXP 插件 | JavaScript、manifest v5、Photoshop API v2、DOM、`batchPlay`、`executeAsModal` | 读取当前选层、文字描述符和 XMP，执行已确认的图层或样式修改。后台与 UXP 沿用带会话与请求身份的文件请求／回执。清单最低 PS 23.3.0，各项能力再按宿主验证。[插件清单](../uxp/offline-probe/manifest.json) · [后台桥接](../backend/uxp_bridge.py) · [Adobe 模态执行](https://developer.adobe.com/photoshop/uxp/2022/ps-reference/media/executeasmodal) |
| Photoshop Generator 的 Node.js 进程 | 项目取图插件 0.2.1，清单声明 `generator-core ^3.12.0` | 读取图层树、排除正在编辑的文字层、输出背景 RGBA 和结果 JSON。Python 校验后建立 Windows 命名内存映射，MenuHost 只读打开并上传纹理。一份 2048×2048 的 RGBA 像素缓冲为 16 MiB。[取图插件](../generator/text-canvas/package.json) · [像素交接](../backend/text_canvas.py) |
| Photoshop 的矢量输出 | UXP 预检、Python COM、ExtendScript／JSX、PS 原生 PDF 导出、图层 XMP 与 SHA-256 | UXP 核对目标，后台在同一请求内通过 `DoJavaScript` 调用现有脚本。PDF 保存可缩放的显示内容，XMP 保存可编辑文字及样式。新对象尺寸和位置要在导出、置入、读回三处一致。[调用入口](../backend/text_object_native.py) · [导出与置入](../jsx/text_object.jsx) |
| 浏览器后台管理页 | SolidJS 1.x、Vite 5、Tornado HTTP／WebSocket | 承载已有管理设置与状态。用户在 Photoshop 里操作的排版、形状和渐变面板继续由原生窗口提供。[前端清单](../frontend/package.json) · [服务入口](../backend/main.py) |

## 1 大文件能打开排版

**先定位读取停在哪一步。** Caps 打开窗口后先发 `text.read`，后台按 `fast=True` 读取所选层，收到正文版本后才发 `text.canvas`。原生窗口在版本为空且无错误时显示“正在读取文字…”，已有的正文编辑框只在部分取图状态下出现。在同一请求上记录窗口打开、目标核对、UXP 正文与样式返回、Generator 图层树与像素读取、结果送达和首次可编辑的时间。复现时保留屏上实际提示，并分别看哪一步启动、返回或被窗口丢弃。[窗口](../native/MenuToolsPanel.h) · [菜单请求](../backend/menu_tools.py) · [文字读取](../backend/text_tools.py)

**按失败阶段处理。**

- 正文读取失败时，返回对应原因和“重新读取文字”。UXP 对正文长度和样式描述符大小分别有 20,000 个 UTF-16 代码单元、524,288 个序列化字符串代码单元的检查。先区分正文过长、样式过大、目标切换、宿主无回执和窗口已关闭，再决定是否需要扩大支持。大小阈值来自不同路径，不能从文件大小直接判定现场原因。[UXP 文字描述符](../uxp/offline-probe/labels.js)
- 正文已就绪时立即允许选择、编辑和本地撤销。把现有取图等待期间的草稿视图延续到背景失败或不支持时。只有拿到可信文本框坐标才启用画布调框。背景单独显示状态及“重试背景”，不调用会覆盖草稿的“重新读取”。Generator 当前限制 RGB 8 位、源画布至多一亿像素，背景最长边 2048 像素，后台等回执约 13 秒。先保留这些限制。若图层树耗时占主导，再试按同一文档历史复用可见性和索引，历史改变即失效。[取图请求](../backend/text_canvas.py) · [Generator 实现](../generator/text-canvas/main.js) · [Adobe 图层 ID 与索引](https://github.com/adobe-photoshop/generator-core/wiki/Understanding-Layer-IDs-and-Layer-Indices)
- 把“只有收到背景才清掉等待”和“后来返回的背景清掉新草稿计划”作为回归情形。按窗口会话、请求号、文档、图层及正文版本接收结果。背景回执只替换对应图像。过期回执释放资源，历史变化提示重新核对。无回执的只读请求按项目规则保留记录，重试使用独立请求号。

**第一轮改动顺序与接口。** 原生 `submit()` 和 Python `MenuTools.request()` 目前各有一个 `pending`。先保留宿主操作串行，只让本地输入、选区、样式草稿和本地撤销脱离背景读取的禁用条件。这样可以先解决开窗和编辑，避免引入两条并发写入路径。

| 顺序 | 修改位置 | 要交付的行为 |
| --- | --- | --- |
| 1 | `backend/menu_tools.py`、`backend/text_tools.py`、Generator 的 `read()` 及窗口接收函数 | 给现有读取记录补阶段耗时和结果原因。用菜单会话及各自的请求号关联，跨进程比较各段耗时，各进程用自身单调时钟计时。记录文档尺寸、图层数（已读取时）和字符数，避开正文、像素和会话令牌。 |
| 2 | `MenuToolsPanel.h` 的 `receive()`、`drawText()` 和画布绘制分支 | `text.read` 返回目标及 `revision` 即呈现现有草稿编辑器。背景处于等待、可用、受限或失败状态，只有可用时绑定纹理。取图失败保留草稿、滚动和选区，按钮只重取背景。 |
| 3 | `backend/menu_tools.py` 的 `text.canvas` 分支与 `backend/text_canvas.py` | 匹配原有会话、文档、图层、历史和请求后交付 `mapping / width / height / box / text_frame`。同版本背景返回只更新显示，不清除新生成的有效计划。发现历史改变就使旧计划失效，保留草稿供用户决定。关闭窗口后到达的像素按现有所有权释放。 |
| 4 | `tests/test_menu_tools.py`、MenuHost 自测及下述隔离文档 | 加入“先收到正文，用户修改，再收到旧背景”和“取图超时后关窗重开”两条回归。确认取图进行中仍可编辑，写入按钮仅在真实目标和当前计划可用时接受一次。 |

**首批验收。** 同一短文字层分别放入 2048×2048、6000×4000 和 12000×9000 像素的 RGB 8 位 sRGB 隔离文档，6000×4000 再做一个多图层对照。中间样本超过现有 PS 准确预览的 16,777,216 像素门槛，仍在 Generator 背景取图范围内；最大样本跨过背景取图上限。这样可分别核对正文、背景及准确预览三项能力。另用长正文或复杂混合样式单测正文限制。逐例记录首次可编辑时间和背景完成时间。在背景等待、失败、重试期间改一段字，核对草稿和选区仍在，关闭后 Photoshop 历史不变。对可写目标另测一次明确写入和一次撤销。[宿主能力判断](../uxp/offline-probe/labels.js)

**本机接口验收（2026-11-25）。** Photoshop 24.0.1／UXP 0.3.139 通过 2048×2048、6000×4000、6000×4000 加 15 层及 12000×9000 的正文先读取。前三组依次获得 2048×2048、2048×1365、2048×1365 背景，最大样本返回 1 亿像素上限提示，草稿仍可用。2048×2048 通过正文写回与一次撤销，各样本关闭前核对历史，原稿回到文档 ID 61、历史 ID 64。通过后台打开实际原生窗口和提交，实体 Caps、重试期间输入及复杂样式上限仍待验。报告见 `.runtime/text-453/draft-452-first-batch.json` 与 `.runtime/text-453/draft-453-background-limit.json`。

## 2 输入法 Hook 与渐变导航

**输入法。** 复用现有 PS 线程内的焦点策略、IMM 关联保存和 TSF 状态核对。一次复现只记录焦点窗口类别、线程、输入或非输入判定、合成开始／结束、上下文是否改变、禁用值和恢复结果。按“画布快捷键 → 点文字上屏 → Esc 或确认 → 立即用快捷键”定位首次错误，再分别测试图层重命名、数值框、手动选择中文、切回 Photoshop。Windows 的活动编辑上下文由当前焦点文档的栈顶决定，现有实现已查询它。若复现指向上下文替换，就在已有焦点或 TSF 通知处理完成后重新核对并恢复旧关联，只在明确的 PS 非输入区应用保护。预期保留用户所选中文输入法，退出编辑后第一个快捷键即生效。[焦点策略](../native/InputContext.cpp) · [TSF 实现](../native/InputContextTsf.h) · [Microsoft TSF 文档管理](https://learn.microsoft.com/en-us/windows/win32/tsf/document-manager)

**渐变叠加。** Z、R、空格已经在放行列表里。按键盘事件、前台窗口归属、鼠标按下命中、叠加层捕获和 PS 视口变化依次核对。若命中窗口截住鼠标，复用现有导航键状态，在手势开始时让出 PS 图像区的指针命中，直到同一手势释放，随后按新视口刷新渐变控制柄。若首个按键已在其他分支被消费，修正该分支的焦点判断。保留文本输入、弹窗、控制柄正在拖动及取色的既有归属。在同一隔离文档实测 Z 缩放、R 旋转、空格平移，分别从画布空白和控制柄附近开始，松开后继续编辑并取消渐变。[导航键](../native/GradientNavigation.h) · [热键路由](../native/HotkeyHost.cpp) · [画布命中与捕获](../native/GradientCanvasPanel.h)

两项独立提交和验收。输入法偶发问题记录正常与受限 CPU 下的重复次数、失败数和首次偏离的事件。没有复现时保留诊断和待验状态。

0.4.446 候选补齐焦点策略遗漏的 TSF 通知。上下文或禁用值变动后，Photoshop 线程合并通知并重新核对当前焦点、上下文及实际禁用值。100 毫秒巡检仍处理缺少通知的情况。实机先测退出文字编辑后第一个快捷键，再测手动选择中文。若后者缺少上下文通知，再单独评估 TSF 输入法配置激活通知。

**实现分工。** 输入法沿用 `InputContextTsf::currentDisabled()` 校验当前焦点栈顶的 `GUID_COMPARTMENT_KEYBOARD_DISABLED`，结合操作前保存的 IMM 关联决定恢复。热键回调只记录有界状态和时间，在回调外输出诊断。渐变由 `GradientNavigation::held()` 提供按键状态，`GradientCanvasPanel` 负责实际画布命中，`HotkeyHost` 负责事件放行。诊断按一次手势配对“键按下、鼠标按下、视口改变、鼠标松开、键松开”，并覆盖松键早于松鼠标的顺序。[TSF 状态实现](../native/InputContextTsf.h) · [输入上下文专项](../tests/test_input_context.py)

## 3 排版窗口、字体与右侧结果

本机已用 2048×2048 隔离文档核对背景返回后的替换、有序列表及去除行首尾空白。每项均检查当前草稿的预览、通过原生窗口对应后台接口明确提交、Photoshop 正文回读和一次撤销。有序列表另核对起始编号和编号格式。按钮的鼠标命中、继续编辑的选区保留、字体首开和跨 DPI 拖动仍待实机核对。报告见 `.runtime/text-453/draft-452-first-batch.json`、`.runtime/text-453/sidebar-list.json` 和 `.runtime/text-453/sidebar-trim.json`。

0.4.448 候选已把可见字体样本交给单独的原生线程，窗口边缘连续缩放时只应用本轮消息中最后一个尺寸。右栏直接确认使用当前计算计划。查找替换、整理和列表操作后，局部样式按 UTF-16 位置映射到处理结果，继续编辑和本地撤销共用这份映射。原生交互与后台专项已通过，仍需补齐 Photoshop 写入、撤销与性能对照。

- **字体。** 现有快速读取已延迟获取字体目录，原生窗口仍会在绘制一帧时同步生成至多三个缺失样本。分开测量目录往返、字体匹配、样本栅格化和显存上传。优先显示可选择的字体名称，再生成可见行的样本。若同步绘制占主耗时，让独立工作线程自持 DirectWrite／WIC 资源生成 CPU 位图，主线程按帧预算上传纹理。用字体 PostScript 名、样本文字、字号、DPI 和实际样本尺寸作为缓存键，按容量淘汰；滚动离屏、换选区或关窗后丢弃过期结果。用同一字体库比较冷开、热开、滚动帧耗时及首次可选时间，并核对选区、缺字提示和实际字重。[字体目录](../uxp/offline-probe/labels.js) · [样本绘制](../native/MenuToolsPanel.h) · [字体栅格化](../native/FontSampleRaster.h)
- **缩放与调框。** 测量窗口边缘拖动时的 `SetWindowPos`、`WM_SIZE`、`ResizeBuffers` 和正文重绘次数。优先同帧合并到最新客户区尺寸，按 DXGI 要求释放后台缓冲引用后调整并重建视图。只改变编辑器窗口时保存文档坐标中的文字框，拖文字框左／右边缘时固定对边。沿用同一份 DirectWrite 布局做光标和选区命中，只有正文、样式或实际排版宽度变化才重排行。实测窗口边缘、文本框两边、跨 DPI、非 100% 视图及中文选字，用拖动帧间隔 P95、松开后的坐标和一次撤销核对。[原生窗口](../native/MenuHost.cpp) · [Microsoft DXGI 缓冲区调整](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-resizebuffers) · [DirectWrite 布局与命中](https://learn.microsoft.com/en-us/windows/win32/api/dwrite/nn-dwrite-idwritetextlayout)
- **右侧结果直接确认。** `drawTypesetAction` 已有携带 `revision + plan_id` 的直接提交路径。先逐项复现列表、查找替换和整理，观察按钮禁用原因、异步结果是否过期，以及后台收到的列表参数。让主按钮消费当前草稿版本的有效结果，草稿改变后重算，旧结果失效。仅点击“将结果继续编辑”才将结果转为新草稿并建立本地撤销点。拆分和表格继续用各自的独立动作。分别从普通文字生成、从已有对象更新，核对列表编号、缩进、字符范围、一次撤销，以及浏览结果后直接关闭时的历史。[确认按钮](../native/MenuToolsPanel.h) · [后台计划与写回](../backend/text_tools.py) · [现有列表验收](../tools/test-text-list-layout.cjs)

**线程与数据。** 建议先在现有路径采样，再按下表拆改，字体、窗口缩放和结果确认可独立验收。

| 场景 | 数据与执行位置 | 如何判定有效 |
| --- | --- | --- |
| 展开字体候选 | UXP 给出字体族、样式、PostScript 名。原生窗口先显示名称。若样本确实占用主线程，工作线程单独初始化 COM、DirectWrite／WIC，仅返回 CPU 位图和样本键。渲染线程核对键与当前设备后创建 D3D11 纹理。 | 记录目录返回、首行可点、首个样本可见三个时间点。已滚离或选区已变的样本不覆盖当前行。关闭窗口及设备重建时清理作业和纹理。 |
| 拖动窗口或文本框 | Win32 回报客户区像素，文本框保存在文档坐标中。窗口尺寸变化只更新视口与交换链，真正调框才更新排版宽度。显示、选区和光标共享同一版布局，使用 DirectWrite 的点、位置和范围命中。 | 连续拖动记录每帧耗时、交换链调整次数、段落重排次数。在相同宽度和缩放下点同一字，字符位置、换行和对边坐标一致。[Microsoft DirectWrite 布局](https://learn.microsoft.com/en-us/windows/win32/api/dwrite/nn-dwrite-idwritetextlayout) |
| 点击右侧生成／更新 | C++ 提交现有 `revision + plan_id`。Python 保存运算结果、UTF-16 位置映射和列表选项，校验后一次性交给 Photoshop。异步回包还要匹配本地 `draftGeneration`，仅悬停试用的字体计划不能提交。 | 算出列表后直接确认，后台消费同一计划一次。改正文后旧计划失效，重新计算的结果包含最新正文和样式。[本地文本运算](../backend/text_plan.py) · [现有写入用例](../tests/test_text_tools.py) |

## 4 形状颜色面板回到展开中心时收起

展开的那一帧，保存当前菜单会话、触发控件和实际屏幕命中矩形。展开期间保留同一中心。鼠标完全离开后再次进入才收起，停在原位只触发一次。窗口因工作区边缘或 DPI 调整时，用同一锚点重新计算显示与命中。现有返回分支可能调用 `commitShapeDraft` 或 `emitMaskPreview`。将“回中心收起”处理为纯界面操作，草稿和已有预览归原会话管理，再次展开可继续，明确确认或取消才结束事务。按住鼠标、拖色标、数值输入或屏幕取色期间经过中心的事件直接消费，松开后才移出再进入才能切换。[模式行和返回逻辑](../native/ShapeColorEditor.h) · [原生菜单交互用例](../native/MenuHost.cpp)

在屏幕中央和右缘分别验收展开、移回、立即重开、切填充／描边、带草稿收起后 Esc。核对恢复的菜单坐标、悬停次数、草稿内容和 Photoshop 历史。

**状态放在 MenuHost。** 用现有 `menuEpoch`、触发分段、Win32 的 `RECT` 和“已经离开中心”标志保存一次展开。命中与绘制共用锚点，拖动／取色时消费经过事件。收起时只改可见状态和悬停命中，重新展开仍使用 `shapeEditor.draft`。原生自测分别断言收起前后动作提交数、预览确认数相同，明确确认时才增加一次。

## 5 文字智能对象

1. **先修入口识别。** 现行菜单用图层类型区分“画布排版”和“文字源”。给单选智能对象沿用现有选层刷新，异步读取该层 XMP，复用文字源的命名空间、版本、结构及摘要校验。结果按 PS 会话、文档、图层和历史绑定，换层、撤销或重开即失效。Caps 首帧直接显示已有菜单，确认有效后才画“文字源”并启用命中。正式更新时仍核对当前嵌入 PDF 的摘要。覆盖普通照片、链接对象、旧版文字、复制件、表格以及损坏的元数据。[菜单入口](../native/MenuHost.cpp) · [图层上下文](../backend/layer_context.py) · [源数据校验](../backend/text_object.py)
2. **用现有全画布对象先修 200% 偏移。** 建立点文字、段落文字各一例，记录本地基线或段落框、嵌入对象原始尺寸、PS 置入四角、父文档分辨率和编辑器缩放。先做平移和等比缩放，以四角定义的映射将本地位置转到父画布，再用逆变换解释鼠标位置。字号随对象变换映射一次，窗口缩放仅影响显示。当前 `object_text_box` 只覆盖部分正向段落框并返回零字号，点文字和退回像素边界的情形单独测试。用原图叠合核对基线、框宽、换行、字号和写回位置。旋转、斜切、透视或变形须单独验证；无法可靠反算时说明可用范围。[对象几何](../backend/text_tools.py) · [窗口画布](../native/MenuToolsPanel.h)
3. **再缩小内部画布。** 为新对象的源格式同时记录原排版坐标、裁切原点与内部尺寸。候选方案是先在隔离工作文档内由 PS 原生排版，再裁切该工作文档并导出 PDF。点文字取实际墨迹外包范围，保留基线和负边距。段落用逻辑段落框与实际外溢墨迹的并集，保留空行和换行宽度。明确 px、pt、分辨率和小数取整。置入时把裁切原点映射回父画布，更新已有对象时以原外部变换补偿新原点。旧 schema 1–3 沿用对应读法，迁移只发生在明确更新且通过对齐验证时。测试斜体外探、多行、段尾空行、200% 更新、独立复制、旧 PSD 重开和一次撤销。[PS 排版与置入](../jsx/text_object.jsx) · [格式及兼容样本](../tests/test_text_object.py)
4. **生成加速与原生入口分别决策。** 记录创建工作文档、重建文字、导出 PDF、置入、校验各段耗时，同时记录前台文档和可见闪切。优先优化实测最慢的一段；若整页工作文档是主要成本，可在第三步对齐通过后试直接在紧凑文档中按补偿原点排版。相同文字比较生成／更新耗时、矢量放大、原位置及文档收尾。仍会显示临时画布的方案如实记录。[矢量对象现状和实测](TEXT-VECTOR-OBJECTS.md)
5. **C++ 原型先验证宿主能提供什么。** 在 24.0.1 的隔离文档试传统 Photoshop C++ SDK Automation 插件，探测可用 suite，用 `PIUXPSuite` 与现有 UXP 交换一次真实源对象身份并打开编辑器。若试 Filter 插件，单独验证双击智能滤镜条目、恢复参数和取消。这与双击智能对象缩略图是两个验收动作。只有对象识别准确、普通对象保留 PS 默认行为、矢量放大和可编辑源成立，才考虑切换正式入口。UXP Hybrid 最低为 24.2，可在相应宿主另试；24.0.1 继续使用已验证的 Caps 路径。原型记录能否静默生成 PDF、是否有可接管的精确双击入口以及所需安装组件。[Adobe C++ SDK 与 UXP 通信](https://developer.adobe.com/photoshop/uxp/ps_reference/media/cpp-pluginsdk/) · [Adobe Hybrid 版本要求](https://developer.adobe.com/photoshop/uxp/guides/hybrid-plugins/) · [既有缩略图命中调查](TEXT-STYLE-PLAN.md#候选双击重新编辑的列表对象)

**坐标接口样例。** 拟以原文字坐标系中的像素保存裁切原点和内部宽高，以父文档像素保存 PS 读回的置入四角。对无旋转的 200% 样本，若裁切原点为 (40, 30)，原基线点为 (50, 45)，则内部基线是 (10, 15)。若置入后左上角为 (300, 200)，父文档基线应为 (320, 230)。编辑器只在最后绘制时叠加视图缩放和 DPI，重新打开或点击该基线应能逆算回 (10, 15)。这个数值样本可同时用于 Python 几何测试、C++ 鼠标命中和 JSX 读回。实际文字另覆盖 72 与 300 ppi，以防字号的 pt 与画布 px 混用。

**原生插件原型的交付物。** 从 Adobe Photoshop C++ SDK 配套的 Windows 示例工程构建 Automation 插件，按示例锁定编译工具集与 SDK 版本。原型记录宿主版本、能否加载、可取得的 suite、是否收到对应文档与图层的一次消息，以及关闭后的文档历史。UXP 与传统插件间首选 Adobe 的 `PIUXPSuite`／`messaging`，先验证一次往返，再尝试打开已有排版窗口。只有要验证“智能滤镜条目重新编辑”时才构建 Filter 样例。分别保存能复现的调用步骤及结果，再决定正式版是否值得引入额外二进制。[Adobe C++ 插件通信接口](https://developer.adobe.com/photoshop/uxp/ps_reference/media/cpp-pluginsdk/)

`text-object-in-place` 是后端隔离验收入口，固定样本覆盖 72 ppi 点文字和 300 ppi 段落文字，以及有可见／隐藏层的分层 PSD。原位导出后核验文件路径、保存状态、选层、图层可见性和原历史，再执行一次置入。撤销空间不足或无法证明回滚时停止本次请求。0.4.453 在绑定和执行阶段核对隔离文档及活动 UXP 会话，正式菜单的 Photoshop 前台校验仍用于日常操作。

0.4.448 候选已实现嵌入矢量文字源的入口校验，并以置入四角换算水平点文字和段落框的编辑位置。合成样例覆盖平移、200% 缩放、72 和 300 ppi。仍需在 Photoshop 对照置入和更新后的字形、框宽及位置，再验证紧凑 PDF 的裁切和原点补偿。

**本机隔离验收（2026-11-25）。** 0.4.453 在两组样本上通过原位生成、200% 缩放后平移 (17, 9) 像素、更新后保持四角位置、一次撤销与重做、源正文和字距回读，以及保存分层 PSD 后重开。生成实测 5.91–6.02 秒，更新 8.53–8.81 秒。生成后脚本切回上一历史状态时，图层树、文字和可见性恢复，选层与已保存标记未恢复。关闭隔离文档后已核对原稿。屏上闪动、编辑框视觉位置、紧凑裁切及 C++ 宿主原型继续待验。报告见 `.runtime/text-453/vector-acceptance.json`。

**撤销对照（2026-11-25）。** Photoshop 24.0.1 的三组独立对照涵盖替换并删除源层、替换并隐藏源层、转换智能对象。直接赋值 `activeHistoryState` 恢复了历史节点，但三组都丢失原选层和已保存标记。调用 `undoEvent` 的对照均恢复选层、保存标记和原历史计数。0.4.454 候选在矢量置入失败且能确认本次历史项时使用该原生回滚。正式和原位矢量路径均以 72 ppi 点文字及 300 ppi 段落文字验收，通过生成后撤销、更新后撤销、恢复选层和保存标记，以及分层 PSD 重开。对照见 `.runtime/text-453/undo-native-baseline.json`，实机报告见 `.runtime/text-454/vector-standard.json` 和 `.runtime/text-454/vector-in-place.json`。

**原层写回对照（2026-11-25）。** 普通文本由 UXP 在原文字层上执行 `set`。本机隔离对照保留文字层 ID 及总层数。对共享同一嵌入 PDF 的两个实例直接运行 `placedLayerReplaceContents` 会同时改变两个实例。0.4.458 起在 Photoshop 24.0.1 核实独占内容后原层更新，共享实例采用独立替换。内置高斯模糊在隔离文字层上应用后，文字已栅格化。复测工具为 `tools/test-text-vector-layer-identity.ps1`。

**传统 C++ 滤镜及 UXP 原型（2026-11-25）。** `tools/photoshop-filter-probe` 使用 Adobe 官方 SDK 构建 x64 滤镜 .8bf 和常驻 Automation .8li，另有独立 UXP 探针。Photoshop 24.0.1 通过独立 RGB 文档调用滤镜事件，消息从滤镜发到 UXP，再经 Automation 回传同一挑战值。连续 3 次通过，Photoshop 持续响应，原稿及已选图层通过核验。早期测试把接收回调注册在 .8bf 中，成功单向送达后 Photoshop 在回传期间异常退出，已保留原始证据。当前 .8bf 只在其调用期间取得并释放 SDK 接口，常驻 .8li 在启动和退出回调中管理监听器。此原型未接入文本写回。实机回执见 `.runtime/photoshop-filter-probe/actual-round-trip.json`。

同一 .8bf 直接应用于隔离文字层时，Photoshop 保留图层 ID 和总层数，但把该层转成了像素层。需保留正文、字体和段落可编辑性时继续用 UXP 文字事务。实机回执见 `.runtime/photoshop-filter-probe/exact-type-filter.json`。

## 6 标记图层，到目标上方或下方移动

标记仅保存当前 PS 会话、文档、源图层 ID 和源堆叠顺序，允许继续在图层面板选目标。父组和子层同时选中时只保留最外层选中对象。提交前重新读取源与目标的存在性、锁定、父组和顺序。多个目标同父时，选择“上方”用最上目标作锚，“下方”用最下目标作锚。跨父的目标集合没有唯一插入位置时给出原因，单一明确目标可接收跨组移动。源与目标重合、目标在源组内部或请求插到背景层下方时保持标记供重选。Esc／右键只清除尚未提交的标记。[现有排序语义](../uxp/offline-probe/labels.js) · [Adobe Layer.move](https://developer.adobe.com/photoshop/uxp/2022/ps-reference/classes/layer)

确认后在原有 UXP 模态事务内对真实源 ID 调用 `Layer.move`，按方向选择遍历顺序，保留同组和跨组源的相对堆叠。提交前后比较父组、兄弟顺序和对象 ID，失败时回滚并读回，结果未知按项目规则保留。设计用例为 A、B 两层及含子层的组移到目标上下，各执行一次，再测跨组、父子同选、目标被删、取消、忙碌和一次撤销。通过条件是原对象和子树身份一致、位置正确、无新增副本。[Adobe 模态事务与历史回滚](https://developer.adobe.com/photoshop/uxp/2022/ps-reference/media/executeasmodal)

**数据放在哪里。** 待移动标记由 Python 后台在内存中持有，跨两次 Caps 唤起保留。MenuHost 只显示标记数量、方向和可用状态。UXP 接收源 ID、有序目标 ID 和放置方向，在进入模态事务后重新解析真实图层。以从上到下的 A、B 为例，放到 T 上方时按 A、B 对同一 T 调用 `PLACEBEFORE`，放到下方时按 B、A 对 T 调用 `PLACEAFTER`。用最终同级序列验证 A、B 顺序，再用 PS 一次撤销核对原父组。先扩充现有排序测试的图层树样本，再验收真实图层组。[图层排序专项](../tools/test-document-sort.cjs)

## 7 专业段落排版及其他待办

- **制表位与中日韩排版。** 用含制表符、中文标点和日文行首行尾禁则的短文做基准。在 PS 24.0.1 原生面板逐项调参数，对照前后 `paragraphStyleRange` 并保存重开。沿用已有描述符读写与段落范围，只更新涉及段落的指定字段。制表位先验证位置、单位、对齐和重开，禁则和标点间距先验证宿主可用的预设与实际断行。较新的 `ParagraphStyle` DOM 有多项属性要求 24.1，旧版使用实测可往返的描述符。只在能正确排版和读回的宿主显示对应控件。[Adobe Photoshop 东亚组版](https://helpx.adobe.com/photoshop/desktop/text-typography/international-text-languages/set-asian-text-composition-options.html) · [ParagraphStyle 版本表](https://developer.adobe.com/photoshop/uxp/2022/ps-reference/classes/paragraphstyle) · [动作描述符采集](https://developer.adobe.com/photoshop/uxp/ps_reference/media/batchplay/) · [InDesign 制表位参照](https://helpx.adobe.com/indesign/desktop/format-and-style-text/tabs-indents-and-spacing/set-and-repeat-tabs.html)
- **样式继承、嵌套、串排和段落保持。** 先以一个基础样式、一个派生样式和一条局部覆盖验证优先级、循环引用、字符范围及撤销。再用两个框、一段会溢出的正文验证连续文字源、框的阅读顺序、每框起止位置和溢出提示。段落保持增加“与下段同框”和最小保留行数的样例。每项先做 Photoshop 存储与读回试验；需要项目自有文字源时，只在原型中实现，待 PS 正式输出、PSD 重开、重新编辑和一次撤销均通过再开放。草稿继续沿用现有选区样式与显式写入。[现有范围模型](../native/TextStyleDraft.h) · [Adobe InDesign 段落样式](https://developer.adobe.com/indesign/uxp/dom/api/p/paragraph-style/) · [InDesign 文本框串排](https://helpx.adobe.com/indesign/desktop/add-and-manage-text/add-and-import-text/thread-text-frames.html)
- **渐变扩展。** 先用现有颜色标、透明度标及插值方式做预设往返。噪点渐变另采集宿主参数与随机种子验证重开。渐变映射和图层样式分别核对目标及自身描述符，在隔离图层上验证确认／取消。多点采样拟合先以已知渐变比较各采样位置的色差，再决定控件和宿主范围。叠加控制柄的旋转与混合 DPI 跨屏复验并入第 2 批。[渐变编辑](GRADIENT-EDITOR.md)
- **首页与大文档兼容。** 首页沿用两列四行和独立剪贴板，在真实 Caps 菜单核对直接新建、长按回中心取消、输入焦点和右缘命中。大文档多选／蒙版使用已有读取对照脚本，正常与受限 CPU 同样本比较就绪 P95。跨设备另记 Windows、PS、DPI 和鼠标回报率，覆盖首次 Caps、滚轮和高回报率鼠标。[首页控件规范](UI-GUIDE.md) · [读取对照](READ-STRATEGIES.md) · [Hook 兼容性](HOOK-COMPATIBILITY.md)

**选区与段落的数据边界。** 原生编辑器用 UTF-8 字节区间保存草稿，用 `TextStyleDraft` 序列化时转换为 Photoshop 使用的 UTF-16 代码单元区间。Python 沿用 `text_plan.units` 和 `origins` 映射，UXP 按 `paragraphStyleRange` 合并已验证字段并保留其余属性。例如在“甲😀乙”中只选“乙”，源文本的 UTF-8 区间为 [7, 10)，提交区间应为 UTF-16 的 [3, 4)。用同一用例覆盖选区换字体、跨段应用、撤销和 PDF 更新，再单测末段标记和真实制表符。每增加一个段落字段，记录它在原生 PS 中的单位、取值、适用宿主及 PSD 重开读回样本。[草稿范围](../native/TextStyleDraft.h) · [文本映射](../backend/text_plan.py)

## 专项验证与构建入口

| 改动范围 | 可直接复用的验证 | 实机还需核对 |
| --- | --- | --- |
| 大文件读取、右侧结果及样式 | Python `unittest` 的 [菜单请求](../tests/test_menu_tools.py)、[正文与计划](../tests/test_text_tools.py)、[草稿与设置](../tests/test_typeset_draft.py)，Node 的 [UXP 文本专项](../tools/test-text-tools.cjs) | 各阶段耗时、关闭无新增历史、一次确认写入和一次撤销 |
| 输入、字体、拖框、形状与渐变 | 原生 `MenuHost.exe --self-test`、`HotkeyHost.exe --self-test`，以及 [输入上下文专项](../tests/test_input_context.py)、[渐变专项](../tests/test_gradient_editor.py) 和 [UI 契约](../tests/test_menu_ui_contract.py) | PS 前台中文上屏、首个快捷键、导航手势、不同 DPI 的鼠标命中、屏幕边缘收起 |
| 智能对象与图层移动 | [XMP 与矢量事务专项](../tests/test_text_object.py)、[排序样本](../tools/test-document-sort.cjs) 和 [PS 文字对象验收入口](../backend/text_object_acceptance.py) | 200% 对齐、旧 PSD 重开、复制件独立、原对象顺序及一次撤销 |

原生菜单由 [菜单构建脚本](../tools/build-menu-host.ps1) 编译，它先运行 UI 契约。完整候选由 [build-exe.ps1](../build-exe.ps1) 编译原生组件并使用 PyInstaller 打包。UXP、Generator、输入 DLL 各按改动范围部署并核对实际加载版本或文件哈希。每批记录同一隔离样本在基线与候选的结果、实际服务和 PS 会话、权限级别、真实 Caps 及受影响操作。视图设置和命名样式预设使用现有独立存储，恢复默认视图后核对预设仍在。编辑过程保留本地草稿，用户明确写入时才提交文字及样式。[设置存储](../backend/typeset_settings.py)
