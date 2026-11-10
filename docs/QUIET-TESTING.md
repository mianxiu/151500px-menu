# 副屏低干扰测试

2026-10-17新增Windows源码维护工具，不是后台新增按钮。主程序快捷键、输入保护和执行队列保持。使用说明同步到[Wiki](https://github.com/mianxiu/151500px-menu/wiki/Quiet-testing)。

用户2026-10-17明确要求后续优先面向低配办公电脑验收。新增／修复默认先跑受限CPU的短批次真实PS事务，再补正常配置对照；同时检查正确目标、新鲜选择、原内容、一次撤销、未知结果不重发和资源恢复。前台变化时暂停，不为完成测试抢焦点。不得延长刷新间隔或删减校验换取通过；真实低内存／集显及故障机器继续独立验收，CPU限制不能替代这些硬件条件。

在项目根目录运行：

```powershell
.\Scripts\python.exe tools/test-quiet-ps.py --inspect
.\Scripts\python.exe tools/test-quiet-ps.py --place
.\Scripts\python.exe tools/test-quiet-ps.py --self-test
.\Scripts\python.exe tools/test-quiet-ps.py --run
```

仅有一个PS主窗口和一个Codex窗口，Codex必须位于副屏。工具只读Codex窗口位置；用SetWindowPos的NOACTIVATE／NOZORDER／NOOWNERZORDER移动PS及OWL浮动面板，不注入键鼠、不激活窗口。原位置保存到.runtime/quiet-ps-<标识>/placement.json；PS继续留在副屏。跨屏DPI、启动画面及模态弹窗另行验收。

测试时仅PS临时使用原允许范围中的最后两个逻辑处理器和Below Normal优先级。正常结束、Python异常及120秒看门狗分别恢复原配置并保存resources.json；不覆盖外部人工更改。看门狗只恢复资源，不终止PS操作；整个测试进程被强杀时需依据日志人工恢复。内存、GPU、磁盘、电源及Chrome设置保持。

--run固定三批：4层形状、40层混合、40层智能对象，每批创建空白／纯色／曲线，核验原顺序和内容、选择、DOM／batch分流、一次撤销。操作间让出2秒、批间4秒；只使用测试文档，终态明确后关闭自建文档并恢复原活动文档。测试期间保持PS空闲，不同时操作PS。进入PS、菜单显示、服务忙碌、恢复保护或.runtime/quiet-ps.stop存在时不再提交下一项；开始下一次前移走停止文件。未知结果保留原pending及任务记录，不重发、不回退COM修改、不强杀PS；不能安全清理时报告cleanup_pending并保留测试文档。

本机实际153／UXP23、PS24.0.1通过9项真实操作，原顺序／内容、选择和一次撤销核验通过，Chrome前台保持；PS已恢复原16逻辑处理器／Normal。证据.runtime/quiet-ps-1792218172731849700/report.json及resources.json。154复测前三项通过，第二批建文档后前台变化触发安全停止，未继续提交UXP修改，已清理自建文档并恢复资源，无待核对结果；来源未确定，不归因为PS或用户。证据.runtime/quiet-ps-1792218732853338000。资源正常／异常／看门狗恢复另有自测。未采集视频掉帧、音画同步及实体键鼠延迟，不以焦点未变宣称视频零影响，也不等于复现故障i5／集显。Codex窗口工具重置后仍报0x80070020；Win32工具恢复了项目窗口定位／移动能力，Codex工具本身未修复。


## 文字对象的服务内专项

323候选的文字智能对象维护验收改用服务内固定`text-object`模式，避免逐个外部JSX文件引发PS执行确认。两CPU对照由维护进程绑定真实PS进程与启动时间，临时使用双逻辑处理器／Below Normal并恢复；正常配置另开独立批次。测试不降低写前校验，不修改PS安全设置，不把资源模拟当成低内存／集显实机结论。入口、原请求与收尾保护见[自动稳定性测试](STABILITY-TEST.md)。

原生颜色通道使用同一服务内的固定`color-native`模式；同一轮依次完成低CPU和正常配置，原稿重载备份与恢复、精确目标及未知保护见[维护用原生颜色验收](STABILITY-TEST.md#维护用原生颜色验收)。
