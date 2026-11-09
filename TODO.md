# 151500px-menu TODO

- [x] **F聚焦所选图层首版319／UXP97**：PS工作区F聚焦单层／多选／组，Ctrl+F保留查找，原生平移／缩放与边界显示，最高400%。低CPU及正常各8项PS接口、低CPU真实F／Ctrl+F完整批次、正常连续F分项和实屏通过；内容、选择、历史与原稿／资源恢复。正常批次因外部前台变化中止，不冒充完整通过。实际入口、分发与范围见RELEASES.md和docs/LAYER-FOCUS.md。

- [x] **F边框自动消失与长按临时聚焦（320／UXP98）**：短按保留聚焦结果，边框约0.5秒后关闭；长按至少250ms，松开恢复原缩放／中心，Esc及失联按原请求收尾。额外内容恢复原值，图层边缘明确关闭以清除319遗留常显；内容／选择／历史不变，切文档不误操作新目标，未知不重发。候选验证与分发范围见RELEASES和docs/LAYER-FOCUS.md。

- [x] **Caps智能对象紧凑布局（320）**：移到右侧新建／调整区下方；单选仅“智能对象”，多选才显示左“单层”／右“逐层”，约减半原多选组宽度并同步实际命中，保留字号、tooltip及跨父组／锁定／输入限制。

- [x] **快速选层→F接管与原生透传专项（321／UXP99）**：PS队列中核验并消费裸F，不在失败时回放；修复全局吞掉已放行按下的配对松开造成后续F被判重复。长按蓝框在模态外持续绘制，松开恢复；移除健康长按30秒上限。最终低CPU／正常各6次实际图层列表消息＋F均命中新目标并保持屏幕模式，各5项F手势、Ctrl+F、空层失败不透传、真实文字编辑F不触发聚焦通过。其他机器／IME、快速多选／组鼠标序列、实体输入和切回首键仍待验，不将偶发候选的全部来源标成已修复。详见docs/LAYER-FOCUS.md。以下保留原计划和历史问题：用户补充“点击图层后快速按F，插件不生效，直接原生功能”，这次没有候选框；此前也报告过候选框及重新点击激活后恢复。319均保留为已知问题。
  - 先区分钩子消费、动作投递、任务创建和聚焦成功后的输入状态；没有任务不能直接判作F未接管。320实测已定位并修正另一条已消费F后被自身busy清空非输入证据而拒绝的路径，原生透传／候选仍未标为修复。将“PS已激活时内部选层”和“从其他程序点击PS图层返回”分别复现，不把前台资格失鲜与输入世代失鲜混为一谈。
  - 明确修复要求：在已核验的PS非输入区域，F按下即由插件保留；焦点／选层证据未就绪或忙碌时拒绝本次，不触发PS原生F、不排队、不等待补执行，也不合成回放。重复／松开继续归属原按下，不能在焦点或世代变化后泄漏原生按键。Ctrl+F查找、Caps+F图层样式、Shift+F及其他应用保持原分工。
  - 文字、重命名、数值、真实组字或输入对话框中仍正常让行F；不能用整个PS一律吞F代替区域识别。尤其验证没有隐藏Edit的画布文字，未知区域不能凭旧non-input值被硬判为非输入。
  - 最小日志只记点击／激活时间、实际root／focus及窗口身份、原生inputEpoch、发布的guardEpoch、searchAllowed／evidenceFresh、按下是否消费、投递结果及任务ID／目标；不记录正文或候选内容，不在钩子回调中新增PS查询、阻塞等待或进程打开。重点定位HotkeyHost.cpp的F资格放行分支和InputFocus的点击失鲜→新证据发布交接。
  - 若F已被消费且聚焦成功，再核对fitLayersOnScreen、缩放、performMenuCommand前后的真实焦点、画布句柄、HIMC关联／TSF上下文与实际禁用值；对照重新激活前后，检查InputContext焦点回调与input_language／input_method是否漏掉重建或重新关联。保留中文输入法，不用切英文、固定延迟、放慢刷新或削减校验掩盖问题。
  - 验收先双逻辑处理器／Below Normal，再正常配置；覆盖快速单选／多选／组、窗口切回、F重复与配对松开、忙碌／失鲜拒绝不落原生、真实文字／重命名／数值／组字让行、Ctrl+F及外部应用。保留原稿、一次操作与未知不重发；门禁未齐不得标为已修复。


- [ ] 后台设置 UI 精简与整理（2026-11-08，用户要求加入 TODO，暂不实施）：现有设置较繁杂，部分入口疑似无用。实施前逐项核对实际调用、适用版本、用户用途及依赖，区分有效功能、重复入口、失效／历史选项与纯诊断内容；合并重复项，移除确认无效的设置，按常用／高级和任务分组减少阅读负担。保留单应用 tab 结构、必要的恢复／安全保护／诊断能力及原配置兼容，不凭“看起来无用”删除仍有用途的项。复用共享控件、中文文案、主题和文字角色，验收保存／即时生效、旧配置迁移、键盘焦点与窄窗口；尚无 UI 修改、删项清单或完成验收。

- [ ] 待移动对象与目标位置粘贴（2026-11-08，用户明确“先加入 TODO”，暂不实施）：
  - 支持把当前单选／多选的图层、组或混合对象标记为“待移动对象”；标记时不立即移动、复制或删除源对象。
  - 标记后仍可正常滚动图层面板、展开／收起组及选择其他单个／多个图层或组，不锁定原选择，不拦截普通选层操作。
  - 有有效目标选择时，自动出现“粘贴到上方／粘贴到下方”按钮；明确点击后移动原对象，保留源对象相对顺序与组结构。多选目标的上下边界、跨父组归属、同时选中组与其子层的归一化规则在实施时明确；不把移动实现成复制。
  - 右键或 Esc 退出待移动状态并清除标记；退出标记不修改图层。已经提交的移动任务仍遵守执行／未知结果保护，不能把取消标记当成强行中断或恢复成功。
  - 实施时核验同一文档／PS会话、源对象与当前目标的真实身份及可移动性；源被删除、目标与源重叠、组移入自身子树、背景／锁定限制及文档切换不能误移动。沿用忙碌拒绝、不排队、写前复核、一次撤销、失败回滚与未知不重发。
  - 按影响范围验收：单层／多层／整组／混合对象、上下粘贴及多选边界、滚动后选层、源顺序和组结构、撤销、右键／Esc、输入／焦点／忙碌／失鲜保护；新增按钮复用共享控件、语义字号和实际命中规则。此项尚无实现、构建或实机通过记录。

- [x] 318本机功能与发布候选：补充文字模态写回修复，最终UXP96；PS隔离文档两配置写回／一次撤销及低CPU文本面板请求链路通过，2026-11-08用户反馈“可以写回”，原截图文字编辑→文本工具写回手动场景已通过。按用户要求将图层查找统一改为Ctrl+F（含菜单内），置顶／置底／上移／下移四段并排。低CPU和正常Ctrl+F连续重开各3/3，裸F放行、真实文字编辑保护／退出恢复有有效脚本样本；原生／67项专项／前端／冻结／EXE门禁及三档预览通过。PS实屏布局已核验，实体鼠标点击、其他机器／冷启动／闲置仍待验。Listary开启时快速重复Ctrl+F会冲突，临时退出后才完成连续测试，不标兼容通过；其配置未改。实际入口与附件分发以RELEASES和Wiki Versions为准，证据 `.runtime/release-318/`。

- [ ] F 键线程内拦截原型（2026-11-08，隔离研究保留；当前触发已改Ctrl+F）：新增独立只观察 DLL `tools/ThreadFProbe.cpp`，沿用已验证 PS UI 线程附着方式；保留生产 InputContext／全局 F，不修改布局、文档或消费按键。低 CPU／正常原生夹具、3项分析测试通过；真实 PS24.0.1 已核验三钩子安装、20秒观察、停止与加载钩子卸除。电脑控制两次报窗口所有者不符，本批 F 有效样本0，不能把生命周期验证当时序通过。下一门禁仍是中文 IME／消息队列实际时序，随后才实施独占消费和后台转发、输入保护／退出及低 CPU／正常对照；当前0.4.317继续作为已验收基线。跨机条件、完整验收矩阵及工具用法见[Hook迁移与验收](docs/HOOK-COMPATIBILITY.md)，原始证据 `.runtime/thread-f-probe/live-observe-1/capture.json`；线程内方案不预先标为更稳定。详见[验证计划](docs/INPUT-LANGUAGE-ASSESSMENT.md#f-键线程内拦截验证计划)。
  - 继续实施：`native/ThreadFKey.h`／`tools/test-thread-f-route.cpp` 已实现仅隔离工具使用的消费核心，低 CPU／正常各1000轮双路径交接模型及真实 InputContext 窗口检查通过；覆盖通知失败不吞键、按下／重复／松开配对、停止排空、Edit／画布／组字消息保护、原生世代与旧通知拒绝。直接回调测试不算真实键盘／PS验收。已保存最新分层原稿、正常重启PS并恢复核对原内容；新PS仍无法绑定，隔离应用控制授权超时，交互窗口样本0但自动退出／关联恢复已核验。生产仍317，未接后端查找、未构建或发布新产品。证据 `.runtime/thread-f-route/` 与 `.runtime/thread-f-prototype/`。
- [x] 历史0.4.317／UXP0.3.95候选（2026-11-08，未提交／未发布）：修正 UXP 工具观察乱序／待写去重，以及自有查找菜单返回时的输入世代与隐藏菜单焦点交接。用户 F 失效已复现；最终候选低 CPU／正常各连续3/3次 Esc→立即F通过，空白画布中文“你”＋同批 Ctrl+Enter→M 两档均有有效通过样本。79项低CPU门禁、16项正常焦点专项、原生边界、冻结与EXE smoke通过；实际入口与dist为`9fa68f48…`。已有文字自动夹具未通过（T候选／合成点击未进入编辑）保留；用户随后实体检查 F／已有文字中文／立即V，明确确认“两项都正常”，本机该序列验收通过。长期／冷启动及其他机器仍待验。原稿64文字与图层、三配置文件、资源恢复均核验；测试文档77已关闭并恢复原稿64，manual-accepted.json保留用户验收与收尾凭据。证据及中间失败见docs/INPUT-LANGUAGE-ASSESSMENT.md与.runtime/ime-317/。
- [ ] 历史0.4.312候选2：过滤 txBx／kind=tool 来源，800ms回收要求新非模态快照，工具缓存重新校验会话／令牌／时效。185项低CPU门禁及正常专项、冻结和smoke通过，但真实低CPU3次前台变化中止，有效样本0。证据.runtime/ime-312-rev2/保留，不能代替317的当前验收。
  - 已完成（代码、离线测试、候选构建）：点击画布进入文字编辑的同步恢复——文字工具单击画布、任意工具双击、选中文字图层按 Enter 时，在 PS 处理消息前恢复根窗口／画布 HIMC；Esc（无组字）、Ctrl+Enter、小键盘 Enter 在按键后重新禁用；UXP 模态事件只作带时间戳的异步兜底。HotkeyHost 全局钩子在非 PS 前台不再 `OpenProcess`／查询镜像，前台身份改读消息线程的 WinEvent 快照，并为非 PS 进程保留持有句柄的否定缓存。低 CPU 非 PS 前台钩子回调中位：鼠标 49→2 µs、按键 55→6 µs；正常配置 50→2、56→6 µs（见 stage1 报告 §4；另一台电脑未复测）。158 项输入专项、81 项诊断／UXP／打包／版本／UI 契约、原生自测、候选构建 `FE100283…`、四组件／UXP 0.3.93 逐字节核对及隔离 smoke 通过。
  - 未通过／未完成：门禁 A 不通过——真实 PS 24.0.1／UXP 0.3.93 下 10 次 Enter 进入文字编辑，UXP 模态进入事件 0/10，事件不随画布文字编辑触发；依赖它的 800 ms“未进入模态”超时回滚不可用，且对不创建隐藏 `Edit` 的点击画布形态可能在编辑中重新禁用画布。门禁 B（PS 刚激活后的第一个 Caps）因菜单可见性缓存、日志轮转和光标裁剪未取得有效时序，未测成。用户实体验收未做。文字工具点击画布进入编辑无法输入中文仍未修复。
  - 上述两项为候选 1 的历史记录，不替代首行候选 2 状态。Q10 已完成 15 个实体录制窗口，报告 `tools/ime-probe/results/q10/REPORT-Q10.md`；原 0/10 原因仍未定位。候选 1、报告与证据保留在 `.runtime/ime-312/` 和 `.local/recovery/ime-312/`。

- [x] 0.4.311正式分发（2026-11-06）：源码 `24e938b`／标签 `v0.4.311` 已推送；Release https://github.com/mianxiu/151500px-menu/releases/tag/v0.4.311 附件 `151500px-menu-Backend-0.4.311.zip`（SHA256 `7f01cf25…`）、校验文件、`151500px-menu-UXP-0.4.311.zip` 与 `151500px-menu-update.json` 已公开；四份附件匿名完整下载、哈希及更新清单 latest 下载核验通过；旧 0.4.310 Release 已删除，Git 标签／源码历史保留。本机实际入口已切换 `0861DDF0…`（0.4.311、PE 版本 0.4.311），内嵌四组件及 UXP 0.3.92 与源码逐字节一致，服务版本 0.4.311、UXP 已安装 0.3.92。Wiki 和最终验证记录见 `.runtime/ime-311/REPORT.md`；文字工具点击画布、实体键盘及其他电脑仍未验收。

- [ ] 0.4.311 PS 线程内同步焦点策略（2026-11-06）：`native/InputContext.cpp` 在 `WH_CALLWNDPROC` 的 `WM_SETFOCUS` 中同步解除已验证 PS 根窗口／根内 `PSViewC` 的 IMM 关联，焦点转到 `Edit`、面板、未知窗口或浮动对话框时同步恢复；策略模式删除鼠标按下、`Enter`／`F2` 恢复和 1.5 秒租约续约，保留失去前台、自有菜单往返、PS 自行替换关联、输入语言重套用和服务退出恢复；状态增加策略开关、受保护窗口数、原因／时间／残留。后台在附着后立即开启策略，不等待 UXP／工具证据；策略模式停止逐次 disable／renew，保留旧 pending／历史未知不重发。UXP 升 0.3.92，列表排版门槛同步；打包门禁补 `InputService.dll`。已完成：原生自测正常与低 CPU 通过；受影响输入／IME／诊断／UXP／打包门禁通过；最终 EXE `0861DDF0…` 构建与内嵌四组件核对；PS24.0.1／中文布局真实场景：问题 A 按住 Z＋投递画布点击＋继续重复＋释放，13 次 Z 均为 `0x5A`、无组字；画布文字 ESC／Ctrl+Enter 各 3 次退出后立即 M 均为选框、重新进入 A 组字（6/6；最终构建再验 1 次）；新建文档对话框内 A 组字且该 PSFloatC 画布未被禁用，关闭后 M 生效；占位应用切换返回 M 生效；连续 20 次退出＋M 低 CPU 20/20、正常 20/20（最终构建 3/3）。冷启动主窗口出现后约 0.47 秒策略保护；F／Caps 在启动早期均到达钩子，F 因 PS 首页无文档被拒、Caps 因鼠标在 PS 外被拒，打开文档并移入鼠标后两者均打开菜单。未完成：文字工具点击画布进入编辑的专项自动化（投递点击不进入 PS 文字编辑，真实点击受前台／光标裁剪影响）留待实体点击验收；该焦点转移与已通过的 Edit 恢复路径相同。合成按住／释放后 PS 临时缩放工具未自动回落，测试后已显式恢复，不能作为实体工具回落证据。实体键盘、其他输入法／PS／Windows、故障机 PS25.5.1 与其他电脑仍未验收。冷启动无 0.4.310 同口径基线，不声明加速。证据 `.runtime/ime-311/REPORT.md`、`live/`、`cold-start2.json`。

- [ ] 2026-11-05 全面更名为 151500px-menu。GitHub 仓库及远程地址已更名；软件显示名、EXE、包名、更新源、文档已调整。旧配置与插件身份保持兼容。更新／安装／语言／数据路径专项33项、插件管理25项、打包4项、UI契约5项、前端46项、原生输入保护及冻结启动／退出／重启通过。0.4.310已构建、打包并切换实际新名称入口，四组件哈希一致，旧配置保留，PS正常重启并从分层备份恢复1份文档。GitHub仓库已改名，Wiki改名与迁移说明已推送b8e6dc6。Windows因目录占用拒绝物理更名；G:/Github/151500px-menu已建立指向原目录的兼容入口，待释放占用后完成实际迁移。0.4.310发布状态见 RELEASES.md 与 Wiki Versions；本轮前已有未提交改动随 0.4.310 发布提交（包含未完成的文字实验代码）。证据 .runtime/rename-deploy/verified.json。旧版需手动下载完成首次迁移。

- [x] 0.4.310正式分发（2026-11-06）：源码7dff927／v0.4.310已推送；Release v0.4.310附件 `151500px-menu-Backend-0.4.310.zip`（SHA256 5da1e6…）、校验文件及 `151500px-menu-update.json` 已公开，匿名完整下载、哈希及更新清单核验通过；旧0.4.306 Release已删除，Git标签／源码历史保留；Wiki版本页及受影响说明已同步（ecc1f12）。复用已切换的0.4.310 EXE，未重建或重跑已通过测试；其他电脑、实体按键及其他PS／Windows仍未实机验收。证据 `.runtime/release-310/REPORT.md`。

- [ ] 输入法通道路线（2026-11-05，仅记录，待用户选择）：现状事实——①进程内注入在本机 PS24.0.1 已实测**有效**（`confirmed_disables=1`、`ime_open=0`、`tsf_disabled=true`，布局仍为 `0x08040804`，焦点 `PSViewC`），无管理员权限，但在 PS 以管理员运行／受管电脑（UIPI、安全软件）会失效；②系统 TSF 服务**无法自动加载**（见下一条），要加载只能把它选为输入法（替换用户中文输入法）或由宿主应用创建，二者都不可接受；③按应用切无 IME 布局（`WM_INPUTLANGCHANGEREQUEST`）无管理员、跨版本／权限最稳，但会改语言栏／输入法指示器，用户此前明确否过。可选方案：**A 只保留注入**（现状；保持已验证行为，环境限制如实说明）；**B 混合：注入优先，检测到注入不可用时自动降级为切布局**，后台显示当前通道（推荐——需要新增：注入失败判定（控制窗口／确认事件在超时内未建立、或属性/owner 不符）、切布局前的原 HKL／IME 快照与切回校验、切回触发点（进入文字／离开 PS／菜单唤出）、以及“其他应用不受影响、切回后指示器一致”的验收）；**C 只切布局**（若接受指示器变化，则最省事最稳）；**D 包装转发型 IME**（自有 TIP 作为输入法再转发给原 IME：工程量与风险都大，不建议）。建议先按 B 设计、以 A 为默认：注入可用时完全不碰布局，只有不可用才切；若用户不接受任何指示器变化，则保持 A 并把限制写进使用说明。验收口径（任一方案都要）：画布上中文输入法开启时 `a/v/F/W/S` 等可用且原布局不变；进入文字／重命名／数值输入时输入法恢复可打中文、离开再禁用；其他应用与 CapsLock 无残留；覆盖 PS 提权、受管电脑、远程会话与 PS 22–27／Win10 1809／Win11；不重发未知请求、不强制结束 PS、临时文档按授权收尾。未完成：B/C 的具体实现与实机验证；A 的实体按键与多版本矩阵。

- [ ] 自有 TSF 输入服务（2026-11-05，结论：**A 方案不可行**；已注册项为惰性，0.4.309 已恢复注入）：已实现并验证的部分——`native/InputContext.dll` 新增 `InputContextAttach/Detach/Owner` 导出（原注入路径改为调用同一 `attach()`），`tools/test-input-context.ps1` 通过；新增 `native/InputService.dll`（`ITfTextInputProcessor`，只对 `Photoshop.exe` 生效，从自身目录载入 `InputContext.dll`，1.5 s 对账定时器，检测到已有控制窗口时**让位**避免双策略），`tools/test-input-service.ps1` 在**无 PS、无管理员**下端到端通过（CoCreateInstance→Activate→attach→控制窗口类正确）；后端 `backend/input_service.py`（HKLM 注册／ProgramData 暂存与哈希、提权 helper＋回执、`OwnerPid` 发布、状态）与动作 `input.service.status/install/remove`、`--input-service` 提权 CLI；后台设置页曾提供注册／卸载（各一次 UAC）。失败与证据：用户完成注册后 HKLM 两处键与两个语言 profile 均 `Enable=1`、ProgramData 文件哈希一致，但 **PS 进程未加载 `InputService.dll`**；决定性实验——用探针激活 TSF 线程管理器后，本机已启用的 `tiptsf` TIP **同样未被载入**，说明 enabled profile 只在被选为输入法时激活，无法在 PS 启动时自动加载；另外首版 profile 结构写错（应为 `LanguageProfile\0x00000804\{profileGUID}`，实际写成 `0x00000000\0x0804`，`EnumProfiles` 一直 `ours=0`），但修正结构也不会自动加载。我的回归：0.4.308 的“已注册就不再注入”把唯一可用通道也关掉，用户遂完全失去输入法保护；0.4.309 已恢复注入（不再因注册而跳过，TIP 侧改为让位）并复验：聚焦 PS 后 `state=non-input`、`disable_requests=1`、`confirmed_disables=1`、`ime_open=0`、`tsf_disabled=true`、`native_events=true`、`keyboard_layout=0x08040804`、`focus_class=PSViewC`，随后已把前台还原。0.4.309 已切换运行：`716BAFBC…`，四个内嵌组件 HotkeyHost `B30C8582…`／MenuHost `5947FCD…`／InputContext `42DB9000…`／InputService `140BF5BD…` 与 `native/` 一致，快捷键／启用状态保留、无 pending；门禁为原生三项自测、`test-input-context`、`test-input-service`、后端 111 项、前端构建＋46 项、冻结隔离核对与全量 smoke；备份 `dist/0.4.308-backup/`。已注册的 HKLM 服务保持惰性、枚举不到、不加载；设置页已改为如实说明并只保留“卸载（管理员）”。待收尾：卸载这条惰性注册（一次 UAC）或按新方案重做；python 侧 `input.verify` 处理路径已无请求来源（随 0.4.307 删除 native 侧后成为死代码）待清理；后台区块未做浏览器视觉核对；若将来重启 TSF 路线，必须先解决“自动加载”而非只修 profile 结构。证据 `.runtime/switch-0.4.308.json`、`.runtime/tsf-probe/`、`.runtime/check-ime-309.py`。

- [x] 0.4.307 本机候选与运行入口切换（2026-11-05，L1 钩子修复已上线）：`backend/version.py` 与 `tools/windows-version.txt` 升到 0.4.307；`build-exe.ps1 -SkipFrontend`（前端源码早于 frontend/dist，已先核对）重建 EXE `7A7F3B0B…`，FileVersion 0.4.307。切换前门禁：重建后的原生组件三项自测 0 退出；隔离启动核对 version／PE 版本、内嵌 HotkeyHost（665FF7DA…）／MenuHost（F44C90DB…）／InputContext（B5DD3C93…）与 native/ 逐个一致、ready、hotkeys、菜单就绪、无 pending、正常退出 0；`tools/smoke-exe.py` 全量通过（HTTP、资源、原生菜单、托盘、设置、插件升级/回滚、重复启动、退出与重启持久化）。随后按正常退出路径停旧 0.4.306（busy=false），备份 `dist/0.4.306-backup/PhotoshopMenu.exe`（60051E9B…），用同数据目录启动新 EXE；核验运行版本 0.4.307、菜单 ready、hotkeys true（新 hotkey PID 38760）、三个内嵌组件与 native/ 一致、`scan_us=0`（每秒全表扫描已随新组件消失）、`dropped_total=0`、快捷键与启用状态原样保留、pending 空、非 busy、数据目录不变；`dist` 与运行 EXE 哈希相同。说明：MinGW PE 时间戳使同一源码两次构建哈希不同（07A370…→665FF7…），以“打包副本与 native/ 一致＋源码一致”为准；UXP 已安装 0.3.91，PS 未运行故 loaded 为空。未完成：实体 Caps/F 手感、PS 22–27／Win10 1809、远程与受管电脑；未推送源码/标签/Release，0.4.307 是本机候选（高于公开 0.4.306，更新检查显示已是最新，不会降级）。证据 `.runtime/switch-0.4.307.json`、`.runtime/hook-l1-live/result.json`。

- [ ] 按键层硬化（2026-11-05，实机矩阵已跑，仍未切换运行版本）：全局键盘钩子回调内不再有任何跨进程等待——删除 `input.verify` 的 200 ms 同步往返，改为只读“输入世代＋工作区根＋焦点窗口”全部匹配的已发布结论；结论失鲜时单键动作放行且不执行，显式 Caps 组合键不受失鲜限制。快捷键资格收敛为“PS 工作区前台”单一条，删除激活点击门槛（原状态机仅保留诊断输出），因此工作区前台不再被 reserve 吞掉 Caps。进程身份改为按 PID 按需 OpenProcess＋镜像名＋创建时间（持有句柄即排除 PID 复用），删除每秒全系统进程枚举线程。配置解析失败保留上一版并报错，不再退出宿主；Caps 采样 16→250 ms，健康日志 1 s→2 s 或异常即时，取消每秒强制 caps.state；输入判定变化时后端立即推送，不再等 100 ms 监视节拍。验证：原生三项自测与 test_native_win_popup 通过，test_menu 15 项、输入／诊断 149 项回归通过，重新构建 `native/HotkeyHost.exe`（首次构建 07A370…；打包与运行副本为 665FF7…，同一源码，MinGW PE 时间戳使两次构建哈希不同）；每秒进程扫描 14–20 ms→0（`scan_us`，样本 90→18），空闲 CPU 下限 12.5→0.0 ms/s、中位 13.0→5.2 ms/s，空闲日志 62–72→27 行/30 s（整机差值含真实输入回调，扫描开销以 `scan_us` 直接计量）。PS24.0.1／Win26200 隔离实机矩阵（异常退出旧 0.4.306 服务、测后同 EXE 同数据目录重启并核验 ready／hotkeys）：新组件在 PS 前台唤出、宿主重启且 PS 已前台、其他应用前台不拦截、PS 非工作区窗口（PSFloatC）保留 Caps 且不切大写四项全部通过，回调峰值 1066 µs；旧组件重启后不动作，且即使 remote_compatibility=true 也无法被注入点击取得资格，因此旧构建的 200 ms 分支无法用合成输入对照（其真实拒绝已由旧服务自身 caps.trace 的 `activation-rejected`／command_scope=false／blocked=true 记录）。以上为注入按键，不是实体手感验收，未打开／修改任何文档，Preferences 打开后 Esc 关闭、CapsLock 已恢复。未完成：PS 22–27／Win10 1809／Win11 多版本矩阵、远程会话与受管电脑、实体按键；组件已随 0.4.307 进入运行版本（见上一条），本机实体按键仍未验收。python 侧 `input.verify` 处理路径已无请求来源，待随下一步清理。证据 `.runtime/hook-l1-2026-11-05.json`、`.runtime/hook-l1-live/result.json`、`.runtime/switch-0.4.307.json`。

- [ ] 306／PS25旧工具会话阻塞：截图确认pending-session-changed；按只读自动收尾授权完成原始证据归档／核验、旧生产者退出与单一当前PS身份核验，再用新身份读取，旧未知不重发／不改终态，写入及IME恢复记录保持。138项受限CPU、46项受影响补验及7项前端通过；故障机实体F与当前IME禁用／恢复仍待306更新后验收。

- [ ] 305／PS25工具读取恢复：修复UIA初始化失败后的定时重连和已有画布安全路径工具读取；失效清除旧点击，保留输入／未知保护。工具失败分类及脱敏证据接入后台和统一报告，51项受限CPU与7项前端专项通过。报告确认激活已放行但6次F仍被输入保护拒绝；具体宿主工具失败原因与故障机实体F恢复仍待305报告核验，不标整机修复通过。

- [x] 304后台检测：Hook健康／过期、配置确认、Caps与F判断、焦点／UIA和IME状态及有界阶段快照接入运行状态与统一报告；检测失败保留证据、正文／凭据脱敏，保护规则不变。102项受影响专项、7项前端检查、构建与冻结核验、隔离启动／退出／重启、实际后台综合报告下载通过。故障机F仍未修复。证据.runtime/diagnostics304-browser-report.json、diagnostics304-candidate-proof.json。

- [x] 303正式分发：源码／v0.4.303标签、Release ZIP与SHA256及更新清单已公开；两附件匿名完整下载哈希核对通过，发布页只保留303，旧Git标签／源码历史保留。Wiki三页已同步并核对远端80af7a4，dist仅保留303发行物；同一已测EXE e977e956继续运行，未重跑测试。故障机用户确认303／UXP0.3.91已连接，但F仍无效；实机修复未通过。证据.runtime/release303-delivery-final.json。

- [ ] 303／PS25.5.1输入故障：有限查询权限替代进程内存读取；原进程已确认退出且归档核验后，新PS会话独立建立IME，原布局未知记录与活动槽保持；F失败读取等待原核对通道。151项受限CPU输入专项、34项后续受影响检查已通过，最终构建、54个冻结模块／24项资源、隔离启动／正常退出／重启已通过；实际入口／dist已切换303，配置及原生组件匹配。PS24接口观察到禁用／离开PS恢复，自建文档／原工具及正常退出收尾通过，交互工具窗口绑定失败未发送按键；故障机用户确认303／UXP0.3.91已连接，F仍无效；实体F／A／V及输入保护仍待实测，不标为修复通过。共享JSON／任务历史重试仅限未成功的本地原子替换，不重发宿主操作。见docs/INPUT-LANGUAGE-ASSESSMENT.md。

- [ ] 302正式发布：从已提交源码独立构建，核对与已验收版相同的输入／快捷键实现；完成冻结包、实际入口、源码标签、Release附件与更新清单下载核验后，仅保留最新版Release。未提交的文字工具改动继续保留，不混入正式包。
- [x] 302连续快捷键与文字退出首键（本机用户验收）：301用户实测前几次仍有候选，再按正常，保持未通过。302对已核验且未过期的非输入首键去除“等待普通字母全部松开”和旧明确拒绝冷却门槛；保留两次工具读取、真实原生上下文／焦点／输入／修订号及未知保护，不提高核验超时。两个生产控制器新用例、196项低CPU、冻结构建和隔离本地启动／退出／重启通过；实际302／UXP91和组件匹配，PS会话／原目标／配置／历史未知保持，本次未重启PS。2026-11-04用户确认退出文字后立即快速按A／V／F“第一轮和连续按键都正常”；该序列本机验收通过，未公开Release，见docs/INPUT-LANGUAGE-ASSESSMENT.md和.runtime/runtime302-verified.json。
- [ ] 302后续验证：长时间使用、冷启动／闲置后首键、其他PS／输入法／原故障硬件；画布文字F／中文、重命名及数值输入保护仍需各自实体确认。本次首轮及连续快捷键通过不代表上述范围通过。
- [ ] 自动版本检查期间的正常退出：302隔离启停碰到在线检查约19.8秒，退出等待只读工作完成，15秒门禁失败保留；隔离配置关闭自动检查后同一EXE通过，本轮未修改生产更新功能或用户设置。后续修复取消／收尾，不能作为首键修复或长期验证已完成。
- [ ] 301历史未通过：已完成F与IME独立判定、单次首键核验及旧读取／迟到回复隔离，194项低CPU与原生两配置、构建通过，用户实测首轮候选仍出现；不能把门禁通过当作输入稳定。
- [ ] 298—300历史未通过：298已拆开F与IME控制，用户仍反馈文字退出立即A出现候选；299／UXP91加入实际工具编辑事件；300修复遗漏真实排队输入，188项低CPU、排队点击／T／Enter的低CPU／正常专项与构建通过，用户实测退出后仍失效。原记录保留，不用夹具通过替代实机验收。
- [ ] 296／297历史未通过：297用户实测快速F偶发失效，296关闭查找后A出现候选。原接口／夹具通过记录保留在docs/INPUT-LANGUAGE-ASSESSMENT.md；298—300不追溯改写这些失败，不声明提速。

- [x] 295代码／专项／构建／实际入口：F共用画布输入证据，按下时核对新鲜窗口／焦点及编辑意图，迟到查找请求再次保护；Ctrl+Enter／Esc按下和松开重新观察，慢模态回复在有界窗口内重读，保留写前第二次读取及所有输入／未知保护。修正明确拒绝后磁盘pending残留。受限CPU175项、正常122项、原生与冻结启动／重启通过；实际295／UXP90、dist／运行组件与加载的原293输入DLL匹配。PS24.0.1两配置各一次查找输入恢复／关闭重新保护、中文布局和健康闲置读取通过，临时文档／工具／CPU恢复，原稿／配置／旧未知保持，PS未重启。294残留pending与明确拒绝凭据匹配后归档结清，不改成功、不清未知；见docs/INPUT-LANGUAGE-ASSESSMENT.md、.runtime/ime295-live及runtime295-verified.json。
- [ ] 295实体输入验收：294用户实测退出立即A仍出候选，等待一两秒后正常；295增加松键触发及慢回复重读，实体F／中文／立即A尚未确认，不把接口通过当作首键已修复。不承诺异步识别零空窗，不声明提速；其他PS／第三方输入法／原故障硬件及长期使用待验，未公开Release。

- [x] 293输入法自动恢复与原生通知：同焦点短暂未知后凭稳定新证据重新保护；未保护时重读模态状态，持续UIA失联重建客户端；原生订阅上下文及禁用值变化，延后核验并恢复漂移租约，写入前校验修订号，未知修改不重发，未知区域不默认禁用。受限CPU88项／正常54项专项、原生两配置事件／20轮订阅释放、构建与冻结启动／重启通过；最终293／UXP90在PS24.0.1两配置共12轮查找面板恢复与重新保护、订阅读回、中文布局和健康闲置读取保持通过。已分层备份并正常重启PS加载DLL，原稿内容／选层、原工具、配置、历史未知及资源恢复，重启前撤销历史不保留；实际入口／dist／组件匹配。首个备份读取夹具失败及最终COM忙碌拒绝保留，不计为产品失败或通过；实体键鼠、原故障机及长时间使用仍未验。见docs/INPUT-LANGUAGE-ASSESSMENT.md和.runtime/runtime293-verified.json；Wiki两页已同步并核对远端（f6e3a79），未公开Release。

- [x] 292输入法启动／焦点恢复：修复原生明确拒绝后已尝试标记阻止重新保护，以及焦点往返原HWND沿用旧观察的问题；先留存拒绝收据，1秒后重新读取核验，未知仍不重发。受限CPU81项／正常47项专项、构建及冻结启动／重启通过；实际292／UXP90在PS24.0.1两配置完成查找面板恢复和关闭后重新禁用，独立原生读回及中文布局保持，临时文档／原工具／CPU收尾。受限CPU正常退出服务恢复、原快捷入口重启无按键自动保护通过；本轮未重启PS，实体首键／PS重命名数值／第三方输入法和原故障硬件未验。证据见docs/INPUT-LANGUAGE-ASSESSMENT.md、.runtime/ime292-live及ime292-startup；实际入口与dist已更新，Wiki两页已同步并核对远端（ae247d0），尚未公开Release。

- [x] 291输入法启动就绪恢复：修复首次工具观察在UXP未就绪且未发布时失败后，不再自动读取的问题；新读取绑定当前PS创建时间，主动启动期间不发布，旧进程心跳拒绝，已发布未知不重发。44项专项与PS24.0.1冷启动无按键保护、受限CPU正常退出恢复／重启服务自动保护通过；中文HKL保持，实际291／UXP90与运行组件已更新，公开Release未发布。另一台故障电脑无原状态、实体首键及第三方输入法未验，不认定其唯一原因。见docs/INPUT-LANGUAGE-ASSESSMENT.md、.runtime/ime291-startup及recovery291finalcold。

- [ ] 289列表符号缺字本机交付收尾：仅符号使用经GDI字形核对且PS实际识别的SegoeUISymbol，正文原字体保持；禁止指定符号字体退成另一款已安装但缺字的字体。用户原稿副本／正常300DPI三级Myriad Pro正式通道、一次撤销重做／PSD重开通过；最终89实际入口、菜单与Wiki收尾中。87字体技术字段读回拒绝已回滚，透明导出不可辨识证据保留，未计通过。

- [x] 288单窗口普通文字列表本机交付：同一Caps文本工具编辑正文与列表；符号回退校验、PS前缀实测、同级正文列、真实悬挂缩进、原缩进持久化与移除恢复完成。专项及冻结构建通过；PS24低CPU72DPI与正常300DPI真实生成、重复应用、移除、一次撤销／重做、PSD重开分项证据保留。最终288／实际加载86及dist一致；真实菜单→PS焦点交接、生成、重开选项和一次撤销通过，原稿／配置／旧未知与CPU恢复、临时文档关闭。证据.runtime/list288-columns、list288-entry、list288-interface-final、list288-interface-normal、list288-undo-persistence及recovery288dreload。83最小新建描述符失败、历史DOM夹具失败及300DPI文本框夹具越界保留，未计通过。Wiki两页已同步并核对远端正文（1869c1a），实际EXE／dist／运行原生组件与加载86、配置和历史未知核验见.runtime/runtime288-verified.json；不把矢量对象或C++双击入口算入交付。

- [x] 287矢量文字对象承载与重新编辑：嵌入矢量PDF显示，同图层XMP保存原生文字／混合样式／列表源；Caps生成、文字源读取与独立更新，保留标签、父组和变换，一次撤销。PS24.0.1／UXP82正式接口低CPU72DPI及正常300DPI生成／更新通过，另验段落文字、保存重开、改名、同／跨文档复制和嵌入内容校验；实际287菜单→PS生成／更新通过。当前是RGB8承载组件，不是完整排版编辑器；最终运行候选与公开交付收尾中。见docs/TEXT-VECTOR-OBJECTS.md及.runtime/object287-interface、object287-entry、object287-native-final。
- [ ] 287公开交付：最终候选构建、实际入口切换与效果核对、Wiki、源码／标签推送、Release下载与清单核验后仅保留最新版。
- [x] 286公开交付：焦点专项、必要构建／启动检查及实际286／UXP81组件核对通过，PS24.0.1低CPU实际菜单→PS交接、写回和一次撤销通过；正常配置激活被Windows拒绝，写入未提交，不计通过。PS未重启，原稿／配置／历史未知保留，无活动未知。源码／标签、Release附件与更新清单匿名下载、Wiki三个改动页面及CI均已核验；仅保留286 Release和dist发行物，旧标签／历史／备份保留。证据.runtime/delivery286-final.json、runtime286-verified.json、text286-lists-live/report.json。测试按用户要求收窄，已启动的689项回归仅收集一次，不重复扩跑。
- [ ] 文字排版编辑器（2026-10-29）：读取当前画布与完整字符／段落样式，独立画布页面缩放／平移编辑换行、列表及排版，暂不纳入查找替换；PS负责准确排版，矢量显示不能丢失字体／混合样式。287已闭环嵌入矢量PDF＋可编辑源的智能对象事务；静默画布读取、字符／段落控件和PS列表续行悬挂缩进仍待实现，不以承载部分代替整个编辑器。
- [x] 矢量承载路线隔离实验：PS24.0.1低CPU／正常均验证PDF文字指令／嵌入字体、置入智能对象和8倍放大；保留PS编辑能力的PDF能重开原生文字，但置入放大呈位图模糊；不保留该私有数据的PDF放大清晰，因此必须把可编辑源与矢量显示分开保留。与原生文字非像素一致；后续287已验证双数据持久化／更新，完整字体和旧版背景读取、独立编辑器仍未完成。原稿／资源保持，自建文档收尾；首轮无效枚举证据及历史未知保留、不重发。见docs/TEXT-STYLE-PLAN.md及.runtime/vector287-proof2、vector287-proof3。

- [x] 285列表选项随文字层保存：复用原生文字层＋自有XMP命名空间，保存编号／符号选项并读取PS最新正文；原生面板恢复列表页／选项，继续编辑保留草稿，移除只清理自有属性，其他插件数据保持。688项回归、原生列表恢复与完整输入／焦点／取消／SHOWWINDOW、JS失鲜／回滚／未知及冻结启动／重启通过；实际285／UXP81，正常PS退出／分层副本备份／内容恢复、配置／组件／旧未知保持。PS24低CPU72DPI点文字／正常300DPI段落文字两组完整保存重开、改名、同／跨文档复制、原生最新正文、仅选项像素及单历史／一次撤销通过，最终冻结生产读取通过。见docs/TEXT-LIST-OBJECTS.md和.runtime/list285-interface-complete、list285-frozen-read.json、runtime285-verified.json。
- [x] 285公开交付：源码3fe7b5c／v0.4.285与Wiki5cbb1e6已推送；统一ZIP／SHA和更新清单匿名下载核验通过，GitHub与dist仅保留285，旧标签／源码／诊断保持。40页／256链接及14个远端页面、图片和README核验通过，源码／标签两项CI成功；实际285／UXP81及组件匹配、原稿恢复、配置和历史未知保持。证据.runtime/release-0.4.285-public-verification.json、manifest285-verification.json、release285-cleanup.json、dist285-cleanup.json、wiki285-public-verification.json、runtime285-verified.json。

- [x] 284修饰键残留造成Caps无法唤出：按键日志复现PS范围／输入保护均正常但残留Alt、Caps被拦却无动作；正常重启旧服务后用户确认恢复。修复版对未消费的Shift／Ctrl／Alt做实际状态同步，避免在当前修饰键事件上读取尚未更新的异步状态；保留左右键／消费配对及Win右键映射，持有修饰键不重挂。原生六侧组合专项、10,000组Caps／Tab、30轮钩子重建、683项低CPU回归、最终冻结启动／重启通过。实际284／UXP80与菜单／热键／冻结资源／配置一致，无当前pending，PS会话和旧未知保持。用户确认最终284正常，按安装时间与轮转日志核验真实PS Caps匹配／打开；两个Windows交互工具初始化失败，未发送自动键鼠，不计自动实屏通过。原始松键遗漏来源未确定，不声明性能提升；证据.runtime/caps284-baseline.json、caps284-restart.json、hotkey284-prove.log、tests284-low.log、smoke284.log、recovery284caps与runtime284-verified.json。
- [x] 284公开交付：源码17aca6d／v0.4.284、Wiki462621a已推送；统一ZIP／SHA与更新清单匿名下载核验通过，GitHub和dist仅保留284，Git标签／历史及诊断证据保留。40页／254链接、13个远端页面和图片／README核验通过，源码及标签CI成功；实际入口284及组件哈希一致，用户确认新版Caps正常。证据.runtime/release-0.4.284-public-verification.json、manifest284-verification.json、release284-cleanup.json、dist284-cleanup.json、wiki284-public-verification.json与runtime284-verified.json。

- [ ] 列表对象的双击入口：285已选择原生文字层＋XMP并闭环保存／复制／撤销。PS24只读UIA完整遍历16控件未提供图层行／缩略图／蒙版独立区域，现有面板识别不能作为精确命中；未启用猜测拦截，证据.runtime/list285-accessibility.json、layers285-hwnd.json。仍需另一条真实缩略图命中来源及新鲜目标、按键配对和原生默认动作验证。不新增PS原生LayerKind，不将UXP通知当拦截，不先打开PSB再关闭；智能对象外壳与内部实例更新仍为候选。见[文字样式方案](docs/TEXT-STYLE-PLAN.md)。

- [ ] 文字样式读取与所见即所得：按2026-10-28用户要求，读取现有PS字符／段落样式，字体、字号、自动／固定行距、缩进和混合状态进入同一目标绑定草稿；准确效果由PS原生排版，预览与确认使用同一草稿，保留取消恢复／一次撤销／未知保护。优先旧版PS24.0.1：先显示原样式，再闭环写入与画布预览；面板准确图片仅在静默能力实测可用时开放，不回退新文档／PNG，不把本地排版当PS结果。先保留文本工具入口、区分内容处理和样式排版，后续可独立但不复制执行链。尚未实施，不能标当前可用；方案与分步验收见[文字样式方案](docs/TEXT-STYLE-PLAN.md)。

- [x] 283代码／测试／构建／实际入口：列表页支持最多六级选中行／当前行缩进、Tab／Shift+Tab、多种符号／自定义单符号、数字／括号／字母／罗马数字／分级／混合编号和起始值，保留空行并替换旧标记。列表结果预览按每项正文起点悬挂对齐续行，不增加显示用换行／空格或PS查询；输出仍为普通可编辑文字，保留PS原段落设置。最终683项回归、原生长文本单步缩进撤销／精确缩进粘贴／选区／焦点／取消／SHOWWINDOW、当前主题三档字号离屏及冻结启动／重启通过。实际入口最终283／UXP80、运行组件／资源、配置、旧未知和PS会话匹配，未重启PS。283前一原生候选的相同Python／UXP代码完成PS24低CPU／正常各四类列表及正常四类282写回／拆分，12组均预览不写PS、正确目标／结果并一次撤销；最终原生补丁后实际只读文字读取通过，追加写回因Windows前台激活拒绝、未发布修改，不将此追加核验记为通过。证据.runtime/text283-lists-live、text283-lists-rest、text283-actions-normal、tests283-wrap-final-low.log、native283-low-output.log、text283-preview、smoke283-final.log、text283-final-read.json、text283-lists-final与runtime283-verified.json。
- [x] 283公开交付：源码1c9feb7／v0.4.283与Wiki2725976已推送，源码／标签两项GitHub UI检查成功；统一ZIP／SHA和Latest更新清单匿名下载核验后，GitHub与dist只保留283，旧标签／源码／诊断保持。40页／251链接及13个远端页面、图片和README核验通过；最终实际283／UXP80、菜单／热键／冻结资源、配置和旧未知匹配，无当前pending，PS会话保持。证据.runtime/release-0.4.283-public-verification.json、manifest283-verification.json、release283-cleanup.json、dist283-cleanup.json、wiki283-public-verification.json和runtime283-verified.json。
- [ ] 283未验范围与失败保留：其他PS／原故障硬件、实体键鼠与PS实屏未测；普通文字列表的自动折行续行仍按原段落设置，不承诺原生列表对齐。首次测试错误调用未公开的text.preview全局动作被拒绝，未提交写入；首批第二组菜单读取已关闭，保留已知拒绝，后续在独立菜单会话完成，不重发未知请求。原生测试起初未等待焦点请求落定，按钮／撤销夹具失败保留；首轮完整回归在旧原生候选上失败，最终原生完成后683项独立重验通过。粘贴撤销沿用上游的选区删除／插入两个步骤，整段缩进仍为一次撤销；首个粘贴撤销断言误设为单步，按已核对的上游行为独立重验，未改供应商粘贴实现。无提速声明。

- [x] 282代码／单测／构建／实际入口：文字写回与拆分分离，各页可明确写回当前草稿，已有处理结果使用明确的预览结果写回；拆分条件仅约束拆成图层，原目标／菜单／版本／忙碌和一次撤销保护保持。按行分组默认留空随草稿CR／LF／CRLF更新，手填优先、清空恢复自动，最多64组；单行原文默认逐行排列。680项低CPU回归、最终原生实际按钮激活／无效拆分禁用／待处理输入／焦点／取消／SHOWWINDOW完整矩阵、当前主题三档字号预览中布局、冻结启动／重启通过；实际282／UXP80及组件一致，未重启PS。PS24低CPU四类菜单接口：非法分组、非法正则、已有有效拆分计划时写回草稿均保留原层并写入当前草稿，独立拆分确认生成正确图层；四组均一次撤销精确恢复。原稿／临时文档／CPU／配置及旧未知保持，无活动pending。证据.runtime/tests282-low.log、native282-low-output.log、text282-actions、text282-actions-focused、text282-actions-foreground、text282-preview、smoke282-final.log与recovery282text。
- [x] 282剩余交付已并入283：正常配置四组写回／拆分与一次撤销通过，最终入口／Wiki和公开下载核验完成；282未单独建立Release，功能随283公开。此前Windows自动激活PS被拒绝，已请求用户为产品交互核验提供真实PS前台；没有放宽产品焦点门槛或重发未知任务。首次菜单交回焦点200ms超时、文档切换时冻结菜单目标失鲜均为已知拒绝、未计通过；另有一次分隔符保留的测试预期漏逗号，保留失败并修正后在新隔离文档通过。原生一次旧取色自测屏幕采样失败，换屏内位置完成最终完整矩阵，未宣称修复该采样观察；工具自动权限审核曾超时，未执行该命令，重新核对状态后重试成功。原故障电脑、其他PS与实体键鼠／PS实屏仍待验。本次调整交互语义，不声明新的性能提升。

- [x] 281代码／测试／构建／实际入口：预览从逐次启动冻结EXE改为复用独立进程，事件唤醒而不空转；保持2秒隔离超时、单计算拒绝并发、独立身份与旧草稿不可写回。计算完成删除临时文字输入／输出。禁用原因显示在结果区，按行拆分显示真实行数。677项回归、预览中正文点击／中文输入与旧计划拒绝、三档语义字号／输入／焦点／取消／生产SHOWWINDOW矩阵、当前主题三档离屏面板、冻结启动／重启通过。实际281／UXP80及组件一致，不重启PS。PS24低CPU3项／正常2项确认及一次回退通过，生产面板低CPU／正常各1组读取／正则预览不改源通过。原稿、配置、历史未知、临时文档和资源保持，无活动pending。速度与完整样本口径见docs/TEXT-PREVIEW-PERFORMANCE.md；证据.runtime/perf281-summary.json、tests281-low.log、native281-low-output.log、text281-panel-final与text281-panel-low、recovery281palette、runtime281-verified.json。
- [ ] 281未验及失败保留：原故障电脑、其他PS、实体键鼠／PS实屏、长时间闲置、原低配硬件未测。正常编辑夹具发生位置变化，其中独立夹具在产品读取前已经变化，未归因产品或计通过；正常列表首次历史计数断言受容量影响而停止，独立文档以实际一次回退验证，不改变PS偏好。面板补测首轮全局／菜单读取身份混用被正确拒绝。原生新增测试先有两次主视口／鼠标帧夹具失败；首批离屏参数遗漏hold渲染了通用菜单，原图保留、不计文本通过。所有原始报告保留，没有重发未知任务；详见技术测量页。

- [x] 280代码／测试／构建／实际入口：在模型复现相同“无法核对文字基线”，修正只接受像素单位而拒绝点单位（含0）的判断；共享点／像素换算，等价基线读回按DPI比较，最多0.0001像素换算误差，其他样式／字形／位置和失败回滚保护保持。旧执行端在发布前拒绝，需加载UXP80。676项低CPU回归及最终JS单位／双向读回／正负混合／篡改拒绝／回滚未知专项、冻结资源与启动／重启通过。实际280／UXP80及组件一致，正常退出、最新分层备份与恢复通过。PS24低CPU／正常各4组混合原位、点文字负偏移逐行、段落零偏移折行与300DPI居中正偏移，共8组像素一致、单历史及一次撤销精确恢复；原稿、临时文档、CPU、配置及旧未知保持，无新活动pending。原生菜单与热键代码／哈希沿用279，其原生矩阵证据不冒充本次重新构建。证据.runtime/baseline280-before.log、baseline280-final-node.log、tests280-low.log、text280-interface、recovery280reload与runtime280-verified.json。
- [ ] 280待验及失败记录：PS24原始基线均读回pixelsUnit，pointsUnit读回兼容与精确报错复现为模型；原截图电脑、其他版本与实体输入／PS实屏未实测，不声明全版本修好或提速。首轮JSX夹具没有正确写入混合基线（错误单位ID及派生值处理），在产品拆分发布前被种子读回断言阻止；保留原始报告，临时文档关闭、CPU恢复，无未知写入。首轮收据目录与包装器不一致使后台保持停止；修正夹具及路径后用新隔离文档完成8组，服务已恢复，不覆盖失败。证据.runtime/text280-final-interface、recover280.log、resume280-tests.log。

- [x] 279代码／测试／构建／实际入口：12项颜色标签由6列2行改为4列3行，8项仍4列2行；单格、枚举顺序、色板顶部／中心和右侧按钮锚点保持，外框／真实命中共同收窄，第三行增加21逻辑单位后字母栏与信息区同步下移。普通色板也使用实际行数。675项低CPU回归、原生完整SHOWWINDOW／hover／输入／焦点／取消矩阵及新增三行宽度／顶部锚点／间距／命中／枚举断言、冻结资源／启动／重启通过。实际279／UXP79、EXE／菜单／热键及配置一致，PS会话保持、无PS重启或文档写入；当前无活动pending，旧未知保持。证据.runtime/native279-low.log、tests279-low.log、proof279.log、smoke279.log、recovery279palette与runtime279-verified.json。
- [ ] 279视觉待验：当前主题下8／12项及小中大字号均已生成GPU和CPU离屏图，但部分图局部缺图，不能标全部视觉通过；几何／命中断言与渲染生成分别记录。CUA按list_windows返回对象绑定PS窗口10297958时，报归属不匹配但期望／实际字符串相同；未重复旧失败绑定、不发键鼠，PS实屏、实体输入、现代宿主及其他DPI仍待验。原失败图保留.runtime/palette279-preview-gpu-first，CPU图在palette279-preview，不把预览当实屏。没有速度提升声明。

- [x] 278公开交付：源码103fc21／v0.4.278及Wiki1800d52已推送；675项回归及两项GitHub UI检查通过，39页／243链接、11个远端主要页面／图片／README核验通过。统一ZIP／SHA和Latest清单匿名下载核验后，GitHub及dist仅保留278，旧标签／源码／诊断保持。实际278／UXP79和组件一致，当前无活动pending；原稿最新分层备份、正常重启恢复、基础标签16项和新增颜色写前拒绝8项均有独立证据。现代PS／原截图电脑和实体输入／实屏待验，不声明全版本已修或提速。证据.runtime/release-0.4.278-public-verification.json、manifest278-verification.json、release278-cleanup.json、dist278-cleanup.json、wiki278-public-verification.json与runtime278-verified.json。

- [x] 278代码／测试／构建／实际入口：复现菜单宿主版本25.2但DOM版本字段缺失时新增标签错误拒绝，统一使用UXP host.version，79执行端报告extended_label_colors；原生和后台发布前核对同一能力，旧插件／未知能力保持8项且无隐藏新增命中。675项低CPU回归、现代四色模型、原生完整矩阵与冻结启动／重启通过；实际278／UXP79、EXE／菜单／热键／配置一致。正常退出、最新分层原稿备份和生产COM启动恢复通过；PS24低CPU／正常各8个基础标签写入与一次撤销、各4个新增标签写前拒绝通过，源文档／主题／资源及旧未知保持，无新活动pending。证据.runtime/labels278-before.log、labels278-after.log、tests278-low.log、native278-low.log、labels278-interface、recovery278reload与runtime278-verified.json。
- [ ] 278待验及失败记录：现代PS25.2+新增四色、原截图电脑、实体输入／PS实屏未实测，不能把模型版本字段复现直接认定为远端已验；无性能提升声明。首轮生产界面通道在一条成功后因焦点切离于发布前拒绝，未重发原请求，保留报告，原稿／临时文档／资源已收尾；后续独立隔离接口16项通过。恢复脚本终端信息遗留UXP78字样，实际心跳／安装／恢复收据和最终资源核验均为79，不以旧日志文字替代版本证据。证据.runtime/labels278-live与recover278.log。

- [x] 277公开交付：源码46f6c1e／v0.4.277及Wiki9185752已推送，674项回归与两项GitHub UI检查通过；39页／241链接及11个远端主要页面／图片／README已核验。统一ZIP／SHA与Latest清单匿名下载后，GitHub及dist仅保留277，旧标签／源码／诊断保持。实际入口277／UXP78和新菜单组件一致，配置及旧未知保持；6组PS列表接口验收通过，实体输入／PS实屏与其他版本待验。IME未知正常退出收尾及最新分层恢复独立保留，不记原请求成功。证据.runtime/release-0.4.277-public-verification.json、manifest277-verification.json、release277-cleanup.json、dist277-cleanup.json、wiki277-public-verification.json与runtime277-verified.json。

- [x] 277代码／测试／构建／实际入口：全部文本页沿用180ms静置自动预览，去掉计算按钮；空查找／拆分条件不请求，旧草稿世代结果不可确认，计算中继续编辑后只计算最新草稿。新增列表页的项目符号／连续编号／移除标记，保留空行、缩进和源样式映射，转换幂等；是文字前缀而非PS自动编号段落。674项低CPU回归、原生完整输入／焦点／取消／SHOWWINDOW矩阵、当前主题三档字号渲染、冻结资源与启动／重启通过；实际277／UXP78及菜单／热键／配置核验一致。PS24低CPU／正常各三种列表，共6组预览不改源、写回单历史／一次历史回退精确恢复通过；原稿／临时文档／资源保持。最终验收发现新原生IME禁用未确认，按全局授权保留原始未知及退出审计，保存最新分层备份，正常退出PS确认原进程结束后释放槽位，以生产COM启动恢复原稿；未重发或记为成功，最终无活动pending。证据.runtime/tests277-low.log、native277-low.log、text277-preview、text277-api、recovery277textlists、recovery277reload、runtime277-verified.json。
- [ ] 277待验：自动预览的实体键鼠、PS实屏及其他PS版本；列表没有多级／自定义起始编号，不是PS原生段落自动列表。原生IME未确认的根因未在本次列表功能中修复，原记录仍为未知，不把正常退出收尾当作其请求成功；不声明性能提升。

- [x] 276已公开交付：源码43029bb／v0.4.276与Wikib60e06a已推送；672项回归和两项GitHub UI检查通过，39页／241链接、11个远端主要页面／图片／README核验通过。统一ZIP／SHA与Latest更新清单匿名下载核验后，GitHub及dist仅保留276发行物，旧标签、源码和诊断保留。实际276／UXP78及组件一致，当前无活动pending；最终8组PS接口像素／一次撤销检查通过，资源与自建文档已收尾，原稿最新分层备份及原源文件保留。另一台故障电脑、其他PS版本、实体输入／PS实屏仍待验，首轮本机进程退出原因未确定；不宣称全版本解决或性能提升。证据.runtime/release-0.4.276-public-verification.json、manifest276-verification.json、release276-cleanup.json、dist276-cleanup.json、wiki276-public-verification.json与runtime276-verified.json。
- [x] 276代码／测试／构建／实际入口：针对“无法读取独立文字字形”修正测量流程，副本可见且完全透明，结束前恢复／核验透明度和可见性；有效像素／点边界明确换算，失败附阶段／单位／DPI。逐行排列按真实前缀测量，不再做空格副本等宽高断言；保留位置严格核验并给出替代模式提示。旧UXP在修改发布前拒绝，需要实际加载0.3.78。672项低CPU回归、字形缺失／点单位／模式分流／透明度恢复与回滚未知专项、冻结模块／资源、启动／重启通过。实际276／UXP78及菜单／热键／配置一致；最新分层原稿已保存并恢复。PS24.0.1通过生产UxpBridge接口进行低CPU／正常各4组（点文字原位、点文字逐行及非默认透明度、段落折行、300DPI居中），8组像素与预期一致、单历史／一次撤销精确恢复；原稿在最终测试阶段保持，临时文档及CPU收尾，无新活动未知。证据.runtime/text276-hidden-before.log、text276-final-node.log、tests276-final78-low.log、text276-final-interface、recovery276reload与runtime276-verified.json。
- [ ] 276待验与失败记录：用户截图来自另一台电脑，PS版本／模式已忘记，未实测该机；缺边界与点单位分支的复现为模型，不冒充该版本PS通过。实体Caps／点击、PS实屏与其他硬件仍待验。首轮直接桌面EXE启动的本机PS进程50084在一组可信成功、像素／撤销通过后退出，原因未确认；未发强制终止命令，未将旧进程资源恢复记为成功。原请求无新pending且未重发，最新分层备份有效；改用项目生产桌面COM启动后，新会话完成最终8组，注册路径C与原路径F经samefile确认同一程序。早期仅校验新错误提示的失败、随后旧实现真实触发同类错误、未交付UXP77和首轮进程／资源证据均保留。不将模式混用问题直接认定为远端截图根因，不声明全版本修好或性能提升。证据.runtime/glyph276-first、text276-before-message-check.log、text276-before-regression.log、text276-interface、recovery276reload/first-test-process-exit.json及recover276-com-and-test.log。

- [x] 275代码／测试／构建／实际入口：查找替换静置180ms后本地计算匹配与输出，主题强调色显示完整匹配，旧草稿结果立即失效；原文编辑与只读匹配预览分开，确认后才写PS。复用Body／textPreview／主题色，UTF-8范围先校验；补齐非法命名替换的错误返回。671项低CPU回归、原生完整矩阵、三档字号／四主题6张渲染、最终冻结模块／资源、冻结计算与超时、启动／重启通过；实际275／UXP76与组件一致，无PS重启，配置／旧未知保持。实机低CPU菜单字面替换与一次撤销通过，正则写回结果核验通过但其撤销未走完；低CPU／正常各两项只读PS读取＋独立冻结计算通过，不冒充正常组菜单写入。实体输入／PS实屏、菜单焦点转交和正常CPU宿主写回仍待验。
- [x] 275已公开交付：源码050c135／v0.4.275及Wiki2508b0a已推送；671项回归与两项GitHub UI检查通过，39页／237链接、11个远端主要页面／图片／README核验通过。统一ZIP／SHA及Latest清单匿名下载后，GitHub和dist仅保留275发行物；旧标签、源码及诊断保持。实际275／UXP76及组件一致，当前无活动pending；只读预览阶段原稿保持，全程新增Nudge仍保留为来源未确认，最新分层备份及原源文件保持，焦点转交／正常宿主写入／实体输入和PS实屏待验。证据.runtime/release-0.4.275-public-verification.json、manifest275-verification.json、release275-cleanup.json、dist275-cleanup.json、wiki275-public-verification.json与runtime275-verified.json。
- [ ] 275验收边界：原生焦点转交一次SendMessageTimeout1460；测试夹具还先后出现多传匹配字段而被写前拒绝、一次完整原始状态比较失败（未留下差异，后续独立证据不变）、下一轮面板会话拒绝、对隐藏成功反馈等待及过严前台类名等待超时。均保留原始记录，不记为产品修复；无新增未知写入，临时文档关闭、CPU恢复。原稿目标与数量保持，但全程历史由2增至3，新增Nudge来源未确认；不记全程原稿历史不变、不擅自撤销，已保存最新分层副本，原源文件未改。证据.runtime/text275-live-first.json、text275-live-second.json、text275-live-third.json、text275-live-fourth.json、text275-rest、text275-api、text275-readonly、originals275-discrepancy-preserved.json、runtime275-verified.json与tests275-final-low.log。没有可比提速基线，不声明速度提升；实际服务预览与独立冷启动计算样本分开保存。

- [x] 274代码／自动检查／构建：取消新建文档和PNG导出回退，仅保留直接像素读取。无直接接口的宿主（本机PS24.0.1）、组和单独调整层不显示预览按钮，也不留隐藏命中；F搜索／筛选／定位保留。后台与UXP拒绝旧layer.thumbnail，旧心跳不能启用回退。669项低CPU回归、原生矩阵、最终冻结包与嵌入源码核验通过；已移除未交付PNG实现及解码依赖，失败实现保留在忽略提交的诊断目录。无性能提升声明。
- [x] 274实际入口／PS接口验收：实际EXE、菜单／热键与UXP0.3.76已核验一致；PS24.0.1低CPU／正常搜索、文字筛选、可见内容与窗口范围、无新文档及不支持预览写前拒绝通过。PNG实验bb461b0b…按用户仅此条授权完整归档、最新分层备份及原进程正常退出后释放活动槽位；原记录仍未知，未重发／记成功。后续原生IME未确认按全局授权同样归档并正常退出，原稿在后台启动前恢复，当前无活动pending。首次直接启动未就绪已退出，桌面启动恢复成功；首轮显示夹具未等首帧的失败记录保留。证据.runtime/runtime274-verified.json、search274-live.json、recovery274authorized、recovery274ime及quick-export274-unknown。实体输入／PS实屏与现代PS直接接口实机仍待验。
- [x] 274已公开交付：源码2490f50及v0.4.274标签、Wiki11091cd已推送；669项回归、原生矩阵、PS24低CPU／正常负向预览检查及两项GitHub UI检查通过。统一ZIP／SHA和Latest更新清单匿名下载核验后，GitHub与dist仅保留274发行物，旧标签、源码、配置及诊断保持；39页／236链接、11个远端主要页面及图片／README核验通过。实际入口274／UXP76、嵌入组件及原稿恢复一致，当前无活动pending；现代PS实机、实体输入／PS实屏仍待验。证据.runtime/release-0.4.274-public-verification.json、manifest274-verification.json、release274-cleanup.json、dist274-cleanup.json、wiki274-public-verification.json及runtime274-verified.json。
- [x] 274实验事实：PS24.0.1 COM单像素层静默导出成功，命令返回112.265ms（不是完整预览耗时）；验证器初缺PIL，未重发原导出，读取现有PNG后关闭自建文档并保持原稿。显式图层ID实验实际导出当前蓝层而非指定红层，故不能据此承诺正确目标。UXP内首次原生导出未确认PNG完成，保留未知；没有将其标为可用或继续试验。证据.runtime/quick-export274-probe、quick-export274-target-id.log、thumbnail274-base.json、quick-export274-unknown/bb461b0b5dec5f001fefad8a2b98f580、tests274-disabled-final-low.log、native274-disabled-selftest.log、proof274-disabled.log及smoke274-disabled.log。

- [x] 273代码／测试／构建／实际入口：复现Desktop.event漏转发menu.tools.preview.result，真实272请求已成功生成128×72图片且临时文档关闭，故障在回传链路；补齐白名单并保留完整会话／请求／像素，匹配结果且实际空闲后清理缩略图遗留busyFeedback，未知仍忙不清。668项低CPU回归、真实JSON管道专项、原生纹理／旧会话／未知保护矩阵及冻结独立复测通过；首轮冻结重复启动超过15秒的失败日志保留，未定因。实际273／UXP75与组件一致，无PS重启，原稿／配置／旧未知保持。证据.runtime/thumbnail273-before-regression.log、thumbnail273-transport-regression.log、thumbnail273-user-evidence、runtime273-verified.json、tests273-final-low.log、native273-low-output.log、smoke273-final.log及smoke273-recheck.log。
- [ ] 273实际预览点击／PS实屏仍待验：实际查找已打开，但Computer Use窗口清单未提供原生工具窗口，未发送替代输入，也未用纹理自测冒充实屏；旧PS临时副本可能短暂切换画布的现有限制保留。
- [x] 273已公开交付：源码e5e498a／v0.4.273标签、Wiki578a8c4已推送；668项低CPU回归、原生矩阵、JSON通道专项、冻结独立复测及两项GitHub UI检查通过。39页／236链接、11个远端主要页面／图片通过；统一ZIP／SHA和Latest清单匿名下载核验后，GitHub与dist仅保留273发行物，旧标签／源码／配置／诊断保留。实际入口273／UXP75组件一致，无当前pending；原稿／选择／历史保持。实屏与实际点击预览仍待验，旧PS副本闪切不承诺消除。证据.runtime/release-0.4.273-public-verification.json、manifest273-verification.json、release273-cleanup.json、dist273-cleanup.json、wiki273-public-verification.json、runtime273-verified.json和originals273-verified.json。

- [x] 272代码／测试／构建／实际入口：按用户明确范围，仅F搜索输入框外圈使用中性焦点边框；筛选恢复原蓝色选中态，移除271额外筛选样式及下划线。667项低CPU回归、原生独立复测、冻结启动／重启通过；当前主题三档字号仅核对输入边框与筛选样式，中／大字号离屏图标缺失保留既有待验，不声明整张渲染全部正确。实际272／UXP75与组件一致，PS24低CPU只读搜索／筛选、窗口内容／范围与历史通过；正常组在all读取后，text因面板关闭／重开被保护拒绝，未计通过。首轮原生自测退出1但脚本未保存stdout，独立复测前补齐输出保存，原失败记录保留；不伪称定因。原稿／配置／CPU恢复；实体输入／PS实屏待验。证据.runtime/runtime272-verified.json、search272-live.json、originals272-verified.json、native272-selftest.log、native272-selftest-diagnostic.log、tests272-final-low.log及search272-preview。
- [x] 272已公开交付：源码23879a5／v0.4.272标签、Wiki4e91f34已推送；667项回归、两项GitHub UI检查及39页／236链接、11个主要远端页面通过。统一ZIP／SHA和Latest清单匿名下载核验通过，之后GitHub与dist只保留272发行物，旧标签、源码、配置、诊断保留。实际272／UXP75及组件一致，原稿内容／选择／历史保持，无当前pending；普通CPU后半段、PS实屏与实体输入仍待验。证据.runtime/release-0.4.272-public-verification.json、manifest272-verification.json、release272-cleanup.json、dist272-cleanup.json、wiki272-public-verification.json、runtime272-verified.json和originals272-verified.json。

- [x] 271代码／测试／构建／实际入口：F查找的输入焦点圈和类型选中底色改用共享中性色，当前筛选细下划线，保留蓝色hover与键盘导航。667项低CPU回归、原生显示／焦点／输入／取消矩阵、四主题三档字号6张渲染及冻结启动／重启通过；实际271／UXP75、运行菜单／热键／嵌入资源一致。PS24低CPU／正常只读查找各一组，原文档／历史／资源保持，未新建文档或重启PS。实体输入／PS实屏仍待验；首轮渲染错误入口未计入，首轮验收记录落盘夹具错误及一次UXP短暂重连的启动拒绝保留，独立重验通过。证据.runtime/runtime271-verified.json、search271-live.json、search271-final-preview、tests271-final-low.log和native271-low-output.log。
- [x] 271已公开交付：源码b4eadb3及v0.4.271标签、Wiki ce2c1dc已推送；667项回归及两项GitHub UI检查通过，39页／236链接与11个远端主要页面核验通过。统一ZIP／SHA和Latest更新清单匿名下载通过，随后GitHub与dist只保留271发行物；旧标签、源码、配置和诊断保留。实际入口271／UXP75、运行组件、原稿内容／历史与无当前pending核验通过。证据.runtime/release-0.4.271-public-verification.json、manifest271-verification.json、release271-cleanup.json、dist271-cleanup.json、wiki271-public-verification.json、runtime271-verified.json和originals271-verified.json。

- [x] 270补充颜色模式覆盖核验：原269／UXP75共用浮点RGB修复无需新增代码；实际270／PS24.0.1低CPU／正常各RGB对照、CMYK／Lab／灰度转RGB，8／16位共16组，读取／原生取样校准／不改色原始描述保持／改色一次撤销／组结构通过，原稿、配置及资源保持，实际EXE仍与已发布270一致。首轮取样器历史及Lab无ColorProfileName夹具失败保留。证据.runtime/colors270-mode-coverage-verified.json、colors271-converted-baseline.json与colors271-converted-rest.json；文件名271仅为实验编号，不代表发布271。
- [ ] 原CMYK／Lab／灰度文档直接使用自定义颜色面板：当前仍限RGB8／16位；需保持原模式、ICC及色域往返校验后再开放，不将应用工作空间转换冒充嵌入ICC。自定义ICC、32位／HDR及位图／索引／多通道不在本次16组通过范围。

- [x] 270已公开交付：源码1f7e906与v0.4.270标签已推送；统一ZIP／SHA和Latest清单匿名下载校验通过，随后GitHub与dist仅保留270发行物，旧标签／源码／配置／诊断保持。Wiki b8ca31d已同步，39页／236链接、11个主要远端页面、图片和README通过；源码／标签GitHub UI contract两次成功。本机实际270／UXP75及PS已加载DLL核验一致，无当前pending；低CPU PS接口循环通过，普通组后半段与实体中文首键继续保留待验。证据.runtime/release-0.4.270-public-verification.json、manifest270-verification.json、release270-cleanup.json、dist270-cleanup.json、wiki270-public-verification.json和runtime270-verified.json。

- [x] 270代码／测试／构建／实际入口：手动重开中文不再持续停用非输入保护；PS布局变化重新读取工具，当前TSF上下文／实际禁用值与IMM关联均核验，暂时无TSF焦点时保留原值保护，真正新上下文不沿用旧指针。666项低CPU完整回归、27项输入专项、原生菜单矩阵、原生IMM／TSF低CPU与正常夹具及冻结启动／重启通过。实际270／UXP75、PS内加载DLL与最终构建一致；PS24.0.1低CPU35→21→35、查找输入21及HKL不变通过，原稿／配置／资源保持。共享应答上限200→1000ms，不增加固定等待；首条未确认原生IME请求按全局授权完整归档、最新分层备份及原进程正常退出后才释放槽位，仍为未知、不重发／不记成功。证据.runtime/runtime270-verified.json、input270-native-tsf-low.log、input270-native-tsf-normal.log、eng270-tsf-foreground-refused.json、recovery270ime与tests270-final-low.log。
- [ ] 270普通CPU的PS重新保护后半段、实体切中文后A／拼音候选／快速输入仍待验：原生夹具正常配置通过，但PS普通组在原生恢复21后焦点切离而暂停，未计正向通过；Computer Use两次相同窗口归属错误，没有发送按键。后续短批次继续按PS真实前台条件，不绕过产品保护。切换后的极快首键仍有异步竞争边界；无实体延迟基线，不声明性能提升。升级270需保存文档并正常重启PS，不能仅重启后台或只核对UXP75。

- [x] 269已公开交付：源码7d515ba与v0.4.269标签已推送；统一ZIP／SHA和Latest清单匿名下载校验通过，核验后GitHub与dist只保留269发行物，旧标签／源码／配置／诊断保持。Wiki d2344c0已同步，39页／236链接及10个主要远端页面、图片和README通过，源码／标签GitHub UI contract两次成功。实际269／UXP75最终空闲、组件／配置／旧未知及两份原稿内容核验通过。证据.runtime/release-0.4.269-public-verification.json、manifest269-verification.json、release269-cleanup.json、dist269-cleanup.json、wiki269-public-verification.json和originals269-verified.json。

- [x] 269代码／测试／构建／实际入口：复现CMYK转RGB后纯色层保留浮点RGB而缺少入口，按原生取样校准显示，未改色保留原始描述；组缩略图隔离合成后读边界，现代接口明确不支持才回退；新增标签边界细分25.2，宿主色表有限重读／小数／同进程多窗口与来源标记。实际269／UXP75组件一致，664项最终低CPU回归、原生完整矩阵、冻结启动／重启通过。PS24.0.1低CPU／正常完成转换纯色编辑与一次撤销、14张图片、10次缓存零宿主请求及16次旧标签写入／一次撤销，原稿／配置／资源保持。证据.runtime/runtime269-verified.json、fill269-api.json、fill269-float-calibration.json、thumbnail269-base.json、thumbnail269-extra.json、originals269-verified.json和tests269-final-low.log；初版不支持浮点的复现、夹具Make／转换命令失败、20步历史容量断言失败均保留，修正夹具后独立新事务复验，不重发未知。
- [ ] 269的PS25原故障／新增四色／现代Imaging实机、原生色块逐像素对照、实体点击及实屏仍待验；没有PS25环境，不把协议模拟或PS24接口检查当作通过。原生组PNG导出未修改，无性能提升声明；公开交付见独立核验记录。


- [x] 268已公开交付：源码c93570a与v0.4.268标签已推送；统一ZIP／SHA和Latest清单匿名下载校验通过，匿名API403改用公开Latest跳转核验，未绕过下载检查。核验后GitHub与dist仅保留268发行物，旧标签／源码／配置／诊断保持。Wiki6a1685d已同步，39页／236链接及9个主要远端页面、图片和README通过，源码／标签GitHub UI contract两次成功。证据.runtime/release-0.4.268-public-verification.json、manifest268-verification.json、release268-cleanup.json、dist268-cleanup.json与wiki268-public-verification.json。

- [x] 268代码／测试／构建／实际入口：搜索右侧独立点击缩略图，旧PS隔离副本文档生成最多128×128白底图，现代路径保留；搜索内16项缓存、目标／源历史／选择／文档集合校验与持久未知保护。661项低CPU回归、原生完整矩阵、最终冻结启动重启及实际268／UXP74组件一致。PS24.0.1低CPU／正常共10项像素／文字／组／隐藏／跨文档图片、10次零宿主请求缓存复用通过，原稿／配置／资源保持。证据.runtime/runtime268-verified.json、thumbnail268-base.json、thumbnail268-extra.json、tests268-low.log及native268-low-output.log。
- [ ] 268实体点击、真实PS实屏、现代直接接口和其他硬件仍待验；两次菜单保护拒绝未计正向通过；离屏中／大字号图标不稳定与267同字库空图对照保留。旧路径RGB／8位、6400万像素及512层组限制保留；公开交付见独立核验记录。


- [x] 267已公开交付：源码6672f87与v0.4.267标签已推送；统一ZIP／SHA和Latest清单匿名下载校验通过，GitHub与dist仅保留267发行物，旧标签／源码／配置／诊断保持。Wiki ca8d482已同步，39页／235链接及9个主要远端页面、图片和README核验通过；源码／标签GitHub UI contract两次成功。证据.runtime/release-0.4.267-public-verification.json、manifest267-verification.json、release267-cleanup.json、dist267-cleanup.json与wiki267-public-verification.json。

- [x] 267标签主题代码／测试／构建／实际入口：按实际PS的UIColors色表提供四主题正常／禁用颜色，清除斜线随主题；按会话缓存并安全回退。UXP73使用本地CSS媒体状态补齐本机PS24缺失的主题通知，不新增PS偏好查询或定时器。658项低CPU完整回归、原生矩阵、8份四主题预览、最终冻结启动／重启及资源一致性通过。实际267／UXP73低CPU／正常各8次标签写入与一次撤销、各4次主题切换通过；原稿／主题／资源／配置和旧未知保持。原生IME未知按新全局授权归档，原PS正常退出后才释放活动槽位，不重发、不记成功。失败版72、版本报告失配与两次独立启动证据保留。证据.runtime/runtime267-verified.json、labels267-live/report.json、label267-preview、recovery267css及tests267-low.log。
- [ ] 267实屏／实体点击及现代PS新增四色仍待验；附加F定位因真实焦点切离而保护拒绝，未当作正向通过。公开发布与Wiki推送见下方独立核验。

- [x] 266已公开交付：源码10c4c38、v0.4.266标签、统一ZIP／SHA及Latest清单已推送并匿名下载核验；GitHub和dist仅保留266发行物，旧标签／历史／配置／诊断保留。Wiki82bde89同步，39页／234链接和9个主要远端页面通过；源码／标签两次GitHub UI contract成功。证据.runtime/release-0.4.266-public-verification.json、manifest266-verification.json、release266-cleanup.json、dist266-cleanup.json和wiki266-public-verification.json。

- [x] 266编辑区软换行代码／测试／构建／实际入口：共享multiline启用官方WordWrap，显示换行不写入原文；保留中文编辑、UTF-8光标、选区、复制粘贴和撤销。固定升级官方ImGui1.92.7，完整来源与SHA保留；字体校准／隐藏预热／平台回调及跨CPU／GPU纹理生命周期已适配。650项低CPU完整回归、优化原生矩阵、三档字号当前主题预览、冻结启动／重启通过。实际266／UXP71在PS24.0.1低CPU／正常共12项文本事务和2组查找通过；一次打开被焦点保护拒绝，已归档并在新文档补齐，未重发未知。原稿／配置／会话／历史未知／资源保持。证据.runtime/tests266-low.log、native266-low.log、text266-preview、text266-live/report.json及foreground-refusal-report.json、search266-live.json、recovery266softwrap、runtime266-verified.json。
- [ ] 266实体键盘与PS实屏、其他DPI／硬件仍待验；WM_CHAR／原生自测、接口与渲染不代替实体输入。本轮未声明性能提升。

- [x] 265已公开：源码ed9d043、v0.4.265标签、ZIP／SHA和Latest清单已推送并匿名下载核验，仅保留265 Release和dist发行文件；旧标签／历史／配置／诊断保留。Wiki efd4ae9同步，39页／234链接及9个主要远端页面通过，源码和标签GitHub UI contract成功。证据.runtime/release-0.4.265-public-verification.json、manifest265-verification.json、release265-cleanup.json、dist265-cleanup.json和wiki265-public-verification.json。
- [x] 265文字筛选代码／测试／实际入口：F查找顶部改为共享文字分段，结果行类型图标保留。三档字号按真实宽度换行并同步外层窗口预留；24项专项、双逻辑处理器Below Normal原生完整矩阵、冻结启动／重启与实际265／UXP71组件核验通过。PS24.0.1低CPU／正常各一次中文／文字类型／空结果／定位／可见内容／PS边界、原稿历史和资源恢复通过。首轮较大字号裁切图与两处旧几何夹具失败保留，修正后复验。证据.runtime/search265-live.json、search265-preview及search265-preview-cpu、tests265-low.log、native265-low.log、recovery265textfilters和runtime265-verified.json。
- [ ] 265实体标签点击／中文候选与其他DPI／硬件仍待验。GPU离屏大字号既有图标缺失观察保留；CPU三档及标签几何通过，不替代PS实屏验收。

- [x] 264已公开交付：实际入口264／UXP71与统一ZIP／原生组件一致；源码d7610d3、v0.4.264标签、ZIP／SHA及Latest清单已推送并匿名下载核验。核验后删除261 Release及附件，dist仅保留264发行文件；旧标签、源码、配置与诊断保持。Wiki fae2918已同步，39页／234链接和9个主要远端页面通过，源码／标签两次GitHub UI contract成功。证据.runtime/runtime264-verified.json、release-0.4.264-public-verification.json、manifest264-verification.json、release264-cleanup.json、dist264-cleanup.json、wiki264-public-verification.json。实体输入及现代宿主限制继续保留待验，不以公开发布冒充全硬件验收。

- [x] 263紧凑搜索代码／测试／实际入口：同家族类型图标、24逻辑单位结果行、整行定位、小预览图标被动悬停；独立有界128px像素读取、失鲜结果丢弃和资源回收。PS24低CPU／正常中文筛选、文字类型、空结果、定位及窗口可见／边界、原稿历史保持通过，三档字号预览已核对。证据.runtime/search263-live.json及search263-final。最终纳入264，不独立公开263。
- [ ] 263搜索实体输入与现代PS像素预览：本机PS24无imaging.getPixels，明确显示不支持，不以模拟预览冒充实机；实体F／中文候选、hover和其他DPI／硬件仍待验。
- [x] 264文本工具代码／测试／构建／实际入口：四个页签、可编辑草稿、折行预览、普通／正则替换、整理及分隔符／正则拆分、两种排列；独立Job计算进程2秒有界，映射样式、一次确认和原目标新鲜校验保留。实际最终264／UXP71低CPU／正常12项PS编辑／替换／整理／两种拆分均通过零预览写入、单历史和一次回退精确恢复；两种拆分零BMP像素差。同一批次12次任务／面板关闭后无需键盘活动即重新禁用IME。证据.runtime/text264-live/report.json（R1保留before-publisher-fix.json）、text264-frozen.json和text264-preview。
- [x] 264临时禁用IME代码／测试／实际入口：PS线程内解除IMM关联及当前TSF禁用，保留中文HKL；输入／点击／离开及租约过期恢复。最终实际入口低CPU／正常各一次35→21读回及中文HKL不变，DLL与最终发行资源相同；原稿／资源保持。修复工具观察发布前文件占用留下幽灵pending、原结果只读核对及菜单／忙碌结束重新启用。最终650项低CPU完整回归、正常专项、原生矩阵／DLL真实IMM与TSF夹具及冻结启动通过，实际入口最终SHA见.runtime/recovery264foreground/installed.json，PS不为最终更新重启。证据.runtime/recovery264foreground/installed.json、tests264-low.log、native264-low.log、eng264-final-live.json。
- [x] 任意前台PS接口测试全局授权已落地：C:/Users/mianx/.codex/AGENTS.md；共享host_scope改为校验唯一PS进程及创建时间，17项范围／稳定性专项通过，不改产品快捷键／IME前台限制。
- [x] 只读观察全局授权已落地：C:/Users/mianx/.codex/AGENTS.md；已确认只读且不修改业务数据的未知观察可自动完整归档、保留未知后释放读取槽位，独立新读。ee6afd89811aa2016080c084905ae00f已按此处理，历史未知／原稿／配置保持，不扩展到未知修改或强制终止。
- [ ] 264实机剩余：普通Caps关闭的真实重新禁用、实体A／B与中文候选／快速编辑／全PS区域、现代PS／其他输入法／原低配待验。普通Caps验收被真实鼠标范围保护拒绝，早期最终ENG批次前台变化停止的记录保留；最终两配置ENG及文本任务结束重新禁用已通过，不冒充完整实体通过。没有可比实体延迟基线，不声明提速。

- [x] 262标签扩展代码／测试／实际入口：现代PS增加海沫／靛蓝／洋红／紫红，共12项含清除；PS24及旧UXP保留8项，执行端对旧PS拒绝新增枚举。预设内部ID与标签分离，8／12色板的命中、单次提交和旧预设tooltip矩阵通过。621项低CPU回归、60项正常专项、原生矩阵和冻结启动通过；实际262／UXP69资源匹配。原稿分层副本／工具／图层选择已恢复，实际PS24低CPU红色／正常灰色写入读回和一次撤销通过，原稿／CPU和历史未知保持。证据.runtime/recovery262reload、labels262-live.json、tests262-low.log和native262-low.log。
- [ ] 262现代PS四枚新增标签及实体色板点击：本机PS24.0.1只支持8项，25+的12项已跑原生和执行端模拟，真实现代PS／其他硬件待验，不宣称已实机支持验证。


- [x] 261临时IME代码／测试／构建／实际入口：生产入口保持中文HKL，PS非输入区域临时关闭IME，输入／未知／离开／暂停恢复原上下文；不再新发英文布局切换。旧布局已恢复及临时文档收尾，未知不重发。621项低CPU回归、60项正常专项、原生矩阵和冻结检查通过；实际PS24.0.1低CPU／正常各一次关闭／恢复，查找面板可见、历史不变；附着菜单焦点读回缺陷实测修复，原恢复pending只读结清。实际261／dist／组件匹配，配置及195历史未知保持。技术证据见docs/INPUT-LANGUAGE-ASSESSMENT.md。
- [x] 261已公开：源码／v0.4.261标签、统一ZIP及SHA已推送且匿名下载核验。Latest清单已匿名下载核验；仅保留261 Release和dist发行物，旧标签／历史保留；Wiki已推送，39页／231链接及6份主要远端页面核验通过。GitHub UI contract通过。证据.runtime/release-0.4.261-public-verification.json、manifest261-verification.json、release261-cleanup.json及wiki261-public-verification.json；完整实体输入仍按下一项待验。
- [ ] 261实体输入与兼容：实体A／拼音首字／候选、快速文字编辑／重命名／数值、完整PS范围、原低配／其他IME与PS尚未通过；Computer Use因浏览器URL无法可靠确认而安全停止，未继续发送桌面输入。不得将open状态和HKL读回冒充此矩阵。

- [x] 260 ENG重启边界代码／归档入口：原PS已退出但259仍持有已确认切换记录，会阻塞新PS。260以原PID创建时间／退出状态核验，先归档再释放已确认记录；任何pending、无权限或退出无法确认均保留。归档／写盘失败及损坏记录不触发旧布局恢复。真实短命Windows进程自然退出、604项完整回归／正常43项专项、原生矩阵和冻结构建通过；实际入口已更新并归档本机旧ownership，配置／历史未知保持。
- [x] 260旧路线收尾：新PS旧布局确认记录已恢复，原隔离文档关闭；失败批次保留，260不再发布，由261临时IME实现替代。完整实体输入验收仍见261待办。

- [x] 2026-10-23 Wiki文档重组：重写项目首页、快速开始、安装、主要功能、更新与故障排查；正式版／候选集中到Versions，完整索引和分组侧栏可达全部39份页面，33个旧地址保留，228个链接目标检查通过。完整历史性能记录保留，README精简并补254原始渲染示例；docs/DOCUMENTATION.md与AGENTS约束后续写法。仅文档与图片，不改变主程序或PS状态；远端推送核验见.runtime/wiki-rewrite-public-verification.json。

- [x] 256 ENG扩展候选代码／自动测试／构建：PS前台及焦点驱动，取消仅画布点击；UIA正向识别输入与非输入、UXP66官方闲置／编辑工具状态、后台检测与切换读回、仅恢复本功能更改的布局。590项低CPU回归、原生矩阵、正常29项专项、自建Windows三类控件UIA及冻结启动／重启通过。
- [x] 258修复／构建／实际入口：真实PS发现并修正pywin32列表与元组比较、UXP实际classId与文档classID差异；不一致双字段仍拒绝。593项低CPU回归、原生矩阵、最终30项专项／UXP协议、冻结启动重启及实际258／UXP68加载与资源匹配通过。原文件／配置／195未知保护保持，分层副本恢复核验通过；只读实际工具接口返回有效gradientTool。详见docs/INPUT-LANGUAGE-ASSESSMENT.md。
- [x] 259实际PS／入口：258真实切换发现UIA超时后自主往返，259增加未知恢复不自循环、新活动重验及500ms有界查询；明确输入结束的返回ENG保持。595项低CPU回归、原生矩阵、正常34项专项、冻结启动重启通过。PS24.0.1低CPU／正常各4次确认切换与恢复、闲置文字工具、原稿／历史／布局／资源和临时文档收尾通过；实际259／UXP68组件一致且低CPU静置16轮无抖动，不冒充实体A验收。证据见docs/INPUT-LANGUAGE-ASSESSMENT.md。
- [x] 259已公开：源码／v0.4.259标签、统一ZIP／SHA256及Latest更新清单已推送并匿名下载核验，核验后删除254 Release及附件，仅保留259；旧标签／源码历史和诊断保留。实际入口259／UXP68匹配，Wiki使用说明已同步。证据.runtime/release-0.4.259-public-verification.json、manifest259-verification.json、release259-cleanup.json。
- [ ] ENG完整实体矩阵：中文首字／候选、快速编辑／重命名／Tab、数值／搜索、A快捷键、跨应用及其他PS／原低配继续待验；已通过的接口切换和静置不替代这些项目。

- [x] 255代码／自动测试／构建／实际入口：统一原生Body／Control／Secondary及预设专用角色；后台语义字号／行高／字重，取消窄窗口缩字；AGENTS／UI规范及三条构建门禁、GitHub工作流落地。572项回归、45项前端、低CPU原生矩阵、三档字号／当前主题及冻结启动／重启通过；实际255／UXP65、dist／原生／前端哈希一致。两项未来视图违规注入均被契约拒绝。
- [ ] 255文字规范PS实屏补验：256／257实际服务的查找低CPU／正常均通过中文筛选、空结果、边界、实屏及关闭不改历史；拆分分别因快照／前台保护拒绝，尚未完成。原稿和资源保持、临时文档已收尾；三档完整PS面板、浏览器桌面／窄屏视觉、实体拼音／键鼠及其他DPI／PS／硬件待验。证据.runtime/panel256-live、panel257-live。
- [x] 255文字规范随259合并公开：统一包与实际入口已核验，完整PS三档／实体输入边界仍按上项保持待验。GitHub UI contract已在提交e5a07cc通过：[远端检查](https://github.com/mianxiu/151500px-menu/actions/runs/39902662542)。后续push／PR自动运行；未更改分支保护设置。

证据：.runtime/tests255-delivery.log、frontend255-tests.log、native255-low.log、typography255/previews.json／enforcement-negative.json、smoke255.log、tray255/report.json、runtime255-verified.json。完整规范docs/UI-TYPOGRAPHY.md。
- [x] 254公开交付：源码／标签、统一ZIP／SHA256／Latest清单已推送并匿名下载核验，仅保留254 Release；旧标签／源码及诊断保留，未额外制作回退包。证据.runtime/release-0.4.254-public-verification.json、manifest254-verification.json、release254-cleanup.json。

- [x] 254“单层智能对象”按钮代码／构建／实际入口：与逐层按钮组成分段组，沿用F5原命令；570项Python回归、低CPU原生完整矩阵、当前浅灰大字号CPU／GPU预览、冻结EXE启动／重启及实际入口／dist／菜单／热键资源一致通过。新增检查包含点击／长按松键只提交一次、共同tooltip组、分隔线取消、跨组／锁定／输入／首页／旧UXP禁用；画布／图层／通用分流、取消和未知保护保持。tools/test-smart-object.cjs原执行端回归通过，涵盖单层／多层／整组／组内层、已有智能对象不新增历史、失鲜、失败回滚与恢复未知。 PS24.0.1实际254服务低CPU／正常各1次，通过原生新按钮的窗口客户区鼠标消息触发生产转换：两层合成一个智能对象、未选图层顺序保留、零RGB像素差、单历史及一次撤销精确恢复。此为实际PS＋模拟窗口输入，不冒充实体鼠标／Caps手感验收。临时文档已关闭，原稿／选择、配置、PS／UXP会话、CPU与195历史未知记录保持，无当前pending。
- [ ] 254实体点击／长按手感及其他PS／原低配仍待验，窗口消息不代替实体输入。最终仅移除tooltip里的快捷键提示后重建，原生低CPU矩阵、冻结启动重启及实际组件重新核验通过。该构建补充PS实测因鼠标不在PS窗口内，在按钮点击／任务提交前被保护拒绝；未重复点击，原稿、临时文档和资源收尾已确认。上述低CPU／正常写入证据来自文案调整前的同一254转换逻辑，不把拒绝项标为通过。补充记录：.runtime/smart254-button-live/1792686472337135200/report.json。

证据：.runtime/tests254-delivery.log、native254-low.log、layout254-cpu.png／layout254-gpu.png、smart254-button-live/1792686250053651000/report.json、tray254/report.json、runtime254-verified.json。

- [x] 253代码／测试／构建／运行入口：共享Caps与工具控件、平滑投影、修正移动按钮边距、中文IME定位／补字及重开旧文字；后台两页和无用组件移除，PS内入口保留。570项Python回归包含真实SetWindowPos可见内容、连续三轮中文WM_IME_CHAR输入／关闭重开（新会话空条件）、画布／图层hover分流、输入／焦点／取消、忙碌和失鲜保护；双逻辑处理器／Below Normal原生完整矩阵、当前浅灰大字号CPU／GPU预览通过。共享UI另查深色主题对比。43项Node前端测试及Vite构建通过（最终前端与251测试代码相同）；旧Caps测试夹具缺少readLayerEffects，已补真实依赖与图层ID读回，保留首次3项失败记录。EXE启动／重启、Windows版本、运行入口／dist及嵌入菜单／热键／网页资源一致。253实际服务低CPU／正常两轮：自动全结果、中文筛选为3项、空结果、PS窗口边界、关闭及不改历史／选择通过。原稿／选择／历史、用户配置、PS／UXP会话及195历史未知记录均保留，测试临时文档收尾、CPU恢复，无当前pending。
- [ ] 253实体中文候选／拼音、F／Shift+F／Caps／拖动、其他PS／原低配硬件及浏览器页面目视仍待验；不得以窗口消息／离屏预览替代。250正常配置两次实屏测试分别因焦点变化／截图前台条件失效而停止，保留记录。251低CPU中文测试通过，正常重开后保留旧查询导致空结果并有界停止；独立原生复现确认后，253按菜单会话隔离输入ID，未重发任何未知修改任务。首次独立复现夹具未启用status能力，未产生搜索请求，补齐夹具后复现251、验证253。

证据：`.runtime/tests253-delivery.log`、`frontend251.log`、`native253-low.log`、`reopen251-events.json`／`reopen253-events.json`、`panel253-live`、`tray253/report.json`、`runtime253-verified.json`。原始证据不含在发行包中。

- [x] 249／UXP65多选逐层转智能对象：新增Caps按钮、F5合并保持；同组／跨组／整组独立转换、已有SO跳过、原父组层序位置和活动多选、单历史／回滚未知保护。568 Python／35 Node／低CPU原生矩阵／冻结启动重启／实际入口一致通过；低CPU／正常8项后台PS零RGB差与精确一次撤销、2项已转换无历史；实际服务两配置各1项通过。原稿／配置／旧未知／CPU保持、自建文档收尾。docs/INDIVIDUAL-SMART-OBJECTS.md记录证据及夹具失败；非实体输入验收。
- [ ] 249实体按钮／长按手感、真实GPU实屏、原低配／其他PS、剪贴与复杂效果矩阵待验；CPU／GPU离屏底部局部缺失观察保留，不以预览代替实屏。

- [x] 248诊断消息泵修复与Win11风格投影：566回归、低CPU／正常WARP持续响应与正常关闭、当前主题预览、冻结构建及实际248／UXP64／dist／原生组件一致；PS会话、配置、旧未知保持。6组有效投影开关对照，每热态侧200样本；完整统计与失败见docs/MENU-RESPONSIVENESS.md、docs/MENU-SHADOW.md。
- [ ] 248截图具体进程归因、正常WARP投影测速前自测断言、GPU离屏缺失、真实PS GPU实屏／实体键鼠及原低配仍待验；不把本次诊断修复当作所有生产未响应均解决。

- [x] 247普通Caps柔和投影：8层预设透明度轮廓，不新增PS读取／纹理／窗口／输入等待；保持真实命中。565回归、当前主题CPU预览、冻结启动重启及实际247／UXP64、dist／原生组件通过。低CPU／正常各GPU和WARP无／有影最终8组对照，每热态侧200原始样本；完整结果docs/MENU-SHADOW.md，首帧不是冷启动，250ms不是长闲置。首次CPU刷新就绪失败保留、缺失两组串行补齐，CPU完成最大49.0815ms不剔除，不声称实体首键或PS操作提速。
- [ ] 247真实PS GPU实屏、实体输入、长闲置／冷启动和原低配设备未验；GPU离屏既有缺失观察保留，CPU回退长帧根因未定。展开编辑及搜索／拆分工具面板未统一加影。


- [x] 246菜单换位代码／构建／实际入口：新建调整两排到标签颜色右边，右侧上排上移下移、下排拆分；同步命中、独立分段提示、旧接缝取消和拆分避让。565回归、双逻辑处理器／Below Normal原生矩阵、当前浅灰大字号CPU预览、冻结启动重启及实际246／UXP64与组件通过；PS未重启，配置／旧未知保持。证据.runtime/native246-low.json、tests246-delivery.log、layout246-cpu.png、tray246/report.json。
- [ ] 246实体鼠标／Caps、真实PS GPU画布视觉、其他硬件待验。GPU离屏底部仍局部缺失，未冒充实屏通过；失败文字能力夹具与编译源变更保护记录保留，最终回归通过。


- [x] 245文档级置顶／底：新增两枚按钮，保留原组内排序；单层／整组／同父组多选按原顺序移出组，背景／锁定／父锁／跨父组拒绝，独立读回父组／层序／子树／选择及单历史回滚未知保护。565 Python、34 Node、冻结启动重启、实际245／UXP64及原生组件一致；低CPU／正常真实PS各3种排序＋一次撤销精确恢复通过，自建文档／资源收尾，原分层内容及文件／配置／历史未知保持。证据.runtime/sort245-live/1792678956606495300、recovery245greload；首轮未记录选择的历史夹具失败保留，不作通过。
- [ ] 245实体新按钮点击／长按松键、边缘及其他PS／低配设备待验；层级变化可能改变组效果，见docs/LAYER-SORTING.md。


- [ ] 244补充实体输入验收：低CPU原生输入框字符消息自动筛选到单个匹配通过；两次模拟清空夹具未得到预期结果，保留panel244-typing及typing-accepted失败记录，不计为清空通过、不重复原调用。实体退格／清空、标签点击及拖动待验。首轮外部点击关闭、第二轮自动结果早于初始高度断言的夹具失败已保留，修正后低CPU／正常自动搜索两组通过。
- [x] 244单框自动搜索／类型标签、紧凑鼠标定位与空白拖动代码、构建及实际入口：244验证：564项Python回归（包含原生输入／焦点／取消、生产SetWindowPos可见内容、防抖与过期结果保护）、冻结EXE启动／重启及实际入口／菜单／热键匹配通过。PS24.0.1低CPU／正常各一次生产原生面板自动搜索，未发送回车、返回结果可见且窗口保持PS范围内，原文档／选择／历史及CPU不变。证据.runtime/panel244-live、tray244/report.json、tests244-delivery.log。实体F／Shift+F、鼠标标签／拖动、冷启动／闲置和其他PS／硬件待验；不将窗口消息或接口验收当作实体操作。公开下载仍230。
- [x] 244有限画布自动ENG：工作线程确认非文字工具／画布焦点、异步切换读回、仅恢复本功能改变的原布局、手动切换让行及持久化未知不重发；T原样通过。低CPU／正常PS合成意图各3次往返及自建窗口12次切换通过，原稿／资源保留。完整实体输入未验，见docs/INPUT-LANGUAGE-ASSESSMENT.md。


- [x] 242文本样式误判修复、构建与实际入口／UXP63：相同继承段落不重复写入，等价重复范围严格归一；混合样式、缺失字体、不同重叠及真实变化保护保持。555 Python／33 Node／冻结启动重启通过，实际EXE、dist、菜单、热键及已加载UXP匹配。PS24.0.1低CPU／正常各3次原失败文字隔离副本拆分，可编辑、单历史及单步回退精确恢复；各1次零像素差。临时文档、CPU、原稿／配置及历史未知保留，无当前pending。证据.runtime/runtime242-verified.json、text242-live/1792674500677375700；详见docs/TEXT-TOOLS.md，无可比成功基线，不声称提速。
- [x] 240文本能力按实际选中图层识别，已包含于242：画布／图层／通用菜单单选未锁文字均可打开，非文字／未知／多选隐藏，锁定禁用；原生矩阵和低CPU／正常8项真实类型读取通过。证据.runtime/text-kind240/1792671773598863300/report.json。
- [ ] 242原生面板完整写入／撤销及实体输入仍待验：旧夹具回退选到非文字；修正夹具后Windows未允许自动激活，未继续抢焦点。后台六次拆分不替代面板验收。保留set弹窗测试失败记录（回退后COM透明度设置），最终独立副本批次已移除该调用。

- [x] 239 F搜索／Caps文字拆分代码、构建与实际入口：搜索仅由单独F打开，Caps不设搜索按钮；Shift+F交PS、Caps+F保留样式；文字层菜单内读取、连续分组预览及确认拆分，复用既有执行器和一次撤销。普通／工具菜单以鼠标为锚点，取消PS中心兜底，屏幕边缘避让。554项回归、原生键盘状态机／输入焦点取消矩阵、实际SetWindowPos就绪与副屏负坐标、当前主题预览、冻结启动重启及实际239／UXP61与组件匹配通过；PS无需重启，配置及旧未知保持。
- [x] 238 PS24.0.1低CPU四类接口验收：搜索／定位、原生文本预览与关闭后旧请求拒绝、3组可编辑文字拆分＋父组顺序＋单历史／一次撤销、空搜索及重复请求拒绝通过，第二批低CPU重复核对通过。正常配置搜索定位、预览与旧请求拒绝通过；接口不代替实体输入。证据.runtime/caps238-summary.json、caps-tools238、tray238/report.json、tests238.log；原失败记录保留，自建文档已收尾、CPU恢复，无新增未知。
- [ ] 238剩余验收：正常配置的Caps面板拆分写入因前台保护暂停；实体F／Shift+F、Caps按钮和边缘手感、冷启动／闲置、原24.2及其他硬件尚未验收。公开Release仍230，不能把554回归或渲染当作全部实机通过。
- [ ] 自动ENG完整输入验收：244已实现有限画布切换、输入恢复及持久化未知保护；PS低CPU／正常6次合成意图切换恢复通过。中文首字／候选、重命名／Tab、数值及自定义输入仍待实体矩阵，不能勾选广义自动ENG全部完成。见docs/INPUT-LANGUAGE-ASSESSMENT.md。

- [x] 234更新完成就绪判断：HTTP响应外核对菜单窗口／菜单与热键进程／托盘线程及注册错误，静默启动保留桌面要求；不读PS、不强制结束失败进程。541后端／构建／最终冻结启动重启／实际234＋UXP61、组件／配置／旧未知／CPU核验通过；隔离233→234低CPU／正常各一次生产安装接口＋独立新助手更新通过，本地可信暂存非公开下载。原测试提前就绪失败保留，等待已修正。证据.runtime/update234、tray234/report.json、smoke234-final.log；非性能提升。

- [x] 233托盘恢复缺口：监听TaskbarCreated、重复通知更新已有图标，注册失败有限重试并保留错误；537后端／构建／冻结启动重启／实际233＋UXP61及原生组件核验通过。实际服务低CPU／正常各2次仅删除自身图标并发恢复通知成功，PS未重启、配置／旧未知／CPU保持。证据.runtime/tray233/report.json；[范围](docs/TRAY.md)。
- [ ] 另一台电脑进程存在但托盘无图标：用户再次点击启动入口已恢复，发生时版本／日志未记录，更新关联和根因待复现。本机注册检查不能代表另一台；工具安全环境启动失败0x80070020，实体图标、真实Explorer重启及开机仍未验。

- [x] 232样式按需字段：532后端／33组Node／构建／冻结启动重启／实际232＋UXP61及菜单热键组件匹配；72对只读样式结果等价、真实返回量减少，实际EXE低CPU／正常8项复制／粘贴／一次撤销／无样式拒绝通过，原稿／配置／旧未知和CPU保持、自建文档收尾。低CPU无样式单选P95有1→3ms退化，多数样本接近时钟精度，不宣称整体提速；完整记录见PERFORMANCE.md、Wiki和.runtime/styles232-summary.json。
- [ ] 232实体快捷键、原生样式对话框、冷启动／闲置持续、原24.2／其他PS和真实低配仍待验；公开Release仍230。

- [x] 231清除标签祖先读取：532后端／32组Node／构建／冻结启动重启／实际231＋UXP60及组件通过；108对同会话、同模态交替读取和12次清除／紧邻历史ID一次撤销，另实际EXE低CPU／正常4项多选子层／根组、继承提示、完整标签层序选择及一次撤销通过。阶段中位／P95／最大、调用及返回量完整见PERFORMANCE.md／Wiki；不是全请求或实体延迟。所有原报告／配置／旧未知和资源保持，自建文档已收尾；证据.runtime/labels231-summary.json、labels231-service及runtime231-verified.json。
- [ ] 231实体键鼠、原24.2／其他宿主、真实低内存集显及冷启动／闲置／持续性能验收；公开Release仍230。

- [x] 230已公开发布：源码／标签、统一ZIP／SHA256与Latest更新清单均已推送并匿名下载核验，ZIP内EXE与实际230入口一致；核验后删除225 Release及附件，发行页仅保留230，全部旧标签／源码历史保留，不额外制作本地回退包。证据.runtime/release-0.4.230-public-verification.json、manifest230-verification.json、release230-cleanup.json。

- [x] 230远端交付同步：实际入口、532回归和PS低CPU／正常验证已完成；源码（含75ef3a5）已推送至master 7a12ab7，Wiki已推送至a80dcb8，两个远端HEAD与本地一致。直连失败后使用既有系统代理完成，未改全局设置、未重复宿主任务；公开Release仍225。

- [x] 230自检误报修复／532后端／构建与冻结启动重启／实际230＋UXP59及原生组件核验：正常运行中匹配任务、新鲜同会话、有效pending且执行未退出时暂缓版本自检，不再把正常长任务当未知；未知／历史恢复／身份不匹配／会话改变或失鲜／记录损坏仍警告，不清busy／pending、不重发、不增加PS查询。低CPU／正常各一次16组真实拆分超过6秒自检期限，逐帧缓存对照旧判断会误报、新判断不误报，实际报告保持正常，独立内容／父组／顺序／一次撤销及自建文档／CPU收尾通过；证据.runtime/text230-limit/1792594133506527000和runtime230-verified.json。普通UI未改，沿用229的31组Node／45前端验证，不声称提速。
- [x] 229剩余300dpi自动换行段落像素检查已在230实际入口补齐：低CPU／正常各一次零像素差、单历史及精确历史状态恢复，临时文档／资源已收尾，无新未知；证据.runtime/text230-wrapped300/1792594188291013500。BMP诊断恢复不冒充实体撤销，真实一次撤销由其他批次独立验证。
- [ ] 230实体键鼠、原PS24.2及其他宿主自检与文本完整验收；公开下载仍225。

- [x] 229文本拆分已实现／529后端／31组Node／45前端／构建／冻结启动重启／实际229＋UXP59及菜单、热键组件核验：按行或连续分组、独立可编辑层、父组与顺序、本地预览取消零写入、单次确认及一次撤销通过。低CPU／正常基础4项、边界6项、替换回归8项、中文200→500上限2项；64组低CPU实际后台55.271秒、正常接口均成功并精确撤销。点文字72／300dpi、旋转及段落／自动换行72dpi已有零像素差样本；BMP诊断使用历史状态恢复，实际一次撤销由独立核心批次验证。原稿／配置／CPU／历史未知保留，自建文档收尾，无当前pending。证据.runtime/text229-live/1792592486587064000、text229-edges/1792592900680237300、text229-replace-regression/1792592878757974300、text229-unicode/1792593669705540300、text229-browser*.json、runtime229-verified.json。
- [x] 文本正则评估：自建Node隔离进程复现嵌套量词回溯阻塞500ms，事件线程定时器不能中断；保持普通字面匹配，任意正则需PS外可终止计算及有界UTF-16编辑方案后再评估启用。工具tools/assess-text-regex.py，说明docs/TEXT-TOOLS.md；这是评估完成，未启用正则产品功能，也不算UXP／PS实测。


- [x] 228文本普通查找替换代码／520后端／30组Node／43前端／构建／实际228＋UXP58及组件核验：本地预览和取消零写入，混合字体／段落映射、活动目标新鲜校验、一次历史／一次撤销、过期及目标改变拒绝。低CPU／正常共8项基础及4项段落／旋转真实接口通过，真实后台无头浏览器确认单次提交及三宽度、PS读回与撤销通过；原稿／配置／CPU／旧未知及临时文档保持，无新未知。初始两次失败已完整恢复并保留，修正旧PS目标引用，生产无COM写入回退。证据.runtime/text228-live/1792588748834136900、text228-layouts/1792588846992182800、text228-browser*.json及runtime228-verified.json。
- [ ] 228／229实体键鼠、原24.2及其他PS验收；300dpi自动换行段落已由230补测通过，公开下载仍225。原64组45秒超时已依据原请求匹配终态核对成功、撤销并收尾，原未知时间线保留；该次CPU中途恢复，不用于低CPU速度比较。Wiki229已推送并核对远端0fcaf77。

- [x] 226最小反馈读取代码／513后端／28组Node／构建／实际226＋UXP56及组件核验；真实PS24.0.1低CPU／正常共22项接口通过，无新未知，原稿／配置／历史未知／CPU恢复。54对原生读取算法诊断能力一致，六次创建及撤销通过；完整速度、计数及范围见架构审查，不能代替PS24.2生产／实体／冷启动验收。
- [ ] 226原PS24.2生产、实体Caps／鼠标、冷启动／闲置及持续退化验收；公开版225。

- [x] 225选层取消／失败恢复原多选：共用执行器覆盖四种上下单选／Shift追加，原文档／顺序／活动层及完整选择读回；目标改变不覆盖，恢复不能确认保留未知，不重发。513后端／28组Node、构建／冻结启动重启、实际225／UXP55及组件一致性通过；真实UXP低／正常各4项多选命令＋各7项通用接口回归，无新未知，原稿／CPU及全部临时文档收尾通过。
- [x] 225故障注入证据：224/225生产函数在Node运行、真实PS COM选层／读回；低／正常各3对，224共6次原选择丢失，225共6次恢复，活动层／顺序／无额外历史通过。不是实际UXP模态自然错误；旧路径无可比恢复完成时间，不声称提速。首次通用测试心跳读盘空窗的失败报告保留，后续新批次通过。证据.runtime/caps225、notifications225、runtime225-verified.json。
- [ ] 225实体键鼠、真实UXP模态取消／失败、原24.2故障机及冷启动／闲置验收；225公开ZIP／SHA／Latest清单已匿名核验，发行页仅保留225，224标签／历史保留。

- [x] 224通知注册修复／代码与实际入口：API v2事件名数组、同步／异步失败可见、读取期间失效保留；513后端／28组Node、冻结启动／重启、实际224／UXP54及EXE／菜单／热键资源一致。本机注册ready并收到真实close回调，原稿分层备份恢复、配置／旧未知和CPU恢复已核验。224源码／标签、公开ZIP／SHA／Latest清单已推送并匿名核验；发行页仅保留224，旧标签／历史保留。
- [x] 224低CPU／正常接口批次：各7项共14项，UXP生产桥选层／新建／标签／清除及COM删除后新鲜目标、能力和颜色独立读回通过，原稿／CPU恢复、全部自建临时文档已正常收尾，无新未知。原停止与测试误要求COM通知的失败报告留存；脚本不发通知时由原轮询更新，不能记为全部回调通过。证据.runtime/notifications224/1792582031757046100/report.json。
- [ ] 224实体Caps／鼠标、全部UI事件、原24.2故障机及冷启动／闲置验收；无可比通知速度基线，不宣称持续卡顿已解决。
- [ ] 旧PS约1.4秒取消等待：阶段计时定位app.refresh约1.12–1.14秒；Wait／redrawComplete候选低CPU1419.921→1435.408ms、正常1394.747→1447.692ms（各1样本），无收益已撤回。四次取消均可信恢复，CPU／原文档恢复；生产脚本未改。

- [x] 227图层搜索／安全选层已实现、构建并切换实际227／UXP57：后台独立页按打开文档文件名／图层名／类型筛选，ID绑定跨文档定位、隐藏保留、结果过期／改名拒绝及失败恢复；搜索只读且给菜单／命令让行。517后端／29组Node／42前端、冻结启动重启、实际入口／组件通过；低CPU／正常16项真实接口和两组填充／调整／改名拒绝通过，三宽度实际服务无头浏览器和英文状态通过，原稿／CPU／配置／历史未知保留。证据.runtime/layers227、layers227-extra、layers227-browser.json及runtime227-verified.json。
- [ ] 图层面板定位实体验收：227已使用PS原生单选，展开／滚动后的可见位置、实体鼠标和原PS24.2机器尚未核验；窗口工具启动器错误，不把接口或无头浏览器当成实体通过。公开下载仍225。

- [x] 223构建、实际运行入口／组件：513项后端／原生回归、28组Node、冻结EXE启动／重启及实际223／UXP53和资源匹配通过。
- [x] 223源码／标签、公开ZIP／SHA／Latest清单及精简Wiki已推送并匿名核验，发行页仅保留223，旧标签／历史保留。
- [ ] 223后续验收：实体Caps／鼠标、24.2原故障机、冷启动／闲置及旧PS约1.4秒取消等待仍待后续。用户要求本项目PS测试和临时文档收尾全部自动，不再逐次申请；原未知保护与审计保留。

- [x] 普通填充恢复未知代码／源脚本实机修复（223已构建并切换入口）：222实机确认／撤销通过，随后取消仍未知；ID153按明确授权仅弃置并保留审计，DOM候选新测ID161仍未知（后续已明确授权弃置，未知留存；服务已恢复）。行号101及实际PS引擎已复现逻辑优先级问题，已撤回DOM引用候选，改显式矢量分支；28组Node及低／正常8项普通填充／形状确认、撤销和取消接口验证通过，原文档／CPU／保护保持，源脚本验证通过，构建／实际入口／公开发布待核验。
- [x] 221收尾补测及旧临时文档收尾：222中预览代码相同，低／正常各3对确认和各1取消已知、一次撤销／恢复／原生保护及CPU／原文档恢复通过；低CPU中位退化164.801→216.620ms，正常206.739→165.292ms，整体增益未稳定。普通填充取消另行失败，不并入形状通过结论。

- [x] 222菜单结果去重代码／513回归／27组Node／构建／实际222入口及组件核验：同请求稳定文件只解析一次，写前／终态保护保持；已实现并完成低／正常测试进程的本地阶段对照，重复单次减少0.10–0.31ms，首次增加0.21–0.33ms。公开ZIP／SHA／Latest清单核验完成，仅保留222 Release；真实PS／实体及故障机待验。证据.runtime/menu222/benchmark.json；221暂停文档与旧未知仍保留。

- [x] 文本拆分、普通替换与正则评估：229已实现上述有界范围并更新实际入口；正则只完成评估，未启用任意表达式。外部机器与实体输入分列待验。

- Wiki写作规则（用户2026-10-21）：只保留精炼的当前用法和必要限制，不在各页叠加版本日志；速度对照集中一页，详细证据链接仓库。

- [x] 221收尾等待优化代码／509回归／27组Node／构建／实际221入口＋UXP53及组件核验：终态即时唤醒和发布器退出即时唤醒，不删最后一帧或核验。真实PS24.0.1低CPU交替4对，中位201.123→172.093ms，P95／最大260.904→205.886ms，8次确认／一次撤销及真实保护解除通过；一项前台切换安全取消，CPU恢复、原未知保留。
- [x] 221源码／标签、公开ZIP／SHA／Latest更新清单及精简Wiki已推送并匿名核验，仅保留221 Release，旧标签与历史保留。
- [ ] 221正常配置、冷启动／闲置、完整取消、实体Caps与24.2故障机验收；测试文档因前台切到其他应用保留，待安全范围内核验收尾。证据.runtime/closing221-summary.json、closing221/1792577729983776900、tests221.log、runtime221-verified.json。

- [x] 220普通填充点色修复代码／构建／实际220＋UXP53：严格属性比较忽略JSON字段顺序，错误已复现、修复后27组Node／21项预览回归及最终EXE核验通过，真实24.0.1普通填充应用和一次撤销已有通过样本。公开ZIP／SHA／Latest清单匿名核验完成，仅保留220 Release，旧标签和历史保留；Wiki使用及验证限制已同步。
- [ ] 220完整实机回归与普通填充取消场景：隔离取消测试出现恢复未知，脚本退出后用户明确授权丢弃该临时文档，原未知记录与弃置审计保留，未改为恢复成功。Caps关闭收尾候选无稳定收益，已撤回，后续单独测量和优化。

- [x] 219代码／505回归／27组UXP检查／构建及实际219／UXP52／组件核验：创建类七项原生策略删除会被丢弃的DOM反馈，写前／位置／一次撤销／新鲜终态不减。真实24.0.1低CPU及正常各4／100／300层，54对只读算法A/B能力一致、六次创建和一次撤销，原文档／CPU／配置／未知保持；读取阶段速度见架构审查，非整操作或实体延迟。源码／标签、公开ZIP／SHA／Latest清单匿名核验完成，仅保留219 Release，旧标签／历史保留；Wiki说明已同步。
- [ ] 219原PS24.2生产创建链路、冷启动／闲置首触发及实体输入验收；当前读取器对照不代表故障机实测，也不代表其他命令已优化。

- 后续性能优化交付规则（2026-10-20）：每批提供同场景优化前→后速度表，含版本／配置／样本数、中位数／P95／最大值及绝对／百分比变化，读取优化补充调用数与返回量；无收益／退化如实报告，未测或不可比明确标注。详见AGENTS.md及架构审查，不以规划或仅模拟宣称提速。

- [ ] 后续结构优化（用户2026-10-20）：后台准备／事件精确失效／按动作最少读取，覆盖全部菜单与直接快捷键；路线见[架构审查](docs/ARCHITECTURE-REVIEW-2026-10-20.md)。先核验现有通知注册的参数／异步结果和真实回调，再统一快照／动作字段契约，收敛写后反馈重复读取，按低CPU实测收益引入有界当前目标预取。只读需求合并、交互优先；写前校验、一次撤销、忙碌拒绝及未知不重发保留。
- [ ] 预取验收：以218为基线，统计宿主调用／返回量、有效命中／无效成本、后台阻塞与首帧／能力／执行／反馈P95及最大值；覆盖冷启动、闲置、连续、改选／切文档、全部菜单家族和4／100／300层。未实现，不能据本条声称已有“预知”或无延迟效果。


- [x] 218代码／505后端原生回归／构建及本机218／UXP51／组件核验：运行中Caps图层菜单成功后，经新鲜基本快照恢复固定普通菜单并轮换轮次，颜色按原请求随后更新；真实24.0.1低CPU2次+正常2次，原文档／CPU／配置／未知保护保持。源码／标签、公开ZIP／SHA／Latest清单及Wiki核验完成，仅保留218 Release，旧标签／历史保留。
- [ ] 218实体Caps及PS24.2／其他宿主恢复菜单实机验收；23.3／24.0／24.2／25.5状态契约模拟不代替实机。

- [x] 217自动稳定性测试：保留原所有者报告、显式同会话收尾、中文原因／原始错误详情、结束状态与锁释放同步已实现；503后端／42前端回归、构建及实际217／UXP51／组件核验通过，低CPU6步2菜单安全停止、正常8步后停止，3次完整菜单读取，原报告保持、原所有者收尾／原文档恢复及新测试启动已核验。非117步整轮通过，实体／24.2此功能及视觉待验；源码／标签、公开ZIP／SHA／Latest清单及Wiki核验完成，仅保留217 Release，旧标签／历史保留。

- 用户复测反馈（2026-10-20）：PS24.2目前测试没问题；具体运行版本和时长未附，保留持续退化场景观察，不据此勾选全机型长期稳定。

- [x] 216／UXP51根级单选按需标量读取代码、495回归／15组UXP检查、构建及本机实际入口／组件核验。750组能力对照，真实4／100／300层接口对照；大文档最多6条记录，小文档保留有界全树，组／多选安全回退。详见架构审查；原24.2硬件／实体键鼠验收未完成，正常大文档／216新54次菜单循环未运行，低CPU100／300层全批完成；4层两配置各9样本后安全停止并清理。216源码／标签、公开ZIP／SHA／Latest清单及Wiki核验完成，仅保留216 Release；旧标签／历史保留。

- [x] 215公开交付：源码／标签、ZIP／SHA及Latest清单匿名下载通过，只保留215 Release，214标签／历史保留。Wiki三页公开核验通过；实际215／UXP50，启动主题字号缓存及正常／低CPU各16秒生产观察通过，CPU／原稿／配置／旧未知保持。其他PS和实体交互待验；本项不等于原24.2持续卡顿已闭环。

- [x] 215主题／字号启动会话缓存代码／构建／实际入口：各项成功后只读取一次，失败／中断仅重试缺项，文档元数据及prefs原时间独立。494回归、11组UXP模拟、冻结启动／重启、实际215／UXP50及PS24.0.1正常／低CPU各16秒生产文件观察通过，CPU资源恢复。原稿／配置／旧未知保持；实体交互及其他PS仍待验。证据.runtime/tests215.log、uxp215-regression.log、recovery215finalreload、runtime215-verified.json。

- [x] 214公开交付：源码／标签、ZIP／SHA及Latest更新清单匿名下载核验通过，只保留214 Release，213标签／历史保持；Wiki三页已推送并公开核验。实际214／UXP49；低78步／36循环、正常58步／27循环均因前台变化安全停止，未记全轮／原24.2故障机通过。证据.runtime/recovery214、runtime214-verified.json、release-0.4.214-public-verification.json、manifest214-verification.json、wiki214-verification.json。

- [x] 214代码／构建／实际入口：UXP49将后台元数据放到交互之后，独立纯I/O心跳与真实metadata_time_ms；494回归、11组UXP模拟及冻结启动／重启通过。低CPU78步／36循环因前台变化安全停止，原请求无未知且原所有者清理／资源／原文档恢复；65秒生产文件23次独立续期不刷新元数据。正常配置58步／27循环后同样前台变化安全停止，原请求无未知且原所有者清理／原文档恢复已核验；两配置均不记全轮通过，未记故障机／实体输入验收。

- [x] 213公开交付：源码／标签、统一ZIP／SHA及Latest更新清单匿名下载通过，Wiki操作与验证边界已推送并公开核验；实际运行213／UXP48，未重启PS。实体右键及原24.2故障硬件仍待验，性能后续路线保持进行中。

- [x] 213空白区域右键关闭代码／构建／实际入口：原生窗口取消、按键配对、松Caps不补提交、取色例外及忙碌保护通过；493回归、低CPU原生完整矩阵、冻结启动／重启及运行版本／组件核验通过。UXP48及配置／原稿／旧未知保持。证据.runtime/native213-low.log、tests213.log、runtime213-verified.json。
- [ ] 213实体鼠标空白右键、取色右键及其他PS／故障硬件验收；本机消息模拟通过不替代实体验收。

- [x] 212公开交付：源码／标签、ZIP／SHA及Latest清单匿名下载通过，只保留212 Release；实际入口212／UXP48，低CPU117步／54循环及正常73步安全停止记录、原所有者清理／原文档恢复保持。未把部分正常配置或故障机计为全轮通过。后续性能批次仍未完成，UU低优先级。

- [x] 211公开交付：源码／标签／ZIP／SHA及Latest清单已推送并匿名下载核验；冻结211更新器模拟API403后真实下载及校验211通过，只保留211 Release，旧标签／历史保留。低CPU和正常配置各117步／54菜单循环通过。
- [x] 212代码／构建／实际入口：菜单文件读取、解析及元数据校验移出窗口锁，提交时检查同一请求／轮次和真实新鲜度；493回归及慢读盘重开屏障通过，冻结启动／重启、低CPU117步／54菜单循环通过。正常配置完成73步后因前台应用变化安全停止，不记为全轮通过；原请求无未知，确认所有权后仅关闭原测试文档并恢复原文档。序列化与同步原生投递尚待进一步收敛。证据.runtime/tests212.log、window-prepare212-final.log、recovery212、runtime212-verified.json。

- [x] 210公开交付：源码／标签、ZIP／SHA及Latest清单匿名下载通过，冻结更新器模拟API403后真实下载及校验通过。只保留210 Release，208标签与历史保留，无额外回退包。证据.runtime/release-0.4.210-public-verification.json、release210-cleanup.json、fallback210-live.json。
- [x] 211性能第一批代码／构建／实际入口：后台端点与图层文件解析复用，保留逐次新鲜度和写前完整发现；492回归、冻结启动／重启及低CPU117步／54菜单循环通过。三轮本机文件读取中位2.52–3.14→0.56–0.61ms，不代表PS或实体延迟。正常完整117步／54菜单循环也通过，原机仍待验。证据.runtime/performance211、recovery211、runtime211-verified.json。

- [ ] 当前执行顺序（2026-10-20用户更新）：先发布210，再连续落实性能路线；优先图层最小读取、交互调度、端点／文件冗余和窗口锁。UU远程Caps双按降为低优先级，不阻塞本轮交付。

- [x] 210代码／构建：24.2原生读取先检查应用文档数量，空首页不查询当前文档；只读描述符silent且关闭竞态失败返回。489后端及20次空首页／关闭竞态／全silent契约回归通过，冻结EXE启动／重启通过。UXP48需重载；实际入口和本机恢复证据.runtime/recovery210finalreload，测试日志.runtime/tests210.log。
- [ ] 210原24.2故障机：首页／关闭最后文档／重新新建后，确认无原生错误弹窗且图层读取可恢复。实际加载UXP48后验收，不以模拟或24.0.1证明。

- [x] 209代码／构建／实际入口：hover收起颜色面板确认后取得新鲜快照、轮换菜单会话并返回普通菜单；实时预览及兼容草稿入口均覆盖。488后端、补充草稿入口9项、双核原生完整矩阵及可见内容、冻结EXE启动／重启、三轮PS24.0.1确认／取消接口事务通过，原稿／配置／资源和旧未知保持。UXP仍47、PS未重启，无新增Release。证据.runtime/switch209.log、native209-low.log、tests209.log、tests209-draft.log、caps209-candidate.json。
- [ ] 209实体鼠标取色→重新hover→普通菜单继续操作、其他PS和故障硬件待验；接口与消息模拟不代替实体验收。

- [ ] 2026-10-20后续优化路线已规划、尚未实施：按[架构审查与路线](docs/ARCHITECTURE-REVIEW-2026-10-20.md)分批推进。第一批闭环24.2卡灰、准确错误归因及真实类型契约；调色提前关闭、UU双按各自验收。
- [ ] 读取重构：统一快照、动作所需最小字段及按需拓扑；集中少量读取适配器与已知版本覆盖，真实PS对照后推广，不在Caps路径竞速探测。
- [ ] 调度与通信：交互优先、后台只读合并、存活／读取／终态分离、会话端点绑定；窗口锁外准备及有界真实投递确认，保留未知不重发。
- [ ] 顺滑与持续使用：最后预览值确认、取消恢复及一次撤销；原生显示／真实Caps／UU分别验收；资源有界后再按证据评估长连接。各批基线、P95及故障机结果满足后才勾选，不以本规划代替实现。

- [x] 208公开交付（2026-10-20）：源码及v0.4.208标签已推送；统一ZIP、SHA256和Latest清单匿名下载通过，实际冻结更新器在模拟API403后经公开清单下载并校验208通过。实际入口／dist／原生菜单及热键、Windows文件版本和嵌入资源一致；Wiki五页已同步并公开核验。低CPU完整117步／54循环；正常配置40步／18循环后因前台变化安全停止，部分报告实际下载通过，不记为正常三轮通过。检查后只关闭确切拥有的停止测试文档并恢复原文档，原配置和历史未知证据保留。未额外制作本地回退包，Git标签及源码历史保留。原PS24.2.0故障机仍待用户验收。证据.runtime/recovery208/runtime.json、release-0.4.208-public-verification.json、manifest208-verification.json、fallback208-live.json、wiki208-verification.json。

- [x] 208正常配置及后台入口补验：正常配置对照完成40步和18次菜单循环，随后前台应用变化按保护规则停止；已有步骤通过，但未记为正常配置三轮全套通过。后台一键启动、运行中禁用及中止后实际报告下载／解析已核验，报告保留，无未知请求。证据.runtime/recovery208/normal-stopped.json、ui208-export-latest.txt；原机仍待用户复测。

- [x] 208代码／构建／实际入口：PS24.2.x批量标量读取与新鲜捕获、原生菜单循环及心跳留证已实现；UXP0.3.47实际加载，配置／原稿／旧未知保持。本机PS24.0.1：双逻辑处理器／Below Normal完成117步、54个独立菜单轮次及151份系统采样，原生接收、按钮能力、可见内容帧、关闭及原文档恢复通过；157.35秒包含测试核对及闲置，不是实体按键延迟，也不是24.2.0提速对照。486项Python、40项前端、25套脚本、原生矩阵和冻结EXE启动／重启通过。24.2原生标量适配另有126组原能力算法对照、锁定／父组／变选／损坏数据和无DOM／无空闲模态检查；本机真实批量协议读回通过。前两次观察失败分别为内容帧观测缺失和后台外部点击干扰，均留证且资源恢复，不计为通过；已知测试文档仅经原所有者核对后关闭，未清除未知或重发修改。
- [ ] 208故障机验收：用户在原Win11／i5-12400／UHD730／PS24.2.0运行新版测试并导出报告，核验实际菜单反复打开、越来越慢／全灰是否消失；25.5.1正常对照及实体Caps另列，尚未宣称原机修复。

- [ ] 2026-10-20新增调色提前关闭：用户明确确认主程序207／实际UXP0.3.46，点击展开的调色面板后尚未调整完即关闭，不是底部颜色标签成功收尾。所称“23旧版”完整版本待确认；昨天可用文件为PS24.0.0／主程序202及PS24.2.0／主程序194，不能把产品年份2023当作23.x。当前打包原生菜单双逻辑处理器／Below Normal自测通过（.runtime/native207-color-report-check.log），未复现故障机首次点色关闭；当前代码普通点色仅更新草稿／预览，而run_preview启动／执行异常会hide(restore=False)。下一步需故障发生后的207统一诊断／任务错误与菜单hide、dismiss、shape.paint、mask.preview.update事件对齐，区分误确认、焦点取消和预览失败；未修改运行代码、未标修复。

- [ ] 2026-10-20故障范围进一步收敛：用户在相同CPU／Win11环境对比，PS24.2.0卡顿而25.5.1正常（正常版本尚无对照报告）。207／UXP0.3.46报告7ac8b026在第一轮第38步多选读取失败：12层，整步22.687秒，错误为UXP未就绪；此前固定4层读取整步中位10.375秒，含测试自身目标核对，实际菜单请求慢样本完成约4.8–5.0秒，期间有短暂快速样本，并非单调退化。增层后段整步约6秒而创建本身多为42–72毫秒；269系统样本总CPU中位7.57%、可用内存最低约10GB。优先验证24.2.0非模态DOM／宿主读取兼容及心跳失鲜，不能认定Win11／集显／降频根因。错误来自有效心跳筛选，报告缺最终心跳年龄和完整菜单投递／按钮状态，仍待补齐。相关一手报告：https://forums.creativeclouddeveloper.com/t/long-time-to-access-api-data-in-photoshop-v-24/6072 。本地原报告与分析归档.local/diagnostics/ps24.2-2026-10-20；本轮未改运行代码、未标修复。

- [x] 207公开交付（2026-10-20）：源码及v0.4.207标签已推送；统一ZIP、SHA256和Latest更新清单匿名下载核验通过，冻结更新器在模拟API403后通过公开清单下载并校验207。最终实际入口207／UXP0.3.46完成116步、140系统样本及后台导出，原稿／配置／旧未知保护保留。Wiki使用说明已同步。按用户最新要求本轮不额外保留本地回退包，Git标签与源码历史保留；故障机、实体键鼠与UU远程未计通过。证据.runtime/recovery207/runtime.json、release-0.4.207-public-verification.json、manifest207-verification.json、fallback207-live.json。

- [x] 207一键稳定性测试代码／构建／实际入口：标准3轮及持续10轮，隔离文档、逐步原始耗时、系统采样、部分报告、停止后续与未知保护。本机PS24.0.1已完成双逻辑处理器／Below Normal的116步标准测试，资源恢复；正常配置也完成116步及真实后台开始／禁用／导出检查。485项Python、40项前端、24套脚本和冻结EXE启动／重启通过。剪贴板尺寸读取增加串行保护及按序号缓存，修复后160次并发状态请求通过。接口测试不等于实体键鼠或故障机验收；两台Win11／i5-12400／UHD730持续退化及UU远程Caps双按仍未确认解决。
- [ ] 207故障机验收：在两台Win11／i5-12400／UHD730及正常独显机用相同模式导出报告，对照持续退化阶段；实体Caps与UU远程问题单列，不能据低总CPU认定Win11或限频。

- [x] 206本机实现／构建／切换／接口回归：大列表逐层读取补齐有界让行，旧代码在新请求到来后仍额外读187个ID的缺陷已复现；修正后中断与378种能力对照通过。首份快照接受标记衔接固定完成窗口，仍校验新鲜度／身份；修复已完成预览进度与退出文件无限积累，160次收据回归验证保留64项及未知证据。476 Python、23套脚本、冻结EXE及PS24.0.1的80次连续操作、3轮实时预览、256层200多选取消／新读和低CPU／正常原生接收通过，资源恢复。实际206／UXP45，原稿分层副本恢复、源文件和旧未知保持。Wiki四页已同步并匿名核验，统一包与实际入口哈希一致；公开Release仍205，原故障仍未闭环。证据.runtime/recovery206、progressive206-fixed-*、preview206-*、layer206-*。

- [ ] 故障机1.25GHz线索（用户2026-10-19补充）：用户确认卡住时CPU总利用率约10%以下；单核曲线和PS进程占用未提供。i5-12400有12逻辑处理器，单线程饱和按线程数平均仅约8.3%，不能用低总占用排除单线程瓶颈，也不能据低频认定限频。继续区分宿主单线程读取与等待PS／文件I/O／菜单消息返回；不直接归因Win11或集显。两机实际205／UXP44版本问题仍未得到明确回答。

- [ ] 两台故障机图层持续退化（2026-10-19最新澄清，优先于UU远程Caps）：用户确认故障机全部Win11；新建文档后，无论图层数量不变的改色／羽化，还是持续新建／复制，均逐步变慢直至图层识别全灰；恢复涉及重启主程序或PS，单纯关菜单不能按已恢复处理。此前“故障机Win10”的归属撤回，原始系统／PS报告保留并待对应，不再用其反证Win11影响。当前205＋UXP44是否在两机复测待确认；旧202报告16项任务全部成功（中位157ms、最大281ms），缺全灰时接收状态。本机205／UXP44／PS24.0.1双CPU／Below Normal完成108项隔离接口操作及新鲜读取（16短序列＋80固定／增层序列＋12固定图层颜色／羽化单次提交），均未复现持续退化，任务历史64项上限及资源恢复已核对；不代表两机或实体菜单验收，也未标为修复。证据.runtime/recovery206/progressive-baseline205.json。

- [x] 205公开交付（2026-10-19）：源码／v0.4.205标签已推送，统一ZIP／SHA256及Latest备用更新清单匿名下载核验通过；实际冻结EXE内更新器模拟API403后经公开清单下载／校验205通过。新包验证后移除195 Release，其全部附件已备份、旧标签与源码历史保留，发行页仅205。实际入口仍205／UXP44且包哈希匹配，Wiki四页已同步发布与跨设备复测说明；i5-12400／UHD730、Win10／Win11、PS23／24原故障和Caps双击仍待实测，不标为已解决。证据.runtime/release-0.4.205-public-verification.json、manifest205-verification.json、fallback205-live.json、release205-cleanup.json。

- [ ] 未来图层读取性能优化（2026-10-19用户要求记录，尚未实现）：评估多线程／异步分工、批量读取、请求合并、预热与有界缓存，缩短菜单从唤出到图层操作可用的时间。先分别测量调度等待、PS读取、数据处理／序列化、传输及原生接收／显示，依据瓶颈选方案；核验COM／UXP及PS宿主的线程与事务限制，可并行的本地处理和通信移出菜单／状态锁，不以增加线程数作为提速保证。保留写前新鲜目标校验、迟到结果隔离、输入／焦点／取消保护、忙碌拒绝和未知结果不重发；不得并发写PS或靠删减校验、放慢刷新换取通过。验收覆盖冷启动、闲置后首触发、连续切层、大文档及背景读取重叠，记录样本数、中位数／P95／最大值，并区分读取耗时与实体按键到屏幕延迟；优先在i5-12400／UHD730的Win10／Win11、PS23／24故障机验证，再与独显正常机对照，CPU受限模拟不替代故障机实测。先闭环当前图层识别故障，再推进此项性能优化。

- [x] v0.4.205代码／构建／实际入口：合法大图案目录98958字节被旧64KB菜单边界拒收的卡灰路径已复现，接收上限调整为256KB；可选编辑数据超界时基础图层能力仍可更新。475 Python、双逻辑处理器／Below Normal的64项相关回归、完整原生矩阵及冻结EXE启动／重启通过；实际205／UXP44、Windows版本、组件及嵌入资源匹配。PS24.0.1低CPU／正常各一轮只读选层→Desktop→实际打包原生接收约99KB本地目录测试包，同一新鲜身份、零执行、显示／隐藏通过；图案目录仅本地注入，未改PS预设。一次前台变化中止不计通过，资源恢复后仅清理已知完成的临时文档。原稿、配置、PS／UXP会话与旧未知保持。缓存诊断增加菜单接收阶段／大小且无额外PS查询，Wiki四页公开访问和最终包SHA256核验通过，无新Release。证据.runtime/recovery205/runtime.json、native205-before.log、native205-low.log、layer205-wire-*/verified.json。
- [ ] i5-12400／UHD730图层一直加载／识别中：旧报告为PS24.0.0、后台202／UXP43，故障机系统归属已按用户最新说明更正为全部Win11，读取11ms／菜单96ms且有单选形状；报告缺菜单收件结果和目录大小。205修复同类可复现缺陷，但该机是否同因尚未确认、仍待205复测；原PS23场景也未闭环。Windows build26200／PS24.2旧报告单独保留，机器映射未确认。
- [ ] 【低优先级，2026-10-20用户调整】远程兼容模式每次关菜单后首次Caps无反应、第二次才开：新报告含两组已发出的hold_begin／hold_pin、注入按键且前台资格通过，仍缺首次无反应时完整投递／窗口显示证据；未标为修复，原Caps／焦点／输入保护保持。

- [x] v0.4.204代码／构建／实际入口：PS24.0.0独立预览能力标志与后台选择／发布门槛补齐，需实际UXP44；旧43在24.0.0不强行启用，23.x兼容草稿及24.1+官方API检查保持。472 Python、双逻辑处理器／Below Normal的100项预览专项、21套脚本、冻结EXE启动／重启与嵌入资源匹配通过。实际204／UXP44、PS24.0.1三轮隔离真实接口（低CPU确认、正常确认、低CPU取消）验证两帧实时新值／重绘、同一目标、一次撤销、取消恢复及资源恢复。原文档分层备份并恢复副本，源文件、配置／凭据及旧未知保持；共享登记仅本插件名称／版本／最低宿主同步。Wiki五页公开访问、最终更新包及SHA256核验通过，无新增Release。证据.runtime/recovery204/runtime.json、preview204-*/verified.json、preview204-low.log。
- [ ] v0.4.204故障机实机：Win10 19045.3803／PS24.0.0 20221013.r.59／32GB的颜色、羽化实时效果及实体鼠标／Caps画布目视待复测；本机24.0.1和版本模拟不替代。

- [x] v0.4.203代码／构建／实际入口：移除打开后台及安装／更新插件后的自动启动PS路径、开关与API；旧auto_start=true设置保留但不读取。手动启动与原请求只读核对保留。469 Python、40前端、双逻辑处理器／Below Normal的33项启动／插件管理回归及冻结EXE启动／重启通过；实际EXE、Windows版本、菜单／热键与嵌入资源匹配。已运行PS上的手动启动返回正常，PS／UXP会话、原稿、配置和旧未知保持。Wiki三页公开访问及最终离线包／校验值核验通过，无新增Release。证据.runtime/recovery203/runtime.json、startup203-low.log、smoke203.log。
- [x] v0.4.203移除自动启动的冷启动场景由204实际入口补验：安装UXP44且PS正常关闭时，真实后台启动后观察8秒及多轮状态轮询，PS保持关闭；随后手动启动请求一次，正常加载UXP44并恢复文档副本。证据.runtime/recovery204/cold-backend.json、reload-verified.json。

- [x] v0.4.202代码／构建／实际入口：配色tip每帧保留全部锚点，60毫秒换字不丢旧几何；色相与SV分流、空白无误报、同侧就近避让。低CPU完整原生矩阵及专项帧间延时回归通过，无预览写入；当前主题9份面板及6种实时预览tip通过。冻结EXE启动／重启、版本元数据、嵌入资源及实际菜单／热键哈希匹配，原稿／配置／会话与旧未知保持。执行端沿用198／199基线。Wiki三页公开访问及最终离线包／校验值核验通过，无新增Release。证据.runtime/recovery202/runtime.json、native202-low.log、ui202。
- [ ] v0.4.202实体鼠标tip稳定性、Caps／Esc到PS画布目视及其他设备待验。

- [x] v0.4.201代码／构建／实际入口：悬停原按钮收起，单次停留防振荡，拖动／输入／取色不补触发；去掉虚线、保留文字及普通实线框。低CPU原生完整矩阵、当前主题9份预览、冻结EXE启动／重启、嵌入资源及实际菜单／热键哈希通过，原稿／配置／PS和UXP会话及旧未知保持。468 Python沿用198，宿主接口沿用199。Wiki三页公开访问及最终离线包／校验值核验通过，无新增Release。证据.runtime/recovery201/runtime.json、native201-low.log、ui201。
- [ ] v0.4.201实体鼠标／Caps／Esc到PS画布目视及其他设备待验。

- [x] v0.4.200代码／构建／实际入口：展开／收起说明文字、圆角虚线、共享图文尺寸及可见／命中同步完成；低CPU原生完整矩阵、当前主题11份预览和三档字号通过；最终EXE启动／重启、嵌入资源及实际菜单／热键哈希匹配，原稿／配置／PS与UXP会话保持。执行端未改，468 Python沿用198、PS接口事务沿用199。Wiki三页公开访问及最终离线包／校验值核验通过，无新增Release。证据.runtime/recovery200/runtime.json、native200-low.log、ui200。
- [ ] v0.4.200实体键鼠／PS画布目视及其他设备待验；原生回归和预览不等同实机。

- [x] v0.4.199代码／构建／实际入口：仅点击原图标收起；取消移出确认；全局钩子按真实USER32捕获将左右取色点击交给菜单，修复屏幕取色被菜单外点击／右键关闭打断。低CPU原生完整矩阵、热键10000组及采样按下／松开路由、当前主题8份渲染、最终冻结启动／重启、嵌入资源与实际菜单／热键哈希通过。PS24.0.1低CPU／正常配置隔离接口确认／一次撤销通过；配置、原稿、旧未知及PS／UXP会话保持，无新待核对请求。468 Python沿用198执行端基线。Wiki三页公开访问及最终离线包／校验值核验通过；实体键鼠单列待验，无新增Release。证据.runtime/recovery199/runtime.json、native199-low.log、hotkey199-selftest.log、icon199-return-*。
- [ ] v0.4.199实体鼠标／Caps／Esc到PS画布目视及其他PS／真实低配待验；自动原生输入和渲染不等同实机。

- [x] v0.4.198代码／构建／实际入口：依据Adobe官方Manifest v5及项目API审计，将声明下限统一为PS23.3.0／UXP6.0，清单／安装／启动／心跳／自检一致；旧版预览范围未扩张。468回归、字体缺失保护、冻结EXE启动／重启及本机PS24.0.1真实只读检查通过；实际198及已安装清单／收据匹配，原稿、设置、脚本字节与时间戳、PS会话、共享索引和旧未知保持，无新待核对请求。原生／前端未改沿用197证据；首次EXE文件锁核验后解除，仅继续替换未重发任务。Wiki四页公开访问及最终离线包／校验值核验通过，未新增Release。证据.runtime/recovery198/runtime.json、live-read.json、adobe198-audit.json。
- [ ] v0.4.198新下限实机：PS23.3～24.0.0、UXP6.0及字体／本地草稿差异待验；文档下限、安装准入与实机支持分别记录。

- [x] v0.4.197代码／构建／实际入口：所有成功反馈约450ms后自动收起整个菜单，旧自动恢复停用，调色终态转发过滤已修复且原条件失败复现留证。461后端回归、双核原生矩阵、9类真实Desktop→原生窗口显示／隐藏串测、当前浅灰8渲染及三轮PS24.0.1接口事务通过；一次撤销／取消恢复、原稿／配置／历史未知与CPU资源保持，未重启PS／UXP43。实际入口、Windows版本及组件匹配。Wiki六页已推送并匿名核验，未新增Release。证据.runtime/recovery197/runtime.json、live-matrix.json、completion197-final/verified.json、completion197-reproduced.log。
- [ ] v0.4.197实体键鼠／画布目视、其他PS与真实办公低配待验；不以接口或原生自动测试代替。

- [x] v0.4.196代码／构建／实际入口：完整高度中性连接通道、颜色拖动结束保持和屏幕吸管已安装，Windows文件版本、dist与实际EXE、菜单及热键组件匹配。459回归、双核原生完整矩阵及新增取色／取消／过期／输入／捕获保护，当前浅灰8类渲染、PS24.0.1三轮接口事务（低CPU2、正常1）通过；原稿、设置、历史未知和资源保持，未重启PS／UXP43。证据.runtime/recovery196/runtime.json、live-matrix.json和native196-low.log。Wiki三页已推送并匿名核验，本轮未新增Release。
- [ ] v0.4.196实体验收：鼠标／Caps／Esc到画布目视、其他PS与真实办公低配硬件；Computer Use启动器0x80070020阻塞，原生消息模拟／PS接口测试不能代替实体。

- [x] v0.4.195修复及交付：描边切回填充误收起已复现并修复；双核原生完整矩阵、22次连续通道／类型点击、当前浅灰8类面板通过。实际195／UXP43入口已核验；源码／标签、独立Release和Wiki四页已推送并匿名核验，冻结更新器API403备用下载通过；194 Release附件已备份，标签／历史保留。并行196改动未混入195包，未替换其工作文件。证据.runtime/recovery195/runtime.json；实体键鼠及其他设备仍待验。

- [x] v0.4.194功能与公开交付：单图标锚点hover展开、连接间隙／两面板内保持、移出返回普通菜单；普通HSV条、统一取色器和全宽羽化／密度滑条。撤销改色后移动衔接、保留四排序按钮。459回归、双核原生矩阵、四主题32＋12布局／104tip像素、PS24.0.1三轮真实接口事务通过。实际入口、Windows文件版本及原生组件一致；Release／源码／标签和Wiki七页已推送并匿名核验，更新器API403备用通道真实下载通过；仅保留194 Release，193附件／标签／历史保留。证据.runtime/recovery194/runtime.json。
- [ ] v0.4.194实机待验：实体鼠标／Caps／Esc到画布目视、其他PS及真实办公低配硬件；自动输入与接口事务不代替这些验收。

- [x] 193代码／构建／实际入口：单选实时预览后可直接选置顶／置底／上移／下移，先确认颜色，再校验原目标移动；重复拒绝、两步各自撤销。中心／关闭提示共用1100ms环形动画，真实阶段／计数保留，工作流未结束不提前显示完成。458后端、双核原生（固定／长按8组合及GDI四主题）、32普通渲染＋128提示纹理＋16运行帧通过；PS24.0.1六轮复合流程（低CPU四移动、正常对照、换目标拒绝）及两轮原预览确认／取消通过，选层／内容／路径／顺序／撤销及资源恢复核验。原生模拟与真实接口分开，实体键鼠／提示音与其他硬件仍待验；一次前台变化安全停止未计通过。实际193／UXP43，无需重启PS；原稿／配置／历史未知记录保留。证据.runtime/recovery193/runtime.json、chain-matrix.json、live-matrix.json、tests193-final.log、native193-low.log、move193-tips/pixels.json、activity193/pixels.json。
- [x] 193公开交付：源码／标签及Wiki七页已推送并公开核验，ZIP／SHA／更新清单匿名下载与冻结更新器API403备用通道真实下载通过。只保留193 Release；192附件已本地备份、标签／源码历史保留。实际193／UXP43及新原生组件匹配，dist仅当前发行物；实体画布／键鼠与其他硬件按待验收项保留。证据.runtime/recovery193/runtime.json、release-0.4.193-public-verification.json、manifest193-verification.json、fallback193-live.json、wiki193-public.json、release193-cleanup.json。

- [x] 192分离取色实际入口：色相条与饱和度／明度方块独立，四点标记置于条外，星形强调色及命中已移除；tip就近上下避让，仅解释配色含义。实际192／UXP43与原生组件匹配；双核完整矩阵＋64悬停场景、四主题32导出＋112张提示像素核对通过，48个标记提示最远17.03逻辑单位。初次8个过远案例留证，修复后重跑。最终两轮PS24.0.1低CPU取消／正常确认一次撤销通过，PS未重启、原稿／配置／资源和旧未知记录保留；Python452／前端40沿用未改基线，实体键鼠／画布与其他硬件仍待验。证据.runtime/recovery192/runtime.json、live-matrix.json、native192-low.log、tooltip192/proximity.json、pixels.json、tooltip192-first-position-failure.json。
- [x] 192公开交付：源码／标签及Wiki五页已推送并公开核验，ZIP／SHA／更新清单匿名下载与冻结更新器API403备用通道真实下载通过。只保留192 Release；191附件已本地备份、标签／源码历史保留。实际192／UXP43及新原生组件匹配，dist仅当前发行物；实体画布／键鼠与其他硬件按待验收项保留。证据.runtime/recovery192/runtime.json、release-0.4.192-public-verification.json、manifest192-verification.json、fallback192-live.json、wiki192-public.json、release192-cleanup.json。

- [x] 191五点配色实际入口：分裂互补／三色配色四点和命中已移除，保留主色、两枚邻近色、互补色和星形强调色；强调色说明互补色相＋明暗反差。实际191／UXP43及原生组件匹配；双核完整矩阵＋80悬停场景、四主题32导出＋124张提示像素检查、PS24.0.1低CPU取消／正常确认一次撤销通过。执行端未改，452Python／40前端沿用已验基线；PS未重启，原稿／设置／资源与旧未知记录保持。实体键鼠／画布及其他办公硬件仍待对应验收。证据.runtime/recovery191/runtime.json、live-matrix.json、native191-low.log、tooltip191/pixels.json、tooltip191-batch/pixels.json。
- [x] 191公开交付：源码／标签及Wiki五页已推送并公开核验，ZIP／SHA／更新清单匿名下载与冻结更新器API403备用通道真实下载通过。只保留191 Release；190附件已本地备份、标签／源码历史保留。实际191／UXP43及新原生组件匹配，dist仅当前发行物；实体画布／键鼠与其他硬件按待验收项保留。证据.runtime/recovery191/runtime.json、release-0.4.191-public-verification.json、manifest191-verification.json、fallback191-live.json、wiki191-public.json、release191-cleanup.json。

- [x] 190提示修复代码／构建／实际入口：修复颜色／羽化实时预览开始后悬停tip消失：只读提示按当前编辑面板是否仍可用显示，不再被“其他图层操作禁用”的条件一并关闭。其他操作、目标、输入与未知结果保护保持。 原189条件已复现；双核原生完整矩阵＋144悬停场景、四主题32普通导出＋172张提示像素核对及两轮PS事务通过。实际190／UXP43，无需重启PS，原稿／配置／资源与未知记录保留。实体鼠标／画布仍待验收；未改Python／前端使用既有回归基线。证据.runtime/tooltip190-reproduced.json、tooltip190/pixels.json、tooltip190-batch/pixels.json、recovery190/runtime.json、live-matrix.json及native190-low.log。
- [x] 190公开交付：源码／标签及Wiki五页已推送并公开核验，ZIP／SHA／更新清单匿名下载与冻结更新器API403备用通道真实下载通过。只保留190 Release；188／189附件已本地备份、标签／源码历史保留。实际190／UXP43及新原生组件匹配，dist仅当前发行物；实体画布／键鼠与其他硬件按待验收项保留。证据.runtime/recovery190/runtime.json、release-0.4.190-public-verification.json、manifest190-verification.json、fallback190-live.json、wiki190-public.json、release190-cleanup.json。

- [x] 189配色标记代码／构建／实际入口：用形状识别配色关系：主色大圆、邻近色小圆、互补色菱形、分裂互补色上／下三角、三色配色方形、强调／明暗对比色星形。九个标记仍同时显示，内部显示真实颜色，选中用蓝色轮廓强调。 452回归、双核原生完整矩阵、四主题32导出及两轮PS24.0.1事务通过（低CPU取消恢复、正常确认一次撤销）；尖角命中、九点拾取／拖动、闲置缓存及输入／焦点／取消保持。实际189／UXP43，未重启PS；原稿／配置／资源与旧未知记录保留。实体键鼠／画布与其他硬件待验收。证据.runtime/recovery189/runtime.json、live-matrix.json、native189-low.log、ui189-verified.json。
- [x] 189功能随190交付：189形状标记保留，用户反馈的tip实际显示失败已由190复现并修复；190公开下载、Wiki和备用更新核验后只留190 Release，188／189附件已备份、标签和历史保留。未把189原悬停目标检查重标成显示成功。

- [x] 188代码、构建与实际入口：预览脚本退出后恢复PS原生操作；未完全结束时继续保留图层列表点击保护和小运行提示。颜色编辑改为中央方块＋外围圆角色相轨道，九种配色点同显、主色点更大。 v0.4.188／UXP0.3.43已构建并切换本机实际入口，EXE及原生组件匹配。452项Python回归、40项前端、脚本模拟、双逻辑处理器／Below Normal原生完整矩阵及四主题32份原生导出通过。PS24.0.1六轮真实事务（低CPU四轮、正常两轮）核对确认一次撤销、取消恢复、选层／内容／路径／顺序及资源还原；包括128层确认及两次调用端暂失确认的故障注入。故障注入不改原收据、不重发；宿主退出后原生配置确认解除保护，插件新写入继续被拒绝，再仅读取原收据完成核对。原稿已备份并正常重载，历史未知记录及配置保持。 原生模拟输入和PS接口测试分别记录，未代替实体鼠标／Caps／Esc到画布目视与提示音验收；现代PS、真实办公i5／集显／低内存及Win10仍待对应设备验证。双核限制仅模拟CPU并发。128层收尾本轮约12.4秒，仍有等待，本次不宣称其首次读取已提速。 证据.runtime/recovery188/runtime.json、live-matrix.json、tests188.log、native188-low.log、ui188-verified.json。
- [ ] 188实体画布／键鼠与跨硬件验收：核对新方块拖动、九点联动、真实Esc、收尾提示及PS原生操作；现代PS／真实办公低配单列，不能用模拟替代。
- [x] 188公开交付：源码／标签及Wiki八页已推送并公开核验，ZIP／SHA／更新清单匿名下载与冻结更新器API403备用通道真实下载通过。只保留188 Release；187附件已本地备份、标签／源码历史保留。实际188／UXP43及新原生组件匹配，dist仅当前发行物；实体画布／键鼠与其他硬件按待验收项保留。证据.runtime/recovery188/runtime.json、release-0.4.188-public-verification.json、manifest188-verification.json、fallback188-live.json、wiki188-public.json、release188-cleanup.json。

- [x] 187取消误报修复：恢复历史后恢复本次原选层，保留写前与写后校验；生产函数复现及双核／正常配置故意错开历史选层的取消实测通过。静止圆点不重复发预览，闲置不写PS。原186未知收据、待核对文件及完整历史未改；备份1.psd后按两次明确授权结束本项目后台组件并切换隔离187，PS与UXP未重启。一次前台变化安全暂停未计通过，其临时文档在已确认取消后清理。证据.runtime/unknown186-readonly/read-only-comparison.json、recovery187/preserved186-journals.json、approval.json及live-matrix.json。

- [x] 187所有配色点进入色环、主色放大：移除配色模式栏和底部色块，九点同显、主色尺寸两倍，独立拾取／拖动联动，重合避让及真实锚点引线，拾取保留偏移；闲置缓存与原单事务预览保持。436回归、40前端、双核原生完整矩阵、四主题32份导出和四轮PS24.0.1事务（低CPU确认／取消、正常确认／取消）通过，核对恢复、一次撤销与资源还原。构建及实际187／UXP42入口和原生组件匹配，PS未重启，原稿／配置／历史未知记录保留；首次测试声明编译错误留档修复，未计通过。实体键鼠／画布及其他办公硬件待验收。证据.runtime/recovery187/runtime.json、live-matrix.json、tests187.log、native187-low.log、ui187-verified.json。
- [x] 187公开交付：源码／标签及Wiki六页已推送，安装包／SHA／清单匿名下载与冻结更新器API403备用通道真实下载核验通过。仅保留187 Release；186全部附件本地备份、Git标签／源码历史保留。实际187／UXP42，dist仅保留当前发行物；实体键鼠／画布与其他硬件范围仍按上项待验收。证据.runtime/recovery187/runtime.json、release-0.4.187-public-verification.json、manifest187-verification.json、fallback187-live.json、wiki187-public.json、release187-cleanup.json。



- [x] 186感知色轮／渐变代码、构建、实际运行：OKLCH／Oklab联动圆点、五种配色关系、感知明度与色块应用；新建／修改渐变默认感知，显式切换插值在本次菜单保留。PS原生填充与描边插值进入版本校验及写后读回；颜色预览首次提交即要求UXP42，防止中途切换渐变时旧执行端忽略插值参数；羽化单独修改保留渐变。436回归、40前端、双核原生完整交互、四主题24渲染及冻结EXE启动／重启通过；PS24.0.1真实事务4组（双核／Below Normal 3、正常1），覆盖感知填充＋描边确认、取消、仅改蒙版、一次撤销与路径／顺序恢复；资源已还原。实际186／UXP42、入口与原生／JSX资源匹配，原稿备份后恢复、配置与历史未知记录保留。早期色域精度和构建并发占用失败已留档修正，不计通过。实体Caps／鼠标到画布视觉、其他PS与真实办公i5／集显仍待对应验收。证据.runtime/recovery186/runtime.json、live-matrix.json、tests186.log、native186-low.log、ui186-verified.json。
- [x] 186公开交付：源码／标签及Wiki五页和侧栏已推送，安装包／SHA／清单匿名下载与冻结更新器API403备用通道真实下载核验通过。仅保留186 Release；184全部附件本地备份、Git标签／源码历史保留，185的RGB／Hex功能并入本次发行。实际186／UXP42，dist仅保留当前发行物；实体键鼠／画布与其他硬件范围仍按上项待验收。证据.runtime/recovery186/runtime.json、release-0.4.186-public-verification.json、manifest186-verification.json、fallback186-live.json、wiki186-public.json、release186-cleanup.json。

- [x] 185同排RGB／Hex代码／构建／实际入口：RGB共框三段输入，Hex同排独立外框和内嵌复制、固定#／过滤粘贴及自动大写；保留字体并按实际文字测宽，RGB支持拖动和双击输入。434回归、40前端、四主题24渲染、双逻辑处理器／Below Normal原生完整交互及冻结EXE启动／重启通过；覆盖RGB数值、带／不带#的Hex编辑、复制标准格式、预览未额外提交、渐变拖动和既有取消矩阵。实际185／UXP41，组件匹配，PS未重启、原稿／配置／资源／未知记录保持。早期小数像素导致渐变拖动断言失败已对齐修正，未将失败计为通过；实体鼠标和其他硬件视觉待验收。证据.runtime/recovery185/runtime.json、tests185.log、frontend185-test.log、native185-low.log、ui185-verified.json。
- [x] 185源码／标签及Wiki已同步；RGB／Hex功能合并进入186发布，不单独新增185 Release。185本地构建与恢复副本、Git标签保留，公开附件核验见186交付项。

- [x] 184全图标布局代码／构建／实际入口：填充／描边与无／纯色／渐变／图案同排分组，孤立勾选框改为无图标；黑白、单格交换、外观复制／粘贴同排。434回归、40前端、四主题24份串行渲染、双逻辑处理器／Below Normal原生完整交互矩阵和EXE启动／重启通过，包含无／纯色保留值与预览合并检查。实际184／UXP41，入口及资源匹配；PS／UXP未重启，原稿／配置／未知保护保持。早期几何断言失败及不完整导出留档、不计通过；后续修正提示几何记录并核对24张图标区域，实体鼠标及其他硬件视觉仍待验收。证据.runtime/recovery184/runtime.json、tests184.log、frontend184-test.log、native184-low.log、ui184-verified.json、ui184。
- [x] 184公开交付：源码／标签、Wiki四页与Release已推送，安装包／SHA／更新清单匿名下载及冻结更新器API403备用下载核验通过；只保留184 Release，183附件本地备份、Git标签／源码历史保留。实际184／UXP41，实体鼠标和其他电脑视觉仍待验收。证据.runtime/recovery184/runtime.json、release-0.4.184-public-verification.json、fallback184-live.json、wiki184-public.json、release184-cleanup.json。

- [x] 183禁用图标辨识代码／构建／实际入口：统一禁用主题灰，形状编辑器按真实禁用状态绘制，禁用项去掉蓝色选中底，后台同令牌；434回归、40前端、四主题24渲染、双逻辑处理器／Below Normal原生完整矩阵和冻结EXE启动／重启通过。实际183／UXP41，入口／组件／前端匹配；PS未重启、原稿／配置／历史未知记录保持。此次仅外观修改，未新增PS写入测试；实体鼠标和其他电脑视觉待验收。证据.runtime/recovery183/runtime.json、tests183.log、frontend183-test.log、native183-low.log、ui183。
- [x] 183公开交付：源码／标签、Wiki四页与Release已推送，安装包／SHA／更新清单匿名下载及冻结更新器API403备用下载核验通过；只保留183 Release，182附件本地备份、Git标签／源码历史保留。实际183／UXP41，实体鼠标和其他电脑视觉仍待验收。证据.runtime/recovery183/runtime.json、release-0.4.183-public-verification.json、fallback183-live.json、wiki183-public.json、release183-cleanup.json。

- [x] 182形状外观复制／单层及批量粘贴、无标题图标面板、羽化／密度双击输入已实现并切换实际入口182／UXP41。434项后台／原生回归、20套脚本、四主题16渲染、9组真实PS复制／粘贴（低CPU6／正常3）、2项整批拒绝和2组实时预览确认／取消通过；128层低CPU渐变粘贴约7.67秒。已实际运行182／UXP41，原稿、配置、资源及历史未知记录保留。实体鼠标／Caps／画布视觉、真实办公低配和现代PS仍待对应验收。 正常重启后插件和后台剪贴板均清空，测试用外观未留给用户。早期夹具map／历史游标断言失败保留未计通过；菜单初次完整取样和闲置快捷键性能仍是下一轮。证据.runtime/recovery182/runtime.json、live-matrix.json、clipboard-restart-verified.json、tests182.log、js182.log、native182-selftest.log、ui182。
- [x] 182公开交付：源码／标签、Release与Wiki已推送，安装包／SHA／最新更新清单匿名下载、冻结更新器API403备用下载及6个Wiki内容页核验通过；只保留182 Release，181附件本地备份，Git标签及历史保留。实际182／UXP41，434后台、40前端、20脚本与真实PS矩阵通过；测试剪贴板已在正常重启后清空、原稿恢复，实体鼠标／Caps视觉与其他硬件仍单列待验收。证据.runtime/recovery182/runtime.json、release-0.4.182-public-verification.json、fallback182-live.json、wiki182-public.json、release182-cleanup.json。

- [x] 181首次批量预览读取／实际运行入口：新鲜原生树和同步校验内描述符／祖先复用，小批及大树兼容保留；完整新鲜内容／路径／顺序／锁定与精确原值校验保持。428回归、19脚本、EXE启动／重启、7项最终真实PS事务（低CPU6／正常1）、4项外部修改及4项选择保护通过；128层同会话首次12.13→6.02秒、后续10.77→6.20秒。实时预览、Esc取消恢复和一次撤销保持，Esc路由矩阵通过；未模拟实体Esc或屏幕延迟。实际181／UXP40，配置／原稿／资源及历史未知保持。证据.runtime/recovery181/runtime.json、live-matrix.json、guards.json、tests181.log、js181.log。
- [x] 181公开交付：源码／标签与Release已推送，安装包／SHA／最新更新清单匿名下载、冻结更新器API403备用通道及Wiki五页核验通过；只保留181 Release，180附件本地备份，Git标签与历史保留。实际181／UXP40，EXE文件版本同步，原稿／配置／CPU资源恢复。证据.runtime/recovery181/runtime.json、release-0.4.181-public-verification.json、fallback181-live.json、wiki181-public.json、release181-cleanup.json。
- [ ] 下一轮性能TODO：128层首次打开菜单的完整UXP取样仍未加速；闲置后快捷键波动、真实i5／集显／低内存、Win10／PS2024和现代PS及实体画布验收仍待处理。181首次预览改善不等于这些项目已完成。

- [x] 核验现有180的Esc取消颜色／羽化：菜单尚未确认时恢复本次填充、描边和羽化／密度，长按Caps直接Esc或固定后Esc均取消，后续松Caps不确认；已确认完成用一次撤销。原生路由矩阵、相关29回归及一个PS24.0.1低CPU接口事务（两次实时改色／描边／蒙版后恢复全部属性与活动历史位置）通过；配置／原稿／资源保持。菜单实测因写前焦点保护拒绝未计通过，未模拟实体Esc；本次仅核验和补说明，运行程序仍180／UXP40，不重新发布。证据.runtime/escape180-key.cpp、escape180-regression.log、escape180-engine-1792317809681686100/verified.json。

- [x] 180收尾点击保护与持续小状态：按用户选择实现冻结图层列表点击，不排队、不补点，宿主写前确认原生配置生效，原事务确定终态及保护解除确认后撤下提示；未知／进度中断继续等待。428回归、四主题16渲染、EXE启动／重启、两个低CPU10层组件事务及一个正常10层执行端对照通过，实时预览、取消恢复、一次撤销、内容／路径／顺序及配置／原稿保持。用户本机原场景确认“不再响，提示结束后消失，选层正常”。实际180／UXP40；前台暂停和写前拒绝轮次未计通过。证据.runtime/recovery180/runtime.json、live-matrix.json、manual180-acceptance.json。
- [x] 180公开交付：源码／标签、Release已推送，安装包／SHA／最新更新清单匿名下载、冻结更新器API403备用通道及Wiki三页核验通过；只保留180 Release，177附件已本地备份，Git标签与历史保留。实际180／UXP40，用户原场景实体反馈通过。证据.runtime/recovery180/runtime.json、release-0.4.180-public-verification.json、fallback180-live.json、wiki180-public.json、release180-cleanup.json。

- [x] 179候选显示恢复／留证／实际入口：空前台过渡不永久隐藏原收尾提示，确定PS前台后恢复；真实外部应用不弹回。新增状态／隐藏原因记录，不增加PS查询或输入延迟。419回归、四主题16渲染、EXE启动／重启和三个10层真实PS事务通过（低CPU快速确认／取消、正常确认），产品异步预览发布路径、内容／选择／取消恢复／一次撤销及资源保持；本机179／UXP40，未重启PS。证据.runtime/recovery179/runtime.json、live-matrix.json、beep179-10-*。
- [x] 原提示音与忙碌点击已由180闭环：178实体反馈失败（仍响、转圈已消失），179显示恢复改动也未冒充提示音通过。用户选择仅拦住图层列表点击、不补做，并保留小状态到确定终态；180实现并在本机原场景确认不再响、提示结束消失、选层正常。没有静音、提前解除busy或重发。178／179保留为未发布的历史候选，跨版本／硬件仍单独待验收。

- [x] 178候选代码／构建／实际入口：修正隐藏菜单焦点交接误判导致小转圈提前隐藏；关闭释放本菜单鼠标捕获／活动控件，由原生菜单进程交还PS焦点，已知终态再补交接，外部应用／新菜单／未知原请求不抢焦点。419回归、四主题16渲染、EXE启动／重启与3次真实PS组件事务（低CPU2／正常1）通过；实时预览、取消恢复、一次撤销及文档／配置／资源保持。实际178／UXP40，证据.runtime/recovery178/runtime.json、close178-rootcause.json、live-matrix.json。
- [x] 178原提示音后续由180闭环：178实体反馈“仍响，转圈已经不在”，其验收失败保留。180新增有限点击保护与持续小状态，用户原场景已确认通过；不将419回归或API读回当作178已解决，也未发布178。

- [x] 177预览收尾提示：用户选择小转圈＋真实阶段文字，关闭后立即显示，点击穿透、不抢焦点，原事务结束隐藏，不重新打开完整菜单；外部应用不弹回。414回归、四主题16渲染、EXE启动／重启和3次真实PS组件联合事务（低CPU2／正常1）通过，确认一次撤销／取消恢复保持。实际177／UXP40，配置／原稿／旧未知记录及CPU恢复，无新增未知。证据.runtime/recovery177c/runtime.json、live-matrix.json。
- [ ] 跨硬件／版本的收尾实体验收：177的点击穿透未解决提示音，180在用户本机原场景已通过；真实低配／Win10／现代PS及完整实体画布视觉仍待对应验证，不能泛化本机反馈。
- [x] 177公开交付：源码／标签、Release已推送；安装包／SHA／更新清单匿名下载和冻结更新器API403备用通道通过，Wiki三页公开核验；只保留177 Release，176附件本地备份，Git标签／源码历史保留。实际177／UXP40。证据.runtime/recovery177c/runtime.json、release-0.4.177-public-verification.json、fallback177-live.json、wiki177-public.json、release177-cleanup.json。

- [x] 2026-10-18后续快捷键统一要求已写入AGENTS、快捷键／性能说明及Wiki：专用动作路径、交互优先、后台读取让行、锁外查询、传输独立分发、忙碌立即拒绝；保留新鲜校验、实时预览、取消恢复和一次撤销。规则记录不代表全部功能已改造。
- [ ] 优先定位176发布后“打开菜单后快捷键明显变快，随后又慢”：具体键／等待间隔／实体延迟待采样；对照冷启动、闲置、菜单开关、连续触发和后台完整读取重叠，核对预热／缓存失效、重读、锁及宿主等待。分别报告分项耗时、原始样本／数量／中位数／P95／最大值及忙碌拒绝率，低CPU与正常配置对照。反馈后瞬时176／UXP40空闲且连接正常，不能据此定因或标记修复；本次只记录需求与规则，未新增代码／构建或实机验收。证据.runtime/shortcut-response-observation-20261018/status.json。

- [x] 176代码／构建／实际运行入口：选层专用路径、批量选择ID、新鲜顺序索引、菜单查询锁拆分、同连接并发分发和修改前抢占锁。409后台／原生回归、18脚本与冻结EXE启动／重启通过。实际176／UXP40，EXE f8df5f31b711450eeb28e44ba6084f569c353a3b1322686c1fd5b6fdcb0492f6。
- [x] 176本机真实PS闭环：108事务（82低CPU／26正常）、4保护、真实过期回执、128层完整菜单和最终EXE服务忙碌拒绝通过；实时预览、取消恢复和一次撤销保持。16层单选174–232ms，多选转单选185–309ms；128层223–1023ms。完整128层编辑数据20.59秒，首次预览13.66秒，仍慢。旧未知／失败原样保留、配置和资源恢复。证据.runtime/recovery176/runtime.json、live-matrix.json。
- [x] 176公开交付：源码／标签与Release已推送，ZIP／SHA／更新清单匿名下载及冻结更新器API403备用下载通过，5页Wiki已同步并公开核验；仅保留176 Release，旧附件本地备份，Git标签／源码历史保留。实际176／UXP40。证据.runtime/recovery176/runtime.json、release-0.4.176-public-verification.json、fallback176-live.json、wiki176-public.json、release176-cleanup.json。
- [ ] 176实体Caps／鼠标、真实i5／集显／低内存、Win10／PS2024及现代PS验收；完整128层编辑与预览仍待继续优化。CPU限制与接口测试不能替代实体硬件及视觉验收。

- [x] 175实际入口／UXP39：保留174选层让行与关闭隐藏，补齐大菜单已接受只读完成窗口。401回归、18脚本、105真实事务（79低CPU／26正常）、4保护、真实过期回执及128层完整菜单通过；16层单选169–238ms，128层多选转单选678–1440ms。实时预览、取消恢复和一次撤销保持。证据.runtime/recovery175/runtime.json、live-matrix.json。
- [x] 175公开交付：源码／标签与Release已推送，ZIP／SHA／更新清单匿名下载和冻结更新器API403备用通道通过，Wiki已同步核验；只保留175 Release，172旧版及174未公开草稿附件已本地备份，Git标签／源码历史保留。实际175／UXP39。证据.runtime/recovery175/runtime.json、release-0.4.175-public-verification.json、fallback175-live.json、wiki175-public.json、release175-cleanup.json。
- [ ] 175实体Caps／鼠标、真实低配／现代PS验收；128层完整菜单与预览仍需等待。

- [x] 174选层读取让行／关闭反馈：基础与颜色／蒙版小批只读让行，关闭立即隐藏且迟到提示不重弹，合法未执行过期请求持久失败回执。实际174／UXP38与dist一致；400回归、18脚本、109真实PS事务（83低CPU／26正常）、4保护与1真实过期检查通过。16层单选161–215ms，128层多选转单选656–1410ms；不是实体键到屏幕耗时。实时预览、取消恢复和一次撤销保持。原51a59a…未知记录及早期失败原样保留，用户授权后正常重载，无强杀。证据.runtime/recovery174/runtime.json、live-matrix.json。
- [x] 174未公开候选：最后完整菜单验收发现128层10秒窗口问题，停止发布，由175修正；标签及失败证据保留。
- [ ] 174实体Caps／鼠标视觉、真实i5／集显／Win10及现代PS验收；128层预览仍需等待，不宣称全配置即时。

- [x] 173低干扰反馈本机交付：2单位细进度线、固定短线轻微明暗变化；去掉已固定／空闲／上次成功及预览期间重复待应用。398回归、20份四主题渲染与EXE启动／重启通过，实际173／UXP37及原生组件匹配，PS未重启，配置和历史未知记录保持。证据.runtime/recovery173/runtime.json、tests173.log、progress173-ui。实体键鼠视觉仍待验收；公开Release保持172。

- [x] 172 首次批量预览读取：减少重复UXP完整准备，原生写前完整新鲜校验，精确双精度／密度交接和旧版本兼容保持。实际172／UXP37；398回归、18脚本、8最终真实事务（低CPU7／正常1）、128层同会话旧准备对照、64层写入中取消及8保护检查通过。首次22.22→12.59秒，后续10.98秒；小数原值、实时读回重绘、取消恢复及一次撤销核验。证据.runtime/recovery172/runtime.json、live-matrix.json、tests172.log、js172.log；早期小数拒绝记录保留。
- [x] 172 公开交付：Release附件／更新清单匿名下载、冻结更新器API403备用通道、Wiki已核验；只保留172 Release，旧附件本地备份，Git标签／源码历史保留。实际172／UXP37与dist一致。证据.runtime/recovery172/runtime.json、release-0.4.172-public-verification.json、fallback172-live.json、wiki172-public.json、release172-cleanup.json。
- [ ] 172 后续：初始菜单取样和后续完整核验仍有优化空间；实体Caps／鼠标、真实低配及现代PS仍待对应实机验收。

- [ ] 画布取样拟合渐变／表面重画（2026-10-18，仅记录，未实现）：以当前画布的可见视觉效果为参考，通过“添加渐变点”在画布上点击放置多个取样点，结合颜色和空间位置拟合可编辑的渐变条，用于修图时重建表面的颜色、明暗与光照过渡。用户提出线条、角度、对称等模式；“线条”暂按线性渐变理解，具体模式、交互和拟合算法在实现时再确认。保留可视化点位与渐变条联动、色标／位置／方向手动修正的设计方向。目标是从取样点重建平滑表面，不做Illustrator式轮廓描摹或整图矢量化。算法需评估取样点颜色吻合度、点间过渡和空间映射；不能承诺任意二维取样都由一条渐变精确重建。后续明确取样半径、可见合成／颜色管理、缩放与坐标转换、输出为现有渐变或新填充层等细节；实现时保留实时预览、取消恢复、一次撤销并面向低配验证。规划说明见[Wiki](https://github.com/mianxiu/151500px-menu/wiki/Planned-features)。

- [x] 171 原生批次加速：8–128层每批最多16层，明确文档／图层ID，批次前新鲜身份和锁定检查，完整帧核验／重绘、取消恢复和一次撤销保持。实际171／UXP36；398回归、18脚本、8真实事务（低CPU7／正常1）、另1项64层写入中取消和4保护检查通过。动作面板与临时文件清理已核验。证据.runtime/recovery171/runtime.json、live-matrix.json、tests171.log、js171.log；原失败记录保留。
- [x] 171 公开交付：Release附件／更新清单匿名下载、冻结更新器API403备用通道、Wiki已核验；只保留171 Release，旧附件本地备份，Git标签／源码历史保留。实际171／UXP36与dist一致。证据.runtime/recovery171/runtime.json、release-0.4.171-public-verification.json、fallback171-live.json、wiki171-public.json、release171-cleanup.json。
- [ ] 171 后续：首次重复准备已在172优化，全量核验仍慢；真实低配、现代PS及实体Caps／鼠标画布观感仍待实机验收。

- [x] 170 低配批量读取优化及运行入口：精确原生快照比较、同步读取内DOM复用；完整新鲜目标／内容／路径／顺序／锁定核验、实时预览、取消恢复和一次撤销保持。实际170／UXP36，398回归、17脚本、8最终真实事务（低CPU7／正常1）、另1刷新中取消和4保护检查通过。同会话128层首帧27.38→24.25秒、下一帧21.33→18.70秒；小文档收益有限，不宣称即时。证据.runtime/recovery170/runtime.json、live-matrix.json、tests170.log、js170.log；合并属性写入失败原型及进度采样失败记录保留。
- [x] 170 公开交付：Release／Wiki已同步，匿名ZIP／SHA／更新清单及冻结更新器模拟API403后的真实备用下载通过；只保留170 Release，旧Release／附件已清理，Git标签和本地回退保留。实际170／UXP36与dist一致。另1项低CPU原生刷新期间取消补测通过，等待／恢复／核验进度和原值均核对。证据.runtime/recovery170/runtime.json、release-0.4.170-public-verification.json、fallback170-live.json、release170-cleanup.json、wiki170-public.json。
- [ ] 170 实体Caps／鼠标视觉、现代PS及故障i5／集显／Win10实机验收；CPU限制测试不能代替这些验收。

- [x] 169 分阶段真实进度与取消恢复提示：实际入口169／UXP36，读取／写入／核验计数、顺滑进度条、刷新／恢复动画、等待原调用返回、关闭不重弹；不增加PS查询或改变事务。398回归、16插件JS套件、四主题、生产SetWindowPos恢复反馈及最终EXE通过；12项真实PS24.0.1事务（低CPU10／正常2）和4项保护检查通过，原文档／资源／配置恢复。证据.runtime/recovery169/runtime.json、live-matrix.json、tests169.log及progress169-ui；128层仍较慢，不能称即时响应。
- [x] 169 公开交付：Release／Wiki已同步，匿名ZIP／SHA／更新清单及冻结更新器模拟API403后的真实备用下载通过；只保留169 Release，旧附件／Git标签／本地回退保留。实际169／UXP36与dist一致。另1项低CPU原生刷新期间取消补测通过，等待／恢复／核验进度和原值均核对。证据.runtime/recovery169/runtime.json、release-0.4.169-public-verification.json、fallback169-live.json、release169-cleanup.json、wiki169-public.json。
- [ ] 169 实体Caps／鼠标视觉、现代PS及故障i5／集显／Win10实机验收。自动事务、渲染和模拟消息测试不替代这些验收。

- [x] 168 多选羽化／密度代码、构建、实际入口及本机真实事务：2–128 层、混合原值、只改指定项；修复历史回滚丢失多选，整批取消恢复和一次撤销。394 回归、16 份插件脚本（批量28组）、四主题、EXE 启动／重启通过；11 项真实事务（9低CPU／2正常）和4项保护检查通过。实际168／UXP35，配置／文档及资源恢复，旧失败记录保留。证据.runtime/recovery168/runtime.json、live-matrix.json；docs/BATCH-MASKS.md记录大批量延迟。
- [x] 168 公开交付：Release／最终Wiki已同步，匿名ZIP／校验文件／备用清单及冻结更新器强制API403后的实网下载通过；仅保留168 Release，旧标签／源码／本地回退保留。dist仅168六项文件，实际入口168／UXP35匹配。证据.runtime/release-0.4.168-public-verification.json、fallback168-live.json、release168-cleanup.json、wiki168-public.json、recovery168/runtime.json。
- [ ] 168 现代宿主、故障i5／集显、Win10／PS2024及实体Caps／鼠标验收。128层旧PS预览仍较慢，不按全配置即时响应交付。

- [x] 167发布闭环（2026-10-18）：本机实际167／UXP34，源码与标签已推送；Wiki 236e596同步。公开ZIP／SHA及备用清单下载核验通过，发布EXE内更新模块在模拟API403后通过真实备用下载。Release仅保留167，165旧附件已备份、Git标签及历史保留。证据.runtime/release-0.4.167-public-verification.json、fallback167-live.json、release167-cleanup.json。
- [x] 167小文档／直接Caps优化及实际入口：一次请求内捕获与执行，保留事务内二次校验、目标落盘和未知保护；后台慢状态连接不阻塞操作。137项PS事务（低CPU111／正常26）、389后台／原生、15份脚本与最终EXE通过，实际167／UXP34及组件匹配。低CPU5层同会话中位182.44→158.62ms；完整限制见docs/CAPS-EXECUTION.md和.runtime/recovery167/runtime.json。
- [ ] 167剩余验收：实体Caps／鼠标与故障i5集显／PS24.2、Win10／PS2024；120／200／300ms连按接受率本轮新旧相同，仍需继续定位其他漏执行原因。双核完整套件中的原生恢复菜单自测失败原因未确认，独立双逻辑处理器原生自测／正常资源套件通过；不以宿主事务通过代替此项。

- [x] 166组合键执行优化代码／构建／实际入口：已到达执行请求优先于可选菜单／颜色读取，捕获交接窗口覆盖菜单读取；根级至少64项的Caps事务使用完整标量树，异常回退DOM，小列表保留原路径，成员核验改集合查找。381后台、15份脚本和最终EXE启动／重启通过。实际166／UXP33与dist、原生组件及前端匹配；75项真实PS事务（低CPU59、正常16），覆盖连续选层、Shift多选、混合类型／组、样式与移动一次撤销、预览确认／取消；资源／原稿／配置和旧未知记录保留。详见docs/CAPS-EXECUTION.md、.runtime/recovery166/runtime.json与live-matrix.json；本机候选，尚未发布。
- [ ] 连续Caps实体体验及小文档进一步优化：旧165实际日志证实两次已识别按键被忙碌拒绝；本轮100层CPU受限事务中位319→250ms，5层218→222ms基本不变，升级间重启过PS，不是同会话A/B。仍不排队、不补执行，不能宣称“重复几次才执行”已彻底解决；故障办公电脑及实体按键到屏幕待验证。

- [x] 165旧版独立预览代码／构建／本机入口：PS24.0.1走独立ExtendScript，UXP保留请求且不嵌套模态；最新版合并、租约取消、一次历史、写前／写后和原终态保护保持。381后台、40前端、15脚本（55组预览模拟）及最终EXE通过。实际165／UXP32匹配；完整通道15项真实PS预览通过（CPU受限13＋正常2），含20秒连续变化、35秒保持、独立像素蒙版／描边／渐变与1000px，资源和原稿恢复。证据.runtime/recovery165/live-matrix.json、runtime.json及native165-themes.json；未把API回执当作实体屏幕验收。
- [ ] 165实体Caps／鼠标和画布显示验收：自动界面工具新PS会话仍在MXC启动层报0x80070020，未完成实体屏幕检查；PS24.1+及故障办公电脑仍待对应机器验证。用户本轮要求发布165，保留未验收状态。
- [x] 165发布收尾：用户要求推送后发布；Backend ZIP／SHA256及latest备用更新清单匿名下载核验通过，实际EXE内冻结更新器模拟API403后经公开通道下载校验通过。旧159Release及附件删除前全部备份，新165为唯一Release；标签、源码及本地回退保留。本机仍实际165／UXP32；实体屏幕验收仍未完成。证据.runtime/release-0.4.165-public-verification.json、fallback165-live.json、manifest165-verification.json、release165-cleanup.json。

- [x] 164自动自检本机交付：版本／能力变化及异常稳定后只读检查、异步脱敏留证、最多20份、后台提示／导出／已读完成；373后台、40前端、13份脚本（51组预览模拟）、四主题／英文界面和最终EXE启动／重启通过。最终164入口／dist／原生／前端资源匹配，UXP31正常加载；真实报告导出与已读跨重启保持。低CPU及正常资源各6项真实PS草稿提交／一次撤销／过期目标拒绝通过，资源、主屏前台及原用户文档恢复。Wiki已同步；旧版实时预览仍未完成，未发布。证据.runtime/recovery164/final-runtime.json、health164-lowcpu/report.json、health164-normal/report.json、smoke164-final.log。

- [x] 独立预览原型探索已并入165：早期精确RGB断言失败是浮点精度差异，另修正单位、单描述符漏写和旧解释器嵌套三元问题；旧原型返回收据及失败记录原样保留，不重发、不伪造成功。

- [ ] Codex MXC界面工具：用户已明确跳过，未修复也不再作为插件优化的前置条件。已确认故障早于界面插件导入，无本项目UXP也可复现；桌面启动参数覆盖用户配置，不能承诺重启就恢复。未修改系统沙箱策略；后续仅保留实体交互未验收限制。

- [ ] 163／UXP31版本能力分流（历史候选记录；当前已切换165，最新状态见上）：代码及候选构建完成，351后台／37未变前端／13份脚本（51组事务）及原生四主题、EXE检查通过。24.1+实际存在官方重绘API时启用预览；旧版暂用兼容草稿，确认一次提交、取消不写PS。旧版PS本身能重绘，缺少的是此UXP接口；独立旧版实时预览尚未实现，不以草稿降级视为交付。实际入口163／内置安装加载UXP31及组件已匹配；原空PS经用户本次明确授权结束，新31会话启动及零文档正常退出通过。界面工具MXC错误可随PS运行／退出复现，停用本UXP对照未做，工具因URL可信度保护停止本轮交互；当前PS／服务已正常关闭，预览实机测试暂停。原4c029及其他未知记录保留，不重发、不伪造终态。只读诊断两次确认主线程仍在ScriptingSupport／ExtendScript链内、约占满单核，旧刷新未退栈，WCT未见等待循环；尚无Adobe内部根因符号。现代宿主实机、旧版实时预览／取消／一次撤销与低配稳定仍待完成；不发布，正式Release159。证据.runtime/preview163-candidate.json、native163-themes.json、recovery163/documents.json。

- [ ] 162／UXP30低配预览候选：减少拖动中同级与全局预设读取，控制文件读取有界并复用在途读取；保留目标／样式／取消／单次历史校验，独立记录执行阶段，失联查询不再触发COM。347后台、37前端、13份插件脚本（49组预览事务）、最终EXE启动／重启与实际原生四主题通过。已切换162，保护修订UXP30已加载；连续改色确认与取消短测两项通过，完整矩阵在前台变化期间出现取消后原目标未核验，不能算稳定通过。修复真实PS异常传递丢掉未知标记、重复resumeHistory及普通错误回滚未核验问题；原22d80ec78b53637ce4bf1482bd37a4eb收据／历史保留，实际后台待核对保护已核验。用户另行允许这次恢复后正常重载，在独立会话继续矩阵；旧161记录也保留，不重发、不伪造终态。完整低配／正常对照及实体画布尚未完成，不发布；正式Release仍159。见docs/MASK-PREVIEW.md、.runtime/rollback162-guard/protected-runtime.json、recovery162-rollback/session.json。 实机后续结论：保护修订11项事务通过，带阶段短测再次出现原4c029674d82992dd046feafa8f05054e持续停在redraw-legacy，独立心跳正常、没有前台变化；已定位旧版app.refresh原生等待，官方取消后仍无终态。资源恢复、停止后续测试，不发布；当前PS仅24.0.1，24.1+官方重绘路径尚无本机宿主验收。

- [ ] 161／UXP29候选稳定性阻塞，尚未发布：本机实际入口已切换161，用户已目视确认UXP29按住拖动时画布变化；7项短测试通过，但随后低CPU连续拖动测试在11项已完成事务后失联。原请求065d9c919ae96b1aace49c41d5706be4仅有accepted及首个未改值回执，没有终态；取消已发布，用户关闭菜单／Esc仍未恢复。资源限制已解除，不重发、不COM回退、不重启清保护。菜单全灰与其他电脑症状相似，不代表内部根因相同；原诊断是PS24.2／UXP14，本次PS24.0.1／UXP29。后续须先核对原请求终态，再定位阻塞调用及验证恢复；当前不能标为低配通过。证据.runtime/paint-preview-live-1792231343638308400/report.json、runtime161-verification.json、preview29-user-verification.json；正式Release仍159。

- [x] 160预览刷新代码、构建与本机入口：UXP27每批实际变化后只读刷新当前层，等待刷新完成才回执，取消后刷新恢复；单次历史、目标校验和未知结果不重发保持。340后台、37前端、13份脚本（36组预览事务）、原生自测和最终EXE烟雾／重启通过。55项真实PS刷新回执／取消／一次撤销通过（颜色31低CPU＋8正常，蒙版10低CPU＋6正常）；资源均恢复。实际160／内置及加载UXP27，入口、dist与原生组件哈希一致；备份旧159及用户配置，正常重载前保留文档副本。
- [ ] 160画布随拖动实时更新的目视验收与正式发布（用户实测仍不更新，本候选未解决，继续修复）：159参数读回不等于画布重绘；本轮真实PS刷新API回执、取消和撤销另留证据，不勾选屏幕效果。界面工具初始化0x80070020，待用户目视确认。正式Release仍159，160构建供本机验证；确认后再发布、核验附件／备用更新并清理旧Release，保留标签与回退。

- [x] 159双面板与填充Set报错：颜色在上、羽化／密度在下，同宽、6单位间隔，空隙取消与共用事务保持；UXP26撤回矢量直接纯色setter。340后台、37前端、13份插件脚本（30组预览事务）、实际组件四主题与32份两字号／四内容预览、最终EXE启动／重启通过。真实PS24.0.1颜色低CPU31项／正常8项、蒙版低CPU10项／正常6项，再加实际恢复文档独立副本低CPU2项，共57项事务通过；取消恢复、末值、路径／独立像素蒙版和一次撤销分别核验。 实际159入口／dist／组件与内置／安装／加载UXP26一致，配置与文档保留。证据.runtime/caps159-final-pending.json、runtime159-verification.json、live159-reports.json、additional159-reports.json、panels159-previews.json、native159-themes.json。原报错匹配终态确认失败后才重载；未保存内容恢复副本在.local/recovery/documents/panels159-1792227890664024000。一次正常对照因前台变化安全停止并清理，最终稳定后6项收尾通过，未强行恢复焦点；主动路径继承样本未复现，另用真实恢复文档独立副本2项验证。
- [x] 159公开交付：ZIP／SHA256及最新备用清单匿名下载核验、最终冻结更新器强制API403后的实网下载与校验通过；Wiki八页面和首页／图片匿名核验。发行页仅159，旧158 Release／附件已清理，全部源码标签与本地回退保留；dist仅159六项文件，实际入口与UXP26匹配。证据.runtime/runtime159-delivered.json、release-0.4.159-public-verification.json、manifest159-verification.json、fallback159-live.json、release159-cleanup.json。
- [ ] 159双面板实体Caps／鼠标命中、空隙取消及真实画布流畅度、跨DPI和故障集显电脑验收。

- [x] 158颜色实时预览代码／测试／构建／实际入口：340后台、37前端、13份插件脚本（含30组实时预览事务）、原生四主题生产显示／命中／输入／取消与最终EXE启动／重启通过。最终UXP25在真实PS24.0.1通过颜色低CPU27项／正常8项及蒙版低CPU10项／正常6项，共51项事务；取消恢复、一次撤销、末值、交叉编辑、普通纯色／渐变、路径／像素蒙版和未编辑属性保留分别核验。 本机158 EXE／dist／原生组件与内置／安装／加载UXP25匹配，文档／配置／主屏前台保持，资源已恢复。证据.runtime/live158-reports.json、caps158-final-pending.json、runtime158-verification.json、native158-themes.json。首轮渐变失败仅根据原请求匹配终态核对；最终批次没有重发或COM写入回退。
- [x] 158公开交付：ZIP／SHA256及最新备用清单匿名下载核验、最终冻结更新器强制API403后的实网下载与校验通过；Wiki八页面和首页／图片匿名核验。发行页仅158，旧157 Release／附件已清理，全部源码标签与本地回退保留；dist仅158六项文件，实际入口与UXP25匹配。证据.runtime/runtime158-delivered.json、release-0.4.158-public-verification.json、manifest158-verification.json、fallback158-live.json、release158-cleanup.json。
- [ ] 158实体Caps／鼠标预览与取消、一次撤销及跨DPI、真实低内存／集显／故障电脑验收；仅CPU受限51项实机事务通过，不冒充实体帧率或所有配置稳定保证。

- [x] 157实时预览代码／测试／构建／实际入口：340后台、37前端、13插件（15组预览事务）通过；同一最终UXP24受限CPU10项／正常6项真实PS预览及19项既有调色通过，取消恢复、末值、持续拖动、6秒停留、4秒失联租约、1000px／0%边界与一次撤销分别核验。157补齐确认分支既有前台授权，实际EXE／dist／原生组件与内置／安装／加载UXP24已匹配，最终启动／重启及实际组件四主题生产显示／输入／取消／预览与单次授权自测通过。证据.runtime/caps157-final-pending.json、runtime157-verification.json、mask-preview-live-1792222080456651600、mask-preview-live-1792222148190491900、shape-live-0.4.156.json、native157-themes.json。156未公开候选的源码标签与本地副本保留。PS资源、原文档已恢复；低配文档测试主屏前台保持，157首轮原生检查前台变化时停止、稳定后检查通过，未强行恢复或归因。
- [x] 157公开交付：统一ZIP／SHA256及最新备用清单匿名下载校验通过，实际冻结更新器强制API403后实网下载与校验通过；Wiki实时预览及八个页面／入口匿名核验。发行页仅157，155及156草稿的Release／附件已清理，全部Git标签（含未公开156）与本地回退保留；dist仅157六项文件，实际入口与组件／UXP24匹配。证据.runtime/runtime157-delivered.json、release-0.4.157-public-verification.json、manifest157-verification.json、fallback157-live.json、release157-cleanup.json。
- [ ] 157实体Caps／鼠标实时画布预览、取消恢复与一次撤销，跨DPI及故障低配／集显电脑验收；原生模拟拖动与UXP实时读回、终态后独立COM检查分别记录，尚不替代实体体验与视频掉帧检测。

- [x] 2026-10-17项目首页整理：README由917行收紧为42行，保留项目介绍／主要功能／两张界面图／安装与Wiki入口；操作、安装、更新、排查及开发说明迁入Wiki，原README完整归档。49条本地链接、PNG完整性、旧内容无损迁移与7个匿名Wiki页面核验通过；修复旧README章节链接。本次仅文档和图片，运行程序仍155／UXP23，不重建或替换发行包。

- [x] 155后台快捷键下拉代码／测试／构建／实际入口：333后台、37前端、1664组合与后台解析一致；双逻辑处理器和较低优先级隔离浏览器的选择／冲突／未完成／保存重载／放弃／未绑定／四主题／窄窗口通过。最终EXE烟雾／重启通过，实际155 HTTP／包内资源及14项原绑定匹配，PS进程／文档／配置及主屏焦点保持。证据.runtime/ui155-1792219818700785800、ui155-1792219992840528200、shortcuts155-parity.json、runtime155-verification.json。
- [ ] 155后台真实Windows下拉弹出／键盘导航与故障低配电脑验收；隔离浏览器DOM交互不替代实体键鼠，原生菜单和UXP本轮未改。

- [x] 2026-10-17确认后续默认低配优先：已写入AGENTS及低干扰测试说明；短批次先验正确性、实时性、撤销和恢复，正常配置随后对照。真实i5／集显／Win10仍单列，未以CPU限制代替实机通过。
- [x] 核对154既有状态显示：运行中持续旋转并显示操作名／经过秒数，蓝勾＋“空闲”表示已完成；补齐Wiki状态说明。本次未修改动画或发行二进制，实体视觉效果未新标通过。

- [x] 副屏低干扰维护工具：定位Codex副屏、无激活放置PS窗口／面板、两逻辑处理器及Below Normal临时限制；正常／异常／看门狗恢复自测及153／UXP23的3批9操作通过，Chrome前台不变，资源已恢复。证据.runtime/quiet-ps-1792218172731849700；视频掉帧／音画同步、启动画面和实体键鼠未验收，Codex工具0x80070020仍未修复。
- [x] 154完成反馈恢复代码／构建／实际入口已交付：成功后450ms＋新鲜只读快照恢复图层菜单并固定，轮换会话、清空旧命中；333后台、33前端、12份插件、最终EXE烟雾及副屏原生生产显示／恢复自测通过，Chrome前台不变，UXP23及PS文档保留。证据.runtime/runtime154-verification.json、native154-selftest.log、tests154.log。
- [ ] 154实体Caps／鼠标自动恢复验收；其他配置及视频掉帧／音画同步待测。154低干扰复测前三项通过后，第二批前台变化触发安全停止，资源及文档恢复、无待核对结果，未标完整三批通过，详见docs/QUIET-TESTING.md。

2026-10-17同步：158颜色实时预览交付；最终运行入口／Release核验在本轮完成记录中单列。155及更早条目保持历史时点。下方版本记录中的“实际”“Latest”“已发布”和测试数量属于当时证据；旧Release已清理，Git标签和本地回退保留。profile146文件是145测量的实验代号，146正式版另有统一诊断交付记录。

## 当前优先事项

- [x] 153按大小分流创建树：32节点内原DOM，大树有界标量批量读取、完整性与已读前缀校验，不支持／损坏／超范围回退。真实目标、写后新鲜结构、一次撤销／回滚、未知结果保护和刷新周期保留；构建并切换实际153／UXP23，验证与范围见docs/CREATION-TREES.md、.runtime/proof153.json。
- [ ] 153跨配置与实体键鼠：本机真实PS事务和原生自测通过；故障i5／PS24.2、Win10／PS2024、更高PS及完整实体Caps／鼠标仍需实测。

- [x] 152扩展类型实机事务：24类每类12命令共288，追加24项独立智能对象转换审计及36项4K命令，去重348项成功、51项旧目标拒绝；另19项形状颜色／羽化／密度回归。原生UUID／剪贴变化对照、描述符、路径／蒙版／样式／文字／变换、父组／顺序与一次撤销核验。部分初轮夹具失败不计入成功，完整范围见docs/DIVERSE-LAYERS.md和.runtime/diverse152-summary.json。实体键鼠和故障机仍未验收。
- [x] 152标量整树候选研究：34普通只读原型、5相同modal、3修正基线普通只读场景，504对结构一致；同域100／300混合中位29→4ms／83→11ms。仅开发探针，不替换生产；不是完整事务或唤出提速，证据.runtime/tree152-modal.json、tree152-cached-readonly.json。
- [ ] 整树候选后续验证（部分完成）：153已完成限定创建路径的本机生产接入、完整连续创建前后对比、首次扫描与写后核验分测、异常回退和真实多类型／撤销回归。排序算法保持原实现；交替A/B、长期尾延时及跨配置仍未完成，不按152只读比例宣称收益。

- [x] 152中部首次定位代码／测试／构建／本机运行：大列表位置未命中才进行有上限的原生ID查询，按父组和原生顺序生成候选位置，每次用新鲜集合精确ID核验并重读属性；异步期间选择／文档变化拒绝，原生失败或超范围回退，小列表／端点／暖提示不增查询。330后台、33前端、12份插件回归、实际原生自测与最终EXE烟雾／重启通过；同宿主13个冷场景、9个暖场景完整字段一致，19种真实变化对照、最终生产13场景、25次自然刷新、16次标签执行／撤销与选层／排序／连续创建／形状回归通过。实际152／UXP22及组件匹配，正常重启加载、临时探针移除。证据.runtime/read152-cold-paired.json、read152-warm-paired.json、position152-mutations.json、read152-production.json、refresh152-live.json、execution152-live.json、read152-runtime-verification.json。
- [ ] 152实体键鼠与跨配置：i5集显／PS24.2及Win10／PS25.5仍需故障机结果；本机读取与事务不替代实体Caps／鼠标验收。

- [x] 151中部重复读取与提交前心跳窗口：最多2048条纯位置提示，每次新集合精确ID核验，未命中回退原扫描，小列表／多数选择保留原路径；不缓存类型／锁定／顺序、不改变轮询、到期或事务。心跳仅在提交前遇到JSON覆盖或Windows共享冲突时有界重读，正常零等待，不重发修改。330后台、26前端、12份插件回归、最终EXE烟雾／重启与实际原生组件自测通过；真实同宿主13场景各20对完整字段相等、19种真实变化对照、最终生产13场景、25次自然刷新、100／300层16次标签执行／读回／一次撤销、连续创建／选层／调色回归通过。实际入口与dist151／UXP21及组件匹配，正常重启PS已加载21，临时探针移除。证据.runtime/read151-paired.json、position151-mutations.json、read151-production.json、refresh151-live.json、execution151-live.json、read151-runtime-verification.json、tests151-final.log、smoke151-final.log。
- [ ] 151跨配置及实体键鼠验收：本轮真实事务和原生自测不替代i5集显／PS24.2故障机、完整实体Caps／鼠标验收；首次未知位置或提示失效仍可能扫描。

- [x] 150形状羽化／密度代码、构建及本机运行：面板底部两条滑条，羽化0–1000px／密度0–100%，本地草稿、松Caps一次提交、固定后再次Caps提交与原取消保持。只对矢量形状读取两项，普通填充不增加读取或显示；仅改蒙版不写shapeStyle，颜色与蒙版共用一次历史事务，读回失败回滚，旧蒙版／旧选择／UXP19拒绝。327后台、21前端、16原生主题／类型导出、实际组件生产SHOWWINDOW与命中／取消自测、最终EXE烟雾／重启通过；实际150／UXP20，本机PS24.0.1羽化、密度、边界、与颜色合并、一次撤销、路径与独立像素蒙版保持及旧状态拒绝通过。证据.runtime/shape-live-0.4.150.json、mask150-runtime-verification.json、mask150-previews.json、native150-delivered-selftest.log、tests150-final.log、smoke150-final.log、reload150-final.log。
- [ ] 150实体滑条拖动与Caps提交：Windows交互工具0x80070020不可用；原生模拟拖动／四主题导出与真实PS事务读回分别通过，不能代替完整实体键鼠、不同DPI及故障电脑验收。

- [x] 149长按延迟代码／构建／运行入口：移除展开前220ms等待，仅合并已入队事件；短按固定／长按释放阈值保留。图层请求单次发布移到已有图层监视线程、窗口锁外，旧菜单／关闭／到期结果拒绝；首帧不等待UXP共享文件。325后台、21前端、原生及最终EXE烟雾／重启通过；生产C++写线程各20次队列到管道中位229.29→0.67ms，实际149／UXP19及组件哈希匹配，PS进程／文档／配置保持。最终EXE后台代码与真实UXP新鲜请求、基础／完整阶段联通，GUI和范围为夹具。证据.runtime/opening149-wire.json、opening149-runtime-verification.json、opening149-frozen-read.json、tests149-final.log、smoke149-final.log。
- [ ] 149实体长按跟手与屏幕显示验收：Windows工具初始化失败0x80070020，未完成真实按键到首帧、图层／画布／输入／焦点／取消矩阵，不把管道计时或夹具渲染当实机验收。用户优先恢复立即展开，第二键较晚按下可能闪现，已在说明明确。

- [x] 148读取优化代码／测试／构建／本机运行与真实PS事务：新鲜集合按需构造对象、单次快照内复用；小列表／多数选择沿用原宿主路径。10,584项完整字段模拟对照、真实同宿主520次旧／新字段一致，9个大列表场景P95改善；322后台、21前端与最终EXE烟雾／重启通过。实际148／UXP19，原生组件匹配；真实图层识别／选择／排序／标签、10次连续原生创建、形状及普通填充调色、一次撤销／旧目标拒绝及25次自然刷新通过。原文档／配置保留，临时探针已移除。证据.runtime/read148-paired-accepted.json、read148-production.json、refresh148-live.json、layer-context-0.4.148-live.json、consecutive-creation-0.4.148.json、shape-live-0.4.148.json、selection148-live.json、read148-runtime-verification.json。实体键鼠和故障i5电脑另列未验收。
- [ ] 超大文档及执行内部的进一步定位：152已覆盖受支持范围内的首次中部定位。原生条目超过4096、接口不支持或查询异常时保留原扫描；后续评估按父组限定原生区间，并分析排序／创建事务完整树校验的成本，不能跳过目标或读回。

- [x] 147加载登记修复的代码／测试／实际入口与本机宿主验收：复现已有用户PluginsInfo索引抑制External兜底，Windows反斜杠路径补齐本插件后两个插件均实际加载。安装／语言同步／卸载／回退仅合并本插件条目，其他插件与未知字段保留；损坏／冲突拒绝覆盖。后台登记状态及主动修复已接通，默认跟随可处理同版本缺失登记；禁用、busy、未知结果及PS运行保护保留。320后台、14前端、最终EXE烟雾／重启通过；实际入口及dist147，自动／手动生产API、正常重启加载UXP18、诊断登记状态及真实标签写入／读回／一次撤销通过，原已保存文档重开、配置保持。证据.runtime/registration147-live-verification.json、registration147-runtime-verification.json、registration147/after-windows-path.json、tests147-final.log、smoke147-final.log。
- [ ] 147故障Win10／PS25.5复测及实体后台界面：本机真实PS24.0.1验证同型索引失败和修复，不能代替用户故障电脑；界面工具不可用，按钮点击／四主题视觉未验收。

- [ ] 146统一诊断实体界面／四主题与故障电脑验收：实际接口与HTTP资源已核对，界面自动化工具无法初始化；PS25.5故障机需升级后从后台导出新综合报告。

- [x] 146统一后台检测代码／构建／实际入口与API验收：综合／菜单与快捷键／UXP安装加载／运行与图层读取四范围，统一进度及JSON；打包两份PowerShell检测器，加载日志仅分析当前宿主及本次启动后的记录，单项失败保留其他结果；运行部分仅导出已有计数／耗时，排除文档、凭据及原始错误。305后台（含12项诊断）、14前端、最终EXE烟雾通过；实际入口及dist146／UXP18，四范围生产API采集与导出通过，运行GPU实际模式、当前宿主日志及组件哈希匹配，PS进程／文档和配置保持。界面工具0x80070020不可用，实体下拉／点击／视觉仍未验收；不将故障PS25.5标为修复。证据.runtime/diagnostics146-live-verification.json、diagnostics146-runtime-verification.json、tests146-final.log、smoke146-final.log。

- [ ] Win10／PS25.5完全无插件条目：已取得用户索引只登记151500PX、未扫描用户External的报告；147已修复并本机复现验证，故障机需升级复测。与已确认可用的PS25菜单问题分开。
- [ ] i5集显／PS24.2连续使用和长按调色：旧诊断实际142／UXP14，需要新版故障机实测，未确认唯一根因。
- [ ] 当前153所含Caps／F5、四段排序／鼠标锚点及完整输入／焦点／取消矩阵；自动测试和真实事务读回均不代替实体交互。
- [ ] Tab仅测试并保留当前绑定；自动ENG仅评估，下一阶段只读观察未完成。

- [x] 图层读取数量基线与本轮优化：145／UXP18的4／100层基线48.5／280ms保留历史；148／UXP19已完成新算法与本机验收，详见上项和PERFORMANCE.md。不同PS会话性能波动大，提升结论仅用同会话交替旧／新对照；1000层旧建样本心跳失效不计成功。跨硬件与下一阶段中部定位仍未完成。
- [x] UXP未加载独立采集脚本：tools/inspect-uxp.ps1在Windows PowerShell5实际运行，采集当前用户安装路径、manifest／脚本哈希、宿主UXP存在性、最近两份受限加载日志与后台安装／加载状态；不读凭据／文档，不修改安装、设置或PS，不覆盖报告。本机正常加载及路径匹配、日志过滤、已有文件保护核验通过；故障机结果待提供，不等同修复。证据.runtime/uxp-startup-local146-final.json。独立脚本更新，实际145及发布附件保持。

## 已交付与按版本验收记录

- [x] 153正式发行及Wiki：源码／注释标签、Wiki正文已推送并匿名核验；统一ZIP／SHA256／备用清单匿名下载、最终EXE内更新器强制API403后真实下载校验通过。随后只删除152 Release，源码标签与本地152回退保留；dist六项均为153，实际入口／内置安装加载UXP23及组件匹配。证据.runtime/release-0.4.153-public-verification.json、manifest153-verification.json、fallback153-live.json、wiki153-verification.json、release153-cleanup.json、runtime153-delivered.json。实体键鼠和故障机仍待验证。

- [x] 152正式发行、Wiki与只留最新版：源码／v0.4.152标签及Wiki推送，Wiki公开正文、统一ZIP／SHA256／备用清单匿名下载核验，最终EXE内更新器强制API403后真实下载／校验152通过；随后移除151 Release，Git标签集合不变、151本地回退保留。dist与实际入口152／UXP22及组件匹配，当前dist仅152。证据.runtime/release-0.4.152-public-verification.json、manifest152-verification.json、fallback152-live.json、release152-cleanup.json、wiki152-verification.json、runtime152-delivered.json。实体键鼠与故障机未验收。

- [x] 151正式发行、Wiki与只留最新版：源码／v0.4.151标签及Wiki推送，Wiki公开正文、统一ZIP／SHA256／备用清单匿名下载核验，最终EXE内更新器强制API403后真实下载／校验151通过；随后移除150 Release，Git标签集合不变、150本地回退保留。dist与实际入口151／UXP21及组件匹配，当前dist仅151。证据.runtime/release-0.4.151-public-verification.json、manifest151-verification.json、fallback151-live.json、release151-cleanup.json、wiki151-verification.json、runtime151-delivered.json。实体键鼠与故障机未验收。

- [x] 150正式发行与仅留最新版：源码／v0.4.150标签推送，统一包／SHA256／备用清单匿名下载核验；最终EXE内更新器强制API403后真实下载／校验150通过。新包验证后移除149 Release，远程Git标签前后相同，149本地回退保留；dist与实际运行150／UXP20。证据.runtime/release-0.4.150-public-verification.json、manifest150-verification.json、fallback150-live.json、release150-cleanup.json、runtime150-delivered.json。故障机与实体键鼠仍另列未验收。

- [x] 149正式发行与仅留最新版：源码／v0.4.149标签推送，统一包／SHA256／备用清单匿名下载核验；最终EXE内更新器强制API403后真实下载／校验149通过。新包验证后移除148 Release，远程Git标签前后相同，148本地回退保留；dist与实际运行149／UXP19。证据.runtime/release-0.4.149-public-verification.json、manifest149-verification.json、fallback149-live.json、release149-cleanup.json、runtime149-delivered.json。故障机与实体键鼠仍另列未验收。

- [x] 148正式发行与仅留最新版：源码／v0.4.148标签推送，统一包／SHA256／备用清单匿名下载核验；最终EXE内更新器强制API403后真实下载／校验148通过。新包验证后移除147 Release，远程Git标签前后相同，147本地回退保留；dist与实际运行148／UXP19。证据.runtime/release-0.4.148-public-verification.json、manifest148-verification.json、fallback148-live.json、release148-cleanup.json、runtime148-delivered.json。故障机与实体键鼠仍另列未验收。

- [x] 147正式发行与仅留最新版：源码／v0.4.147标签推送，统一包／SHA256／备用清单匿名下载核验，最终EXE内更新器强制API403后真实清单与147包下载校验通过。新包验证后移除146 Release，Git标签前后相同，146本地回退保留；dist及实际服务147／UXP18。证据.runtime/release-0.4.147-public-verification.json、manifest147-verification.json、fallback147-live.json、release147-cleanup.json、runtime147-delivered.json。故障Win10／PS25.5复测仍未勾选。

- [x] 146正式发行与仅留最新版：源码／v0.4.146标签已推送；公开ZIP／SHA256／备用清单匿名下载核验，最终EXE内更新器强制API403后真实清单及146包下载／校验通过。145 Release在新包验证后清理，Git远程标签前后相同，145本地回退保留；dist仅当前146。证据.runtime/release-0.4.146-public-verification.json、manifest146-verification.json、fallback146-live.json、release146-cleanup.json、runtime146-delivered.json。

- [x] 2026-10-16说明同步：README、安装／更新、发行记录、UXP、UI／协作规则、识别范围、目录、性能及读取／ENG实验说明共14份统一到145／UXP18；修正旧快捷键与Latest，保留历史证据并明确未完成边界。17份Markdown／45个本地链接及当前规则一致性检查通过；实际服务、内置／磁盘／加载版本和dist核对通过。GitHub145公开Release说明已同步，附件及发布时间保持，发行页仅145。证据.runtime/docs145-consistency.json、docs145-release-sync.json。本次仅说明更新，不提高运行版本。

- [x] 145 Caps组合键／F5代码与本机交付（2026-10-16）：输出线程区分Caps前缀意图，快速组合键不输出hold_begin；短按松开、220ms长按及鼠标移动提前显示保留，按下坐标贯穿到原生菜单。F5迁移为转换智能对象，旧插件在发布请求前拒绝，保留输入／焦点／busy／未知结果保护。300后台、13前端、原生状态机及真实生产管道序列、最终EXE烟雾通过；实际入口／dist145，已正常重启加载UXP18，组件与资源哈希匹配。真实PS24.0.1单层／多层／不相邻／组／组内／背景／生产API及一次撤销通过；撤销选择遵循原生历史。证据.runtime/caps145-wire.log、smart145-live.json、caps145-final-verification.json、restart145-verification.json、tests145-final.log。
- [ ] 145实体Caps／F5及闪现视觉验收：键鼠工具不可用；生产管道证实快速组合键无菜单消息，不替代实体按键到屏幕观察。既有选择命令生产回归本轮因PS前台保护被拒绝，未标为通过；原生输入／焦点／取消和UXP选层回归通过。
- [x] 145发行核验与仅保留最新版：公开附件、备用清单和最终EXE内更新器强制API403后的真实下载校验通过；发行页仅145，清理144 Release前后Git标签集合一致，144本地回退保留。证据.runtime/release-0.4.145-public-verification.json、manifest145-verification.json、fallback145-live.json、release145-cleanup.json、runtime145-delivered.json。
- [ ] Win10／PS2024已安装但未加载（2026-10-16）：已区分文件安装成功与宿主加载；版本检查与生命周期22项测试通过。新诊断确认PS25.5／后台144／已安装UXP17，loaded_uxp=null、uxp_ready=false；用户确认增效工具菜单完全无本插件，面板识别及原生组件正常。尚无宿主加载错误或根因，离线External部署的跨机识别仍需核验，不能把安装成功等同加载成功。

- [x] 144慢读取修复／本机交付（2026-10-16）：基础图层能力先于形状颜色发布，后台不再持续读颜色／图案；慢读取按耗时限频，只读请求10秒，仍拒绝陈旧快照、旧菜单／会话／目标。实际EXE及dist144／UXP17、组件哈希匹配；297后台、13前端、原生及EXE验收通过。真实PS24.0.1切换24轮，35秒草稿及颜色／渐变／图案应用、一次撤销、旧目标拒绝通过。证据.runtime/caps144-final-verification.json、shape-switch144.json、shape-live-0.4.144.json、tests144-final.log。
- [ ] 144故障电脑实机验收：用户诊断实际142／UXP14／PS24.2.0，热键发出、layers识别、busy=false；报告缺少快照耗时，尚不能确认故障唯一根因。需在i5集显电脑验证连续使用和长按Caps后填充／描边选色松键；本机键鼠工具因CreateProcessSecurityEnvironment 0x80070020不可用，真实PS执行和原生120秒草稿自测不替代实体键鼠验收。
- [x] 144 GitHub更新／发行清理：源码和v0.4.144标签已推送；统一包与校验附件、更新清单公开并匿名下载核验；直接执行最终EXE内更新器代码，强制API403后读取真实清单并下载／校验144通过。其余14条旧Release及草稿已删除，发行页仅144；前后远程标签集合相同，143本地备份保留。证据.runtime/release-0.4.144-public-verification.json、manifest144-verification.json、fallback144-live.json、release144-cleanup.json、runtime144-delivered.json。

- [x] 后台快捷键下拉选键（原2026-10-16记录）：155已实现并切换实际入口；可选组合键／触发键，保留原绑定及后台冲突校验，完成证据见顶部155条目。

- [x] Tab／Caps+Tab基础评估（2026-10-16，仅测试）：复用生产routeKey的真实隔离钩子，100组Tab及重复／抬起无泄漏；Caps+Tab单次与带标记Tab单次转发通过，作用域外／对话框Tab放行。PS24.0.1独立togglePalettes显隐5→0→5通过，文档／选择／历史保持，当前143／UXP16与Caps绑定未改。证据.runtime/tab-assessment143.json、tab-isolation-result.json、tab-panels-ps-readback.json。
- [ ] Tab完整PS联动／迁移：用户明确仅测试，暂不切换。当前Tab唤出配置在textInput=true时仍吞Tab，需要补输入导航保护；实际PS拦截阶段因前台不稳定未验收，连续两次失败后停止，最后一次未发送按键。Caps+Tab实际集成、实体按键／输入／焦点／取消／钩子重装矩阵均未完成；不据隔离零泄漏承诺任何状态下绝不漏键。

- [x] 143 Caps选层代码／构建／实际入口（2026-10-16）：Caps+W/S从冻结活动层单选上下图层；Shift+Caps+W/S保留选择并从选区边缘追加多选，原生顺序及边界循环保留。旧配置迁移加入Shift绑定，上移／下移快捷键仍禁用。291项后台、13项前端、原生左右Shift／顺序／重复／释放／输入保护及最终EXE烟雾／重启通过；实际后台与dist143，UXP16正常重启加载，已保存文档重开。真实PS普通层／组内／多选及全选收敛／旧目标拒绝，顺序与历史不变；生产API四命令成功，完成快照与PS选中ID一致。证据.runtime/selection143-live.json、selection143-production.json、restart143-verification.json、tests143-final.log、smoke143-final.log。源码与v0.4.143标签已推送，Release公开为Latest；匿名附件大小／SHA256与142生产更新器检查／下载／校验143通过，证据.runtime/release-0.4.143-public-verification.json、release143-updater-verification.json。
- [ ] 143完整实体Caps+W/S及Shift组合验收：真实键盘按法／快速连按、输入编辑／跨应用焦点、菜单重开与取消矩阵待确认；原生状态机和生产API测试不代替该项。

- [x] 142代码／构建／实际入口完成：四段图文、上下移动快捷键移除及宽调色菜单鼠标锚点已实现；验收范围与证据见下项，源码与v0.4.142标签已推送，GitHub Release已公开为Latest，匿名附件大小／SHA256及141生产更新器隔离检查／下载／校验通过；证据.runtime/release-0.4.142-public-verification.json、release142-updater-verification.json。

- [ ] 142排序／移动四段图文与鼠标锚点（2026-10-16）：按用户最新要求统一置顶／置底／上移／下移四段胶囊，底部仅c/v/f/w/s；上下移动快捷键与后台绑定字段移除，旧绑定归零；宽调色菜单非对称分配空间以贴近鼠标，调色面板跟随定位。289项后台回归、13项前端、原生生产显示／焦点／输入／取消自测、四主题预览及最终EXE烟雾／重启通过；实际入口与dist已切换142，原生组件／前端／PE版本一致，PS进程／文档／配置保留，实际上下移动绑定为空。证据.runtime/caps142-final-verification.json、caps142-final-pending.json、tests142-final.log、smoke142-final.log、move142-previews.json。实体新布局／点击／鼠标锚点验收待确认，不以预览和自动测试勾选。

- [x] PS25 菜单可用实机确认：2026-10-16 用户实机反馈“25已经可以了”：PS25 菜单可用／不唤出问题按用户确认闭环。此前报告和测试保留为历史证据；该机本次具体后台版本、故障根因及图层调色／输入保护／取消等完整交互矩阵未逐项确认。

- [x] 141正式发行：源码、标签及Latest已公开，统一ZIP与SHA256匿名下载验证通过；dist与实际后台入口141，UXP14，旧发行物已归档保留。PS25／RDP实体提示验收仍单独待确认。

- [x] 141诊断倒计时代码／构建／实际入口：后台和PS前方提示已接入5秒准备、真实采样进度及完成状态。287项后台、13项前端、最终EXE烟雾与重启通过；实际141后台浏览器倒数5→4、10样本和导出通过。原生独立窗口验证可见绘制、焦点不变及点击穿透；PS进程／文档／配置保留。证据.runtime/countdown141-browser.json、caps141-final-verification.json。
- [ ] 141 PS25／RDP实体提示验收：新电脑更新后确认提示位置、倒计时可读且不抢焦点；自动窗口测试不代替该机实机验收。
- [x] PS25 RDP报告核对：用户上传2026-10-15 20:30:56至20:31:06报告，实际后台137／PS25.5.1／UXP14；多次捕获可见PhotoshopMenuImGui且root-owner为PS，确认该次菜单已打开。面板为generic/fallback，137未含138新增的PS25签名；报告无trigger_decisions，不能推断输入／弹窗保护或非RDP触发已通过。仅只读解析附件，未修改新电脑。

- [x] 140发行：源码／标签与公开Latest140，匿名附件大小／SHA256及139生产更新器检查／下载／校验通过；实际入口及dist为140，原PS进程／文档保持，UXP仍14。证据.runtime/caps140-final-verification.json、release-0.4.140-public-verification.json及release140-updater-verification.json。远程兼容开启后的实机验收继续保持未完成，不据发行完成勾选功能整体验收。

- [ ] 140远程兼容交付：用户已明确授权默认关闭的收窄方案，并补充同机RDP模式可唤出。代码已加入菜单设置开关、旧配置默认关闭迁移及两秒内同一有效PS工作区注入左键配对；不修改注入Alt+Tab／任务栏过滤，输入／弹窗及交互保护保持。284项后台及12项前端回归通过；原生窗口范围回归及最终EXE烟雾／重启检查通过，实际入口140、两原生组件／前端资源哈希一致，PS4024及文档／原配置保持。开关关闭的真实注入Shift+Z仍activation_not_qualified（.runtime/remote-0.4.140-off-verification.json）；开启状态的实机自动对照因前台窗口被切换未通过，临时文档／窗口／设置均在finally恢复，不虚报成功。向日葵PS25实机结果尚未验收。

- [ ] 向日葵远程菜单激活兼容：用户确认Shift+Z仅在远程新电脑无法唤出。v0.4.139本机PS24真实热键进程中，Windows注入点击及Shift+Z复现process_scope=true、activation_allowed=false、无hold_begin、菜单不打开，摘要activation_not_qualified；证据.runtime/remote-0.4.139-verification.json。临时文档／窗口、设置、工具及鼠标已自动恢复。尚未取得远程PS25真实事件，不能认定已修复。统一移除注入事件过滤的方案被自动审批拒绝，未改动运行代码；用户已明确授权默认关闭、仅有效PS工作区显式点击的远程兼容选项，140实现见上；不放宽全局Alt+Tab或弹窗／输入保护。

- [x] 139交付：已公开Latest，匿名附件／138生产更新器校验通过；实际服务、原生热键组件及资源均139构建，PS进程保持，自动测试结束后无busy／菜单／采集任务，原快捷键恢复。282项回归及最终EXE烟雾通过，证据.runtime/caps139-final-verification.json、shiftz139-native-live.json、release-0.4.139-public-verification.json和release139-updater-verification.json。PS25根因与完整实体视觉未验收。

- [x] 139 Shift+Z自动测试与分段诊断：原生自测覆盖左右Shift、重复、两种松键顺序、PS范围外；真实热键进程在自建窗口捕获4条当前绑定决定、范围外不弹菜单，原配置／窗口自动恢复（.runtime/shiftz139-native-live.json）。诊断期间记录绑定触发键，原生35秒到期、结束回调关闭，固定队列无钩子IO；报告带事件／激活快照／设置及摘要分段。生产菜单API可见／取消通过，但截屏不完整，不标记视觉验收通过（.runtime/shiftz139-popup-live.json）。PS25端到端仍未通过，不能据本机24自测称已修复。


- [x] 138发行已完成：源码／标签与公开Latest138，匿名附件及137生产更新器检查／下载／校验通过，实际入口138；证据.runtime/release-0.4.138-public-verification.json及release138-updater-verification.json。PS25实体触发／调色仍待该机复测。

- [x] 138 PS25图层签名代码与回归：用户报告首次给出完整空标签控件ID，PS25使用6092/6093输入及6094滚动条，旧PS24使用6093/6097及6098。新增独立完整签名；不按2/2/1/1数量放行。输入、画布、PSFloatC及缺项拒绝回归通过。280项完整后台回归、最终EXE烟雾通过，实际入口138与原生组件／前端一致，PS进程／文档／配置保持；证据.runtime/caps138-final-verification.json及tests138-final.log。PS25实体调色与触发仍保持未完成，不据报告匹配推断全部兼容。
- [x] PS25 Shift+Z不唤出进一步核对：用户确认本次按的是shift+z。报告显示PS前台正常、同权限、后台／UXP14／原生组件就绪、无busy；一次菜单与PSFloatC期间应保护，其余工作区可用。采样仅有Caps状态，缺少Shift+Z的原生按键决定；不能归因旧绑定或单凭图层识别说明整个菜单不出现。需结合按键同时段Native hook action／activation及Desktop action failed日志继续定位。（历史排查记录；2026-10-16用户确认PS25已可用，此故障关闭，根因未单独确认。）


- [x] 137发行：统一包及SHA256已公开为Latest，匿名下载大小／哈希通过；实际入口137及内置诊断资源哈希一致，原PS进程、文档、配置保持。旧版136生产更新器隔离检查／下载／校验137通过，未重复安装。证据.runtime/release-0.4.137-public-verification.json、caps137-final-verification.json。

- [x] 137内置诊断：后台运行状态一键启动只读采集，异步有界子进程及单任务保护，完成后导出完整JSON；打包到EXE并随应用升级，独立脚本继续可用。实际137后台浏览器验证10样本及导出、后台触发键／窗口链／权限／组件状态通过，PS进程与文档保持；证据.runtime/diagnostics137-browser.json及diagnostics137-browser-report.json。该机PS25的触发故障仍待报告定位，未标为修复。
- [x] 137 UXP安装跟随内置版：默认启用，后台约15秒检查／启动PS前检查；PS正常关闭、操作锁／busy／pending均安全时使用现有安装备份／校验／回退流程，仅升级更高版本。缺失／禁用／外改不自动修复，不降级；主动回退／卸载关闭跟随；失败同版本不反复安装，重新启用后才重试。读状态不占交互锁，键盘钩子与菜单唤出不等待检测。临时真实插件目录13→14、凭据保持／回退、运行中／未知保护／禁用／失败限制测试通过。279项后台、12项前端和最终EXE烟雾通过；实际137已部署，UXP仍14无需重装，PS与配置保持；跟随开关真实后台保存通过。真实PS25升级与完整实体输入矩阵仍待确认。


- [x] PS25面板／完全无菜单只读诊断脚本：tools/inspect-panels.ps1支持倒计时及多次采样，记录前台进程名／类／启用与最小化、权限状态、hover和focus父链、root-owner、面板控件ID、后台版本／触发键与MenuHost／HotkeyHost就绪；完整JSON默认桌面CreateNew保存，禁止覆盖已有文件。不读取输入、文档、凭据，不发送输入或UXP命令。Windows PowerShell5真实只读运行及两样本报告结构／隐私字段／本机图层ID核验通过；无Python可用。PS25实机报告仍待提供，此项不等于修复完成；Caps采样不能替代原生键盘事件日志。证据.runtime/panel-trigger-final.json。仅独立脚本更新，136已发布附件保持。
- [x] PS25完全不唤出菜单：用户最新确认不是“通用操作”而是完全没有菜单。提供的52行日志前台Chrome_WidgetWin_1、process_scope=false、allowed=false，无Caps按下／松开事件；钩子健康、丢弃0，仅能确认该段没有有效PS触发资格，尚不足以断定PS窗口类或权限原因。随后用户截图显示实际触发键为shift+z，须先按该键或改回capslock验证，不能把按错绑定视为PS25不兼容。需新版诊断确认实际PS前台进程与窗口链，并结合按Caps时的日志定位；不能用面板签名修复代替触发链验收。（历史排查记录；2026-10-16用户确认PS25已可用，此故障关闭，根因未单独确认。）


- [x] 136正式Release：源码／标签、统一ZIP与SHA256已公开，Latest为136；两份附件匿名下载哈希／大小核验通过（.runtime/release-0.4.136-public-verification.json）。dist整理为136，生产更新器以隔离133状态检查／下载／校验136通过，未安装或修改运行服务。旧发行物已保留归档；本机实际后台已切换136，原F已核对取消终态，PS正常重启及UXP14重连通过；PS25.5.1实体确认仍待完成。

- [ ] 136通用操作移除移动功能：代码已隐藏顶部／底部、压缩底部字母栏为c/v/f/w/s并修正收紧后的tooltip分组边界，旧命中及移动请求拦截；图层菜单恢复全移动入口，独立PS组合键保留。272项回归、原生完整显示／输入／焦点／中心取消及通用与图层四主题预览通过（.runtime/tests136-final.log（包含原生自测）、.runtime/generic135-*.png及generic136-tip-*.png）；候选EXE烟雾及隔离统一包通过（.runtime/smoke136-final.log、.runtime/caps136-final-pending.json）；实际入口已切换136并核对组件；完整实体菜单布局／手势验收仍待完成。
- [ ] PS25.5.1中文新机无标签面板：用户反馈labels为空、ComboBox/Edit/ScrollBar/PSViewC数量2/2/1/1；文字归一化不足以覆盖。新增完整原生ID签名路径，严格匹配本机测得的图层控件组，不因类名／数量放行；无文本读取、UIA／COM查询或新轮询。只读诊断新增控件ID输出；该机ID与实体结果仍待核对，不能标为兼容已确认。


- [ ] 新电脑 PS25.5.1 中文图层面板识别（2026-10-15）：用户确认在图层面板唤出仍为“通用操作”，已确定面板分流未识别，0.3.13本身包含调色功能。源码补充全角／助记键／不可见标记归一化、自有浮动OWL.Dock链及最近Palette选择，严格保留三字段签名和输入／焦点保护。269项后台回归通过；新增无需Python的只读tools/inspect-panels.ps1在本机Windows PowerShell5运行通过，只输出控件类及白名单标签／版本，不读取输入内容或凭据。含修复的候选EXE烟雾及统一ZIP／内嵌UXP14验证通过（.runtime/smoke134-panels.log）；打包工具支持隔离候选输出，避免覆盖正在使用的dist。修复已随136切换实际入口，新机签名和实机效果待核验；不能声称跨机问题已解决。
- [x] 134源码／标签／发行草稿已推送：269项回归及候选EXE烟雾、隔离统一包通过，GitHub草稿413449016包含ZIP与SHA256两份附件，远端大小／哈希一致（.runtime/release-0.4.134-draft-verification.json）。此项仅表示草稿备妥；实际运行切换／正式发布未完成，原F请求accepted且窗口仍开，未知结果保护保留；公开Latest仍133。
## 2026-10-15历史进展（当时入口v0.4.136／UXP0.3.14；当前优先事项见顶部）

- [x] 旧F阻塞恢复、136实际切换与PS重启：用户授权其他自动化后，严格匹配PS原生Layer Style及Cancel按钮，定向BM_CLICK取消；原请求04e983a697aa60c93e62938c51601aaa返回completed/cancelled，历史原任务核对后解除busy，未重发或清除未知记录。备份配置／旧EXE后切换实际136，菜单／热键／前端哈希一致；正常关闭PS，经后台桌面COM助手启动新进程，UXP0.3.14就绪，两份保存文档重新打开。临时文档中原生窗口等待50.266秒、重复F拒绝、取消保留身份／数量／历史、后续实际标签读回通过；六种Caps命令、一次撤销及旧目标拒绝通过。证据.runtime/style134-original-final.json、caps136-final-verification.json、restart136-verification.json、style136-production-verification.json、release-0.4.136-verification.json、caps136-live.json。长等待测试空闲WS在后续请求前关闭，另建连接后验证通过；完整实体手势与新机矩阵单独待验收。
- [x] 134参考Caps命令／底部分段按钮与实际入口：C/V复制粘贴样式、F原生样式、W/S上下选层、Ctrl+W/S上下移动一层；菜单打开可直接按字母，字母按钮hover显示动作名。默认冲突绑定迁移为空、自定义保留，调色字段保留输入。266项后台、12项前端／脚本及原生热键／菜单自测通过；真实PS六命令、样式一次撤销和过期目标拒绝通过，证据.runtime/caps134-live.json。F窗口打开／取消核验与完整实体按键矩阵单独记录。
- [x] 134快速切换形状的只读确认：每次图层菜单唤出使用独立无修改请求，在确认前不冻结旧图层为可执行目标；响应按当前请求ID／菜单代次／会话匹配，关闭／输入保护／草稿不能被迟到响应覆盖。24次真实PS普通层／形状层快速交替，目标与编辑器可用状态全部正确，中位197ms、P95 241ms；仅只读往返，不冒称按键到首帧。证据.runtime/shape-switch134.json。
- [ ] 134完整实机键鼠验收：底部按钮布局／hover、菜单内字母及直接Caps组合、RGB／Hex输入保护、快速切层后调色、中心／空白／分隔线取消和右键重开；键鼠工具本轮初始化失败，不能用模拟自测替代。136发布与公开附件校验已完成；该项实体交互验收仍待完成。

- [ ] 评估Photoshop非输入场景强制切换ENG键盘（2026-10-15，第一阶段评估已完成，自动切换未实现）：避免中文输入法与菜单／PS快捷键冲突。文字工具编辑、图层重命名以及其他文本／数值输入、搜索和输入法候选／组合输入等场景保留正常输入；其余PS操作场景评估自动切换英文键盘。先核验原生／自绘输入框、面板与对话框的焦点识别，以及ENG键盘布局和中文输入法英文模式的区别；确定进入输入场景时的恢复策略与未知焦点保护。限定PS前台，不影响其他应用；验收覆盖中文输入、重命名、快捷键、跨应用切换，且不能引入指针闪动或快捷键响应延迟。
- [x] ENG第一阶段评估：核对现有标准输入保护／100ms刷新及Windows／Adobe官方接口；本机已有英文布局，只读2000次GUI焦点＋布局配对读取中位4.5µs、P95 8.4µs。结论为系统切换能力可用，但所有PS自绘输入／重命名排除尚不能保证，异步首键存在竞态；暂不启用强制切换，下一步应只读观察。详见[评估记录](docs/INPUT-LANGUAGE-ASSESSMENT.md)；真实切换、完整输入矩阵及实体快捷键延迟尚未验证。

- [x] 133后台主题／文字统一：跟随PS四种主题，复用菜单灰色／蓝色hover与选中；字体、输入／按钮留白、标签、状态、分段页签共用指标，移除四份旧样式覆盖。主题来自现有状态消息，断线保留，重复心跳不改样式、不增加PS查询。260项后台与8项前端测试、72个隔离渲染页面／草稿／键盘／中英文检查通过。实际后台133、原生菜单／热键及UXP保持；更新未重启PS。实际服务的六页及插件页签自动浏览器只读检查通过；最终EXE烟雾通过。证据.runtime/admin133-browser.json、.runtime/admin133-live-browser.json、.runtime/tests133.log、.runtime/smoke133-final.log、.runtime/admin133-verification.json。
- [x] 133后台实体配色／排版确认：2026-10-15用户刷新实际后台后确认“配色与排版正常”，主题、文字比例及控件留白验收通过。四主题与窄窗口的完整实体矩阵、选中／禁用及PS分流／取消仍单独待确认，隔离浏览器渲染不替代这些项目。

- [ ] 渐变条样式待细化（2026-10-15）：用户复测反馈“渐变条样式需调整”，随后明确“先写入TODO和看下TODO，我要思考下”。本轮仅记录，等待用户确定条的尺寸／比例、色标外形及下方调色区等具体方案后再实施；这些方向尚未选定，不推断为已授权的设计。当前渐变条和执行逻辑保持。

- [x] 132渐变条代码／构建／实际入口：连续颜色预览及色标，替换下拉和位置输入；点击选节点、拖动位置并限制相邻顺序，选择不改草稿，仍单次提交。260项回归、原生真实控件模拟拖动／边缘／相邻限制与原取消矩阵、四主题／三字号通过；UXP不变，无需重启PS。证据.runtime/native-color132.json、.runtime/tests132.log、.runtime/gradient132-verification.json。
- [ ] 132渐变条实体确认：当前样式未获通过；后续按确定方案调整后，再确认点击／拖动色标、选中色标调色、Caps应用及中心／右键取消。此次反馈未逐项确认操作，不以131确认替代新组件验收。

- [x] 131调色面板统一控件：复用菜单面板正文缓存字体，24行高、6面板留白／行距、9输入文字留白，共享边框／主题／圆角；按类型收紧高度，保持调色与执行规则。260项回归和原生四主题／三字号／显示与取消检查通过，实际入口／组件核验见.runtime/ui131-verification.json。
- [x] 131实体外观与调色操作：用户确认“样式与操作都正常”，字体／留白、复制、松Caps应用及取消通过；130曾被指出字号问题，其旧确认不替代此次131验收。

- [x] 130调色范围／字号：图层面板中单选未锁定形状和普通纯色／渐变／图案填充层；普通填充层不提供形状描边／交换并保持类型，其他类型置灰，原生／后端拒绝非该范围。文字统一PS字号95%，共享指标。260项回归、6项前端、原生显示／取消／焦点／设备重建及13项真实PS颜色／一次撤销／身份／顺序验证；证据.runtime/tests-0.4.130-final.log、.runtime/shape-live-0.4.130.json、.runtime/fill130-backend-verification.json。
- [x] 设备渲染选择：自动／GPU优先／CPU WARP，默认硬件优先并支持软件回退；后台保存后菜单收起时切换，显示实际设备；真实服务五次切换保持同一菜单进程，恢复原配置。证据.runtime/render-live130.json。GPU／CPU短期渲染与设备重建对照详见PERFORMANCE.md。
- [ ] 后台渲染下拉外观与GPU／CPU切换后的实体鼠标体验；完整图层／面板分流矩阵、长期稳定性及其他设备兼容性仍未实测。

- [x] UI默认指南：docs/UI-GUIDE.md汇总共享指标、主题、字体、布局、提示、图标、颜色编辑及验收规则；AGENTS.md明确后续优先遵循。颜色控件统一24／6／4／1指标，勾选12，复制反馈不跳宽。
- [x] Hex一键复制：当前纯色／选中渐变节点转换为大写#RRGGBB，真实写入后读回核对；不修改草稿、PS或撤销，失败反馈在下方。后端和原生两层限制只在图层面板，画布／其他面板／输入保护隐藏；非该范围不能提交颜色修改。257项回归、四主题渲染及9项真实颜色／撤销／目标保护通过，用户实机确认“样式、复制和分流都正常”。证据.runtime/tests-0.4.128-final.log、.runtime/color128-final-verification.json、.runtime/shape-live-0.4.128.json。
- [x] 颜色模块性能：四主题原生CPU绘制、4／100图层的交替inspect比较和缓存校验、六种完整PS颜色事务已测量；拖动及复制不向PS提交请求。范围与数据详见PERFORMANCE.md，工具tools/benchmark-shape-editor.py、原生--benchmark-shape-output。CPU／通信／完整事务不等于实体按键到屏幕延迟。

- [x] 应用图标改为用户EXE的手写PS／MENU风格，保留深蓝／蓝紫配色，下方AHK改MENU，角标A改M；可编辑矢量与来源native/app-icon-design.json，统一SVG／PNG／9尺寸ICO。6项前端与7项图标／服务测试，实际资源与组件验证见.runtime/icon127-final-verification.json；保留文档和配置，不重启PS。
- [ ] 新手写图标Windows任务栏／窗口实体外观核验；源图与生成图已检查，HTTP／PE／ICO资源核对不等于实体截图验收。

- [x] 形状颜色代码／构建及真实PS事务验证：RGB 8／16位单选未锁定矢量形状，自动读取填充／描边／类型，Tab切换纯色／渐变／图案，RGB／Hex、节点位置、黑白、宽度、交换；本地草稿后单次提交。255项回归、原生生产显示／键鼠状态机自测、四主题预览，真实PS颜色读回／矢量路径及顺序／一次撤销／失效目标拒绝通过。证据.runtime/tests-0.4.126-final.log、.runtime/shape-live-0.4.126.json；tools/verify-shape-paint.py仅操作自建临时文档。
- [x] 形状颜色实体验收：2026-10-14用户确认“显示、应用和取消都正常”，已切换最终126／UXP0.3.12，内置插件、实际EXE和原生组件哈希一致，配置凭据保留。证据.runtime/shape-live-0.4.126.json、.runtime/shape126-final-verification.json；完整输入／焦点／分流矩阵仍单独待验收。

- [x] 最新指针复测：124的禁止ImGui全局改指针策略保留，但用户随后更正“闪动没有消失”，不再把124记为症状解决。125／UXP0.3.11取消状态路径定时JSX，主题／字号／文档从UXP心跳读取，未就绪回退只读COM原生属性、不执行脚本；250项回归、四主题／三字号／无JSX测试、最终EXE烟雾和真实十次连续创建通过。正常重载UXP，文档恢复副本保存并重开；用户最新实机确认“不再闪动”。证据.runtime/tests-0.4.125-cursor.log、.runtime/smoke-0.4.125-cursor.log、.runtime/status-performance125.json、.runtime/consecutive-creation-0.4.125.json。
- [ ] 大图层数量性能：整树ID／类型／拓扑批量读取及同modal复用仍待完整连续创建对照；已测过的选择元数据get／multiGet／事件刷新未达到替换门槛，生产保留原实现。
- [ ] 当前版本完整实体输入／焦点／hover分流／取消矩阵及按键到首帧耗时；其他面板的独立识别没有完整实现，不能把所有属性／历史／工具等面板声称为已支持。
- [ ] 后台最终留白、窄窗口及新应用图标实体视觉确认；全新Windows用户与更高PS版本离线生命周期实测。
- [x] 2026-10-15发布v0.4.133为GitHub Latest：统一后台ZIP及SHA256两份附件公开匿名下载校验通过，ZIP SHA256为7f3f977edf09b5270738d591145b27084a537f77ed91eb6d0594654e5ab2a824，内置UXP0.3.13。沿用生产更新器、在隔离数据目录报告当前120，真实检查／下载／校验133通过，未再次安装或改动用户后台。EXE与本机实际入口一致，源码／v0.4.133标签及ENG评估TODO已推送。证据.runtime/release-0.4.133-public-verification.json、.runtime/release133-updater-verification.json、.runtime/release133-preflight.json。历史验收范围保留，未完成实体矩阵不因发布勾选；dist保留本次已公开原件，不重新生成覆盖。

## 后台应用图标（v0.4.123，2026-10-14）

- [x] Ps＋菜单徽标：保留应用配色，SVG／PNG／9尺寸ICO由同一几何生成，字样转路径；后台Logo和浏览器图标增加资源标记以加载新版。同步EXE、窗口、托盘所用ICO。
- [x] 6项前端测试／构建、7项图标加载／窗口修复／实际服务身份测试、最终EXE烟雾／重启通过。实际入口123、HTTP和内嵌图标、原生菜单／热键父进程与组件哈希一致，文档／配置／凭据保留，未重启PS。证据.runtime/icon123-final-verification.json、.runtime/smoke-0.4.123-icon.log；可编辑SVG在frontend/public/icon.svg，生成器tools/generate-icons.ps1。
- [ ] Windows任务栏／后台窗口新图标实体视觉验收：浏览器／键鼠工具前两次初始化失败的限制仍在，资源校验不等于实体截图验收。123为本地构建，公开Latest120不变。

## 后台简化与留白统一（v0.4.122，2026-10-14）

- [x] 六个主导航页，应用更新独立入口；状态与插件常用操作优先，启动／连接／维护详情折叠，菜单与图层快捷键集中管理。设置草稿跨页保留，只在有修改时显示全局保存栏；成功通知5秒收起，错误保留。
- [x] 统一正文13px／20px、按钮与输入36px、4px圆角、卡片20px／小窗口16px、表单间距16px、徽标／checkbox／标题对齐；宽屏内容最大960px。保留现有配色与单应用分页、仅活动区域滚动。
- [x] 248项后台回归、6项前端测试／构建、最终122 EXE烟雾／重启及原生自测通过。正常退出121、备份并替换实际入口122；实际服务／dist／原生组件路径父进程和哈希、HTTP CSS／JS与构建逐字节一致，文档／配置保留，UXP0.3.10启用且与内置一致，未重启PS。证据.runtime/admin122-final-verification.json、.runtime/admin122-deployment.json、.runtime/tests-0.4.122-ui.log、.runtime/smoke-0.4.122-final-ui.log、.runtime/native-selftest-0.4.122-ui.log。
- [x] 中间121真实后台浏览器：跨页草稿、保存362及恢复360、键盘切换快捷键页、应用更新独立页、插件禁用反馈通过；浏览器工具中断后通过同一后台API恢复启用，最终122状态核验启用。此前121预览517×711／1120×820布局和中英切换已检查，不能替代最终122视觉验收。
- [ ] 最终122留白与窄窗口视觉确认：浏览器工具两次初始化失败（HRESULT0x80070020），未使用未授权UI替代路径，未标为视觉通过。实体PS hover分流／输入保护／焦点限制／中心取消／右键重开矩阵沿用未验收项；本轮未改原生操作逻辑。
- [ ] 122排版随本地123保留，尚未推送或发布；GitHub公开Latest120与附件不变，当前dist为本地123统一包。

## 读取策略、特殊图层与数量实测（2026-10-13）

- [x] 缓存DOM、定向get、异步multiGet使用相同最小选择元数据，比较完整十次连续创建而非孤立getter；三种共享实时目标／模式／位深校验、原生创建、整树／位置读回、一次撤销及最终结果持久化。基础每种6轮，特殊类型及数量每种3轮、轮换先后，2100次正式计时创建；预热与失败不完整序列排除，P95小样本限制明确。
- [x] 真实矢量形状、纯色、渐变、文字、嵌入智能对象、曲线、色阶、带蒙版像素：8项读取一致性、168项原生位置／默认名称／0移动／原内容保存／一次撤销、24项旧目标拒绝。数量补测形状及混合10／100／300节点、文字／智能对象100节点，真实数量及完成新选择核验。矢量路径／特殊内容在创建后及撤销后都读回，原生打组成员独立检查。
- [x] 事件＋1秒兜底与原轮询各6轮连续创建对照；两种通知注册格式、选择／锁定／新建／删除／撤销重做／8与32位转换共40项严格文档ID／新时间戳观测。空闲读取20→2次，但脚本路径未收到通知、反馈约1秒，且连续创建提升仅约0.4%，保留原轮询；没有将脚本事件观测冒称实体鼠标验收。
- [x] 120项模拟能力／异步选择变化／缺字段／过期及文档范围检查，实验与生产主脚本的持久接受、补写结果不重跑、去重、未知保留回归通过。失败任务／未完整序列及首次工具错误保留记录；大规模COM准备后的只读就绪屏障放在计时外，不自动重发修改。
- [x] 没有候选达到各场景中位至少缩短5%且P95不恶化超过5%的替换门槛，生产保持DOM／原刷新。临时0.3.11经后台同一UxpManager流程四次回退至内置0.3.10、正常PS重启；两份文档或恢复副本及原活动文档恢复，原配置凭据保留，实际120 EXE／菜单／热键／内置插件与dist哈希一致。恢复后真实十次连续创建及三项丢失结果读取／桥重建核对再次通过，每项修改只发布一次。完整证据、表格、样本范围与复现见docs/READ-STRATEGIES.md；实验文件代号121不代表发布121，公开Latest仍120。
- [ ] 根据量增阶段数据继续测整树ID／类型／拓扑的批量读取及同modal复用；保留全部原层／数量／顺序／位置／类型校验、一次撤销、实时目标与未知不重发，再以完整连续创建决定。此次选择元数据AM实验不等于已经测过整树AM替换。
- [ ] 实体按键到菜单／提交的分段耗时、完整焦点／输入矩阵、复杂大像素文档和更高PS版本仍未测；300节点简单／重复特殊内容不能当成所有大文档验收。

## v0.4.120 GitHub应用更新

- [x] 独立应用更新tab、中文／英文、每日检查可关闭；只检查正式Release，不自动下载／安装，不混用旧JSX更新。
- [x] 匿名真实118统一包下载、GitHub digest／ZIP／EXE SHA256及PE版本校验通过；线程下载，不阻塞热键。证据.runtime/github-update-source.json。
- [x] 实际入口119→120独立EXE助手正常退出／等待父进程／备份／替换／重启通过，配置凭据／PS文档／UXP0.3.10保留，原生组件哈希一致。证据.runtime/installed-0.4.120-verification.json。
- [x] 最终120已正常切换实际入口，EXE／菜单／热键／UXP哈希及原设置凭据核验；248项后台回归、6项前端测试／构建、最终EXE烟雾／重启、真实PS十次连续创建通过。自动化接口与真实键鼠验收分开记录。证据.runtime/installed-0.4.120-final-verification.json、.runtime/tests-0.4.120-final.log、.runtime/frontend-tests120.log、.runtime/smoke-0.4.120-delivered.log、.runtime/consecutive-creation-0.4.120.json。
- [x] 发布120为Latest，功能提交63fbcd7／v0.4.120标签已推送，两份公开附件匿名下载哈希一致，ZIP SHA256：9dfc63abbba56f7db83a4ac41777b495e53037506060274acd7a788575ddce6b。隔离较低版本私有引导构建通过真实界面检查／下载／校验／安装／自动重连，设置／偏好／安装归属／旧EXE备份通过，中文／英文控件正确；不冒称公开119支持更新。初次检查早于120公开，正确未提供草稿升级，公开后检查才提供120。证据.runtime/release-0.4.120-public-verification.json、.runtime/github-update120-gui-lifecycle.json、.runtime/github-update120-installed.png、.runtime/github-update120-english.png。119性能功能随120发布；118公开附件不变。
- [ ] 完整实体输入／焦点矩阵、全新Windows账号和更高PS版本沿用未验收项。

更新日期：2026-10-13。

## v0.4.119历史交付与剩余验收

- [x] v0.4.119／UXP0.3.10优化实际运行：单次属性／父组／兄弟列表复用、排序两端提早确定、刷新完成后节流；同modal准备新选择、结束核对身份后复用，变化／读取失败退回完整读取或未知禁用。原目标／类型／锁定／模式／位深检查及结果持久化不省略，不排队、不重发。
- [x] 本轮重新测量v0.4.118基线与最终v0.4.119，固定有内容中间层、同机64×64文档、每路径10样本：空白285.98→200.85ms（29.8%）、曲线410.92→305.84ms（25.6%）；含完成前新状态准备。详细阶段和范围见PERFORMANCE.md，不冒称实体按键延迟。
- [x] 378种原／新排序能力一致性、快照内属性复用／下轮锁定新值、慢读不立即连刷、prepared身份变化回退、元数据失败不撤销成功创建、去重／结果写盘重试测试通过。236项后台回归、原生自测、最终EXE烟雾／重启通过。
- [x] 最终真实PS十次无额外等待连续创建（含组）、39项类型／标签／排序／锁定／跨组／旧目标拒绝、8项空白默认名／位置／一次撤销、三项未知结果核对通过。早期119候选40项填充／调整对照通过；不把早期候选冒称最终实体验收。实际入口／菜单／热键／内置插件哈希一致，配置凭据保留，正常服务退出和PS新会话重载、文档恢复副本重开。证据.runtime/benchmark-0.4.119-prepared.log、.runtime/consecutive-creation-0.4.119-prepared-ten.log、.runtime/layer-context-0.4.119-prepared.log、.runtime/blank-layer-0.4.119-prepared.log、.runtime/recovery-0.4.119-prepared.log、.runtime/tests-0.4.119-final.log、.runtime/smoke-0.4.119-prepared.log、.runtime/installed-0.4.119-verification.json。
- [x] 用户于2026-10-13实机确认最终119方案“更顺畅，操作正常”：连续创建／立即重开／选中组新建空白层复验通过。较早候选的“体感差别不大”保留为中间反馈，不代替最终确认。
- [ ] 完整实体输入／焦点矩阵、全新Windows账号及更高PS版本沿用118未验收项；长期驻留、多DPI等后置。
- [x] v0.4.119本地统一包已生成，嵌入资源与源码逐字节一致、Windows版本元数据及实际入口核验通过；包内保留许可和离线说明。
- [x] v0.4.119优化随v0.4.120 GitHub Release一起发布，公开Latest为120；不单独建立119 Release。

## v0.4.118交付与剩余验收

以这一节作为当前优先清单。下方各版本未勾选记录保留原始证据，不要求逐版本重复旧布局／旧字号验收；当前集中回归不能冒称历史版本本身通过。长期驻留、多DPI、资源增长和复杂工作流仍按用户要求后置。

- [x] v0.4.118／UXP0.3.9创建优化已运行：复用冻结单选身份，PS内修改前重新检查；省去无关排序能力／缓存刷新及有内容层临时层，空白／未知像素保留保护。40项填充／调整与8项空白层位置／默认命名／原层顺序／一次撤销、PS内旧目标拒绝、模拟结果读丢核对通过。用户确认无挪动、组内位置正常及间隔改善；后续立即重开旧目标故障补充修复已运行，用户实机确认连续创建正常、无无效点击，完整执行耗时见PERFORMANCE.md同机固定中间层、旧新各10样本基准。236项回归、原生自测和最终EXE烟雾通过；组件／配置凭据核验及正常PS新会话重载通过。证据.runtime/creation-performance-0.4.117-fixed-anchor.json、.runtime/creation-performance-0.4.118.json、.runtime/created-layers-0.4.118-live.json、.runtime/blank-layer-0.4.118-live.json、.runtime/recovery-0.4.118-photoshop-live.json、.runtime/installed-0.4.118-verification.json。
- [x] 修复保护层DOM先到顶部再移回：改为原生位置创建；空白／纯色／调整不再事后移动，40项填充／调整及8项空白实测placement_moves全部0，空白保护／原生命名／一次撤销通过。选中组空白按用户最新选择在组内直接创建。用户确认普通／空白像素层新建曲线及纯色无挪动；用户确认组内位置正常、间隔改善；后续立即重开故障补充修复已获用户实机确认，不以代码计数冒称实体视觉通过。证据.runtime/created-layers-0.4.118-group-ready.log、.runtime/blank-layer-0.4.118-group-ready.log。
- [x] 修复创建完成后立即重开冻结旧图层：完成前准备新选择快照，最终结果直接同步后台菜单缓存；236项回归、八次真实连续创建无等待／轮询／重发、三项结果读丢／执行桥重建核对通过。最终EXE烟雾、正常服务／PS重载和组件／配置哈希核验通过。证据.runtime/consecutive-creation-0.4.118.json、.runtime/recovery-0.4.118-photoshop-live.json、.runtime/tests-0.4.118-final.log、.runtime/smoke-0.4.118-final.log。
- [x] 用户于2026-10-13实机确认组内位置正常、间隔改善，并在补充修复后确认“连续创建正常，没有无效点击”；立即重开实体复验通过。完整输入／焦点矩阵另列，未混同验收。
- [x] docs/CONTEXT-COVERAGE.md明确识别范围；其他面板、自绘编辑器和全部菜单状态不冒称已识别或验收。
- [x] UXP结果核对与不自动重发已运行；后续ZIP导入键盘访问和搜索／通知配色已完成并运行v0.4.117／UXP0.3.8。
- [x] 实际后台Tab到达导入按钮、可见焦点、Enter／Space打开选择器、重复选同一ZIP及校验预览通过；不点击安装。共享按钮也用于旧JSX导入。证据.runtime/controls117-browser-verification.json、.runtime/controls117-upload-proof.png。
- [x] 原生搜索选择／文字选中／按下态统一主题蓝色，通知正文跟随主题；四主题文字与选中／按下／通知背景对比度自测及12份预览通过。原分流、输入焦点、中心／空白／右键取消等原生自测与232项回归、5项前端测试／构建、最终EXE烟雾／重启持久化通过。证据.runtime/native-selftest-0.4.117.log、.runtime/controls117-previews.json、.runtime/tests-0.4.117.log、.runtime/smoke-0.4.117.log。
- [x] 当前真实插件部署目录经实际后台完成安装／同版本ZIP更新／禁用启用／卸载／回退；文件和凭据恢复，正常PS重载新会话，恢复文档副本重新打开。真实PS35项填充／调整位置、8项空白层默认命名／位置／一次撤销对照及生产EXE标色／清除通过；执行端实测不替代实体菜单点击。证据.runtime/uxp-management-live-0.4.117.json、.runtime/all117-lifecycle.log、.runtime/created-layers-0.4.117.log、.runtime/blank-layer-0.4.117.log、.runtime/production-0.4.117.log。
- [x] 禁止网络调用的两份隔离文件环境首次部署／更新／禁用启用／卸载回退与独立凭据通过，使用实际安装代码和本机PS版本文件检查；关闭宿主条件为模拟。证据.runtime/fresh-profiles-0.4.117-verification.json。此项不冒称新Windows账号或跨机实测。
- [ ] 当前完整实体CapsLock／hover分流／输入框／跨应用焦点／取消及恢复反馈视觉确认；Windows键鼠工具两次0x80070020初始化失败，停止重试，已请求用户确认，未将预览和接口当作通过。
- [ ] 全新Windows用户与更高PS版本完整离线安装／加载／卸载回退实测；本机仅mianx用户和PS24.0.1，没有对应环境，文件隔离测试不替代。
- [x] 最终统一包已构建，功能提交8377b65／v0.4.118标签已推送，GitHub Release已发布为Latest（包含未单独发布的v0.4.116／117功能）。两份公开附件匿名下载哈希核验通过，ZIP SHA256：f5ad7e94f0389aff40c83d08fa9fbf62a32f5e005ed36c6c3ddf27ba169c6b1e。旧v0.4.115发布附件不变；116／117本地构建包可恢复归档到.local/archive/dist。证据.runtime/release-0.4.118-public-verification.json、.runtime/118-release-dist-archive.json。

## v0.4.116 UXP超时／断线后的结果核对（UXP 0.3.8）

- [x] 修改提交前持久保存无凭据的请求ID／会话／操作／原任务ID／固定插件目录；插件修改前保存接受记录、完成后保存对应最终记录。结果写盘失败只重写结果，不重跑PS命令；插件重载根据已消费ID和会话拒绝重复执行。
- [x] 后台定期及手动只读核对原结果：匹配明确最终结果后更新同一任务、持久化再解除busy；恢复历史任务仍不可取消／执行。缺记录、会话不符、损坏记录、历史缺失及回滚未确认均保持阻止修改；后台重启不清除未确认请求，不COM回退。
- [x] 10项恢复边界测试与232项后台回归、前端5项测试／构建、插件结果写盘重试／重载去重测试通过。真实PS临时文档中模拟后台收不到结果：红色标签最终核对成功，空白层在重建后台执行桥后核对成功；每项仅提交一次修改，同一任务ID／请求ID保留，空白层只增加一层并一次撤销恢复。证据.runtime/recovery116-photoshop-live.json；不把模拟丢失读取称为真实进程崩溃或物理键鼠验收。
- [x] 最终EXE烟雾／重启持久化、原生菜单分流／输入焦点／取消自测、最终232项回归通过；正常退出旧服务并更新实际入口v0.4.116，UXP0.3.8更新重载。实际服务／菜单／热键组件与dist哈希一致，配置和凭据保留，文档恢复副本重新打开。最终真实PS恢复测试及生产EXE标色／清除读回通过。首轮烟雾端口15154被其他服务占用，改用空闲15163后通过；最终重载检查曾早于COM注册，等待正常启动完成后通过，未强制退出。证据.runtime/installed-0.4.116-verification.json、.runtime/recovery116-photoshop-live-final.log、.runtime/recovery116-production-final.log、.runtime/smoke-0.4.116-final.log、.runtime/native-selftest-0.4.116-final.log、.runtime/tests-0.4.116-final.log。当前构建已运行，未创建或发布新的GitHub Release。
- [x] 分发校验拦截旧PE版本元数据后修正Windows FileVersion／ProductVersion为0.4.116，重建并再次正常切换实际入口；最终交付EXE／运行入口／资源哈希一致，实际菜单／热键已就绪，最终生产读回和烟雾／重启再次通过。后台手动tasks.reconcile接口通过、原任务列表和请求文件未改变。统一离线ZIP已生成，SHA256 d85709cb0f8207d2e430a17604132eb149df7170ba5ac15de48c5b931fe222ec；v0.4.115已发布附件原样保留在.local/archive/dist/v0.4.115-published，未覆盖远端。最终证据.runtime/recovery116-delivery-verification.json、.runtime/smoke-0.4.116-delivered.log、.runtime/recovery116-production-delivered.log。
- [ ] 新增核对按钮及原生底部“待核对／已核对”反馈的实体视觉确认；保留输入保护、画布／图层分流、焦点与取消回归。不以源码桥实测代替这一项。

## 目录整理（2026-10-12）

- [x] 目录整理提交c20eb84已推送master，现有v0.4.115 Release已补充整理后的源码ZIP／SHA256与说明；两份新增附件匿名下载哈希核验通过，原标签、程序附件ID／大小／digest及下载链接不变，Release仍为Latest。源码ZIP SHA256：745e329e6625f613648de01e92df90409e28892485a62a9f7733536f4315a0fd。证据.runtime/organization-release-verification.json；README加入下载链接。

- [x] 旧独立 Python／JSX 实验集中到legacy；保留当前入口、COM兼容模块和仍被后端引用的photoshop_action。
- [x] dist仅保留当前v0.4.115；旧发行包、诊断及配置／文档恢复资料可恢复归档到.local，原Figma ZIP／JSON和实验PSD分别保存到design／recovery。公开前Git历史备份在.local/recovery/git-history/private-history-backup.git。删除可重新构建的旧缓存，保留现用Python环境；锁定的capslock-history日志留原位。归档索引.local/archive/organization-manifest.json。
- [x] 整理后222项源码回归通过；13份图标生成输出哈希不变，当前发行ZIP哈希不变；实际服务仍为同一PID的v0.4.115、UXP0.3.7，dist与已安装EXE及实际菜单／热键资源哈希一致，数据目录保留，未重启PS或服务。1909项归档逐项核对源已移除、目的地存在；按文件长度统计净减少约597MiB，旧发行包／原稿归档不计释放空间。证据.runtime/organization-verification.json、.runtime/organization-tests.log、.runtime/directory-inventory-before.json／after.json。本次未修改UI或操作行为，无新增实机交互验收项，也未将回归当作实机测试。

## v0.4.115 GitHub Release

- [x] 准备 Windows x64 统一离线 ZIP、SHA256、中文发行说明，修正安装说明中的 UXP 版本为实际内置0.3.7。EXE与已运行版本一致，沿用已通过的回归／原生／烟雾与实机证据。
- [x] v0.4.115 标签已推送，GitHub Release 已发布并设为 Latest，统一 ZIP 与 SHA256 上传完成。GitHub资产digest与本地一致，匿名重新下载两份文件并核验哈希通过；ZIP SHA256 c320f2b33c44545ae325f986b7c4aa9d8d5ad6fa2482d6c205587b346ea7c96a。证据.runtime/release-0.4.115-artifact-verification.json、.runtime/release-0.4.115-public-verification.json；下载链接已加入README。

## 仓库公开与 MIT 许可

- [x] 添加标准 MIT、第三方声明及现有库／字体的许可副本；前端包标记 MIT，分发包保留项目／第三方许可。检查405个历史文本对象的常见令牌／私钥格式，未发现匹配，也未发现历史凭据配置文件。
- [x] master／work 全部可达历史清理完整 Figma 导出、测试 PSD、本机 VS Code 配置与 pyvenv.cfg，并分别加入 MIT；以明确旧 SHA 的 force-with-lease 原子推送。GitHub 已公开，匿名访问与 MIT 识别核验通过。原文件与历史备份在本地保留，44个工作文件哈希保留，首次公开时未纳入功能修改；本轮另行同步已验证的v0.4.115／UXP0.3.7更新。证据.runtime/open-source-public-verification.json、.runtime/open-source-local-preservation.json、.runtime/public-history-exclusions.json；恢复备份已于目录整理时移至.local/recovery/git-history/private-history-backup.git。

## v0.4.115 横向分隔线tip与自然宽度过渡

- [x] 根因是图标分隔线两侧中性窄缝处实际命中返回-1，但tip间隙判定把整格都视作按钮，立即清空。复用同一窄缝判定，提示保留最多90毫秒，松键仍取消，禁用按钮仍隐藏；不扩大点击／执行命中。
- [x] 移除最长提示预留宽度；当前文字加左右各6单位内边距，背景120毫秒平滑改变，文字交叉淡入淡出，中途反向从当前几何继续。首次150／同组稳定60毫秒及中心／组外／输入／忙碌／关闭清空保留。AGENTS／README同步。
- [x] 原生横向窄缝保留及松键取消、动画起点不跳跃／中间值连续／反向连续／精确结束内边距和既有分流／输入焦点／取消／无命中自测、222项后台回归通过。最终EXE完整烟雾及重启持久化通过。初轮预览读回不完整，最终预览采用独立离屏重绘纹理，不改生产绘制路径；30份最终四主题／不同字号预览完整按钮／色块／状态标记核验通过。
- [x] 正常退出旧服务后解除短暂EXE文件锁并更新实际入口v0.4.115；入口／dist／收据、实际菜单／热键路径／父服务／组件哈希一致，配置凭据及PS文档／UXP0.3.7保留，未重启PS。备份dist/0.4.114-backup、.runtime/before-0.4.115-data；证据.runtime/native-selftest-0.4.115.log、.runtime/tests-0.4.115.log、.runtime/tip115-preview-readback.json、.runtime/smoke-0.4.115-final.log、.runtime/installed-0.4.115-final-verification.json、.runtime/tip115-active-workers.json。
- [x] 用户于2026-10-12实机确认v0.4.115“提示与操作都正常”：横向跨分隔线提示连续、长短文字宽度变化自然、左右留白一致，回中心／分隔线松键只取消，操作验收通过。先前生产API工作区保护拒绝记录仍作为未执行保留，不替代本次用户确认；记录.runtime/tip115-popup-live.json及用户回复。

## v0.4.114 快速切换tip防闪烁

- [x] 加入与执行命中独立的提示状态：首次停留150毫秒、同组切换60毫秒、间隙保留最多90毫秒；预设和右侧图标固定各自同组框位置／宽度，剪贴板独立。移出组／中心／禁用按钮、输入保护、动画、关闭、忙碌与提交立即隐藏。保留原即时hover、点击、松键执行与取消；AGENTS／README同步。
- [x] 原生确定性时序核验快速划过不显示、间隙连续、稳定换文案、间隙到期、中心离开、跨组重新等待、禁用清空和同组固定几何；既有预设／图标无命中／避让／单次提交、画布图层分流、输入焦点与取消自测和222项后台回归通过。四主题及不同字号共30份预览通过。
- [x] 最终EXE首次烟雾临时进程关闭超时／文件锁清理失败，未计通过；第二次完整烟雾及重启持久化通过，WMI确认无临时测试EXE残留。正常切换实际入口v0.4.114，配置凭据、PS文档及UXP0.3.7保留，未重启PS；组件路径／父服务／dist／收据哈希一致。备份dist/0.4.113-backup、.runtime/before-0.4.114-data；证据.runtime/native-selftest-0.4.114.log、.runtime/tests-0.4.114.log、.runtime/tip114-preview-readback.json、.runtime/smoke-0.4.114-retry.log、.runtime/installed-0.4.114-final-verification.json、.runtime/tip114-active-workers.json。
- [x] 用户于2026-10-12实机确认v0.4.114“提示更稳定，操作正常”：快速切换tip改善、停留更新、回中心隐藏及点击／松键取消验收通过。此前生产API工作区保护拒绝记录保留为未执行，不替代本次用户实机确认；证据.runtime/tip114-popup-live.json及用户回复。后续横向窄缝／padding问题按v0.4.115重新验收。

## v0.4.113 首页卡片比例、宽度与预设tip

- [x] 尺寸数字10／10.75／11.5、单位／分辨率及标签8.5逻辑字号，两行整体居中、间距3、统一图文起点34。左右列分别测量内容后确定宽度154～228，允许不等宽；剪贴板独立收窄，行高随实际字体计算，保留2列×4行及比例图标。去除“最近预设”常驻标题，悬停卡片使用上方黑色半透明tip，剪贴板独立说明，命中只限原卡片；首排仍与圆环等高。README／AGENTS同步。
- [x] 222项后台回归、原生非等宽／剪贴板独立宽度／字体层级／紧凑高度／首排对齐、每张卡片tip避让与无命中、退出／中心取消／单次松键提交及原有画布图层分流／输入焦点／禁用保护自测通过。四主题、不同字号及首尾卡片tip共20份预览及内容读回检查、最终EXE烟雾通过；首轮自测日志重定向与脚本自身写同一文件冲突，修正调用后完整自测通过。
- [x] 正常退出旧后台后更新实际入口v0.4.113，配置凭据和PS文档／UXP0.3.7保留，未重启PS。入口／dist／收据与实际菜单／热键组件路径／父服务／哈希一致。备份dist/0.4.112-backup、.runtime/before-0.4.113-data；证据.runtime/tests-0.4.113.log、.runtime/native-selftest-0.4.113.log、.runtime/cards113-preview-readback.json、.runtime/smoke-0.4.113.log、.runtime/installed-0.4.113-final-verification.json、.runtime/cards113-active-workers.json。
- [ ] 新版实体外观、预设／剪贴板点击和中心松键取消确认。生产API被PS工作区保护拒绝，文档数量保持，未标为实机显示／点击通过；证据.runtime/cards113-popup-live.json。

## v0.4.112 填充与调整层按PS原生位置新建

- [x] 修复DOM临时层默认建在文档顶部导致最终填充／调整层同样置顶：先定位临时层，创建后清理并再次核验。普通／组内层同父组紧邻上方；组本身／空组在组内顶部，与PS原生命令一致；空白层在组外上方的既有规则保留。移除填充／调整／组的固定名称，使用默认编号；同一历史事务、读回、失败回滚和冻结目标保护保留。AGENTS／README同步。
- [x] 30种位置模拟，含临时层被替换／保留、移动失败回滚和默认名称；222项后台回归、原生分流／输入焦点／命中／取消／tooltip自测、最终EXE烟雾通过。
- [x] 真实PS24.0.1对照35项填充／调整：根层中间／底部、嵌套组内层、组本身、空组、多选活动层、背景；名称、父组、相邻位置与原生命令一致，原层及顺序保留，一次撤销恢复；目标改变拒绝。空白层8项原生命名／位置／撤销回归通过。证据.runtime/created-layers-0.4.112-live.json、.runtime/blank-layer-0.4.112-live.json。
- [x] 已正常切换实际入口v0.4.112／UXP0.3.7并通过正常桌面COM助手重启PS；更新前没有打开文档。配置凭据、入口／dist／收据、实际组件路径／父服务／哈希及部署插件资源核验一致。备份dist/0.4.111-backup、.runtime/before-0.4.112-data；证据.runtime/installed-0.4.112-final-verification.json、.runtime/position112-active-workers.json、.runtime/tests-0.4.112.log、.runtime/native-selftest-0.4.112.log、.runtime/smoke-0.4.112.log。
- [x] 七个创建／调整／打组命令、纯色前景色及精确打组成员回归通过，临时文档清理。生产菜单API因工作区保护未打开，单独记为未执行，未当作点击通过；证据.runtime/quick-layers-0.4.112-live.json。统一后台分发包dist/PhotoshopMenu-Backend-0.4.112.zip已构建，内嵌插件与工作区资源核验一致。
- [ ] 实体菜单点击创建后的插入位置／默认编号／一次撤销验收；真实执行端对照不替代实体键鼠验收。

## v0.4.111 基本操作图文比例与首页图标统一

- [x] 带文字的相邻／组内选择、上下移动、首页新建统一12图标／95%PS文字／左9、间隔6、右9，按文本尺寸同步按钮和命中。右侧16纯图标不缩小，原锚点／配色／上方tooltip保持；组内选择和新建新增同风格SVG，预设／剪贴板18×18范围、真实比例、灰填充及内边框，极窄比例收紧边框避免反向。
- [x] AGENTS记录未来所有图标按同一Figma家族规范，明确网格、边框、黑／灰层级、图文指标、源资源／补绘说明、SVG／原生生成和主题／命中验收。README同步使用说明与来源。
- [x] 222项后台回归、原生分流／输入焦点／命中／取消／tooltip／首页自测、四主题／三字号／首页有无剪贴板及极窄比例共42份预览控件读回与最终EXE烟雾通过。预览捕获改为实际绘制纹理，修复不完整帧读回；异常初始预览没有标为通过，修正后重新检查完整42份。最终入口v0.4.111、实际组件路径／父服务／构建／dist／收据哈希一致，配置凭据和PS／UXP0.3.6保留。备份dist/0.4.110-backup、dist/0.4.111-first-build-backup及.runtime/before-0.4.111-data／before-final-0.4.111-data。
- [x] 生产API工作区保护拒绝，文档数量保持，未标为实际显示通过；证据.runtime/balance111-popup-live.json。构建／运行证据.runtime/tests-0.4.111.log、.runtime/native-selftest-0.4.111.log、.runtime/balance111-preview-readback.json、.runtime/balance111-*.png、.runtime/smoke-0.4.111.log、.runtime/installed-0.4.111-final-verification.json、.runtime/balance111-active-workers.json。
- [x] 用户确认v0.4.111“图标没问题”；另报告曲线等新建置顶，按v0.4.112修复，不把外观确认记为新建位置通过。
- [ ] 首页点击、中心取消／右键重开及完整输入焦点矩阵单独确认。

## v0.4.110 缺少图标按Figma风格补绘

- [x] 六枚缺少对应组件的空白／纯色／打组／曲线／色彩平衡／黑白图标，按原9×9网格、1单位边框、黑／灰填充及留白补绘；文件夹参考749:0、去除排序标记。数据／SVG／生成原生头文件及来源哈希完整，原四枚直接复用与六枚风格扩展明确区分；import工具统一生成，两排位置、大小、配色、上方tooltip和命中保持。AGENTS／README同步。
- [x] 222项后台回归、原生画布／图层分流、输入焦点／禁用／命中／取消／tooltip自测、四主题28份正常／hover／禁用预览及最终EXE烟雾通过；正常更新实际入口v0.4.110，配置凭据及PS／UXP0.3.6保留，实际组件路径、父服务及构建／dist／收据哈希一致。备份dist/0.4.109-backup、.runtime/before-0.4.110-data。
- [x] 生产菜单API被鼠标工作区保护拒绝，文档数量保持，没有标为显示／键鼠通过；证据.runtime/style110-popup-live.json，其余证据.runtime/tests-0.4.110.log、.runtime/native-selftest-0.4.110.log、.runtime/style110-*.png、.runtime/smoke-0.4.110.log、.runtime/installed-0.4.110-final-verification.json、.runtime/style110-active-workers.json。
- [ ] 新版实际显示与补绘图标实体风格外观、hover／点击、中心取消／右键重开及完整输入焦点矩阵确认；渲染和API可见性不替代用户实机验收。

## v0.4.109 tooltip上方定位与推送

- [x] tooltip优先按钮／整组上方、必要时横向避让；上方空间不足才回退侧面／下方，保持黑色半透明底、浅色文字和无命中，布局不移动，AGENTS／README同步。
- [x] 222项后台回归、5项前端测试及前端构建、UXP命令／类型／标色模拟、原生画布／图层／基本操作上方定位、避让、输入焦点、取消自测、四主题16份预览及最终EXE烟雾通过。正常更新实际入口v0.4.109，配置凭据和PS／UXP0.3.6保留；实际菜单／热键路径、父服务及组件／dist／收据哈希一致。备份dist/0.4.108-backup、.runtime/before-0.4.109-data。
- [x] 真实PS生产API菜单显示可见，文档数量保持；证据.runtime/tip109-popup-live.json。其余证据.runtime/tests-0.4.109.log、.runtime/frontend-tests-0.4.109.log、.runtime/native-selftest-0.4.109.log、.runtime/tip109-*.png、.runtime/smoke-0.4.109.log、.runtime/installed-0.4.109-final-verification.json、.runtime/tip109-active-workers.json。
- [ ] 上移后的新版tooltip实体外观、点击／中心取消／右键重开及完整输入焦点矩阵确认；用户确认的是v0.4.108配色显示。

## v0.4.108 黑色半透明tooltip

- [x] 悬停tooltip改为黑色alpha 218/255、浅色文字及弱边框，四主题共用；4圆角、6内边距、位置避让、无控件／无命中保留。底部信息区仍用主题色，AGENTS.md与README同步。
- [x] 222项后台回归、原生画布／图层分流、输入／焦点／取消／tooltip自测、四主题16份预览及像素透明度／浅色文字核验、最终EXE烟雾通过。正常退出旧后台，更新实际入口v0.4.108，配置凭据、PS／UXP0.3.6保留；菜单／热键实际路径、父服务、构建／dist／收据哈希一致。备份dist/0.4.107-backup、.runtime/before-0.4.108-data。
- [x] 真实PS生产API菜单可见、文档数量保持，无文档写入；证据.runtime/tip108-popup-live.json。其他证据.runtime/tests-0.4.108.log、.runtime/native-selftest-0.4.108.log、.runtime/tip108-*.png、.runtime/tip108-color-verification.json、.runtime/smoke-0.4.108.log、.runtime/installed-0.4.108-final-verification.json、.runtime/tip108-active-workers.json。
- [x] 用户确认v0.4.108“tip显示正常”，黑色半透明配色外观验收；随后要求移到上方，v0.4.109实现。
- [ ] 实体点击／中心取消／右键重开及完整输入焦点矩阵仍未单独验收。

## v0.4.107 完整Figma导出与原始排序图标

- [x] 使用完整ZIP，确认123份SVG、1471个带几何节点、无缺失矢量路径；新增同父组顶部／底部原始轮廓，与已有相邻层／色相饱和度共四枚直接复用。保留贝塞尔及变换，修正ImGui所需绕向，生成来源／源SVG哈希和六项无对应组件说明；构建编译，无运行时设计读取。右列位置、大小、命中、PS配色及蓝色hover保持。
- [x] 222项后台回归、原生画布／图层分流、输入／焦点／取消／tooltip自测、四主题16份预览和最终EXE烟雾通过；正常退出旧后台、更新实际入口v0.4.107，保留配置凭据。入口／dist／收据和实际菜单／热键进程路径、父服务及组件哈希一致，PS／UXP0.3.6未重启。备份dist/0.4.106-backup、.runtime/before-0.4.107-data。
- [x] 真实PS生产菜单API显示可见，文档数量保持，无文档写入；证据.runtime/figma107-popup-live.json。构建／运行证据.runtime/tests-0.4.107.log、.runtime/native-selftest-0.4.107.log、.runtime/figma107-*.png、.runtime/smoke-0.4.107.log、.runtime/installed-0.4.107-final-verification.json、.runtime/figma107-active-workers.json。
- [ ] 实体新版图标外观／hover／点击、中心松键／右键重开与完整焦点矩阵验收；自动检查与API窗口显示不替代实体键鼠。

## v0.4.106 空白图层位置／原生命名与恢复右列高度

- [x] 空白层通过原生make自动命名，冻结活动层为锚点，读回后按需PLACEBEFORE；核验同父组、紧邻上方和原有顺序，单历史事务失败回滚，原文档／选择冻结保护保留。组内目标保留组、组本身目标在组外上方；多选按实际活动层创建。AGENTS记录。
- [x] 用户指出v0.4.105图标上移过高，恢复v0.4.104靠近圆环的右侧两排：下排与环等高，上排紧邻，左对齐／4间隙；保留16图标、24×30命中、邻近tooltip、PS主题蓝色和其余环绕布局。
- [x] 222项后台回归、原生双上下文高度／七图标命中／取消／tooltip避让与无命中／既有输入焦点分流、UXP创建／类型／标色模拟、四主题16份预览和最终EXE烟雾通过；正常退出旧服务并更新实际入口v0.4.106，正常重启PS加载UXP0.3.6。配置凭据、入口／dist／菜单／热键／收据、已部署插件代码一致，实际worker路径与父服务核验。备份dist/0.4.105-backup、.runtime/before-0.4.106-data；PS重启前恢复副本.runtime/uxp036-recovery-106已重新打开为工作副本，原文件未改动。
- [x] 实机8种临时文档验证根层顶部／中间／底部、默认名计数、组内层、组本身、多选活动层、背景：紧邻上方、同父组、原层不变、默认名与原生命令一致（Layer 7→Layer 8），一次撤销恢复；冻结目标改变不创建。七创建／调整／打组、前景色及原层保留回归通过。证据.runtime/blank-layer-0.4.106-live.json、.runtime/quick-layers-0.4.106-live.json；测试脚本的异步guard和跨调用JS局部变量问题修正后完整实测通过。
- [x] 生产菜单API窗口可见、文档数量保持；证据.runtime/blank106-popup-live.json。构建与运行证据.runtime/tests-0.4.106.log、.runtime/native-selftest-0.4.106.log、.runtime/blank106-*.png、.runtime/smoke-0.4.106.log、.runtime/installed-0.4.106-final-verification.json、.runtime/blank106-active-workers.json。
- [ ] 实体菜单新建位置／命名、恢复布局与邻近tooltip、点击／中心松键／右键重开和完整输入／焦点矩阵验收。真实执行端／API及模拟不替代实体键鼠验收。

## v0.4.105 基本操作内圈、图标外圈与邻近tooltip

用户反馈图标整体上移过高，布局未获验收；v0.4.106已恢复原右列位置。以下为历史实现与测试记录。

- [x] 文字选择／排序保留基本操作内圈，七个无名称图标放在基本操作上方外圈，右侧两排左对齐、间隙4、离基本操作上缘12单位；保留16图标、24×30命中、PS主题蓝色、色块和首页。绘制与命中同步、AGENTS偏好更新。
- [x] 图标／文字基本操作／色块使用当前控件附近独立tooltip，优先下方；避让其他操作、中心标题、预设和状态区，动画／提交隐藏，纯绘制无控件／无命中，空白／中心取消保护保留。
- [x] 222项后台回归、原生外圈层级／画布与图层分流／七图标点击与松键／禁用／空白取消／tooltip无命中和避让／既有输入与焦点保护、四主题16份预览及最终EXE烟雾通过。正常退出旧服务后解除短暂文件锁，更新实际入口v0.4.105；实际菜单／热键路径、父服务及组件／dist／收据哈希一致，配置凭据及PS／UXP0.3.5保留。备份dist/0.4.104-backup、.runtime/before-0.4.105-data；证据.runtime/tests-0.4.105.log、.runtime/native-selftest-0.4.105.log、.runtime/outer105-*.png、.runtime/smoke-0.4.105.log、.runtime/installed-0.4.105-final-verification.json、.runtime/outer105-active-workers.json。
- [ ] 新版实机显示、外圈与tooltip外观、实体点击／中心松键／右键重开及完整输入／焦点矩阵。生产API被鼠标工作区保护拒绝，文档数量保持，未记为显示或键鼠通过；证据.runtime/outer105-popup-live.json。

## v0.4.104 放大图标并收紧右列间距

- [x] 七图标和相邻层图标内容12→16逻辑单位，图标按钮与命中18×24→24×30；右侧两排左边缘对齐，边缘间隙4，PS配色／蓝色强调／统一圆角及其余环绕布局保持，偏好写入AGENTS.md。
- [x] 222项后台回归、原生七图标点击／松键／禁用／两排对齐／间隙和旧位置取消／提示无命中与避让／原有输入分流和焦点保护自测、四主题12份预览、最终EXE烟雾通过；正常退出旧服务后更新实际入口v0.4.104，配置凭据、PS文档与UXP0.3.5保留。菜单／热键进程路径、父服务及组件／dist／收据哈希一致。备份dist/0.4.103-backup、.runtime/before-0.4.104-data；证据.runtime/tests-0.4.104.log、.runtime/native-selftest-0.4.104.log、.runtime/large104-*.png、.runtime/smoke-0.4.104.log、.runtime/installed-0.4.104-final-verification.json、.runtime/large104-active-workers.json。
- [x] 真实PS内生产菜单API显示可见，文档数量保持，无文档写入；证据.runtime/large104-popup-live.json，范围限窗口可见性，不替代实体键鼠验收。
- [x] 用户确认v0.4.104“其他正常”，菜单操作确认；Figma图标来源问题已说明仅完整几何可直接复用，未声称全部替换。
- [ ] 右键重开及完整输入／焦点矩阵未单独验收。

## v0.4.103 创建图标组移到右侧

- [x] 右侧上排空白／纯色／打组、下排四种调整，左边缘对齐、40逻辑单位行距；选择／颜色／排序／首页保持。提示在整列上方，避让排序且不盖另一排；主题蓝色、尺寸与分段样式保留。绘制与命中同步更新，旧左侧区域不执行，偏好写入AGENTS.md。
- [x] 222项后台保护回归、原生新位置七图标命中／单次松键／点击／禁用／取消、两排对齐／旧位置失效／提示不遮控件及无命中、四主题12份预览、最终EXE烟雾通过；正常退出旧服务后更新实际入口v0.4.103，配置凭据、PS文档和UXP0.3.5保留。实际菜单／热键进程路径、父服务及组件／dist／收据哈希一致。备份dist/0.4.102-backup、.runtime/before-0.4.103-data；证据.runtime/tests-0.4.103.log、.runtime/native-selftest-0.4.103.log、.runtime/right103-*.png、.runtime/smoke-0.4.103.log、.runtime/installed-0.4.103-final-verification.json、.runtime/right103-active-workers.json。
- [x] 用户实机确认v0.4.103右侧上下排对齐、提示、点击及中心松键取消正常；随后要求再次放大图标、收紧行间隙，见v0.4.104。
- [ ] 右键重开及完整输入／焦点矩阵未单独验收。

## v0.4.102 放大内容图标

- [x] 按用户反馈将七个小图标和相邻层选择图标从9增至12逻辑单位，保持18×24分段尺寸、命中、提示位置及现有主题蓝色。原始9几何与12显示适配分别记录在来源文件，原生编译资源，不引入唤出加载。
- [x] 222项后台回归、原生七图标点击／松键／提示空白取消及既有分流保护、四主题12份预览和最终EXE烟雾；实际入口v0.4.102正常替换并运行，dist／菜单／热键／收据核验一致，配置凭据及PS／UXP0.3.5保留。证据.runtime/tests-0.4.102.log、.runtime/native-selftest-0.4.102.log、.runtime/icon102-*.png、.runtime/smoke-0.4.102.log、.runtime/installed-0.4.102-final-verification.json；备份dist/0.4.101-backup、.runtime/before-0.4.102-data。
- [ ] 放大后实体图标外观；右键重开／完整输入与焦点矩阵未单独验收。v0.4.101的布局、提示和点击／中心取消已由用户确认。

## v0.4.101 导出控件样式与上方悬停提示

- [x] 从源组件326:514、223:20生成样式常量，应用统一4圆角、24高主按钮、18×24图标分段／9内容图标、6提示／状态内边距及1边框。PS配色与字号保留，按钮与面板填充分层；预设／剪贴板／标签采用同一圆角，保留卡片／色块尺寸及环绕锚点，剪贴板与新建底色一致。已更新AGENTS.md。
- [x] 悬停动作名移到所属图标组上方，避让排序，动画／离开／提交后隐藏；底部保留文档／类型／任务。提示无控件、无命中，图标命中同步可见尺寸；原生回归核验七图标、排序避让、提示空白不执行以及原有点击／松键／取消／输入分流。新增测试最初在帧外读取当前窗口导致异常，改用主视口后完整原生测试通过，失败构建未部署。
- [x] 222项后台回归、四主题悬停及首页预览、最终EXE烟雾、正常退出旧服务并更新实际入口v0.4.101；入口／dist／原生菜单／热键／收据一致，配置凭据与PS／UXP0.3.5保留。备份dist/0.4.100-backup、.runtime/before-0.4.101-data；证据.runtime/tests-0.4.101.log、.runtime/native-selftest-0.4.101.log、.runtime/{tip101,style101}-*.png、.runtime/smoke-0.4.101.log、.runtime/installed-0.4.101-final-verification.json。
- [ ] 右键关闭后重开及完整输入／焦点矩阵验收；生产API曾被PS激活资格保护拒绝，文档数量保持；随后样式、上方提示和点击／中心松键取消已由用户实机确认。记录.runtime/tip101-popup-live.json。
- [x] 用户实机确认v0.4.101“正常”：控件样式、图标上方提示、hover／点击及中心松键取消通过；同时要求内容图标略大，强调色保持之前蓝色。右键重开和完整输入／焦点矩阵未单独确认，仍待验收。

## v0.4.100 Figma导出图标与样式参考

- [x] 从用户提供的151500PX.figma-export.json导入相邻图层选择、色相饱和度两枚完整矩形／渐变图标；构建时生成原生几何并记录节点及源文件哈希。保留PS主题色、蓝色hover、8逻辑单位统一圆角及现有环绕／分段布局，其余图标与卡片统一1逻辑单位线条。其余VECTOR没有路径，保留现有图形，不用整页预览替代运行界面。
- [x] 222项后台保护回归、原生点击／松键／空白取消／禁用／输入分流自测、四主题12份预览和最终EXE烟雾通过；正常退出旧服务后原子替换实际入口v0.4.100，短暂文件锁已解除。入口／dist／菜单／热键／收据哈希一致，配置凭据及PS／UXP0.3.5会话保留。备份dist/0.4.99-backup、.runtime/before-0.4.100-data；证据.runtime/tests-0.4.100.log、.runtime/native-selftest-0.4.100.log、.runtime/smoke-0.4.100.log、.runtime/installed-0.4.100-final-verification.json。
- [x] 真实PS临时文档七个创建／打组命令、纯色前景色读回和冻结选择拒绝通过，测试文档已清理；脚本最后生产菜单显示步骤被鼠标工作区保护拒绝，因此整体脚本未记为通过。证据.runtime/quick-layers-0.4.100-live.log、.runtime/quick-live-tasks.json。
- [ ] 新版实际菜单显示、实体图标外观／hover／点击、中心松键／右键关闭后重开，以及完整画布／图层分流、输入／焦点矩阵验收。生产API被工作区保护拒绝的记录为.runtime/figma100-popup-live.json；自动回归及预览不替代实机验收。

## v0.4.99 统一菜单圆角

- [x] 主按钮、预设／剪贴板卡片、预设外框、标签、分段图标组与底部提示共用FrameRounding=8逻辑单位、按DPI缩放；填充／边框／hover一致，保留布局、尺寸、配色和命中区域，偏好已写入AGENTS.md。
- [x] 222项后台保护回归、原生点击／单次松键／取消及输入／分流自测、四主题12份首页／hover／图层预览、最终EXE烟雾、实际入口v0.4.99及组件／收据／配置核验通过。旧进程完全退出后解除暂时文件锁并原子替换，PS文档和UXP0.3.5会话保留；真实PS生产API菜单可见且文档数量不变。证据.runtime/tests-0.4.99.log、.runtime/native-selftest-0.4.99.log、.runtime/round99-*.png、.runtime/smoke-0.4.99.log、.runtime/round99-popup-live.json、.runtime/installed-0.4.99-final-verification.json；备份dist/0.4.98-backup、.runtime/before-0.4.99-data。
- [ ] 用户实体统一圆角外观／hover／点击／长按取消及完整输入／焦点矩阵验收；预览及API验证不替代此项。

## v0.4.98 分段图标与七个图层创建操作

- [x] 左侧三个空白／纯色／打组图标、右侧四个曲线／色相饱和度／色彩平衡／黑白图标；图标组及排序共用圆角外框与内部直线分隔，沿用PS主题／蓝色hover。动作名称在底部显示，保留原环绕、下方色块、首页、输入／焦点保护和取消行为；布局偏好已写入AGENTS.md。
- [x] 离线UXP0.3.5固定命令、当前前景色纯色、默认调整、准确所选层打组、文档／选择目标保护、模式／位深及父组锁定资格。填充／调整使用历史事务内临时空白层避免PS复用原有空白层，仅新增一个最终图层，失败回滚；忙碌拒绝，不排队，不重发。
- [x] 222项后台回归、七命令及临时层消耗／保留两种行为模拟、原有UXP协议／类型／标签／最近预设测试、原生七图标点击／单次松键／禁用／移回空白取消与已有分流保护、四主题12份预览、最终EXE烟雾通过。证据.runtime/tests-0.4.98.log、.runtime/native-selftest-0.4.98.log、.runtime/quick98-*.png、.runtime/smoke-0.4.98.log。
- [x] 实际后台v0.4.98及插件0.3.5已部署、正常桌面COM重启／新会话就绪；真实PS24.0.1七个命令及独立类型／数量／前景色／组成员读回、冻结选择取消、生产菜单API创建、灰度与锁定父组禁用、39项原有图层操作回归通过。入口／dist／原生组件／收据及内置插件一致、设置凭据保留，恢复副本.runtime/uxp035-recovery。证据.runtime/quick-layers-0.4.98-live.json、.runtime/quick-boundaries-0.4.98-live.json、.runtime/layer-context-0.4.98-live.json、.runtime/installed-0.4.98-final-verification.json；旧入口备份dist/0.4.97-backup及dist/0.4.98-first-build-backup。
- [ ] 用户实体七图标hover／点击／长按松键、快速划过返回空白、右键关闭后重开及画布／图层、输入框／跨应用焦点完整矩阵验收；真实命令及API显示验证不冒充实体键鼠验收。

## v0.4.97 颜色标签下移及上方排序

- [x] 颜色面板在圆环下方；置顶／置底在上方左右并排，各自命中，中缝取消。保留8色、无色清除、无间距4×2排列和原大小／主题，信息底板位于色块下方。画布不露出排序、首页布局不变。
- [x] 原生新布局／上方两按钮对齐／分隔取消／全部色块命中／单次释放／划过后空白取消／动画取消／下方信息无命中及既有输入／分流回归、221项后台测试、四主题预览、最终EXE烟雾、实际入口及配置／组件核验与生产API可见性；PS文档未改动。证据.runtime/tests-0.4.97.log、.runtime/native-selftest-0.4.97.log、.runtime/smoke-0.4.97.log、.runtime/installed-0.4.97-final-verification.json、.runtime/footer97-popup-live.json。
- [ ] 实机新位置点击／长按选色与排序、中心松键／右键关闭后重开及完整hover／输入／焦点保护矩阵验收。

## v0.4.96 隐藏按键提示

- [x] 移除环绕菜单数字、普通／上下文菜单快捷键和右键／Esc等引导；保留快捷键功能、取消行为、底部状态与原布局／命中范围。
- [x] 221项回归、原生自测、浅深主题预览、最终EXE烟雾和实际入口替换；组件／dist／收据哈希与配置核验、真实PS生产API显示且不改文档。证据.runtime/tests-0.4.96.log、.runtime/native-selftest-0.4.96.log、.runtime/smoke-0.4.96.log、.runtime/installed-0.4.96-final-verification.json、.runtime/footer96-popup-live.json。
- [ ] 用户实机确认按键提示已隐藏，中心松键／右键取消及重新打开正常；实体hover／输入／跨应用焦点矩阵仍待验收。

## v0.4.95 底部信息区

- [x] 图层类型／锁定／跨组限制、文档名、输入及固定提示、运行／结果集中放在菜单下方；主题色半透明圆角底板和正常文字对比度。失败显示原因，忙碌反馈沿用相同样式，首页避让预设；绘制不新增命中范围。
- [x] 221项后台回归（画布／图层hover分流、输入／焦点保护及取消等）、原生下方操作无重叠／信息区不可命中／首页避让及既有松键取消回归、四主题12份预览、最终EXE烟雾；v0.4.95已构建并正常替换实际入口，旧版与配置已备份，未重启PS／UXP。实际入口／dist／原生组件／安装收据哈希一致、配置凭据保留，生产API在真实PS打开菜单可见／不透明且文档未变；此项仅验证显示，不冒充键鼠验收。证据.runtime/footer95-popup-live.json、.runtime/installed-0.4.95-final-verification.json、.runtime/tests-0.4.95.log、.runtime/native-selftest-0.4.95.log、.runtime/smoke-0.4.95.log、.runtime/footer95-*.png。
- [ ] 实体各主题可读性、真实键鼠hover／输入框／跨应用焦点／中心松键与右键取消验收；自动测试及生产API可见性不替代用户实机确认。

## v0.4.93 图层类型菜单

- [x] 代码及真实识别完成：UXP后台读取背景／像素／组／文字／智能对象／形状填充／调整及混合类型，缓存选择ID、位置锁、父组及排序边界；后台独立40ms读文件线程不被UIA查询／窗口锁拖住，唤出只用内存快照，复用单次原生准备消息。菜单四方向／色块位置不变，不适用操作置灰。多选仅开放共同可用操作，不替换为单层选择；组选择原有子图层，非组选择原有连续相邻图层。
- [x] 自动验证：221项后台回归、UXP类型及协议模拟、原生禁用／取消自测、生产SetWindowPos显示／关闭回归、最终EXE构建与烟雾通过；缓存打开不读取提供器、半写文件／会话变更、UI窗口锁占用时独立缓存仍更新均通过。浅深主题预览已检查。证据.runtime/tests-0.4.93.log、.runtime/native-selftest-0.4.93.log、.runtime/type-menu-visible-test.log、.runtime/smoke-0.4.93.log。
- [x] 实际安装入口及UXP0.3.4更新、真实PS类型／已有操作验证与配置核验：用户授权以后自动允许丢弃未保存文档并重启，已记录AGENTS.md；本次正常关闭Untitled-1，经后台管理更新并桌面COM助手重启，新心跳／名称正确。PS24.0.1临时文档39项类型、标签、选择／排序、空组／位置锁／父组锁／跨组／混合背景拒绝及冻结目标保护通过；生产菜单可见内容及标色成功、打开后改选择取消均通过，任务不写入、不增加历史。入口／dist／组件／收据哈希一致、设置凭据保留。证据.runtime/layer-context-0.4.93-live.json、.runtime/type-production-{api,cancel}-0.4.93.json、.runtime/installed-0.4.93-final-verification.json；备份dist/0.4.92-backup、dist/0.4.93-first-build-backup及.runtime/before-0.4.93-data。
- [x] 用户实机确认v0.4.93“类型、布局和操作正常”：像素／文字／组／背景类型标题及置灰项、原环绕布局、中心松键取消和右键关闭后重开通过。
- [ ] 完整实体画布／图层hover、输入框／跨应用焦点保护及极快连续按键矩阵；本次问题没有逐项要求或确认这些边界，保留自动回归，不冒充完整矩阵通过。

## v0.4.94 首页预设垂直位置

- [x] 代码：右侧第一排预设与中心环／左侧新建按钮等高，按固定间距向下展开，取消整块垂直居中；保留2列×4行、顶部剪贴板、尺寸／标签／配色和命中逻辑。首页底部提示向左避让下移后的预设区，图层菜单不变。
- [x] 原生首排对齐、全部入口可见／命中／取消与边界回归、221项后台回归、四主题有／无剪贴板预览、最终EXE构建／烟雾及实际入口更新；入口／dist／组件／收据哈希一致，配置凭据保留。旧EXE退出后的短暂锁阻挡首次拷贝，确认进程退出后原子替换并启动成功，未关闭PS／用户文档。UXP0.3.4同会话保留；生产API画布弹窗可见且不透明，当前PS不是首页，未执行新首页预设任务。证据.runtime/tests-0.4.94.log、.runtime/native-selftest-0.4.94.log、.runtime/smoke-0.4.94.log、.runtime/home94-popup-live.json、.runtime/installed-0.4.94-final-verification.json。后续优先首排靠近中心／按固定间距下展开的偏好已记录AGENTS.md。
- [ ] 新版实体垂直位置／hover／点击及取消验收。

- v0.4.51：10:38:05.168捕获菜单内toggle关→开，附近重复DOWN出现62ms间隔（平时约31ms），同时发生钩子维护，队列丢失0，证据 `.runtime/capslock-state-103805/`。已将PS扫描移到后台并以消息交接句柄；按键按住及输入后250ms内保留现有钩子，不增加菜单等待。原生系统toggle变化／基线与实际维护标记已进入持续日志，独立只读状态记录器持续轮转。原生10000轮状态／维护门禁与30轮钩子自测、98项回归、最终EXE烟雾／重启通过。空闲旧服务正常退出，备份dist/0.4.50-backup及.runtime/settings-before-0.4.51.json，统一入口v0.4.51和组件哈希已核验，证据 `.runtime/release-0.4.51-verification.json`。实体长按／快速连按已由用户确认无误切换、菜单正常；外部Caps正常行为本轮未单独验收，长期稳定性继续观察。

目标：原生悬浮菜单配合 Photoshop 内执行端，支持不断扩展的脚本、复杂连续操作和长任务；后台长期运行，PS 忙碌时等待，不主动抢焦点。

## 当前执行顺序（2026-10-11用户调整）

1. 优先主要功能：菜单唤出／取消、图层与画布分流、选色／清除／排序、文档与选择目标保护；针对实际复现故障修复，不将长期验收清单当成全部功能未实现。
2. 完善执行确定性：忙碌不接受新交互任务，目标变化拒绝写入；处理超时／断线后的结果确认与反馈，不自动重发，不因请求超时判定脚本已停止。不重复扩展已完成的诊断功能来替代主功能修复。
3. UXP主菜单迁移已在v0.4.69完成；v0.4.72将UXP资源和安装／禁用／更新管理整合到主程序后台；独立OfflineSetup只保留为历史部署方式。离线仍为硬条件，不依赖Adobe账号、Marketplace、云服务或额外Adobe工具；当前使用本地文件认证通信，无插件网络权限。继续验证跨用户／PS版本的离线分发兼容性，不恢复未知结果自动重发或COM写入回退。
4. 长期驻留／资源增长、多DPI完整矩阵及广泛长期稳定性优化后置。相关未验收记录保留，但不阻塞前述主要功能工作。

迁移前初步调查（历史）：官方常规ccx安装使用Creative Cloud Desktop或UPIA，开发加载使用UDT；离线使用支持不等于无Adobe工具依赖。本机标准UPIA路径不存在。当时继续v0.4.68 COM；后续本地部署与生产迁移结果见文末v0.4.69／v0.4.70。

## P0：CapsLock 按住菜单交付与真实验收

2026-10-09 已交付环绕饼菜单及右侧两排颜色图标；v0.4.9 恢复按画布／图层 hover 区分内容，移除 Photoshop 原生右键接管，并增加清除标签结果核验。v0.4.10 修复隐藏窗口时提前移除透明样式导致的矩形闪帧路径。实际运行版本以文末核验记录为准。自动测试通过不等于真实 Photoshop 实体键鼠手势已验收。

- [x] 以鼠标附近为中心显示透明四向环绕饼菜单；短按≤220ms松开后固定，长按松开按实际命中执行，无命中取消。
- [x] 修复环绕菜单忽略 hover 识别结果：画布显示选择／标色，图层面板额外显示顶部／底部排序，标题同步区分；隐藏方向不能通过命中残留或方向判断执行。
- [x] 菜单显示期间冻结唤出面板，不因透明区域露出 Photoshop 而重采样跳变。
- [x] 移除 Photoshop 右键唤出拦截及设置开关；旧设置强制关闭，保持 PS 原生右键。
- [x] 关闭环绕菜单时保留透明样式，仅使命中区域失效；下次打开时再切换样式。原生回归测试覆盖此关闭路径。
- [ ] 实机连续关闭／重开核验矩形闪烁消失，并覆盖不同显示器 DPI 与渲染环境。

- [x] 完成运行版本切换，确认实际服务、原生热键和菜单组件均来自新版；打开新版时不得静默复用旧服务而误报升级成功。
- [ ] 在真实 Photoshop 中验收：短按CapsLock松开后原地固定；长按时移动鼠标高亮，松开执行一次，无需点击；固定后CapsLock、右键或Esc关闭。
- [ ] 验收长按没有指定选项时松开只关闭：初始位置、标题、空白区域、移出选项均不得执行默认或上次选项；短按固定不得自动执行。
- [x] 快速划过颜色标签后离开再松键：v0.4.47增加即时位置取消否决，用户确认不再误执行且正常选色可用；95项回归及最终EXE检查通过。
- [ ] 中心／空白松键偶发执行：日志确认有hold_release→label.blue，旧日志未记录鼠标坐标，尚不能确定该次根因。已改为钩子冻结松键坐标、与已提交高亮核对、执行前再次校验菜单会话；旧锁等待事件与负屏幕坐标回归已通过，最终93项回归及EXE烟雾／重启通过，已切换v0.4.41；实体连续开关验收仍待完成。
- [ ] 验收重复按键、连续快速呼出、Esc、组合键、应用切换、PS 最小化或关闭、输入框、忙碌状态及多显示器 DPI；组合键之后松开不得重复执行。
- [x] CapsLock按PS进程拦截：v0.4.46复用进程身份缓存，工作区触发与非工作区仅屏蔽分开；95项回归、EXE烟雾／重启及用户实机短按／长按／弹窗／外部应用验证通过。
- [x] 左Win按PS进程识别：v0.4.45已实现进程身份缓存及失效保护，95项回归通过；用户确认PS及右键菜单内映射、外部应用保留Win行为均正常。
- [x] 左Win连续按键漏到系统：v0.4.44补充PSFloatC窗口归属检查，真实窗口回归通过，用户确认在PS及右键菜单状态下连续按键正常。
- [ ] 左Win→右键：用户确认仅PS及自定义菜单生效。原生状态机与消息循环已实现物理按下／松开配对、重复键去重、跨作用域保持首次决定及正常退出补发释放；91项回归及最终EXE烟雾／重启通过，实际运行已切换v0.4.45；v0.4.44连续按键及PS右键弹出窗口已获用户实机确认，跨应用释放／右Win保留／右键拖动仍待完整实体键鼠验收。右Win及外部应用左Win保持原行为。
- [ ] 检查呼出位置靠近屏幕边缘时是否意外落入选项；保证初始取消区域和选择反馈明确。
- [ ] 浮动文档／浮动面板模式：已扩展原生钩子与后台的同进程、主窗口归属判断，hover归一到同一工作区；实机只读核验浮动文档为canvas、停靠图层为layers、浮动属性面板为generic。88项回归及最终EXE烟雾／重启通过，已切换v0.4.37并核验原生组件；实体按键验收仍待完成。
- [x] 统一发布入口与当前版本显示，避免新版放在子目录但用户继续启动旧版；提供可靠的退出、更新与重新启动流程（README 已记录，实际退出、备份、替换、重新启动及版本核验已完成）。

## P1：图层标签颜色图标与即时执行

- [x] 扩展为当前 Photoshop 24.0.1 支持的全部原生标签：红、橙、黄、绿、蓝、紫、灰及“无颜色／清除标签”；已核实命令映射并逐一实机执行验证。
- [x] 在环绕菜单中两排四列无间距排列颜色图标，不显示颜色文字或独立悬停提示；清除使用斜线标识。Photoshop 原生右键菜单不替换。
- [x] 点击颜色图标后立即应用到当前选中的图层，无需确认；单选与多选命令均已在真实 Photoshop 临时文档验证，未选图层保持不变。
- [x] 实现按住滑到具体色块、松开应用；未命中色块则取消。原生测试覆盖所有色块松开执行，以及 ImGui 点击立即执行后松开不重复触发。
- [x] 自动回归覆盖无文档、输入框焦点／焦点限制、忙碌拒绝、未知颜色拒绝及旧脚本包切换后颜色命令仍可用。
- [x] 补测已标色图层清除、多选清除及橙色父组中的子图层清除；所有命令逐层读回核验。清除自身后父组仍有颜色时提供提示，不擅自修改父组或兄弟图层。
- [ ] 在真实 Photoshop 完成实体键鼠端到端验收：点击及按住滑动选色、多次快速呼出、焦点切换、输入框与忙碌反馈、多屏 DPI。命令实机测试和原生输入测试不替代这项验收。
- [ ] 升级 Photoshop 版本后重新核实新增标签颜色；当前完整性范围为已验证的 24.0.1，不宣称覆盖后续版本新增颜色。

## P1：统一任务协议与调度

- [x] 建立命令注册表：12项已有命令已纳入版本化契约，包含参数校验、适用条件、当前文档／选择目标策略、COM通道、预期结果、取消和恢复限制；已接入API与实际执行校验。84项回归及最终EXE烟雾／重启通过，已切换v0.4.36并核验实际注册表查询与参数拒绝接口。当前目标策略不等于持久化文档／图层ID，后者见下项。
- [ ] 为每次任务生成唯一 ID，定义排队、等待 PS、执行中、取消请求中、成功、失败、已取消、结果未知等状态：v0.4.59 已实现接受的 COM 操作唯一 ID，以及目标读取、执行、取消等待、已取消、成功、失败、结果未知和迟到结果更新；提供最近任务列表、状态查询与有界历史。v0.4.65交互排队在v0.4.66按用户确定性要求停用，当前忙碌直接拒绝新交互请求；等待用户模态及UXP调度仍未实现；v0.4.63已接入读取目标阶段的取消按钮／接口（仍待实体按钮验收），运行任务取消及 UXP 通道仍待后续；v0.4.60已增加最近64条任务持久恢复（见下项），并非自动恢复执行。实际服务只读任务API与轮转日志已核对：8次实体菜单标色／清除的ID全部不同，最终均成功；后台页面视觉呈现仍未验收。
- [ ] v0.4.59 本轮任务记录交付验收：代码、113项后台／3项前端测试、前端及EXE构建、真实PS临时文档标色／清除和任务ID／状态顺序／目标关联已验证，实际统一入口已切换并核验v0.4.59及原生组件。只读任务API与实际前端资源哈希通过；证据 .runtime/task-lifecycle-photoshop-verification.json、.runtime/task-api-0.4.59-verification.json、.runtime/release-0.4.59-verification.json。旧程序和配置备份dist/0.4.58-backup、.runtime/settings-before-0.4.59.json。页面检查工具两次初始化失败0x80070020，未冒充视觉验收；2026-10-11 12:47日志核验：实际服务8次标色／清除均为不同ID且成功，耗时156～234ms；前7次完成后均有菜单重开记录，最后一次没有后续打开动作，不能据此判断失败。证据 .runtime/task-0.4.59-live-log-verification.json。任务数据与菜单重开记录已验证，后台页面视觉呈现仍待验收。列表仅内存保留最近64条（页面最新12条），日志不支持恢复或自动重放；排队、UXP和取消按钮不属于本次完成范围。
- [ ] 同一个 PS 实例的修改任务统一串行调度：现有COM单线程执行；v0.4.66交互请求忙碌直接拒绝，不自动排队。未来明确启动的工作流步骤需要COM／UXP统一执行协调，不应恢复交互请求自动排队；跨通道工作流协调尚未实现。
- [ ] 区分工作流等待期限、通讯超时、执行期限；长任务不再仅靠固定请求超时判定失败。v0.4.65内部队列原型有30秒期限，但v0.4.66交互入口不再使用排队；COM读取／执行仍沿用45秒等待，通信与实际执行期限尚未分离。
- [x] UXP超时或断线后核对原请求的持久最终结果，禁止自动重发结果未知的修改命令，也禁止自动改用COM重跑。v0.4.116／UXP0.3.8已实现；没有可信最终记录时继续保持结果未知和busy，不能推断成功／失败或靠重启放行。当前仅核对已有证据，不自动重建／重演任务。
- [ ] 文档／图层目标保护：v0.4.55代码、103项回归、真实PS临时文档6组验证、构建和实际入口切换已完成；现有12命令捕获文档／选择／活动图层ID并在写入前校验，变化时取消，不重连或重试。新版实体菜单正常操作仍待用户验收，证据见文末。
- [ ] 持久化任务日志、步骤结果与必要的恢复信息；重启后核对未完成任务，不盲目继续或重放。v0.4.60已实现最近64条任务的ID／目标／耗时／最终结果持久快照及重启只读恢复；中断记录统一标为结果未知，保留先前状态，不重放。独立写盘线程、修改前持久屏障、等待后的菜单会话检查、损坏记录保留和写盘失败拒绝修改已验证。实际入口v0.4.60恢复旧版13条真实任务，证据.runtime/task-history-0.4.60-live-verification.json。长期审计日志、步骤结果／检查点与实际PS未完成结果核对仍未实现，因此总项不勾选。

## P1：PS 内 UXP 执行端

- [ ] 离线分发跨环境实机验收：本机无额外Adobe工具的用户目录部署、自动加载和本地文件通信已通过；v0.4.70的独立安装器是历史部署；v0.4.72已内置UXP资源到主EXE并由后台管理，真实用户目录安装／同版本ZIP更新／禁用／启用／卸载／回退通过，用户直接启动后重载UXP、独立桥标色／清除读回通过；既有桌面Explorer打开原快捷方式已单次自动启动成功，重复启动稳定性仍待验收。没有插件网络权限或安装下载依赖。全新Windows用户环境的首次安装、真实卸载回退及其他PS版本仍待验证；不将本机更新和隔离文件系统测试视为这些验收，也不宣称官方CCX注册安装或整机断网测试完成。用户此前明确采用现有PS出站屏蔽环境，不要求再次整机断网。


- [ ] 建立 Photoshop 版本兼容与能力检测矩阵；v0.4.70已移除PHSP/24硬编码，自动发现新鲜插件目录，多实例拒绝而不选择错误目标。v0.4.71按用户要求限制安装／manifest／执行端最低24.0.1，不设上限；实机仅24.0.1／UXP6.3.3，更高版本、模态及取消完整矩阵仍待验收。
- [x] 实现用户要求的纯本地认证通信：采用PluginData文件代替WebSocket，包含本机凭据、协议／插件版本、会话、请求ID、期限、同会话去重与跨进程文件锁。v0.4.69生产接入；v0.4.70补充Windows读取句柄导致的提交前原子替换等待，不重发已提交任务。
- [ ] 原生菜单负责交互，本机后台负责调度，UXP 负责 PS 状态读取与 DOM／batchPlay 执行。
- [ ] 修改 PS 状态时使用 executeAsModal；处理已有模态操作、等待期限和失败结果。PS 25.10 起支持的 timeOut 仅作为一部分等待机制。
- [ ] 验证插件首次加载、面板隐藏、关闭、重新加载、PS 重启及长时间驻留的生命周期，不假设插件始终存活。
- [ ] 实现心跳、退避重连、会话重建与重连后的任务核对；区分通讯可达与 PS 可执行。
- [x] 将现有四类JSX菜单操作及八种标签／清除迁移UXP；v0.4.69生产执行桥与实际EXE已实机读回。菜单不使用COM写入回退，旧JSX包不能更换UXP操作；只读COM依赖另行处理。

## P1：忙碌等待、状态读取与焦点

- [ ] 已明确启动的工作流内部遇到PS忙碌／模态操作时有界等待，不强制激活、不抢焦点、不高频重试。菜单／快捷键提交的新交互请求按v0.4.66规则忙碌拒绝，不属于自动排队需求。
- [ ] 建立带时间戳的状态缓存，展示连接状态、状态新鲜度、任务进度与等待原因；读取不可用时标记等待更新，菜单保持响应。v0.4.64已实现后台状态时间戳／年龄／新鲜度／等待原因，150ms有界返回、1秒缓存、并发读合并和手动刷新，busy不读取PS、失败保留旧状态。实际服务API与前端资源已核验，完整模态忙碌下实体菜单及状态卡视觉待验收；阶段进度／PS事件机制仍未实现。
- [ ] 结合可用事件与有节制的状态补读；事件可能缺失或被模态执行抑制，执行前及执行后重新校验关键状态。
- [ ] 将实时键鼠响应与 PS 通讯完全解耦，防止 COM 阻塞、UXP 等待或长任务拖住原生热键与菜单。
- [ ] 明确不抢焦点与独占文档修改的区别；长时间修改期间不能承诺用户可同时编辑同一文档。

## P2：复杂工作流与长任务

- [ ] 支持有依赖关系的步骤串行执行，前一步成功后进入下一步，并记录失败步骤和已有副作用。
- [ ] 耗时计算、网络请求和文件准备尽量放在 PS 外部；仅在需要修改 PS 时申请执行范围。
- [ ] 可分段任务在安全边界释放执行权限，再次进入前校验目标状态；需要连续性的步骤在同一执行范围完成。
- [ ] 报告阶段、进度、耗时与取消状态；支持取消排队任务及运行任务的协作式取消，不宣称单个长命令一定可立即中断。
- [ ] 使用历史状态管理组织可撤销操作；区分文档历史回退与文件、网络等外部副作用，不能视为通用事务回滚。
- [ ] 仅对经过设计的安全检查点提供恢复；恢复前校验文档、会话和已完成步骤。
- [ ] 提供任务列表、等待原因、错误详情和诊断日志，便于管理长时间运行任务。v0.4.68后台已展示全部最近64条，新增新任务阶段表及JSON诊断导出；真实PS命令／阶段持久化及实际资源已验证，页面视觉与点击导出仍待验收，复杂工作流步骤日志仍未实现。
- [x] v0.4.68 COM任务后台阶段记录：状态切换计时、同状态目标更新不重复创建阶段、结果未知后迟到结果保持同一ID、持久恢复只读且不继续计时；真实PS临时文档和138项回归通过，实际入口及原生组件已核验。旧任务不补造时间线。
- [ ] v0.4.68阶段表和诊断导出实体页面验收：实际前端资源已核验，新任务可查看阶段和导出JSON；视觉／下载交互未验收。52条旧任务保留，证据 `.runtime/task-diagnostics-0.4.68-live-verification.json`、`.runtime/task-diagnostics-photoshop-verification.json`、`.runtime/release-0.4.68-verification.json`。

## P2：扩展与稳定性验收

- [ ] 扩展脚本包格式与命令发现，支持参数、能力声明、版本约束、安装升级与回滚。
- [ ] 对新增命令规定执行条件、进度报告、取消点和结果确认方式，避免每个脚本自行处理通讯与调度。
- [ ] 在真实 PS 上测试长时间驻留、连续任务、断线、后台重启、插件重载、PS 忙碌与重启、文档关闭和焦点切换。
- [ ] 验证队列不会重复执行、状态缓存不会误导、内存和句柄不会持续增长，记录测试时长与版本。
- [ ] 如后续出现重型像素计算或 UXP 能力缺口，再评估 C++ SDK／UXP Hybrid；按实际瓶颈选择，不以更换语言代替调度设计。

## 当前完成证据

- CapsLock恢复配置（v0.4.47）：用户确认大写误切换发生于此前CapsLock配置，要求切回CapsLock。已通过settings.save即时恢复toggle和CapsLock+A/C/W/S，运行热键PID不变，状态核验通过；Tab配置备份 `.runtime/shortcuts-before-capslock.json`，证据 `.runtime/capslock-settings-verification.json`。本次配置恢复完成，未声称偶发漏键已修复。
- [ ] CapsLock仍偶发切换大写：用户在此前进程拦截版本短时验收后补充仍偶发，已恢复CapsLock配置，须采集漏键时前台进程／按下释放／钩子状态确认根因，不能仅凭进程识别或短时测试标记长期稳定。用户补充可能发生于任务运行／快速切到其他窗口，两种按法不确定；代码检查任务busy不改变热键enabled，已消费的重复／释放保留首次决定。PS外新按CapsLock按约定保留默认大写；临时只读诊断尚无新事件证据，未据此修改代码或声称修复。
- Tab试用配置（v0.4.47，已恢复CapsLock）：通过settings.save即时将toggle改为tab，原CapsLock+A/C/W/S组合同步改为Tab前缀，Esc保持；无需构建／重启，实际热键PID不变，状态API确认配置与组件正常，证据 `.runtime/tab-settings-verification.json`，旧配置 `.runtime/shortcuts-before-tab.json`。工作区触发规则保留，PS非工作区Tab导航未扩大拦截；CapsLock配置已移除，恢复原行为。实体Tab短按／长按效果待用户确认。

- v0.4.49：用户报告v0.4.48再现，67条CapsLock记录中发现00:48:21.614菜单内UP、scope均true、repeat=false／owned_before=false但blocked=false；此前00:48:21.426已有一次已消费UP，缺少两者之间的DOWN。其余PS新按均拦截；三次外部Chrome窗口新按按当前规则正常放行。修复仅在PS且配置含CapsLock时消费未配对／重复UP并保持Action=None，已知外部按下跨入PS的UP不接管；新增空状态UP、保护窗口UP、外部UP、未配置CapsLock、重复UP和外部已知按下跨入PS回归。未证明缺失DOWN的根因或该UP就是大写变化唯一原因；96项回归及最终EXE烟雾／重启通过；已正常退出空闲旧服务并切换统一入口v0.4.49，PS连接及原生组件哈希一致，备份 `dist/0.4.48-backup`，证据 `.runtime/release-0.4.49-verification.json`。2026-10-11用户回复“这次没有误切换”，该边界修复本轮实机验证通过；整体偶发漏键仍保留为长期观察项，未勾选完成。

- v0.4.48：用户确认CapsLock在任务／切窗口场景实机复现；此前临时观察文件为0字节，周期性重装生产钩子后外置低级钩子可能看不到已消费事件，不能据此否定复现。改为生产组件直接固定队列记录CapsLock按下释放的HWND／PID／类别／flags／原始时间、command/process范围、enabled、repeat、owned_before和blocked，后台只记日志不提交动作。前台两种范围核对采用同一HWND快照。新增诊断不触发动作回归；96项回归及最终EXE烟雾／重启通过；首次构建原生菜单文件占用失败，回归结束后重构成功，没有停止PS。已正常退出空闲旧服务并切换统一入口v0.4.48，PS连接及原生组件哈希一致；备份 `dist/0.4.47-backup`，证据 `.runtime/release-0.4.48-verification.json`。诊断持续记录，实际漏键根因与修复仍待闭环。

- v0.4.47：用户报告快速划过色块后在中心／空白松键仍执行。日志00:02:03释放冻结坐标与已提交高亮均为绿色并执行，00:02:04旧高亮6但冻结命中空白正确取消；不足以证明用户感知时刻和采样一致。发现提交只验证冻结点／已提交高亮，不检查最新指针。新增提交即时位置仅作取消否决，三者不同则hide，不能改选新项，无新增等待。原生自测遍历8色离开中心／移到其他色取消、冻结空白晚到色块不能选、同色正常且单次执行；日志增加冻结命中、当前位置和当前命中。95项回归及最终EXE烟雾／重启通过；已正常退出空闲旧服务并切换统一入口v0.4.47，PS连接及原生组件哈希一致，备份 `dist/0.4.46-backup`，证据 `.runtime/release-0.4.47-verification.json`。用户确认快速划过后移到中心／空白松键不再误执行，正常停在色块松键选色也可用，本次实机验收通过。

- v0.4.46：CapsLock保留范围改用共用PS进程缓存，不再依赖PS弹窗类别／所有者链；仅配置含CapsLock时消费，非工作区按下不产生菜单事件或组合脚本，Tab／其他导航按键不受扩展范围影响。释放进入保护窗口须HoldCancel，防止保护范围被当作可执行范围；重复及跨应用释放沿用首次消费，外部按住再切入不半途捕获。原生自测新增一万次保护窗口配对、组合不执行、工作区进入保护后释放取消、外部按下进入PS不捕获及下一次正常短按固定。95项回归及最终EXE烟雾／重启通过，已正常退出空闲旧服务并切换统一入口v0.4.46，PS连接及原生组件哈希一致。备份 `dist/0.4.45-backup`，证据 `.runtime/release-0.4.46-verification.json`；用户确认真实PS连续短按／长按、右键弹窗CapsLock拦截、大写状态不变及外部应用CapsLock保留均正常，本次范围实机验收通过；PS重启／跨应用按住释放仍待专项实机验证。

- v0.4.45：左Win改用前台HWND所属进程匹配，启动前及后台约每秒枚举并核验Photoshop.exe，缓存PID与进程句柄；钩子只查窗口PID和进程存活，失效句柄立即拒绝，避免PID复用。原PSFloatC／系统菜单类别补丁删除，PS进程内未知窗口与对话框均可右键映射，通用命令和CapsLock继续使用工作区／输入保护。独立重命名Photoshop.exe测试进程创建Photoshop、PSFloatC和未知类别窗口，验证接受；原HotkeyHost.exe伪造同名窗口全部拒绝，并覆盖禁用窗口及失效PID句柄。95项回归及最终EXE烟雾／重启通过；只读实际窗口检查PS主窗口和PSFloatC为true、系统DV2ControlHost为false，无注入用户文档操作。统一入口已切换v0.4.45，PS连接及原生组件哈希一致，备份 `dist/0.4.44-backup`，证据 `.runtime/release-0.4.45-verification.json`；用户确认新版PS内及已打开的右键菜单中连续左Win正常映射，切到其他应用左Win正常打开系统菜单，两个范围实机验收通过；PS重启后恢复、跨应用按住再释放及右键拖动仍待专项实机验收。

- v0.4.44：用户复现上一版仍漏键，临时只读WH_KEYBOARD_LL记录显示漏键时前台PSFloatC（PID50316），所有者为Photoshop主窗口，GUI菜单标记为0，键盘flags仅扩展／释放无注入标记；随后前台变为系统DV2ControlHost。证据 `.runtime/trace-left-win.jsonl`。仅左Win映射新增PSFloatC范围，须所有者工作区有效且同进程、可见／启用；不扩展通用命令作用域。新增4项真实隔离Win32窗口回归，覆盖PSFloatC接受但一般工作区拒绝、外部同名窗口拒绝、隐藏／模态禁用拒绝及对话框拒绝。97项回归及最终EXE烟雾／重启通过，已切换统一入口v0.4.44，PS连接与原生组件哈希一致；备份 `dist/0.4.43-backup`，证据 `.runtime/release-0.4.44-verification.json`。旧版误触发已实机复现；用户确认新版在PS及已打开右键菜单状态下连续按放左Win正常，系统Win菜单不再误弹，连续漏键修复实机验收通过。

- v0.4.43：用户报告左Win第一次屏蔽、第二次触发系统Win；发现映射范围遗漏PS原生菜单状态。新增仅左Win使用的菜单循环／系统弹出菜单范围检查，要求菜单所属工作区有效且前台与所属工作区同一PS进程；键盘消费与消息循环注入均采用此检查，保留物理按键配对、跨应用释放、右Win及其他快捷键原作用域。93项回归及最终EXE烟雾／重启通过；已正常退出空闲旧服务并切换统一入口v0.4.43，PS连接及原生组件哈希一致，备份 `dist/0.4.42-backup`，证据 `.runtime/release-0.4.43-verification.json`。未向用户文档模拟右键；实体连续左Win验收待完成，该次根因未经实机复现确认。

- v0.4.42：运行状态移至底部文档名下一行，使用预热的小字号字体，与名称保持5逻辑单位行间距；未改动命中、hover分流与按键取消逻辑。93项回归及最终EXE烟雾／重启通过；布局预览 `.runtime/status-caption-0.4.42.png` 已检查，状态在名称下方且无重叠。已正常退出空闲旧服务并切换统一入口v0.4.42，PS连接和原生组件哈希一致；备份 `dist/0.4.41-backup`，运行证据 `.runtime/release-0.4.42-verification.json`。实体PS显示验收待完成。

- v0.4.41：用户确认松键时在中心／空白且无高亮，日志有一次长按释放发出label.blue；旧记录缺少坐标，不能确定该次实际根因。发现生产路径松键经过管道／后台后再GetCursorPos，存在晚取样窗口；改为钩子回调冻结坐标并经新WM_APP+21提交，须匹配已Present成功帧中的命中，否则取消。每次prepare清空高亮，结束后不恢复；旧菜单命令在获得窗口锁后再次检查会话，防止排队期间已重开仍执行。松键与选择决策新增坐标／高亮／最终候选日志。93项回归通过，覆盖冻结负屏幕坐标与重复释放、旧操作等待锁后重开不执行；新增原生自测覆盖空白冻结坐标、不显示高亮、不匹配高亮取消及真实高亮选色。隔离真实HWND验证负坐标SendMessage可传递，未向PS注入按键或操作文档。最终EXE烟雾／重启通过，统一入口已切换v0.4.41，连接PS及原生组件哈希一致，备份 `dist/0.4.40-backup`，证据 `.runtime/release-0.4.41-verification.json`。保留短按固定、长按松键执行已高亮选项，无新增停留阈值；实体反复开关移动鼠标仍待验收。

- v0.4.40：用户确认不接入AHK，左Win仅PS及自定义菜单映射右键。新增原生物理按键映射状态机，首次按下决定消费，自动重复不发送额外点击，释放沿用首次决定；外部应用按住Win再切入PS不接管，启动已按住时不捕获中途按键。右Win不变。SendInput在热键消息循环发送右键按下／松开，前台已改变时跳过新按下；已发送的按下在释放／正常退出配对结束，保留钩子续装状态。菜单内映射右键沿用取消和焦点恢复。原生自测新增一万次配对及跨作用域／启动持有回归，91项回归和最终EXE烟雾／重启通过。已正常退出空闲旧版，备份 `dist/0.4.39-backup`，统一入口切换v0.4.40，PS连接及菜单／热键组件哈希一致，证据 `.runtime/release-0.4.40-verification.json`。未向用户文档模拟点击，实体左Win右键点击／拖动、菜单取消、跨应用释放与右Win保留仍待实机验收。

- v0.4.39：用户报告未操作却显示最近操作失败。实机日志确认先有菜单焦点拒绝，后有label.gray结果读回的PS Get/8800异常；原实现把所有notice错误归为taskFailed并跨重开保留。新增notice的operation／interaction分类，只有实际脚本执行分支产生任务结果；原生任务结果与普通提示分离，历史显示“空闲 · 上次失败／上次已完成：具体操作”，交互错误不再修改任务结果。91项回归及最终EXE烟雾／重启通过；新增原生断言覆盖无任务交互错误、灰色标签失败命名、跨重开保留及后续交互不覆盖。统一入口已切换v0.4.39，PS连接和组件哈希一致，备份 `dist/0.4.38-backup`，证据 `.runtime/release-0.4.39-verification.json`。未对用户文档重试标签命令；本次修复提示分类，灰色标签在当时文档读回失败的具体原因及实体屏幕提示仍待进一步验收。

- v0.4.38：菜单下方使用预热的小字号字体显示活动文档名，UTF-8长名称安全省略，不改变按钮／标签命中。窗口监视器约100ms读取活动工作区标题，仅变化时推送，打开菜单补读；原约3秒COM状态同步保留，无新增高频COM调用，无法解析标题时使用COM结果，忙碌状态可能使用缓存。实机只读确认浮动Untitled-1和主窗口Untitled-2标题分别正确提取；91项回归通过，覆盖Unicode、文件名内@、小数缩放及文档切换只在变化时推送；最终EXE烟雾／重启通过。预览 `.runtime/document-caption-0.4.38.png` 已检查，小字不覆盖按钮；统一入口已切换v0.4.38，PS连接和组件哈希一致，备份 `dist/0.4.37-backup`，运行证据 `.runtime/release-0.4.38-verification.json`。真实PS切换文档后的屏幕刷新手感尚未实机验收。

- v0.4.37：针对浮动文档／面板不触发，真实枚举确认前台文档类为OWL.DocumentWindow、浮动面板为OWL.Dock；旧判断只接受Photoshop主类，浮动文档被按弹窗处理。原生钩子及后台改为同进程＋PS根拥有者＋限定工作区类判断；禁用或隐藏窗口、真正对话框、外部进程不放行，PS模态对话框仍保留CapsLock拦截但不执行。hover及点击归一工作区，仍保留输入焦点保护，布局允许属于该工作区的浮动窗口区域，关闭恢复原唤出窗口。只读实机与原生交叉核验浮动文档canvas、停靠图层layers、浮动属性generic，证据 `.runtime/floating-workspace-0.4.37.json`。初次全套回归有4个旧测试替身缺少新增Win32 API，补齐后88项回归及最终EXE烟雾／重启通过；已正常退出空闲旧版，备份 `dist/0.4.36-backup`，统一入口切换v0.4.37，PS连接及菜单／热键组件哈希一致，证据 `.runtime/release-0.4.37-verification.json`。未修改用户文档；浮动模式实体CapsLock、右键关闭重开和文档切换仍待实机验收。

- v0.4.36：统一12项命令契约，接入实际API和COM执行前校验，不支持的参数在关闭菜单或提交操作前拒绝。清理当前TODO中过时的快速松开取消、颜色名称提示描述。用户澄清右键关闭后不能再次触发；日志显示dismiss以restore=False隐藏后焦点仍在菜单，下一次hold_begin被PS焦点保护拒绝。右键改发Hide恢复菜单持有的PS焦点，并清除当前pending手势；仍消费右键按下／抬起，PS／菜单以外不抢焦点。84项回归和3项前端测试通过，最终EXE烟雾／重启通过，统一入口已切换v0.4.36，连接PS且原生组件哈希一致，备份 `dist/0.4.35-backup`；运行证据 `.runtime/release-0.4.36-verification.json`。实际服务的commands.list及非法参数拒绝已核验，未向用户文档提交修改。首次完整构建受本机npm.ps1包装器影响，直接npm CLI构建成功；中间构建烟雾遇一次临时脚本包目录重命名拒绝访问，最终构建整套通过。右键关闭→实体CapsLock重开仍待PS实机验收；任务队列、目标ID锁定及UXP尚未实现。

- v0.4.35：CapsLock短按≤220ms松开后原地固定，长按>220ms松键按命中执行或空白取消；固定后再按CapsLock关闭，右键／Esc保持取消。移除双击时序，固定不隐藏重开。77项回归通过，覆盖短按／长按、重复按键、焦点丢失、组合键使用及固定不重开；最终EXE构建及烟雾／重启通过。统一入口已切换v0.4.35，Photoshop连接正常，菜单与热键组件哈希一致，旧版保存在 `dist/0.4.34-backup`，运行证据为 `.runtime/release-0.4.35-verification.json`。实体短按／长按手感尚未验收。

- v0.4.34：保留CapsLock唤出。PS主窗口拥有的弹窗保留CapsLock拦截但不执行命令，Tab等导航保留；每秒先安装新键鼠钩子再撤旧，保留按住／消费状态，连续三次安装失败退出并由后台恢复。77项回归及最终EXE烟雾／重启通过，新增30次人工移除钩子后重新安装与按住配对回归，无模拟输入。统一入口已切换v0.4.34并核验实际原生组件哈希，备份dist/0.4.33-backup，证据 `.runtime/release-0.4.34-verification.json`。用户本次漏大写的实际原因尚未确证；定期续装仍有约一秒恢复间隔，真实PS弹窗及长期实体按键稳定性尚未验收。

- v0.4.33：按钮内侧半径100→65，减少35%鼠标行程；中心环按32ms时间常数平滑跟随鼠标，跨180度走最短路径，空白用次要色、实际命中用强调色。仅视觉插值，命中不延迟。回归发现旧空白测试点因收近落入按钮及色块，已改为实际空白点；最终76项回归及EXE烟雾／重启通过。统一入口已切换v0.4.33，备份旧版至dist/0.4.32-backup，实际组件核验 `.runtime/release-0.4.33-verification.json`。实体距离手感和环动画尚未验收。

- v0.4.32：按当前边长放大50%解释用户要求，环绕色块14×14→21×21，容器98×56，无间距和无文字保留，命中同步放大。76项回归及最终EXE烟雾／重启通过；正常备份旧版并切换统一入口v0.4.32，菜单／热键哈希一致，证据 `.runtime/release-0.4.32-verification.json`。实体视觉与键鼠验收尚待确认。

- v0.4.31：用户报告未选中却在收起时触发脚本。移除按钮外的方向扇区命中，限制为实际卡片／色块；动画未完成和选择已结束时禁止命中。76项回归及最终EXE烟雾／重启通过，覆盖中心／空白取消、动画取消、卡片命中、颜色命中、重复释放。实体PS收起过程误触发仍待用户验证；运行版本核验见 `.runtime/release-0.4.31-verification.json`。

- v0.4.30：用户确认v0.4.29菜单不显示。发现淡入条件依赖WM_SHOWWINDOW缓存，而生产入口为SetWindowPos(SWP_SHOWWINDOW)，缓存可能不更新。已改为读取IsWindowVisible；新增真实窗口10次显示→完全不透明→松键→隐藏回归，交替覆盖固定模式忽略松键。76项回归及最终EXE烟雾／重启通过；正常退出空闲旧服务并备份至 `dist/0.4.29-backup`，统一入口已切换v0.4.30，已连接PS且原生组件哈希一致，证据 `.runtime/release-0.4.30-verification.json`。此前纯窗口过程状态测试漏掉生产显示路径，已补充AGENTS规则。测试检查真实HWND可见性和合成透明度，仍不等同真实PS实体按键及屏幕视觉验收。

- v0.4.29：用户报告单次CapsLock闪两次；日志中多数单次操作仅一组hold_begin／hold_release，代码确认重新准备时先把保留旧帧的合成表面设为不透明，首帧动画再归零。已改为准备／显示从零透明度开始，结束／隐藏立即归零，关闭后的绘制不恢复透明度；新增100次旧帧→准备→显示→隐藏回归。75项回归和最终EXE烟雾／重启通过；旧服务空闲时正常退出，备份至 `dist/0.4.28-backup`，统一入口已切换v0.4.29，原生菜单／热键组件哈希一致并已连接PS，证据 `.runtime/release-0.4.29-verification.json`。保留低延迟等待与60ms展开，未增加按键延迟；真实屏幕单次按键是否完全消除双闪仍待实机确认。

- v0.4.28：单帧队列、可响应消息的绘制前等待、一次提交唤出状态和避免同尺寸缓冲重建已交付；75项回归和最终EXE烟雾／重启通过。新旧独立进程各180次消息响应P95为3.935ms／0.089ms；各30次重开可观测CPU渲染进度中位数为2.648ms／3.057ms，未证明首帧显示更快，证据 `.runtime/menu-response-before.json`、`.runtime/menu-response-after.json`。原生自测覆盖各面板和输入保护的一次提交；连续重开无停帧。首次更新因用户正在执行图层操作而自动拒绝；确认操作完成后正常退出旧版，备份 `dist/0.4.27-backup` 并切换统一入口，实际v0.4.28及原生组件哈希核验见 `.runtime/release-0.4.28-verification.json`。实体按键到屏幕延迟、持续PS使用稳定性仍未验收。

- v0.4.27：针对全局锯齿／黑边改用DirectComposition逐像素透明，移除LWA_COLORKEY，修正预览PNG预乘转换。74项回归及最终EXE烟雾通过；透明预览检测到166档Alpha，浅灰／深色叠加灰底的边缘效果已检查，证据 `.runtime/alpha-verification.json` 和 `.runtime/alpha-light-gray-composited.png`、`.runtime/alpha-dark-composited.png`。旧服务空闲时正常退出并备份至 `dist/0.4.26-backup`，统一入口已切换v0.4.27，连接PS并核验菜单／热键组件哈希一致，证据 `.runtime/release-0.4.27-verification.json`。真实屏幕视觉、动画及实体键鼠验收尚未完成，不以预览代替。

- v0.4.26：处理边缘移位导致起点取消区覆盖色块的路径，增加八种起点下逐像素命中回归和固定模式覆盖。74项回归及最终EXE烟雾通过；统一入口已切换v0.4.26，组件哈希一致，证据 `.runtime/release-0.4.26-verification.json`。用户已确认表现为部分色块无悬停高亮；是否覆盖此次全部情形仍待实体鼠标确认。

- v0.4.25：用户确认重影／毛边／发虚，移除透明标题和状态的偏移副本，统一正文对比度，文字与动画位移对齐物理像素。19项菜单回归通过，浅灰／深色预览 `.runtime/text-clean-light-gray.png` 与 `.runtime/text-clean-dark.png` 已检查。初始两轮EXE测试出现可见／隐藏帧变化，检查发现测试实例启用全局热键可能受用户操作干扰；改为隔离实例从启动即禁用热键后，EXE烟雾及重启通过。实际入口已切换v0.4.25，原生组件哈希一致，证据 `.runtime/release-0.4.25-verification.json`。实体屏幕清晰度尚待用户确认，不能将渲染预览等同实机验收。

- v0.4.24：按需字符集合及三档字号缓存已实现。动态补字首次测试发现BuildRanges追加旧结束标记问题，已修复。74项回归和最终EXE烟雾通过，预览 `.runtime/font-cache-preview.png` 已检查。自测首次构建16ms、补字31ms，证据 `.runtime/font-cache-benchmark.json`；正式进程三档一起构建32ms，相比上版单档422ms约减少92%，切换large复用缓存无重建。统一入口已切换，原生组件哈希一致，证据 `.runtime/release-0.4.24-verification.json`。构建耗时不等于端到端按键延迟，实体弹出手感尚未验收。

- v0.4.23：针对用户确认的CapsLock弹出卡顿，将待更新字体预热移至隐藏等待之前，并新增字体及弹出分阶段耗时日志。74项回归和最终EXE烟雾通过；实际入口已切换v0.4.23，原生组件哈希一致，证据 `.runtime/release-0.4.23-verification.json`。正式进程日志显示小字号构建453ms、大字号422ms；核验菜单仍隐藏（IsWindowVisible=0）时大字号已构建完成。此数值是字体构建耗时，不是端到端弹出改善测量；实体弹出手感未验收。

- v0.4.22：处理用户PS重启后Font Atlas not built断言。正常字号动态切换未复现；确认旧代码忽略GPU资源创建失败，在隐式构建字体前失败会进入未构建图集的新帧。新增显式图集构建、GPU失败恢复与新帧前检查，测试注入此失败路径。故障注入首次暴露恢复后主视口平台数据被销毁，已补齐Win32后端与显示回调重建；最终73项回归及EXE烟雾通过，实际入口已切换v0.4.22，原生组件哈希一致，已连接PS并读取large字号，证据 `.runtime/release-0.4.22-verification.json`。旧服务此前已停止，替换前再次确认无遗留菜单进程，旧版保存在 `dist/0.4.21-backup`。用户当次GPU失效原因未确证，真实关闭／重启PS后实体唤出仍待验收。

- v0.4.21：读取PS界面字号并同步菜单字体，实机查询为Large；档位映射11/12/13逻辑em，仅应用一次DPI。73项回归、大字号原生自测及最终EXE烟雾通过，预览 `.runtime/ps-font-large.png` 已检查。实际入口已切换v0.4.21，原生组件哈希及实机字号large核验通过，证据 `.runtime/release-0.4.21-verification.json`；首轮核验早于PS状态刷新，刷新后核验通过。PS字体逐像素比较和实体操作尚未验收。

- v0.4.20：移除标签区域标题、悬停色名及文字tooltip，色块保持原尺寸并居中，卡片70×42。18项菜单回归和最终EXE烟雾测试通过，预览 `.runtime/labels-icon-only.png` 已检查；实际入口已切换v0.4.20，原生组件哈希一致，证据 `.runtime/release-0.4.20-verification.json`。实体键鼠尚未验收。

- v0.4.19：PS配色保留，标题／状态无底板、删除常驻教程横条、中心环连续角度及双描边、方向数字快捷键与对齐已实现。72项回归（包括原生数字键单次执行与画布／输入／忙碌保护）、最终EXE烟雾测试通过，预览 `.runtime/blender-0.4.19.png` 已检查；统一入口已切换运行，原生组件哈希一致，证据 `.runtime/release-0.4.19-verification.json`，旧版本备份 `dist/0.4.18-backup`。完全复刻仍有整窗淡入／仅按钮淡入、抗锯齿及边缘布局差异，实体操作尚待验收。

- v0.4.18：CapsLock短按后300ms内再次按下固定环绕菜单，第二次松键不执行、不收起；固定后点击动作／色块执行，右键／Esc取消，原单次按住滑动释放保留。仅自定义菜单可见且处于作用域时消费右键取消，未显示时保留PS原生右键。菜单事件带会话号，忽略旧菜单排队关闭。新增双击时序、固定后释放、旧关闭等待锁回归；72项测试及最终EXE烟雾通过，实际入口已切换，组件哈希一致，证据 `.runtime/release-0.4.18-verification.json`。实体双击及右键手势仍待实机验收。

- v0.4.17：环绕菜单标签卡片100×70→70×50（面积50%），色块20×20→14×14（面积49%），保留两排四列无间距布局；清除斜线按色块比例缩放，标题缩短防止溢出。8项原生菜单回归及最终EXE烟雾测试通过，预览 `.runtime/labels-half-area.png` 已检查。统一入口已切换运行，原生组件哈希一致，证据 `.runtime/release-0.4.17-verification.json`；实体鼠标命中手感仍待用户实测。

- v0.4.16：只读核验本机 Blender 5.2.1 LTS 默认参数，采用60ms线性中心展开／淡入、100单位半径、30单位按钮、0.4圆角系数、12–19单位中心环和11pt字体，打包同版 Inter／Noto CJK 及OFL许可；校准stb与FreeType的em换算。保留PS配色、无缝色块及任务状态。动画中使用稳定方向判定并暂停色块提交，避免重叠误选。70项回归、最终EXE烟雾通过；30ms／60ms预览已检查。已正常切换统一入口运行，菜单／热键／字体哈希一致，运行证据 `.runtime/release-0.4.16-verification.json`。真实PS连续手势与屏幕淡入尚待实体交互验收；因渲染器、PS主题及扩展内容不同，不宣称与Blender逐像素相同。

- v0.4.15：修复已绑定 CapsLock 在额外修饰键下被放行，以及菜单隐藏后尚未交还前台时作用域失效的漏拦截路径；同一次物理按键的重复／释放保持首次决定。新增左右修饰键、跨作用域自动重复、按住期间改配置／禁用和未绑定放行回归，一万次按键配对及70项测试通过。已切换统一入口，原生组件路径／SHA256 核验见 `.runtime/release-0.4.15-verification.json`。首次 EXE 烟雾重启阶段出现一次“菜单启动时可见”失败，独立重跑整套通过，尚不能确认首次异常原因；实体 CapsLock 长时间稳定性仍需实机复测，不能将状态机测试视作系统钩子永不失效的保证。

- v0.4.14：八个标签色块两排四列无缝拼接，选中边框内嵌；修复 v0.4.12 引入的隐藏竞态跳过 `UpdatePlatformWindows()` 导致下次 NewFrame 断言的问题。每帧完整更新平台窗口，主菜单隐藏时不显示附属窗口。新增连续 100 隐藏帧回归，70 项测试及最终 EXE 烟雾通过，预览确认色块无间隙。旧服务已退出并备份，统一入口已切换 v0.4.14，原生组件路径／SHA256 一致，证据 `.runtime/release-0.4.14-verification.json`。用户真实连续按松手势仍待复测。

- v0.4.13 最终原生状态回归和 EXE 烟雾测试通过，布局预览确认提示不覆盖按钮；已备份 v0.4.12 并切换统一入口，服务／原生组件路径及 SHA256 核验记录为 `.runtime/release-0.4.13-verification.json`。真实长任务执行期间的实体键鼠验收尚未进行，测试未对用户文档运行耗时脚本。

- v0.4.13：菜单新增任务空闲／执行中、操作名称、等待秒数和最近完成／失败提示，重开菜单保留状态，任务执行中继续禁止重复触发。监测范围为本菜单提交的操作，不涵盖 Photoshop 全部内部任务。70 项原有回归通过；新增原生状态回归覆盖重开、刷新计时、超时仍运行和完成／失败。最终构建与运行核验记录见后续条目。

- v0.4.12：取消环绕菜单独立色块 tooltip，颜色名称保留在菜单内；主窗口隐藏后禁止旧帧重建独立浮动窗口。移除无标色功能的旧“参考颜色”选择器。70 项回归与最终 EXE 烟雾测试通过，统一入口已切换运行，原生组件路径／SHA256 核验通过，当前 PS 浅灰主题仍正确读取。证据：`.runtime/release-0.4.12-verification.json`。用户截图所示提示残留的实体操作复测仍待验收；第一张颜色选择器截图的实际来源尚未确证。

- v0.4.11：原生菜单背景、文字、边框、快捷键提示和悬停配色随 Photoshop 四档界面主题更新；后台每约 3 秒只读查询，忙碌／断开／未知枚举时保留最近配色。标签颜色与透明关闭机制保留。70 项回归测试通过；最后的标题底板修正后再通过 8 项原生菜单测试，四档预览已检查，最终 EXE 烟雾测试通过。
- v0.4.11 已正常退出旧服务并备份到 `dist/0.4.10-backup`，统一入口已切换运行；实机读取 Photoshop 当前 `light-gray`，菜单及热键组件路径／SHA256 均与本次构建一致。证据：`.runtime/release-0.4.11-verification.json`。没有修改用户 Photoshop 主题或文档；在 PS 首选项手动切换四档后的实体交互验收仍待进行，不能以模拟切换和渲染预览替代。

- v0.4.5 已有按住菜单代码、构建和显示预览；54 项后端／原生测试、3 项前端测试及 EXE 烟雾测试通过。
- v0.4.6：60 项后端／原生测试、3 项前端测试、前端构建及 EXE 烟雾测试通过；烟雾覆盖原生组件来源、重复启动、退出清理和重启持久化。
- 在真实 v0.4.4 服务运行时启动 v0.4.6，已验证拒绝复用并明确记录版本冲突，随后正常完成版本切换。管理页显示实际版本和程序路径，不再用旧版本号作占位。
- v0.4.8：65 项后端／原生测试通过，新增实际 ImGui 色块点击事件测试后又通过 8 项菜单回归；EXE 烟雾测试通过，已切换实际运行入口。运行证据：`.runtime/release-0.4.8-verification.json`；菜单预览：`.runtime/pie-labels-preview.png`，透明背景 alpha 已核验。
- `tools/verify-layer-labels.py` 在真实 Photoshop 24.0.1 的临时文档验证全部八项标签、单选、多选、清除及未选图层不变，临时文档已关闭、原文档已恢复。实体键鼠手势尚未完整验收，因此 P0 整体保持未完成。
- v0.4.9：68 项后端／原生测试、前端构建、EXE 烟雾测试通过；实机窗口结构和用户操作日志均确认能够识别 canvas/layers，分别渲染验证了两种菜单内容。修复后的清除命令在临时文档验证“已有颜色 → none”和“子层 none / 父组 orange”。
- v0.4.10：新增隐藏透明样式回归后，8 项原生菜单测试及 EXE 烟雾测试通过；已切换统一入口运行，菜单／热键二进制 SHA256 核验一致，右键接管为关闭。证据：`.runtime/release-0.4.10-verification.json`。实体键鼠闪烁表现仍需实机复测。
- `AGENTS.md` 已要求每次改动主动同步 README 使用说明与 TODO 验证状态，并在菜单改动时回归 hover、输入框、焦点及取消行为。
- 上述 UXP、任务队列与长任务能力为待办规划，尚未实现。

- 长期CapsLock诊断（2026-10-11）：用户确认v0.4.49随后又在应用颜色标签附近出现大写变化，整体问题继续未完成。已保存原始记录 `.runtime/capslock-incident-20261011/`；近期354条记录中PS范围未见blocked=false，00:56:48.231未配对UP已消费，未确定本次误切换根因。独立只读归档已运行（归档PID40088，启动器PID12864），输出 `.runtime/capslock-history/history.jsonl`；无3分钟截止，容量轮转约512 MiB，保留按键决定、菜单选择、脚本执行与错误。半行续读、轮转和去重检查通过，实际归档写入已核验；不能捕获服务未记录的事件，电脑重启需重启归档器。

- 钩子线程检查（2026-10-11）：确认每秒的refreshPhotoshopProcesses（进程枚举／路径验证）与renewHooks仍在安装低级钩子的同一消息线程执行；回调本身不执行COM、脚本或管道写入。独立只读扫描100次、416进程，中位11.76ms／P95 13.24ms／最大14.25ms，包含Python ctypes枚举开销，不是生产线程现场耗时，也未测钩子更新；不能据此认定超时。复发时段354条Caps记录无PS范围放行、无注入标记、无ERROR／重启，00:56:48.231未配对UP已拦截。队列满时当前静默丢记录／动作，无计数；尚不能区分回调漏收与诊断丢失。证据 `.runtime/hook-thread-audit.json`。建议后续把进程扫描移出输入线程，并补回调／维护耗时及队列丢失计数；本轮仅检查，运行版保持v0.4.49，未宣称修复。

- v0.4.50诊断增强：用户已确认01:02测试时大写异常在PS内先发生，44条Caps记录无PS放行、无孤立UP／注入／热键错误；Codex内两次新按是之后事件，不能当作本次根因。保存 `.runtime/capslock-test-010202/`。新增决定耗时、事件到达延迟、每秒扫描／更新耗时、维护间隔、回调峰值和队列丢失计数；健康消息只记录不派发操作的回归通过，原生状态机及30轮钩子更新自测通过，97项回归及最终EXE烟雾／重启通过。已正常退出空闲旧服务，备份 `dist/0.4.49-backup` 和 `.runtime/settings-before-0.4.50.json`，统一入口切换v0.4.50，PS连接／组件哈希核验通过，真实健康记录和长期归档写入通过。证据 `.runtime/release-0.4.50-verification.json`、`.runtime/hook-health-0.4.50-verification.json`。只完成诊断增强，漏键根因及增强后的实机复发验证仍未完成。

- v0.4.50快速连按复发（2026-10-11 01:13）：用户确认大写先在PS内出现，随后补充快速连按似乎必现。保存 `.runtime/capslock-incident-20261011-011300/`；61条Caps记录、395条维护记录，PS范围已收到的事件均拦截、队列丢失累计0、维护间隔约1000ms、更新均成功、无注入标记或ERROR。PS内决定最长361μs；01:12:33.314出现无配对DOWN的UP且已拦截。后续任务栏的正常放行不能解释用户确认更早的异常。回调峰值（含下游链）最大22.024ms，扫描最大62.086ms，更新最大1.099ms，未观察到明确长阻塞；不能排除记录之外的事件。新重点为快速连按配对／钩子维护交错；尚无根因结论，系统实际toggle变化尚未记录，不再把正常外部放行作为PS异常解释。运行版仍v0.4.50，长期诊断继续。

- 长按复发补充（2026-10-11 01:15）：用户补充长按后松开也似乎必现，调查范围扩展到松键触发，不仅快速连按。最新长按记录中的重复DOWN和配对UP仍均blocked=true、dropped_total=0；01:15:21.950 PS内UP决定6μs／到达延迟0。保存 `.runtime/capslock-incident-20261011-011300/long-hold-followup.json`。用户随后确认CapsLock灯亮／英文输入变大写，属于实际系统toggle变化。现有诊断未记录系统toggle改变时刻，blocked=true不能替代最终系统状态验证；根因仍未确定。

输入进程排查（2026-10-11）：发现3个AHK进程，分别运行change_4k.ahk、win11 user config.ahk和OneQuick.ahk。已只读检查前两份完整脚本及OneQuick的AHK／JSON／YAML／INI配置、扩展，未发现CapsLock、SetCapsLockState或键盘状态修改；OneQuick含动态Send能力，不能仅凭源码搜索排除运行时或驱动干扰。还运行Logi Options+、Wacom、Listary等输入工具，未关闭或修改它们。异常附近收到的Caps事件没有LLKHF_INJECTED标记，但漏收的事件无法据此排除注入；该结构不提供注入来源PID。普通SetKeyboardState只影响调用线程，不能设置全局键盘灯。进程快照 `.runtime/input-process-inventory.json`，AHK路径 `.runtime/ahk-window-inventory.json`。当前没有其他进程修改CapsLock的直接证据。

v0.4.51实体复测：用户确认长按松开、快速连按及菜单操作没有大写误切换、菜单正常。本轮日志110条PS范围Caps事件均拦截，无注入标记、队列丢失0，系统toggle无变化；长按期间3次维护明确跳过。证据 `.runtime/capslock-0.4.51-user-test.json`。本轮验收通过，长期稳定性仍观察，不据一次通过认定所有潜在原因已排除。
v0.4.52点击激活门禁（2026-10-11）：服务启动后及从其他应用返回PS后，先物理左键点击PS一次，才允许CapsLock菜单及快捷操作。只hover进PS即使其他工具让PS成为前台，也不获得命令资格；失去PS／菜单进程前台范围时清除资格。PS内持续移动鼠标仍正常分流画布／图层，无需每次点击；菜单内右键／Esc取消保留。从外部按住再进入不补触发，按键消费与外部正常行为继续按已有配对规则执行。未授权但PS已成为前台时，CapsLock仍保留拦截避免切换大写，不提交菜单动作。程序只接受真实左键，注入的点击不授予资格；左Win到右键映射保留，不用右键授予资格。

新增原生门禁回归覆盖hover进入无资格、PS内点击授权、内部移动保留、外部焦点撤销和点击跨焦点交接；原生状态／维护自测、98项回归、最终EXE烟雾／重启通过。旧服务空闲正常退出，备份dist/0.4.51-backup及.runtime/settings-before-0.4.52.json，统一入口已运行v0.4.52，PS连接／原生组件哈希核验见.runtime/release-0.4.52-verification.json。实体hover进入／点击恢复和菜单取消待用户验收，不能用自动测试代替。
v0.4.53主动激活修复（2026-10-11）：用户指出v0.4.52只能点击PS内部恢复，任务栏切回无法触发。现将真实左键点击主／副任务栏或任务栏缩略图、真实Alt+Tab按键识别为主动切换意图，在随后2秒内激活PS时授予资格，无需再点画布；PS内部点击继续有效。若实际切到其他非任务栏应用，待用意图立即丢弃；PS内部点击已使用的资格不遗留为后续切回授权。仅hover或失去前台后未经主动激活不能打开；从外部按住进入仍不补触发，PS保留Caps拦截避免大写切换。左Win右键映射、画布／图层分流、输入保护和取消行为保留。

新增原生主动切换与意图过期／已用点击不遗留的自测通过；98项回归、最终EXE烟雾／重启通过。复用未改动的MenuHost并重新构建HotkeyHost，实际统一入口已切换v0.4.53，空闲旧服务正常退出，备份dist/0.4.52-backup及.runtime/settings-before-0.4.53.json。PS连接、服务／原生组件路径和SHA256核验见.runtime/release-0.4.53-verification.json。任务栏／Alt+Tab切回和hover限制的实体交互待用户验收。
v0.4.54 Alt+Tab手势修复（2026-10-11）：用户确认v0.4.53问题为Alt+Tab切回PS无法打开。代码检查发现切换器临时前台会触发外部应用判定，过早清掉切换意图。现从真实Alt+Tab开始直到松开Alt跟踪完整手势，期间临时前台不消费意图，松开时PS已激活则授予资格，否则保留短期最终焦点交接。Alt检测同时使用已跟踪键位与事件ALTDOWN标记；长时间按住Alt后释放不受旧2秒起始时间影响。普通hover仍不授予，离开PS仍撤销资格，任务栏主动激活规则保留。

新增原生回归覆盖临时前台、长按Alt超过意图期、PS内松Alt确认、松Alt后最终焦点交接和离开后hover不继承；原生自测、98项回归、最终EXE烟雾／重启通过。旧服务空闲正常退出，备份dist/0.4.53-backup及.runtime/settings-before-0.4.54.json。统一入口已实际运行v0.4.54，PS连接和菜单／热键组件哈希核验见.runtime/release-0.4.54-verification.json。用户已确认v0.4.54 Alt+Tab正常、hover不触发；本轮焦点门禁实机验收通过。v0.4.53的失败记录保留。
v0.4.54实机验收（2026-10-11）：用户确认“Alt+Tab正常，hover不触发”。本轮Alt+Tab切回及长按Alt多次切换后的菜单唤出、仅hover不授权的验收通过；记录.runtime/activation-0.4.54-user-acceptance.json，实际服务仍v0.4.54。整体Caps大写问题继续长期观察，本次焦点门禁通过不代表已证明长期无复发。
v0.4.55文档／图层目标保护（2026-10-11）：现有12个命令的契约升级为v2。请求进入执行通道后先只读确认文档ID、选中图层ID集合和活动图层ID，成功后才关闭触发菜单并提交修改；执行前在同一次DoJavaScript调用中校验目标，再运行原脚本。文档或选择改变、图层删除、文档关闭时取消，不自动切回、不改用新文档或新选择、不重试。捕获与写入使用同一COM线程上保留的应用引用，不在中间GetActiveObject重连；instance_binding是每次目标捕获的连接绑定记录号，不是操作系统PID或跨PS重启的持久身份。目标快照进入服务日志和返回结果，长期归档器也保留该日志；文档／图层ID仅在当前PS会话有效，重启后不重放。

目标读取失败或超时时未提交修改；读取或写入尚未实际结束时仍保持busy，避免取消等待后重复执行。取消范围保持执行前；已启动的COM脚本不能强制中断。只保证进入脚本前目标不变，不把自定义脚本自身有意切文档的行为变成自动回滚。新增目标确认不改变Caps／左Win钩子、菜单分流、输入保护或焦点门禁；目标捕获／执行期间不持有菜单锁，允许重新唤出菜单。捕获后发现菜单会话已变，取消旧请求。

103项回归通过（捕获前后顺序、同一COM引用、非法快照、取消捕获仍busy、菜单重开保留及原有交互保护）；真实PS24.0.1临时文档验证正常写入、文档切换（图层ID可复用）、单选改变、多选数量不变但替换成员、删除图层、关闭文档，共6组通过。另一临时文档标签未改变，所有测试临时文档已关闭并恢复用户活动文档。证据.runtime/operation-target-photoshop-verification.json。多选重新排列顺序未真正改变PS返回的活动ID，没有将该案例误记为独立实机通过。

最终EXE烟雾／重启通过，旧服务空闲正常退出，备份dist/0.4.54-backup和.runtime/settings-before-0.4.55.json。统一入口已实际运行v0.4.55，未变更的原生组件沿用v0.4.54构建，路径／SHA256核验通过；12个实际命令契约均为固定ID策略。证据.runtime/release-0.4.55-verification.json。新版菜单实体键鼠正常操作复测待用户验收。UXP、持久任务队列与重启恢复仍未实现，目标读取快照的确认时刻不等于实体按键的硬件时间。

- [ ] v0.4.56 连续标色后菜单重开／运行反馈实机验收：v0.4.55 用户复测未通过，日志确认主动隐藏后前台仍是菜单的执行焦点误拒绝。已实现仅限主动恢复 PS 的隐藏菜单过渡授权、重开处理、外部窗口撤销，以及目标读取前即显示操作名称和等待秒数。工作线程执行焦点检查，忙碌时禁止重复提交，任务结束不关闭新菜单。105项回归及最终 EXE 烟雾／重启通过，实际统一入口已运行 v0.4.56，PS 连接与原生组件 SHA256 已核验。备份 dist/0.4.55-backup、.runtime/settings-before-0.4.56.json；证据 .runtime/release-0.4.56-verification.json。仍需确认连续标色后实体短按／长按能唤出、忙碌提示正常及画布／图层分流、输入保护、焦点限制、右键取消保持正常；未勾选目标保护整体验收。

- [ ] v0.4.57 任务栏切回稳定性实机验收：用户反馈 v0.4.56 间歇性无法唤出，未通过。日志有 PS 前台 command_scope=false 及正常打开两类记录；旧日志不足以唯一确定完整任务栏路径。现已跟踪任务栏物理左键按下至松开，保护中途焦点交接，松开时 PS 已激活直接授权，修复已有前台但无资格时意图不生效；最终进入其他应用撤销，纯 hover 不授权，Alt+Tab 保留。新增激活状态诊断；原生自测、105项回归、最终 EXE 烟雾／重启通过。实际统一入口已运行 v0.4.57，PS 连接、新热键组件及菜单路径／SHA256 核验见 .runtime/release-0.4.57-verification.json；备份 dist/0.4.56-backup、.runtime/settings-before-0.4.57.json。仍需实体反复任务栏切回、Alt+Tab、纯 hover 不触发、标色及取消行为复测。

- [x] v0.4.58 标签后立即重开菜单实机验收（仅本项）：用户反馈 v0.4.57 普通情况正常，但选色后无法唤出、稍后恢复，整体验收未通过。滚动日志确认标签约183／231／251ms完成，后续 command_scope 与 activation_allowed 均 true，后台中性隐藏后隐藏菜单占前台，新请求被焦点检查拒绝；证据 .runtime/label-0.4.57-incident.json。已将菜单 owner／恢复及监控目标绑定稳定 PS 主窗口，浮动文档仅用于标题；保留中性关闭和 PS 暂时禁用后的既有会话，主窗口禁用时仍拒绝操作，外部前台撤销会话。新增自动隐藏原因记录，原生门禁不变。106项回归、最终 EXE 烟雾／重启通过，实际统一入口已运行 v0.4.58；路径／组件 SHA256 见 .runtime/release-0.4.58-verification.json，备份 dist/0.4.57-backup 和 .runtime/settings-before-0.4.58.json。用户已回复“正常，选色后立即可反复唤出”，本轮浮动文档标色后立即短按／长按及切文档重开问题验收通过，证据 .runtime/label-0.4.58-user-acceptance.json。勾选仅代表标签后立即重开；忙碌提示、组内选择／排序及完整 hover／输入保护／取消实机回归未由这次回复新增验证。旧版本失败和长期观察记录保留。

v0.4.58 本轮实机验收（2026-10-11）：用户回复“正常，选色后立即可反复唤出”。浮动文档选色后立即反复短按／长按及切文档重开菜单的本轮问题验收通过，实际服务仍 v0.4.58，菜单 ready、操作空闲。证据 .runtime/label-0.4.58-user-acceptance.json。未据此认定忙碌提示、组内选择／排序或完整输入保护／hover／取消行为重新实机通过，也不把一次通过视为长期无复发；目标保护总项仍保留其剩余验收。

- [x] v0.4.59 实际服务任务数据／菜单重开日志核验（仅本项）：直接读取运行服务 /api/tasks 并合并轮转日志，8次标色／清除ID全部不同，状态均按目标读取→执行→成功结束，耗时156～234ms；前7次操作后均有菜单再次打开记录，当前空闲且last_error为空。最后一次没有后续打开动作，不视为重开失败。证据 .runtime/task-0.4.59-live-log-verification.json。后台页面视觉、队列及UXP不在此勾选范围。

- [x] v0.4.60 任务历史持久恢复（限定数据核验）：118项后台／3项前端测试、最终EXE启动／重启通过；真实PS临时文档标色／清除读回及持久结果／目标恢复通过。正常退出空闲旧服务并迁移13条实际任务，新服务API确认ID／目标／结果全部保留，persistent=true且无存储错误；实际统一入口版本及两个原生组件路径／SHA256核验通过。证据.runtime/release-0.4.60-verification.json、.runtime/task-history-0.4.60-live-verification.json、.runtime/task-lifecycle-photoshop-verification.json。备份dist/0.4.59-backup与.runtime/settings-before-0.4.60.json、.runtime/tasks-before-0.4.60.json。勾选仅代表数据恢复；新版实体键鼠与后台页面视觉、断电场景仍待实机验收，不宣称队列／UXP／步骤恢复已完成。

- [ ] v0.4.61 点击PS后激活资格丢失复测：13:23日志确认QQ前台CapsLock放行后大写点亮，随后PS内CapsLock全部拦截但activation_allowed=false，13:23:26资格恢复后菜单打开。用户确认点击PS切回，旧日志缺少点击轨迹，唯一根因尚未确定。已补充直接点击PS按下至松开期间的交接保护与只读点击诊断，松开PS已激活时授予资格，外部窗口撤销、纯hover不授权。原生自测、118项完整回归、增加点击诊断测试后的菜单重开21项及最终EXE启动／重启通过；实际统一入口v0.4.61及原生组件路径／哈希核验通过。证据.runtime/hook-switch-060-incident.json、.runtime/release-0.4.61-verification.json。备份dist/0.4.60-backup和.runtime/settings-before-0.4.61.json、.runtime/tasks-before-0.4.61.json。仍需实体点击切回、任务栏／Alt+Tab／纯hover、菜单取消和输入保护复测，长期CapsLock稳定性总项不勾选。

- [ ] v0.4.62 菜单点击误撤销PS激活资格复测：v0.4.61用户回复“第一次正常，后面无法触发，再点击一下才可以”，未通过。新增点击日志确认13:28:26自定义菜单点击被归类为外部应用并撤销allowed，后续PS内CapsLock全部屏蔽但不打开；再次PS点击恢复，证据.runtime/hook-switch-061-incident.json。仅本次菜单进程且PS目标有效的菜单点击保留已有资格，不授予新的资格，外部点击／纯hover限制不变。原生10000轮新增回归、维护自测、菜单／进程12项及最终EXE启动／重启通过，实际统一入口v0.4.62与两个组件哈希核验通过（.runtime/release-0.4.62-verification.json）；备份dist/0.4.61-backup和.runtime/settings-before-0.4.62.json、.runtime/tasks-before-0.4.62.json。待实体点击色块后连续唤出、取消、任务栏／Alt+Tab与输入保护复测；长期大写状态异常仍持续记录。

- [x] v0.4.62 持续唤出本轮实机确认（限定范围）：用户回复“可以持续唤出”；日志确认连续打开14次，PS／菜单内246条CapsLock均屏蔽且资格有效，多次选色后可重开。证据.runtime/hook-switch-0.4.62-acceptance.json。本轮未采集到菜单内物理点击事件，主要覆盖长按选色与连续重开，不据此将上一条完整物理点击／其他焦点路径验收全项勾选；长期稳定性继续记录。

- [ ] v0.4.63 读取目标阶段取消任务实机按钮验收：已实现后台列表取消按钮、tasks.cancel接口、重复取消幂等、读取完成前不释放busy及完成后禁止提交，取消不显示为失败；拒绝已执行／完成／历史任务。122项后台／3项前端测试、最终EXE启动／重启通过，真实PS临时文档读取时取消蓝色标签后红色保持不变、取消记录可恢复且后续清除正常（独立执行桥实机测试）。实际入口v0.4.63与组件哈希核验通过，运行接口拒绝历史／未知取消、前端资源及24条历史保留已核验。证据.runtime/task-cancel-0.4.63-live-verification.json、.runtime/task-lifecycle-photoshop-verification.json、.runtime/release-0.4.63-verification.json。备份dist/0.4.62-backup、.runtime/settings-before-0.4.63.json、.runtime/tasks-before-0.4.63.json。仍待后台按钮视觉和实体点击取消验收；运行命令不可强制中断，排队／UXP不属于本次实现范围。

- [ ] v0.4.64 状态缓存实机界面验收：125项后台／3项前端测试、最终EXE启动／重启通过。真实PS独立只读验证600ms延迟下20请求约156ms返回缓存、仅1次COM读取，完成后恢复新鲜；不将接口测量当作按键显示延迟。实际入口v0.4.64、两个原生组件哈希、状态API20次并发、WebSocket手动刷新、实际前端资源及24条历史保留已核验。证据.runtime/status-cache-photoshop-verification.json、.runtime/status-cache-0.4.64-live-verification.json、.runtime/release-0.4.64-verification.json；备份dist/0.4.63-backup、.runtime/settings-before-0.4.64.json、.runtime/tasks-before-0.4.64.json。仍需后台状态卡视觉、PS真实模态忙碌时菜单输入／取消和完整hover／输入／焦点限制复测；未改变命令目标校验，不声称全部COM已解耦。


- 历史方案v0.4.65（v0.4.66按用户确定性要求停用，不再作为交互排队验收TODO）：实现8项FIFO／30秒排队期限、同一固定文档与选择及原COM引用、排队取消、执行前目标／焦点／菜单会话复核。前项失败／曾超时／等待中断后取消后续，不自动重试；目标未固定或当前结果未知拒绝追加。135项后台／3项前端测试、最终原生菜单自测、最终EXE启动／重启通过。真实PS独立执行桥在自建临时文档读回FIFO颜色、确认取消／目标变化／排队期限均阻止修改，原文档恢复；证据.runtime/task-queue-photoshop-verification.json。实际入口v0.4.65、两个组件路径／哈希、队列状态字段、真实前端资源和取消接口保护核验通过，24条旧历史保留，证据.runtime/task-queue-0.4.65-live-verification.json和.runtime/release-0.4.65-verification.json。备份dist/0.4.64-backup、.runtime/settings-before-0.4.65.json和.runtime/tasks-before-0.4.65.json。实体菜单排队／取消、后台视觉及完整hover／输入／焦点保护仍待验收；用户模态等待和UXP未实现，因此总项不勾选。

- [ ] v0.4.66 忙碌拒绝新交互任务实体验收：菜单和action.execute生产入口不排队，连续请求不创建额外任务，完成后不补执行；原子接收锁覆盖同时到达，结果未知仍保持busy。原生菜单忽略旧queue_ready并禁用忙碌选择，允许原有唤出与取消，后台显示完成后重新选择。136项后台／3项前端测试、最终原生自测和EXE启动／重启通过；真实PS独立执行桥验证红色执行期间10次蓝色拒绝、无延后执行、完成后重新主动选蓝色正常，原文档恢复。实际入口v0.4.66、执行策略、两组件哈希及前端资源核验通过，38条旧历史保留，证据.runtime/busy-reject-photoshop-verification.json、.runtime/busy-reject-0.4.66-live-verification.json和.runtime/release-0.4.66-verification.json。备份dist/0.4.65-backup及.runtime/settings-before-0.4.66.json、.runtime/tasks-before-0.4.66.json。仍待实体busy菜单开关／选择禁止及完整hover／输入／焦点／取消、后台视觉验收。

- [ ] v0.4.67 忙碌仅中心圆环实体验收：运行中打开仅圆环＋文档名／操作耗时，无按钮或色块；旋转为不定进度，不显示虚构百分比。清空命中与高亮，完成后保持紧凑，明确关闭重开后才恢复菜单；不改忙碌拒绝。136项后台回归、最终原生自测和最终EXE启动／重启通过。原生生产SWP_SHOWWINDOW有实际绘制、紧凑命中／数字拒绝／完成不展开／重开恢复回归通过，原生透明PNG不同圆弧阶段检查和人工预览通过；不据此冒充PS实体交互。实际入口v0.4.67、PS连接、两组件哈希与原生诊断核验，38条历史保持。证据.runtime/busy-circle-native-verification.json、.runtime/busy-circle-native-test.log、.runtime/busy-circle-0.4.67-live-verification.json、.runtime/release-0.4.67-verification.json。备份dist/0.4.66-backup及.runtime/settings-before-0.4.67.json、.runtime/tasks-before-0.4.67.json。仍待忙碌期间实体唤出／关闭／完成重开、多DPI及完整hover／输入／焦点保护复测。

UXP手动部署后识别核验（2026-10-11）：用户截图显示 Plugins → Photoshop Menu Offline Probe → Run offline probe，PS新进程PID28976，说明本机24.0.1重新扫描后识别了用户目录的插件。本地接收器目前0条启动／心跳记录，不能将菜单入口识别等同于执行成功或完整离线验收。待点击命令核验脚本运行；网络记录仍有Ethernet／Local Area Connection为Up，不能仅凭Wi-Fi断开认定全机断网。证据 `.runtime/uxp-offline-install-verification.json`。

UXP探针执行修订：PS显示安装提示且命令入口可见；用户点击0.1.0反馈无反应，接收器也无记录，执行验收未通过。测试插件已修订为0.1.1纯JS命令入口，加上plugin-data内probe-last.json本地证据，版本信息缺失不会阻止通信。模拟验证通过，部署文件一致，等待用户断网重启并点击命令；根因尚未由真实错误日志证实，不将入口修改误报为实机修复。主程序仍v0.4.68。

UXP实验后续状态（2026-10-11）：0.1.1自动加载和持续heartbeat已由真实PluginData/probe-last.json证实（UXP6.3.3、读取文档数量正常）；本机fetch因Manifest entry not found被拒绝，网络通道未通过。0.1.2限定回环权限修订尚未加载验证。用户报告PS启动后自行退出并显示无法通信CCD；探针已移出到.runtime/uxp-probe-disabled-0.1.2备份，无测试插件时仍退出，按现有快捷方式工作目录启动也未恢复。未发现对应系统崩溃记录；停止重复启动，等待用户正常启动方式对照。没有重置PS偏好、修改Adobe程序或安装CCD，不能宣称完整离线安装及宿主恢复已通过。证据.runtime/uxp-offline-install-verification.json。

PS启动恢复对照（2026-10-11）：用户确认停用探针后，手动启动也仍提示无法通信Creative Cloud Desktop并退出。按Adobe首选项排障步骤备份并临时隔离完整Settings目录后启动一次，25秒后进程已退出；立即恢复原目录，34个文件SHA256全部一致（0差异）。备份及结果：.runtime/ps-startup-recovery-20261011-152019。失败启动仍未生成新UXP日志；这不能证明根因，也不能判定需要迁移或不能离线。探针保持停用，实际主服务仍v0.4.68；未修改Adobe程序、账号或授权数据。宿主尚未恢复，不继续反复启动或宣称修复。下一步需用户保存其他应用后重启Windows，再以原快捷方式对照启动；尚未安装CCD。

PS恢复更新（2026-10-11 15:22）：用户直接点击后成功启动；实际PID40732、入口仍为C:/Program Files/Adobe/Adobe Photoshop 2023/Photoshop.exe，与此前自动启动路径相同。产生15:21:59的新UXP会话日志；实际v0.4.68主服务已重新连接PS，状态新鲜、无任务运行、无打开文档。说明当前宿主已恢复，但之前CCD退出与本次成功的原因未定位，不能宣称已永久修复或重启Windows是必要步骤。探针保持停用，不再次重启、不改偏好。UXP离线通信与完整离线安装验收仍未通过。

UXP纯本地通信探针0.1.3（2026-10-11）：新增插件自身PluginData文件请求／回应通道，取消网络权限和fetch；只读inspect，带本机令牌、请求ID、30秒内期限及重复保护。Node协议模拟通过（不调用网络、元数据缺失容错、令牌／期限／操作／重复拦截）；Python验证脚本语法检查通过。用户明确允许丢弃Untitled-1后，核对无执行任务、仅该文档，正常关闭文档退出PS。3个部署文件核对哈希一致，生成本地0.1.3归档，旧版备份保留。按原快捷方式自动启动再次出现CCD错误，未产生新UXP会话；10秒真实请求测试超时，旧0.1.1心跳不算新版验收。已请求用户直接点击启动。主入口仍v0.4.68 COM；0.1.3实机请求回应、断网启动／重启恢复及完全离线安装尚未通过。证据.runtime/uxp-file-probe-deployment.json，工具tools/verify-uxp-file-probe.py。

UXP0.1.3实机通信通过（2026-10-11 15:31）：用户直接启动后PID51276，新心跳版本0.1.3、PS24.0.1、UXP6.3.3、phase=ready。后台提交全新请求9677ed7f17f8abcbcf9dcffe2a7a8880，收到同ID的只读inspect回应，往返126.85ms；非旧缓存、非COM模拟、无文档写入。生产v0.4.68仍COM且连接恢复。尚未证明断网安装／启动；新增5分钟本地观察器，仅在所有枚举网卡非Up且PS退出时重复制自有插件文件并校验，记录断网状态、进程及新请求回应；不改变网卡、不自动关闭PS或启动Adobe安装器。离线总项仍不勾选，证据.runtime/uxp-file-probe-verification.jsonl。

断网观察器补充：本机Xbox无线外设网卡可保持Up且无互联网出口，不能要求关闭该外设作为离线条件。观察器现分别记录网卡、活动默认／半默认出口路由与PS进程；在无活动出口路由且PS退出时重复制自有探针。路由缺失不能绝对排除局域网代理等路径，因此最终离线判定仍结合用户断网确认及同期新请求回应；不把网卡Up或单次路由状态单独当成联网／断网结论。代码tools/observe-uxp-file-offline.py，记录.runtime/uxp-file-offline-observation.jsonl。实际断网步骤待用户完成。

离线验收范围更新（用户要求，2026-10-11）：用户明确不再做整机断网，PS原已屏蔽联网。只读核验ActiveStore：Photoshop.exe出站Block已启用，Any配置文件／远端地址／协议／本地与远端端口；Domain、Private、Public防火墙均启用，没有更改规则。结合用户确认此屏蔽配置原已存在，以及此前PID51276直接启动后的0.1.3真实心跳与匹配请求回应，本机纯文件探针在现有PS联网屏蔽环境下可用。手动本地复制部署未使用Adobe安装器；不能据此宣称官方CCX安装、整机断网或所有PS版本兼容已通过。断网观察器已停止，不要求用户关闭Xbox外设等无关网卡。此后PS已退出，新请求因此超时；桌面Shell正常启动另尝试一次，仍无新UXP日志／回应且退出，停止自动启动尝试。现阶段重载使用用户直接点击启动；自动启动CCD问题仍未修复。主入口v0.4.68、执行端COM不变，UXP写操作／目标保护／取消与忙碌策略迁移尚未实现。证据.runtime/photoshop-network-block-rules.json、.runtime/photoshop-network-block-ports.json、.runtime/uxp-offline-install-verification.json及.runtime/uxp-file-probe-verification.jsonl。

UXP标签执行实验0.2.0（未切换生产）：已实现八种标签／清除、独占操作区内文档／活动图层／多选集合及插件会话复核、期限与PS取消检查、结果逐层读回、历史事务失败回滚，以及同会话请求去重。新增实验Python客户端与只操作自建临时文档的实机检查脚本。最终Node协议和标签保护模拟通过，Python语法检查通过；实际部署四个文件哈希一致，旧版备份.runtime/uxp-0.1.3-before-labels。用户要求自动启动后，通过Explorer再尝试一次，仍未加载新版UXP，请求超时；CCD问题未修复，等待直接点击启动后实测。八种颜色真实写入、多选、清除继承提示、与COM协调的全局忙碌拒绝／取消、菜单接入和生产切换尚未通过／实现，不勾选UXP迁移。主服务仍v0.4.68 COM，禁止将实验客户端并发运行或与菜单写操作混用。工具tools/verify-uxp-labels.py、tools/uxp_label_client.py。

UXP0.2.0标签执行实机验证通过（2026-10-11）：用户直接启动新版后，真实临时文档依次完成八种颜色含none；两层多选蓝色／清除亦逐层通过独立COM读回。更改文档ID、插件会话、选择集合的三类目标请求均被拒绝；过期红色请求没有执行，随后inspect仍回应。测试只对自建临时文档写入，finally关闭临时文档并恢复此前文档。第一次多选测试因COM全选图层命令不可用中止并清理，改用明确图层ID加入多选后完整复测通过。证据.runtime/uxp-labels-physical-verification.json。生产主入口仍v0.4.68 COM；未勾选整体迁移，下一步是生产跨进程单任务协调、取消／未知结果处理与标签菜单接入，再构建切换并实机回归。自动启动CCD问题未修复，不将此次用户直接启动误报为自动启动成功。

## v0.4.69：主菜单操作迁移至 UXP

实际运行入口 dist/PhotoshopMenu.exe 已切换 v0.4.69，插件0.3.0。主菜单与快捷键提交的八种标签／清除、橙色快捷操作、组内选择、顶部／底部排序全部经UXP插件执行，无COM写入回退。Photoshop主题、字体及连接状态仍保留COM只读查询；此版本不是完全移除COM依赖。插件未就绪时明确拒绝。任务通道及命令契约显示uxp-local-files；后台脚本页改为UXP状态，旧JSX包不能替换菜单操作。

执行保留前台／输入／菜单会话限制，UXP会话与文档／活动图层／多选集合在独占操作区内复核。单任务忙碌拒绝、不自动排队；跨进程文件锁保护请求通道。提交前仍可取消，提交后等待结果，不声称可强制中断。45秒等待超时保留结果未知和busy；传输最终超时也保持阻止，不自动重试或回退COM；检查PS后可重启服务。标签逐层读回、清除继承提示、排序／选择读回和失败历史事务回滚均在UXP内完成。

141项后台回归、3项前端回归、最终EXE启动／重启及原生组件检查通过。真实PS生产执行桥验证标签／清除、根级与组内排序和组内选择；最终运行EXE的action.execute端点实际执行红色／清除，独立读回正确、任务为UXP succeeded。临时文档已清理，原文档恢复。本次自动重载已观察到新的0.3.0心跳，不能据此认定之前CCD启动问题永久解决。实体CapsLock短／长按、完整hover／输入／焦点／取消行为未重新做人工逐项验收；代码回归和原生组件哈希检查不冒充这项验收。

旧EXE备份dist/0.4.68-backup；用户配置和历史备份.runtime/before-0.4.69-settings.json、.runtime/before-0.4.69-task-history.json；旧插件.runtime/uxp-0.2.0-before-production。证据.runtime/release-0.4.69-verification.json、.runtime/uxp-production-physical.json、.runtime/uxp-live-0.4.69.json、.runtime/tests-0.4.69.log、.runtime/smoke-0.4.69.log。UXP凭据保存在当前用户PhotoshopMenu/uxp-config.json，不打入EXE或提交Git；本机安装已配置，其他电脑仍需要单独部署插件与凭据，不承诺独立复制EXE即可运行UXP。

- [x] 本机主菜单执行链迁移 UXP（v0.4.69，所有主菜单操作已切换，生产执行桥与实际EXE端点实机验证通过）。
- [ ] v0.4.69 实体热键／hover／输入／焦点／取消完整复测；移除只读COM依赖另行实现。通用离线安装器已由v0.4.70实现，本项不再将它列为未实现。

## v0.4.70：离线分发包与迁移性能

- [x] 构建可分发单文件离线安装器与ZIP：`dist/PhotoshopMenu-OfflineSetup-0.4.70.exe`、`dist/PhotoshopMenu-Offline-0.4.70.zip`，附SHA256、说明、许可证与性能原始样本。内嵌程序／manifest/main/labels白名单及哈希核验通过；未打包本机凭据／设置／文档／日志，首次安装生成独立令牌。无需Python、网络或额外Adobe工具。
- [x] 实现用户安装、更新快照、失败恢复、卸载及最近安装回退，保留用户设置／任务历史／备份。运行服务时拒绝更新／维护；PS运行且插件变化时拒绝，插件相同时核验并保留文件不动。安装目录与快捷方式绑定实际文件／数据路径，处理Windows宿主AppData重定向。16项安装器／UXP专项回归通过；新用户凭据、回退／卸载与故障恢复通过隔离文件系统验证。
- [x] 本机实际安装更新与运行切换：最终离线安装EXE执行成功，Windows开始菜单启动的实际服务为v0.4.70；运行安装EXE与统一`dist/PhotoshopMenu.exe`哈希相同，原生热键／菜单组件与本次构建一致。真实EXE红色／清除逐次独立读回正确，任务通道UXP且成功，临时文档清理并恢复原文档。155项后台、3项前端、最终EXE启动／重启通过。真实安装版维护入口在服务运行时拒绝，未删除文件。
- [x] 最终分发完整性：ZIP CRC及外部SHA256通过，实际安装的维护EXE与最终分发安装器哈希一致；内嵌payload仅五项白名单（含SHA256清单），不含凭据。证据`.runtime/offline-package-0.4.70-verification.json`。
- [x] 真实PS迁移性能测试：COM／UXP生产执行桥，单层与八层场景各30次／路径，三次预热、交错顺序；120次计时操作全部独立读回正确。单层中位数160.66→78.52ms，P95 188.07→95.15ms；八层中位数177.70→92.10ms，P95 207.86→123.64ms。包含目标读取／保护／执行／核验／任务持久记录，不是实体按键到屏幕延迟。首轮暴露提交前文件锁冲突，修复后完整复测无失败；不能据此宣称长期稳定性或所有功能更快。说明见PERFORMANCE.md。
- [ ] 全新Windows用户环境完整离线安装／启动／卸载回退实机验收；其他PS版本、完整实体热键／hover／输入／焦点／取消、按键到首帧与大文档性能另行验证。

实际服务位于安装目录，当前由Windows宿主重定向到LocalCache/Local/PhotoshopMenu/app，开始菜单已绑定实际路径和数据目录；这不是另一份未启用的暂存构建。原v0.4.69统一入口备份`dist/0.4.69-backup`，设置／任务历史备份`.runtime/before-0.4.70-settings.json`和`.runtime/before-0.4.70-task-history.json`；安装快照位于当前数据目录install-backups，不对外分发。证据`.runtime/offline-install-final-0.4.70.json`、`.runtime/release-0.4.70-verification.json`、`.runtime/offline-maintenance-guard-0.4.70.json`、`.runtime/execution-performance-0.4.70.json`、`.runtime/tests-0.4.70.log`、`.runtime/smoke-0.4.70.log`。主菜单写入UXP，COM只读状态仍保留。


## v0.4.71：Photoshop 最低版本限制

安装要求固定为 Photoshop ≥24.0.1，不设上限。安装器读取本机EXE文件版本，比较前三段；24.0.1.112符合要求，24.0.0及以下拒绝，未检测到支持版本或不能确认版本也拒绝。多版本共存时允许安装到支持环境，但任何正在运行的低版本PS都会阻止安装；自定义位置可指定--photoshop-exe。插件manifest及新鲜UXP心跳使用相同门槛。仅manifest最低版本变化且代码／凭据一致的更新可在符合要求的PS运行时原子完成，不关闭用户文档。

实际安装并通过开始菜单启动v0.4.71，统一dist入口与安装程序哈希一致，原生菜单／热键组件哈希一致。PS24.0.1临时文档红色／清除独立读回通过，任务为UXP成功；清理临时文档并恢复用户Untitled-1。162项后台回归、23项版本／安装／UXP专项验证及最终EXE隔离启动／重启通过。最终冻结安装器对低版本元数据拒绝写入；该负例不等于安装真实旧版PS验收。ZIP CRC／SHA256、内嵌五项白名单及实际安装manifest最低24.0.1均通过。旧EXE备份dist/0.4.70-backup，配置／历史备份.runtime/before-0.4.71-*。

证据：.runtime/offline-install-final-0.4.71.json、.runtime/version-floor-rejected-0.4.71.json、.runtime/offline-package-0.4.71-verification.json、.runtime/release-0.4.71-verification.json、.runtime/tests-0.4.71.log、.runtime/version-floor-tests-0.4.71.log、.runtime/smoke-0.4.71.log。更高PS版本允许安装不表示兼容性已经验收；全新Windows用户环境、真实低版本PS拒绝安装及完整实体键鼠矩阵仍待验证。性能数据仍是v0.4.70的已测样本，本次未改执行算法。

- [x] 用户指定最低 Photoshop 24.0.1：代码／边界测试／冻结安装器检测／构建／实际安装和运行切换完成。
- [ ] 真实低版本PS安装拒绝、更高版本兼容和全新Windows用户环境实机验收。


## v0.4.72：UXP 管理统一到主程序后台

- [x] 主EXE白名单内置manifest/main/labels三份资源，首次缺插件／凭据异常自动打开管理页。用户在后台安装内置版、禁用／启用执行、导入／导出离线UXP ZIP、更新、回退和卸载；独立OfflineSetup不再是当前交付入口。统一包dist/PhotoshopMenu-Backend-0.4.72.zip及可导入插件包已构建，主EXE/ZIP/SHA256和原生资源一致性核验通过。
- [x] 忙碌及结果未知时拒绝管理，不排队、不强制结束任务。禁用持久化且在创建任务／读取目标前拒绝菜单和快捷键操作；保留驻留插件文件，彻底移除要正常关闭PS后卸载。安装／更新／回退／卸载要求PS已关闭，后台保持运行。最低PS24.0.1，本地版本检测，自定义目录输入。更新包兼容0.3.x、固定最低宿主与启动模式、限制本地文件权限、文件白名单／SHA256、拒绝降级和外部修改覆盖。ZIP导出不带凭据。
- [x] 实际安装目录与生产后台生命周期：关闭PS后真实安装、同版本ZIP导入更新、禁用／启用、回退、卸载及恢复通过，插件四份文件恢复且凭据不变。浏览器真实管理页安装、禁用与启用按钮及状态提示通过，截图.runtime/uxp-management-ui-0.4.72.jpg。实测发现原凭据格式重写，已修复为完整保留文件及额外字段；新增回退保留当前主应用安装收据保护。前述实机生命周期先于最后收据保护补丁，该补丁有专项回归，未再关闭PS重复正向回退。
- [x] 用户明确允许丢弃Untitled-1，已正常关闭并执行真实生命周期。自动启动仍遇到历史CCD通信错误；用户直接启动后新UXP会话就绪，真实临时文档经独立生产执行桥标色／清除、独立COM读回通过，并清理临时文档。最终主EXE实际运行v0.4.72，PS/UXP已连接，原生菜单／热键与构建哈希一致，dist和安装运行EXE一致。旧EXE/config/history备份dist/0.4.71-backup与.runtime/before-0.4.72-*。
- [x] 179项后台、3项前端、最终EXE隔离启动／重启通过，包含真实HTTP/WebSocket UXP管理API（隔离目录）、更新失败恢复、忙碌／禁用门禁、资源白名单、凭据格式保留及插件回退不回滚主应用收据。
- [ ] 历史记录（v0.4.72）：原自动启动方式的CCD根因未定位；后续通过既有桌面Explorer打开原快捷方式已单次成功，重复启动稳定性当时待验收；v0.4.73正常桌面COM助手的本机功能交付见文末，具体CCD环境变量及跨机长期稳定性仍待定位；新插件版本／全新Windows环境／其他PS版本矩阵另行验收。当前PS重载正向执行为独立桥；生产EXE action.execute因Chrome前台正确拒绝，等待PS前台50秒未满足，未创建操作任务。实体热键／hover／输入／焦点／取消完整正向复测未完成，不以源码测试代替。

证据：.runtime/uxp-management-live-0.4.72.json、.runtime/uxp-management-running-guard-0.4.72.json、.runtime/uxp-reloaded-0.4.72-verification.json、.runtime/backend-package-0.4.72-verification.json、.runtime/release-0.4.72-focus-wait.log、.runtime/tests-0.4.72.log、.runtime/smoke-0.4.72.log。性能数字仍沿用v0.4.70真实样本，未因本次管理集成重新标注为新测量。


## 桌面快捷方式自动启动对照（2026-10-11）

已改用现有桌面 Explorer 的 `Document.Application.ShellExecute` 打开原 `Adobe Photoshop 2023.lnk`，而非仅创建 Shell.Application 后直接打开 EXE。本次无需用户点击，Photoshop PID47176 的父进程确认为原 Explorer PID7672；约33秒后实际 v0.4.72 后台连接、UXP新会话 `1791710834710-f308362615ca4` 就绪，独立只读COM确认PS24.0.1可访问、0份文档，新心跳约699ms。没有关闭或修改文档、没有改Adobe文件或CCD配置。这证明本机此启动路径单次成功，不能据此确定旧错误根因或保证长期稳定。

保留工具 `tools/launch-photoshop-from-desktop.py`，指定原快捷方式后只启动一次、有界等待；PS已运行时安全跳过，不自动关闭或重启。实际验证已运行跳过分支及语法检查。示例：

```powershell
.\Scripts\python.exe tools/launch-photoshop-from-desktop.py --shortcut "C:\ProgramData\Microsoft\Windows\Start Menu\Programs\Adobe Photoshop 2023.lnk"
```

证据 `.runtime/photoshop-desktop-launch-verification.json`、`.runtime/photoshop-desktop-launch-ready.json`。工具供仓库后续自动重载使用，尚未加入主EXE／后台启动按钮；实际主程序仍v0.4.72，本次没有改动菜单或热键组件。

- [x] 本机既有桌面Explorer打开原PS快捷方式，单次自动启动及新UXP就绪实机通过。
- [ ] 多次冷启动／重启和其他Windows环境稳定性；按需将验证后的启动路径集成后台。


## COM 冷启动实测（2026-10-11）

用户要求测试 COM 启动。启动前进程枚举确认无 Photoshop，后台无运行任务；执行一次 `win32com.client.Dispatch("Photoshop.Application")`，等待后返回 `0x80080005`（服务器运行失败），未得到 Application 对象，PS 未保持运行，UXP 未进入新会话。与之前 PS 已运行时的 COM 连接成功不同，本次是真正冷启动失败。不能仅凭该 HRESULT 断言 CCD 是根因，不修改注册表、Adobe配置或授权文件，也不重复同一失败启动。

证据 `.runtime/photoshop-com-launch-verification.json`。暂不将 COM 作为生产自动启动默认方案，保留已成功的既有桌面Explorer打开原快捷方式路径；冷启动 COM 兼容问题仍待定位。

失败后用既有桌面Explorer原快捷方式恢复启动成功：PS PID33584／父进程7672，约35秒后v0.4.72后台连接和全新UXP会话 `1791711847742-ff6d7604b5cbd` 就绪，最终只读心跳年龄约230ms。证据 `.runtime/photoshop-desktop-after-com-verification.json`。该桌面路径本机已有两次成功记录，仍不等于跨环境或长期稳定性验收。


COM失败层级补充（2026-10-11）：系统17:43:20记录DistributedCOM事件10010，CLSID与Photoshop.Application注册类完全一致，消息为“没有在要求的超时时间内向DCOM注册”。注册启动命令为 `Photoshop.exe /Automation`；正常桌面启动实际命令不含此参数，父进程为既有Explorer。Windows文档说明COM本地服务器启动还会追加 `-Embedding` 并等待类注册，本次未捕获短暂失败PS的实际命令行，因此追加参数属于系统机制说明而非进程抓取证据。已运行PS的COM连接可用；可确认是冷启动未达到COM就绪，尚不能确定是自动化参数、启动环境或Adobe初始化的哪一项导致。其他CLSID的10016权限日志不归因为PS故障，不修改DCOM权限。证据 `.runtime/photoshop-com-startup-system-evidence.json`；系统机制参考 https://learn.microsoft.com/en-us/windows/win32/com/localserver32 。


## openFile.dist 启动方式与正常桌面环境对照（2026-10-11）

用户指定 `G:/Github/151500px-py/openFile.dist`。只读源码和同期Nuitka编译目录确认：通过 photoshop_python_api 0.22.1 的 `ps.Application()`，底层 comtypes先GetActiveObject，失败则CreateObject(dynamic=True)。默认先尝试未注册的 `.0`，再查Adobe版本注册表回退本机 `.170`。`.170`与无版本Photoshop.Application解析同一CLSID／同一Photoshop.exe /Automation；openFile.exe为asInvoker，实际进程由Directory Opus启动，普通用户中等完整性，与当前命令Python权限相同。未执行旧工具“测试运行”按钮，其辅助脚本包含taskkill和文档写入，不用于本轮对照。

最小原库对照：同一旧Python3.11／库在当前命令环境下因platform.machine()为空，误选32位注册表而无法检测PS；显式64位读取可见170.0。让既有桌面Explorer启动同一最小原库调用时machine/PROCESSOR_ARCHITECTURE为AMD64，CreateObject(Photoshop.Application.170, dynamic=True)真正冷启动成功，实际PS命令行 /Automation -Embedding，后台／UXP就绪，未写文档或偏好。

为区分库与环境，再正常关闭本轮测试刚启动的0文档／无任务PS，让既有桌面Explorer启动原pywin32 Dispatch冷启动代码：约31秒返回COM对象，PS24.0.1，0文档，v0.4.72后台连接及新UXP会话 `1791712600324-1b0b9fc35bb5` 就绪，实际PS PID35252，父进程1740（COM服务）。因此不是COM本身或/Automation参数必然失效，pywin32也无需改成comtypes；可工作的关键是通过正常桌面环境运行启动调用。尚未逐一分离出CCD依赖的具体环境变量，不能将PROCESSOR_ARCHITECTURE单项误判为CCD根因，也不修改注册表或DCOM权限。

证据 `.runtime/openFile-com-inspection.json`、`.runtime/openFile-original-api-context.json`、`.runtime/openFile-original-api-start-verification.json`、`.runtime/openFile-original-api-desktop-verification.json`、`.runtime/photoshop-pywin32-desktop-verification.json`。最后对照脚本报告名曾覆盖原冷启动失败文件，成功结果已独立保存，原失败记录依据先前已捕获的输出恢复并注明来源。实际运行主程序仍v0.4.72；本次为启动诊断，未新增后台启动按钮或更换菜单执行端。


## v0.4.73 正常桌面 COM 启动助手（2026-10-11）

- [x] 同一主EXE内置 --ps-launch-worker，既有桌面Explorer启动助手后调用 Photoshop.Application；后台不直接COM冷启动，不依赖外部Python／AHK／旧openFile工具，不修改Adobe、DCOM或联网配置。
- [x] 后台启动按钮、默认开启且可持久关闭的自动启动：正常后台启动／插件安装更新后触发；PS已运行跳过，关闭PS后不循环重开。后台阶段／用时反馈，90秒有界观察，未知结果不重试，后台重启不重新派发。
- [x] 忙碌拒绝、启动中重复请求拒绝、插件更换与新图层任务拒绝；每请求消费记录和文件租约，助手独立入口不启动第二后台。9项专项回归，188项后台及3项前端回归通过，最终冻结EXE烟雾／重启通过。
- [x] 实际入口v0.4.73正常桌面COM冷启动和新UXP会话就绪；UI按钮冷启动约31.98秒、后台自动冷启动约31.85秒，均为真实冻结助手，PROCESSOR_ARCHITECTURE=AMD64、existing_instance=false；重复请求拒绝，后台状态接口持续响应。开关关闭后真实后台重启不启动PS，开启后自动启动通过。
- [x] 实际入口生产 action.execute 临时文档红色／清除独立读回正确、UXP任务succeeded，原生菜单／热键与本次构建SHA256一致。测试仅关闭本轮启动的0文档／无任务PS，未强制结束或丢弃用户文档。
- [ ] 跨Windows用户／多PS注册版本／Explorer不可用环境及长期冷启动稳定性另行验收；本轮没有重新逐项做实体CapsLock、hover、输入保护、焦点与取消矩阵，不以回归测试冒充。

证据：.runtime/photoshop-start-0.4.73-live-verification.json、.runtime/photoshop-autostart-0.4.73-live-verification.json、.runtime/release-0.4.73-verification.json、.runtime/tests-0.4.73.log、.runtime/smoke-0.4.73.log。统一包包含同一主EXE与插件资源；安装后的实际入口不是暂存新版。

- [x] 真实后台“安装／更新为内置版本”按钮部署插件后自动冷启动通过：冻结助手约32.16秒，新的UXP会话ready，重复启动拒绝。证据 .runtime/photoshop-install-autostart-0.4.73-live-verification.json。三条路径本轮均无CCD错误；不将未逐一分离的具体环境变量宣称为确定根因。


## v0.4.74 安装检测与中英文后台／插件名称（2026-10-11）

- [x] 自动读取当前用户本应用插件ID目录，区分缺失、不完整／文件被修改／凭据异常和安装校验通过，展示版本、实际名称、检测目录；安装与PS加载状态分开，启动中不展示可执行。
- [x] 默认中文，后台即时中英文切换并独立持久化；PS插件名称按偏好在关闭PS后应用，运行中拒绝改名，首次安装／更新使用偏好语言。修改有快照／失败恢复，保留ID、启用状态、执行代码、凭据及任务记录，不改环绕菜单或热键逻辑。
- [x] 内置0.3.1名称“Photoshop 快捷菜单”／“Photoshop Quick Menu”，入口“检查连接”／“Check connection”；心跳缓存并报告本轮加载manifest名称。194项后台、5项前端含静态词典覆盖，协议模拟及最终冻结EXE启动／重启通过。
- [x] 实际入口v0.4.74管理页语言切换、英文后台重启持久化、PS运行时改名拒绝、正常关闭本轮0文档PS后英文改名代码／凭据不变；内置0.3.1加载后真实新UXP会话报告英文名称与入口一致。
- [ ] 原生PS插件菜单项视觉文字与完整实体键鼠／hover／焦点／输入／取消矩阵本轮未直接捕获或人工验收；不同PS／Windows用户环境及长期兼容性另行验证。不以文件校验和模拟测试冒充实机加载。

证据 .runtime/tests-0.4.74.log、.runtime/smoke-0.4.74.log、.runtime/i18n-0.4.74-english-name-verification.json、.runtime/i18n-0.4.74-english-live.json、.runtime/i18n-0.4.74-english.png。旧版备份dist/0.4.73-backup与.runtime/before-0.4.74-*。

- [x] 切回默认中文后正常关闭0文档测试PS，真实管理页应用名称并启动，新UXP会话报告“Photoshop 快捷菜单”／“检查连接”，版本0.3.1；实际EXE临时文档红色／清除独立读回、UXP succeeded和原生组件构建哈希通过。最终实际程序等于dist，原设置／凭据不变，收据归属同步。证据 .runtime/i18n-0.4.74-chinese-live.json、.runtime/release-0.4.74-verification.json、.runtime/installed-0.4.74-final-verification.json。统一包dist/PhotoshopMenu-Backend-0.4.74.zip。

最终补齐常见操作错误及结果译文后重新构建、烟雾／重启验证并切换实际入口。第一轮正向操作因后台页面占据前台被焦点保护正确拒绝，临时文档清理，没有图层任务提交；改由正常桌面环境运行同一COM测试后，最终实际EXE标色／清除读回成功，未绕过保护。证据 .runtime/final-0.4.74-desktop-check.log 和 .runtime/final-0.4.74-desktop-check.exit（0）。最终中文版页面 .runtime/i18n-0.4.74-chinese.png，语言／名称及配置／收据哈希核验见 .runtime/installed-0.4.74-final-verification.json。


## v0.4.75 后台应用布局与标签切换（2026-10-11）

- [x] 固定应用窗口，左侧五个主页面、页内标签，页面自身不随卡片／任务数量长滚动；启动、插件状态、离线更新、快捷键、版本信息分开显示，说明默认折叠。仅当前面板内部滚动。
- [x] 最近64条任务独立列表／详情，点击选择；阶段、目标、错误、结果、历史只读、未提交取消入口和诊断导出保留。运行状态更新不重置已展开详情说明。
- [x] 设置底部固定保存／放弃，切换页内标签及主页面保留未保存表单，放弃恢复原配置；方向键、Home／End导航，中英文切换保持详情状态。
- [x] 194项后台回归、5项前端检查、最终EXE烟雾／重启通过。实际安装入口v0.4.75，主EXE等于dist、收据哈希同步；原生组件构建哈希一致，UXP0.3.1就绪，原设置及凭据未变。旧版备份dist/0.4.74-backup和.runtime/before-0.4.75-data。
- [x] 实际管理页1120×760五主页面及插件子标签、560×700任务／设置区域无外层滚动；保存栏在视口内，任务选择和跨页编辑／放弃通过。最终内置index-ByaysUET.js中文截图及详情在英文状态刷新后仍展开已核验。
- [ ] 本轮未重新人工验收完整实体CapsLock、画布／图层hover、输入保护、焦点和取消矩阵。v0.4.75第一构建临时文档实际UXP标色／清除读回成功；最终仅详情UI修正后复测时其他应用在前台，被已有焦点保护正确拒绝、未提交任务，临时文档已清理，不将其标为最终构建正向操作通过。

证据：.runtime/tests-0.4.75.log、.runtime/smoke-0.4.75.log、.runtime/backend-tabs-0.4.75-ui-verification.json、.runtime/backend-tabs-0.4.75-ui.png、.runtime/installed-0.4.75-final-verification.json。第一构建正向记录.runtime/release-0.4.75-verification.json；最终复测焦点拒绝记录.runtime/final-0.4.75-desktop-check.log（exit 1），两者范围不混同。


## v0.4.76 Photoshop 首页／新建文档（2026-10-11）

- [x] 根据可见非零面积OWL.WelcomeScreenView识别首页，首页覆盖保留的画布／图层上下文；实际PS窗口只读检测为home。输入框焦点优先保护，不以无文档或文档标题猜测首页。
- [x] 首页环绕菜单只显示“新建文档”，文档加号图标，菜单内数字4／点击／长按松键共用入口；中心／空白取消逻辑保留，隐藏标色和排序，首页暂停直接图层快捷键。
- [x] 专用原生Ctrl+N新建窗口请求，执行前重验首页／焦点／输入／按住修饰键／忙碌／启动／菜单会话。原子按放序列、部分注入清理，不排队、不重试、不自动创建文档，不依赖更换UXP。
- [ ] 实体键鼠验收：PS首页短按／长按呼出、点击／松键选择后正确打开原生新建窗口，中心／空白／右键取消，返回文档画布／图层hover分流和输入／焦点保护。已请求用户在实际运行版本验证；预览与自动回归不能替代。

构建／运行版本v0.4.76，UXP0.3.1未变；运行入口与dist哈希、原生组件构建哈希、配置凭据保留证据.runtime/installed-0.4.76-final-verification.json。实际首页识别.runtime/home-menu-0.4.76-live-verification.json，布局预览.runtime/home-menu-0.4.76-preview.png。旧版备份dist/0.4.75-backup及.runtime/before-0.4.76-data；最终回归结果与实体反馈分别记录，不混同。


## v0.4.77 首页取消后立即重开（2026-10-11）

首页新建入口不再等待 COM 连接缓存刷新，仍要求原生首页／焦点／输入保护及忙碌检查。203 项回归、冻结 EXE 启动／重启和实际入口／dist／原生构建哈希核验通过。用户实机确认：取消原生新建窗口后可立即重新打开，原约 1 秒不可用已消失。证据 .runtime/tests-0.4.77.log、.runtime/smoke-0.4.77.log、.runtime/installed-0.4.77-final-verification.json。最近预设列表与 UXP 直接新建继续实现中。


## v0.4.78 首页最近预设直接新建（2026-10-11）

- [x] 只读匹配当前 UXP 宿主版本的本地 MRU 文件，mtime 缓存，最近使用排序、尺寸／单位／分辨率／名称；部分写入保留有效快照、版本变化不复用其他 PS 数据。
- [x] UXP 0.3.2 扩展受限 new_document 指令，presetJSON 原参数，固定会话／文档集合／激活文档／过期／取消保护、单次创建、不重试；使用已有任务生命周期与忙碌拒绝策略。
- [x] 实机 UXP 像素及 A4 参数独立读回、旧目标重复拒绝，本轮测试文档已关闭；正常关闭无文档 PS，经后台桌面 COM 助手重启并加载新插件，凭据保留、有回退备份。
- [x] 207 项后台／原生回归、5 项前端、3 组协议模拟通过；冻结 EXE 启动／重启、切换实际 v0.4.78、主 EXE 等于 dist、原生与插件0.3.2哈希／归属及配置凭据保留核验通过。证据 .runtime/tests-0.4.78.log、.runtime/smoke-0.4.78.log、.runtime/installed-0.4.78-final-verification.json。
- [ ] 预设实体点击、划出取消及画布／图层／输入／焦点矩阵仍待验收；用户已实测长按选预设新建。v0.4.79 按最新要求改为前 10 个按钮，旧列表滚动验收不再适用。

证据 .runtime/recent-uxp-live-verification.json、.runtime/recent-uxp-upgrade.json、.runtime/recent-home-preview.png。


- [x] v0.4.78 实际菜单运行记录：2026-10-11 19:36:57，用户滚动至列表末项后长按松键，高亮／冻结松键／当前鼠标均命中31，原生菜单发出 document.recent 请求；固定新UXP会话及空文档集合，任务以 uxp-local-files 渠道 succeeded。没有关闭用户新建文档。证据 .runtime/recent-production-user-verification.json；实体中心划出取消与完整旧功能矩阵仍待反馈，不以任务完成替代其验收。此前自动生产测试被鼠标在PS外的保护拒绝，未提交任务；后续真实用户操作通过。

## v0.4.79 最近预设按钮排列（2026-10-11）

- [x] 仅保留最近 10 项，按两列五行按钮显示，不滚动；长尺寸分行、字号／配色保留，沿用快照、直接新建与划出取消保护。
- [x] 208 项回归、最终十按钮可见／命中／顺序及取消／单次释放原生自测通过；2× DPI 大字体预览核验，冻结启动／重启通过。
- [x] 已切换实际 v0.4.79，核对 dist／原生组件／收据哈希，保留配置凭据，UXP0.3.2 ready，实际 API 返回十项及预览顺序一致。没有重启 PS 或修改用户文档。备份 dist/0.4.78-backup、.runtime/before-0.4.79-data；证据 .runtime/installed-0.4.79-final-verification.json、.runtime/recent-grid-79-runtime.json。
- [ ] 新排列实体点击／长按选择／中心取消，以及完整画布／图层 hover、输入框、焦点和右键／Esc 回归验收；已向用户提出测试，不能以预览、自测或 v0.4.78 成功记录替代。

## v0.4.80 任务栏切回修复（2026-10-11）

- [x] 前台 HWND 空窗不再消耗已确认点击意图或撤销 PS 会话；真实其他前台撤销资格、意图过期及 hover 不授予规则保留；增加有界的前台／资格变化记录。
- [x] 原生空窗／超时／真实外部前台回归、208 项最终回归和冻结启动／重启通过，实际 v0.4.80 及菜单／热键／UXP资源哈希、配置凭据保留核验通过。
- [x] 用户实机确认任务栏切回可唤出；19:52:16 恢复资格、19:52:18 CapsLock 唤出并固定，证据 .runtime/taskbar-80-user-acceptance.json。旧日志不足以证明空窗为唯一根因，不标记为全场景永久稳定。
- [ ] 完整 hover／跨应用／输入框／右键取消实体矩阵；v0.4.79 分行设计已由 v0.4.81 加宽单行设计替换。

## v0.4.81 加宽预设与主题蓝色 hover（2026-10-11）

- [x] 前 10 个按钮保留两列五行、无滚动，按钮加宽 50%，首页弹窗同步加宽；取消尺寸拆行，不改 PS 字号。
- [x] 普通副文本对比度修正，统一预设与环绕按钮的蓝色 hover，按四档 PS 主题调整饱和度／亮度，hover 三行文字使用主题文字色；图层色块颜色不变。
- [x] 209 项回归、十按钮可见／命中／排序／取消／单次执行与四主题至少4.5:1文字对比度自测、2×大字体预览通过。原生测试尺寸刷新失败记录保留，修复后通过；冻结 EXE 启动／重启通过。
- [x] 实际 v0.4.81 入口／dist／菜单／热键／收据哈希一致，UXP0.3.2 ready，原配置凭据保留，无 PS 重启或用户文档操作。备份 dist/0.4.80-backup 与 .runtime/before-0.4.81-data；证据 .runtime/tests-0.4.81.log、.runtime/installed-0.4.81-final-verification.json。
- [ ] 用户实体确认新布局悬停文字与直接新建，以及完整画布／图层／输入框／焦点／取消矩阵；尚未以预览和自动回归代替。


## v0.4.82 两行预设与单位徽标（2026-10-11）

- [x] v0.4.81 显示与点击实机反馈：用户确认“显示与点击都正常”；不代替新版及完整保护矩阵验收。
- [x] 预设卡片改为两行：第一行尺寸＋单位徽标，px 蓝色、mm 绿色，随浅／深主题调整；第二行名称（自定／A4 等）＋分辨率＋颜色模式。仍为最近10个、两列五行、不滚动；卡片高度36逻辑像素。
- [x] 新版四主题渲染、原生自动交互、209项回归及最终EXE启动／重启通过；已切换实际v0.4.82，安装入口／dist／菜单／热键／收据哈希一致，UXP0.3.2 ready，配置凭据保留。备份dist/0.4.81-backup与.runtime/before-0.4.82-data；证据.runtime/grid82-ui-verification.json、.runtime/installed-0.4.82-final-verification.json。
- [x] v0.4.82 两行显示、hover、直接新建／取消：用户反馈“显示与操作都正常”。
- [ ] 画布／图层分流、输入／焦点／右键关闭完整实体矩阵。

## 界面独立控件检查（2026-10-11）

- [x] 原生代码清点：环绕操作按钮、中心方向／忙碌圆环、标签色块、最近预设、搜索输入与列表、上下文菜单、通知文字。预设与环绕按钮使用同一主题 hover；标签色块和单位徽标保留语义色。四主题渲染与自动交互测试单独记录，不视为实体验收。
- [x] 实际后台浏览器检查：五页导航、菜单行为／快捷键标签切换、禁用保存／放弃按钮、插件安装检测、更新包链接、任务阶段表、日志页均可显示。检查未执行安装／卸载／更新或修改用户设置；没有验证这些破坏性按钮的实际动作。
- [x] 后台“导入 UXP ZIP”及旧JSX导入改为标准按钮＋隐藏文件输入；v0.4.117实际Tab／Enter／Space与重复同包校验预览通过，焦点可见。
- [x] v0.4.117原生搜索选中／按下态统一蓝色、通知文字使用主题正文；四主题对比度自测与预览通过，保留物理键鼠矩阵为独立待验收项。


## v0.4.83 九宫格与插件版本名称（2026-10-11）

- [x] 首页只提供最近9个预设，三列三行；比例图标按宽高矢量绘制，无图片加载。两行文字保留；第一行尺寸＋单位，第二行名称＋分辨率＋颜色模式整条蓝／绿底色。首页锚点与边界同步调整。
- [x] 插件manifest名称加入真实插件版本，中英文均自动生成；安装／更新／名称语言应用沿用此规则，检测旧无版本名称为待应用。改名保留插件ID、入口ID、代码、凭据、启用状态，保持关闭PS后应用的保护及失败恢复。
- [x] 最终211项回归、四主题原生渲染／交互及EXE烟雾通过；已切换实际v0.4.83，安装入口／dist／原生组件／收据哈希一致，配置凭据保留。PS无文档时正常退出，通过后台改名并由桌面COM助手重新启动，真实新UXP会话心跳确认名称“Photoshop 快捷菜单 v0.3.2”、入口“检查连接”、插件0.3.2。仅manifest名称改变，代码／凭据保留；未截图验收PS插件面板文字。证据.runtime/plugin-name-83-applied.json、.runtime/plugin-name-0.4.83-live.json、.runtime/installed-0.4.83-final-verification.json、.runtime/grid83-ui-verification.json。
- [x] v0.4.83 用户实机反馈“布局、操作和版本名称都正常”：3×3、比例图标、第二行蓝／绿信息背景、点击创建、中心松键取消及Plugins面板版本名称已确认。
- [ ] 完整画布／图层hover分流、输入保护、跨应用焦点及右键关闭实体矩阵仍待单独回归；本次反馈不代替该矩阵。


## v0.4.84 预设 @ 格式（2026-10-11）

- [x] 根据截图调整两行：第一行自定／A4等名称与颜色模式，第二行“536×228 px @ 72 ppi”，不再用圆点分隔。保留九宫格、比例图标和第二行单位语义底色；按钮小幅加宽，中心锚点同步保持在鼠标唤出点。
- [x] 四主题预览最长样本完整、原生交互及211项回归、最终EXE烟雾通过；已切换实际v0.4.84，入口／dist／菜单／热键／收据哈希一致，UXP0.3.2 ready，配置凭据保留，无PS文档操作。证据.runtime/grid84-ui-verification.json、.runtime/installed-0.4.84-final-verification.json。
- [ ] 新文本格式的实机显示／直接新建／中心取消；完整hover／输入／焦点／右键关闭矩阵仍需单独验收。


## v0.4.85 剪贴板首项与行序（2026-10-11）

- [x] 首页九宫格第一项固定为剪贴板，另8项为最近预设。只读Windows CF_DIB／CF_DIBV5图像头获取宽高，不读取完整像素、不改变剪贴板；按尺寸以72ppi、RGB、白底新建空白文档，不自动粘贴。无支持的图像格式时提示并禁止选择；剪贴板序号变化时拒绝旧入口，提交前再次检查。
- [x] 按反馈互换行序：第一行“尺寸 单位 @ 分辨率”，第二行“名称 颜色模式”，深色蓝／绿背景包裹第二行；剪贴板卡片加深底色并带夹板轮廓图标。保持3×3、原有hover／取消／焦点保护。
- [x] 原生组件四主题渲染、禁用首项／命中／取消及213项回归、最终EXE烟雾通过；实际v0.4.85入口／dist／原生组件／收据哈希一致，UXP0.3.2 ready，配置凭据保留。真实PS按当前354×385剪贴板尺寸新建、独立COM读回尺寸／72ppi／RGB正确，剪贴板保留、自建文档清理。证据.runtime/clipboard-85-live.json、.runtime/installed-0.4.85-final-verification.json；实体菜单入口仍需验收。
- [ ] 新剪贴板入口与交换行序的实体菜单显示／点击／中心取消，以及完整保护矩阵。用户反馈“显示还需调整”，随后截图明确名称／模式应拆成独立标签，由v0.4.86落实；不视为v0.4.85整体验收通过。


## v0.4.86 预设独立标签（2026-10-11）

- [x] 预设第二行名称及颜色模式分别包裹为独立圆角标签，按文字收紧、小间距；去掉整行色带。保留第一行尺寸 @ 分辨率、3×3、剪贴板首格深底色、比例图标及主题hover。仅修改绘制，不增加标签独立动作，整张卡片仍为一个预设。
- [x] 213项回归、原生命中／禁用／点击／中心取消及四主题预览、最终EXE烟雾通过；实际运行v0.4.86，安装入口／dist／原生组件／收据哈希一致，配置凭据保留，UXP0.3.2 ready。正常退出服务再更新，无PS文档操作；备份dist/0.4.85-backup、.runtime/before-0.4.86-data。证据.runtime/grid86-ui-verification.json、.runtime/installed-0.4.86-final-verification.json。
- [x] 用户实机确认v0.4.86“显示与操作都正常”：独立标签、hover、预设点击和长按中心松键取消通过。
- [ ] 完整画布／图层hover、输入保护、跨应用焦点及右键关闭实体矩阵仍待单独回归；该反馈不代替完整矩阵。


## v0.4.87 尺寸与标签字号层级（2026-10-11）

- [x] 第一行尺寸数字大于第二行标签，单位及分辨率保持小字号；字体全部取现有缓存。按钮与首页容器／Win32窗口／后台边界同步加宽，当前长尺寸样本完整单行，九宫格与中心锚点保留。
- [x] 最终213项回归、原生菜单自测与四主题渲染、EXE烟雾通过，窗口边缘钳制测试同步新尺寸。已正常退出旧服务，实际运行v0.4.87，入口／dist／原生组件／收据哈希一致，配置凭据保留，UXP0.3.2 ready；无PS文档操作。证据.runtime/grid87-ui-verification.json、.runtime/installed-0.4.87-final-verification.json。
- [x] 用户实机确认v0.4.87“显示与操作都正常”：尺寸大于标签、长尺寸单行、hover／点击及中心松键取消通过。
- [ ] 完整画布／图层hover、输入保护、焦点及右键关闭实体矩阵仍待单独验收，不由此反馈替代。


## v0.4.88 顶部剪贴板与4×2预设（2026-10-11）

- [x] 有剪贴板图像时入口单独居中置于圆环上方；无图像则隐藏且无命中区域。右侧独立保留最近8个预设，按4列×2行排列。保留深底色、尺寸／标签字号、比例图标和主题hover，后台及原生真实窗口宽度同步更新。
- [x] 213项回归、原生顶部位置／全部命中／隐藏后8项命中／中心取消／单次提交自测、有／无剪贴板四主题渲染及EXE烟雾通过；实际运行v0.4.88，安装入口／dist／原生组件／收据哈希一致，配置凭据保留、UXP0.3.2 ready。无PS文档操作。证据.runtime/grid88-ui-verification.json、.runtime/installed-0.4.88-final-verification.json。
- [ ] 新位置与4×2实体显示、剪贴板与最近预设点击／长按中心取消；完整画布／图层hover、输入保护、跨应用焦点及右键关闭实体矩阵仍待单独回归，自动测试不替代此项。

用户对v0.4.88反馈顶部剪贴板“有”，要求去掉其独立深底色，配色与左侧新建按钮一致；本反馈仅确认入口存在，不替代完整点击／取消验收，配色由v0.4.89调整。


## v0.4.89 尺寸放大与剪贴板配色统一（2026-10-11）

- [x] 尺寸数字进一步放大（13／14／15随PS字体设置），标签缩小到10；单位与分辨率保持紧凑。六种字号启动时缓存、原有PS字号不变；卡片与Win32／后台首页窗口同步加宽，当前长尺寸保持单行。
- [x] 剪贴板取消独立深底色，普通／hover使用与左侧新建按钮完全相同的主题色；顶部位置、有图像才显示、4列×2行及原有目标保护保留。
- [x] 213项回归、原生字体缓存／命中／隐藏／点击／取消自测、四主题渲染和最终EXE烟雾通过；实际v0.4.89入口／dist／菜单／热键／收据哈希一致、配置凭据保留、UXP0.3.2 ready，无PS文档操作。证据.runtime/grid89-ui-verification.json、.runtime/installed-0.4.89-final-verification.json。构建脚本仅在内容变化时写字形头文件，解决无变化文件映射占用时的编译阻塞。
- [ ] 新字号与统一剪贴板配色实体显示／hover／点击／中心松键取消；完整画布／图层hover、输入保护、跨应用焦点及右键关闭实体矩阵仍待单独验收，自动检查不替代此项。


## v0.4.90 纵向4行／横向2列与字号减半（2026-10-11）

- [x] 按用户明确方向改为2列×4行；保留8个最近预设和独立顶部剪贴板，窗口宽度／命中／边缘钳制同步更新，顶部与右侧容器不重叠。
- [x] 用户确认“字号直接缩小50%”：第一行数字及单位／分辨率使用v0.4.89的一半字号，第二行标签维持10；字形预先缓存，统一剪贴板主题色保留。
- [x] 213项回归、原生8项加顶部入口命中／禁用隐藏／取消／缓存自测、有／无剪贴板四主题渲染及EXE烟雾通过。实际运行v0.4.90，入口／dist／原生组件／收据哈希一致，配置凭据保留、UXP0.3.2 ready，无PS文档操作。证据.runtime/grid90-ui-verification.json、.runtime/installed-0.4.90-final-verification.json。
- [ ] 新2列4行和50%字号实体显示／hover／点击／中心松键取消；完整画布／图层hover、输入保护、跨应用焦点及右键关闭实体矩阵仍待单独验收，不由自动检查替代。


## v0.4.91 恢复可读字号（2026-10-11）

- [x] 用户截图已确认2列×4行，但反馈整行50%字号过小。本版恢复尺寸12／13／14、单位与分辨率11，标签保留10；保留布局、顶部条件剪贴板、统一配色和既有保护。
- [x] 213项回归、原生字体缓存／命中／隐藏／点击／取消自测、四主题预览及EXE烟雾通过；实际v0.4.91入口／dist／菜单／热键／收据哈希一致，配置凭据保留、UXP0.3.2 ready，无PS文档操作。证据.runtime/grid91-ui-verification.json、.runtime/installed-0.4.91-final-verification.json。
- [ ] 新恢复字号的实体显示／hover／点击／中心松键取消及完整画布／图层hover、输入保护、跨应用焦点、右键关闭矩阵；v0.4.90截图不视为新版或操作验收。


## v0.4.92 背景标签与状态刷新（2026-10-11）

- [x] PS24.0.1临时背景复现旧标签命令隐式转换／新ID／旧ID核验失败；新版UXP0.3.3在写入前明确拒绝锁定背景。单选及含背景的混合选择都不写入、不增加历史、不部分标色；手动解锁后可正常标色。真实已安装UXP协议、独立任务桥与COM读回通过，证据.runtime/background-labels-0.4.92-live.json。
- [x] 新鲜缓存在后台刷新期间维持连接信息，不反复显示待更新；保留pending诊断，真正陈旧／失败／忙碌不伪报已更新。真实COM600ms读取20并发仅1次读取并维持有效缓存，12轮任务后状态空闲有效。
- [x] 用户要求当前字号小1/4：第一行尺寸9／9.75／10.5与后缀8.25（v0.4.91的75%），标签10及其他PS菜单字号不变；2列×4行、条件剪贴板、配色与卡片命中保留。原生新增背景拒绝提示自测，既有hover分流／输入／焦点／取消自动回归保留。
- [x] 214项后台、5项前端、UXP协议自测、原生自测、四主题预览、EXE烟雾通过；实际v0.4.92／UXP0.3.3及新心跳，入口／dist／组件／收据哈希一致，配置凭据保留。用户授权丢弃Untitled-1并自动重启，后台管理更新插件，历史版本可回退。
- [x] 用户实机确认v0.4.92“提示和菜单都正常”：锁定背景明确提示先解锁，之后菜单可正常连续唤出，不再反复待更新。
- [ ] 缩小25%字号／点击／中心取消及完整画布／图层hover、输入、跨应用焦点、右键关闭实体矩阵。用户本次回复未单独确认这些项目；隔离桥验证不代替完整生产键鼠验收。

154公开交付核验：本机实际入口／dist／菜单组件匹配，UXP23保持；公开ZIP、SHA256、备用清单匿名下载及最终冻结更新器API403回退实网通过，Wiki两页公开正文已核验。发行页仅154；153及全部旧Git标签、源码历史和.local/recovery/releases/0.4.153-before154回退保留，当前dist仅154六项发行物。实体交互及安全暂停后的剩余复测未勾选。

155公开交付核验：实际入口／dist／HTTP前端与包内资源匹配，原生及UXP23不变；公开ZIP／SHA256／备用清单匿名下载与最终冻结更新器API403回退实网通过，Wiki操作说明正文已核验。发行页仅155；全部Git标签／源码及.local/recovery/releases/0.4.154-before155回退保留，dist仅155六项文件。实体Windows下拉和PS键鼠矩阵仍单列未验收。

239渲染核验限制：文字层GPU预览和像素层CPU预览完整；像素层GPU离屏导出出现部分内容缺失，原238对照导出完整，原因尚未确认。未以顶点数、窗口存在或CPU预览代替实际GPU显示验收，保留.runtime/menu239-pixel*.png及menu238-pixel-baseline.png；本次未宣称GPU显示已通过。
