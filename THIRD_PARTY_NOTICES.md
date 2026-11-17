# 第三方许可与来源

项目自有代码与文档采用根目录 LICENSE 中的 MIT License。第三方代码、字体及随附运行时仍适用各自许可证；本项目的 MIT 不取代这些许可。

| 组件 | 许可与保留文件 | 来源 |
| --- | --- | --- |
| Dear ImGui 1.92.7-docking | MIT；native/vendor/imgui/LICENSE.txt | https://github.com/ocornut/imgui |
| nlohmann/json 3.12.0 | MIT；native/vendor/json/LICENSE.MIT | https://github.com/nlohmann/json |
| Inter / Noto Sans CJK | SIL OFL 1.1；native/fonts/OFL-1.1.txt、COPYRIGHT.txt | native/fonts/README.md 中记录字体转换来源 |
| SolidJS | MIT；licenses/solid-js.txt | https://github.com/solidjs/solid |
| seroval / seroval-plugins | MIT；licenses/seroval.txt、seroval-plugins.txt | https://github.com/lxsmnsyc/seroval |
| Tornado | Apache 2.0；licenses/tornado.txt | https://github.com/tornadoweb/tornado |
| pywin32 的 win32 / COM 模块 | 原始许可；licenses/pywin32-win32.txt、pywin32-com.txt | https://github.com/mhammond/pywin32 |
| comtypes / Windows UI Automation COM 封装 | 原始许可；licenses/comtypes.txt | https://github.com/enthought/comtypes |
| CPython 3.11 运行时 | Python 原始许可及附带声明；licenses/python.txt | https://www.python.org/ |
| PyInstaller 引导程序／构建器 | 原始许可及引导程序例外；licenses/pyinstaller.txt | https://github.com/pyinstaller/pyinstaller |
| Microsoft Edge WebView2 SDK 1.0.2903.40 | licenses/WebView2-LICENSE.txt、licenses/WebView2-NOTICE.txt | https://www.nuget.org/packages/Microsoft.Web.WebView2/1.0.2903.40 |

native/vendor/manifest.json 保存直接纳入的 C++ 依赖版本与哈希。字体版权／许可取自字体名称表，并保留原始字形与名称；不将字体改为 MIT。

ImGui 1.92.7-docking 的本地单行修改：将 imgui_internal.h 中的 IMSTB_TEXTEDIT_UNDOCHARCOUNT 从999增为80000，以容纳文本工具整段缩进的一次撤销。原始／修改后哈希与原因记录在 manifest.json 的 local_changes；版本与 MIT 许可不变，未将本地修改冒称为上游原文件。

frontend/package-lock.json 与 requirements*.txt 记录安装／构建依赖；它们由上游独立维护，适用上游许可。更新依赖或字体时，同时核对并更新相关声明。

统一后台分发 ZIP 随附项目 LICENSE、本文件、licenses/ 中的声明，以及 C++ 库与字体的原始版权／许可。仅安装 Photoshop 插件不提供 Photoshop 或 Adobe 软件许可。

感知颜色数学参考 Björn Ottosson 发布的 [Oklab 转换矩阵与说明](https://bottosson.github.io/posts/oklab/)及 [W3C OKLCH 定义](https://www.w3.org/TR/css-color-4/#ok-lab)，本项目在 native/PerceptualColor.h 中实现本地计算与色域映射。
