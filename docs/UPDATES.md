# GitHub 应用更新

## 只重启后台

在后台「概览 → Photoshop 连接」点击「重启后台」。原服务正常退出后，新服务沿用当前版本、端口、设置和数据目录。已打开的后台管理窗口会重新打开，Photoshop 和文档保持运行。

如果 Photoshop 的操作仍在执行，请先等它结束。若后台只剩已保存的待核对任务，可以重启并继续等待原任务的结果。后台收到有效回执后恢复修改功能。

## 从后台更新

1. 进入「维护 → 应用更新」，点击「检查应用更新」。正式版本号和发行说明来自上次检查，刚发布时需要重新检查。
2. 有新版时点击「下载并校验」。默认每 24 小时检查一次，可关闭。自动检查只读取发行信息。
3. 等待任务结束、收起菜单并保存后台设置，点击「安装并重启后台」。更新助手等待旧服务和打包父进程正常退出，备份旧 EXE，再替换原运行入口。启动参数、端口和数据目录沿用原设置。
4. 在「概览」确认应用版本与 Photoshop 连接，再核对 Caps 菜单和托盘状态。已打开的 Photoshop 和文档可保持运行。

内置 UXP 随主程序更新。磁盘插件落后时，先保存并正常关闭 Photoshop，在「插件管理 → 插件与版本」安装内置版，或让已启用的自动更新跟随，再启动 Photoshop 核对实际加载版本。自动更新选项在「安装详情与维护」内。已加载匹配插件且原生输入组件无需重载时，可单独更新主程序。 [当前正式版与内置插件](https://github.com/mianxiu/151500px-menu/wiki/Versions)

网络只用于在线检查和下载。离线更新可正常退出后台，再用统一 ZIP 中的新 EXE 替换原入口。桌面后台需要 Windows WebView2 Runtime，缺少时可手动打开本机后台地址。 [安装说明](https://github.com/mianxiu/151500px-menu/wiki/Installation)

## 从旧名称迁移

项目现名为 **151500px-menu**。旧版程序内置了旧仓库地址和严格的下载校验，更名后请手动下载新名称安装包完成第一次迁移。

1. 在旧后台的“运行日志”确认实际应用数据目录，等待任务结束。
2. 保存文档并正常退出 Photoshop 和旧后台。
3. 解压新名称安装包，运行 `151500px-menu.exe`。普通旧安装会继续使用原 `PhotoshopMenu` 数据目录；使用过自定义目录的用户，启动时继续传入原来的 `--data-dir`。
4. 核对设置、快捷键与插件连接。后续更新使用新仓库。

旧配置目录及插件内部身份用于识别原安装，请保留这些数据。新安装默认使用 `151500px-menu` 数据目录。0.4.310 起公开包已采用新名称；此前正式包仍为旧名称，需要按上述步骤手动迁移，下载与核验状态见[版本状态](https://github.com/mianxiu/151500px-menu/wiki/Versions)。

程序使用用户系统或环境代理连接 GitHub，网络错误和校验失败会显示原因，当前程序仍可继续使用。正式下载和旧版本记录见[发行记录](../RELEASES.md)。

v0.4.144起，GitHub API返回403／429时读取同一仓库最新Release中的`151500px-menu-update.json`小型清单，仍校验正式版本、仓库、标签、附件身份及下载摘要。备用清单不可用时明确报错，不把缓存版本宣称为最新；SSL／证书／DNS／超时均保留真实失败，不关闭HTTPS证书验证。受限的旧版142需要手动下载新版，或等待GitHub解除限制。

## 校验与恢复

- 固定源为`mianxiu/151500px-menu`的`/releases/latest`，只接受已发布、非预览版本且版本号高于当前；不自动降级。
- 附件必须为`151500px-menu-Backend-<版本>.zip`及同名`.sha256`，HTTPS地址绑定仓库／标签／文件名，仅允许GitHub发行附件CDN重定向。
- 核对GitHub SHA256 digest（如有）、ZIP SHA256、包内EXE SHA256、Windows产品身份和版本。拒绝不安全路径、重复条目、过大文件、独立UXP包及源码ZIP；仅提取EXE，不执行ZIP中其他脚本。SHA256校验不等同代码签名。
- 暂存、任务和旧程序在数据目录`application-updates/`；检查偏好／发行缓存在`application-update.json`。下载及安装尝试使用独立目录，旧就绪标记不能触发新安装。不改菜单设置、UXP凭据和文档。
- 忙碌、排队、PS启动中、未知结果待核对、菜单未收起时拒绝安装，准备期间拒绝新修改。不取消或重发PS命令，不转COM。
- 只等待原进程，不强制结束。旧进程未退出就取消替换；新进程明确退出且启动失败时恢复旧EXE和安装归属记录。新版仍运行但未确认就绪时保留备份并报告原因，不强行覆盖运行程序。

## 维护约定

发布统一 ZIP、SHA256、独立 UXP 包和更新清单。使用 `tools/build-update-manifest.py --release-json <Release 记录>` 生成 `151500px-menu-update.json`。匿名下载校验新版后，清理其他 Release 及附件，保留旧 Git 标签和源码历史。应用更新助手为安装失败保留事务备份。发行流程无需另建离线回退包。

后续 EXE 保持 `--apply-update <job.json>` 的 schema 1 兼容。应用更新附件使用正式发行的统一后台 ZIP，独立 UXP 包通过插件管理导入。

参考[GitHub Releases API](https://docs.github.com/en/rest/releases/releases)、[附件API](https://docs.github.com/en/rest/releases/assets)和[REST最佳实践](https://docs.github.com/en/rest/using-the-rest-api/best-practices-for-using-the-rest-api)。使用ETag条件请求；匿名请求仍可能限流，显示错误，不高频重试。

旧版加载登记和图层读取兼容见[UXP 加载说明](UXP-LOADING.md)与[发行记录](../RELEASES.md)。
