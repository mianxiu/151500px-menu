"""Prevent new native tool views from silently reverting to default widgets."""
from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parents[1]


class MenuUiContractTests(unittest.TestCase):
    def test_compact_shortcut_role_and_metrics_are_shared(self):
        typography=(ROOT/'native/MenuTypography.h').read_text('utf-8')
        self.assertIn('MenuTextRole::Shortcut',typography)
        host=(ROOT/'native/MenuHost.cpp').read_text('utf-8')
        self.assertIn('MenuTextRole::Shortcut',host)
        metrics=(ROOT/'native/MenuUiMetrics.h').read_text('utf-8')
        for name in ('menuShortcutHeight','menuShortcutMinWidth','menuShortcutTextInset'):
            self.assertIn(name,metrics)
            self.assertIn(name,host)

    def test_native_views_cannot_choose_fixed_fonts_or_local_text_scales(self):
        for path in (ROOT/'native').iterdir():
            if path.suffix not in ('.h','.cpp') or path.name=='MenuTypography.h':
                continue
            source=path.read_text('utf-8')
            with self.subTest(view=path.name):
                self.assertNotRegex(source,r'menuFonts\[(?:\d|fontIndex)')
                self.assertNotRegex(source,r'(?:GetFontSize\(\)|FontSize|LegacySize)\s*\*\s*[.\d]')
                self.assertNotRegex(source,r'SetWindowFontScale\((?!1\.f\))')
                self.assertNotRegex(source,r'CreateFontW\(-MulDiv\(\d')
                if path.name!='MenuHost.cpp':
                    self.assertNotIn('AddFontFromFileTTF',source)
        for name in ('MenuControls.h','ShapeColorEditor.h'):
            self.assertIn('menuFont(MenuTextRole::Control)',(ROOT/'native'/name).read_text('utf-8'))
        source=(ROOT/'native/MenuToolsPanel.h').read_text('utf-8')
        self.assertNotIn('menuFonts',source)
        for role in ('Body','Secondary'):
            self.assertIn('MenuTextRole::'+role,source)

    def test_frontend_typography_is_tokenized_including_future_views(self):
        # Check every source, so a new CSS module / inline style cannot bypass it.
        for path in (ROOT/'frontend/src').rglob('*'):
            if path.suffix not in ('.css','.jsx','.js','.mjs'):
                continue
            source=path.read_text('utf-8')
            with self.subTest(view=path.name):
                self.assertNotRegex(source,r'(?<![-\w])(?:font-size|line-height|font-weight)\s*:\s*(?:[\d.]|["\'][\d.])')
                self.assertNotRegex(source,r'font(?:Size|Weight)|lineHeight')
                self.assertNotRegex(source,r'(?<![-\w])font\s*:\s*[\d.]')
                for match in re.finditer(r'(?<![-\w])(?:font-size|line-height|font-weight)\s*:\s*([^;}\n]+)',source):
                    self.assertTrue(match[1].startswith('var(--ui-') or match[1]=='inherit',match[0])
                outside_root=re.sub(r':root\s*\{[^}]*\}','',source) if path.name=='admin.css' else source
                self.assertNotRegex(outside_root,r'--ui-[\w-]*(?:size|line|weight[\w-]*)\s*:')
        css=(ROOT/'frontend/src/admin.css').read_text('utf-8')
        for role in ('body','control','secondary','title','section','emphasis','symbol'):
            for metric in ('size','line'):
                self.assertIn('--ui-'+role+'-'+metric+':',css)
        # Reflow at narrow sizes without shrinking text, including badges.
        for match in re.finditer(r'@media[^{}]*\{',css):
            depth=1;end=match.end()
            while depth and end<len(css):
                depth+=(css[end]=='{')-(css[end]=='}');end+=1
            self.assertNotRegex(css[match.end():end],r'font-size|line-height|font-weight|font:')

    def test_tool_views_use_caps_controls_without_local_palette_or_widget_styles(self):
        for path in (ROOT/'native').glob('*Panel.h'):
            source=path.read_text('utf-8')
            with self.subTest(panel=path.name):
                self.assertNotRegex(source,r'ImGui::(?:Button|Selectable|InputText\w*|Checkbox|DragScalar)\s*\(')
                self.assertNotRegex(source,r'ImGui::PushStyleVar|IM_COL32\(|ImVec4\(')
                self.assertIn('MenuControls::PanelStyle',source)
        source=(ROOT/'native/MenuToolsPanel.h').read_text('utf-8')
        for shared in ('PanelStyle','button','input','resultRow','previewTile','multiline','textPreview','segments'):
            self.assertIn('MenuControls::'+shared,source)
        caps=(ROOT/'native/MenuHost.cpp').read_text('utf-8')
        for shared in ('surface','outline','segment'):
            self.assertIn('MenuControls::'+shared,caps)

    def test_search_uses_shared_neutral_emphasis_without_disabling_input_navigation(self):
        controls=(ROOT/'native/MenuControls.h').read_text('utf-8')
        panel=(ROOT/'native/MenuToolsPanel.h').read_text('utf-8')
        self.assertIn('enum class Emphasis {Accent,Neutral}',controls)
        self.assertIn('Emphasis emphasis=Emphasis::Accent',controls)
        self.assertIn('ImGui::GetStyleColorVec4(ImGuiCol_Border)',controls)
        self.assertIn('"##layer","搜索图层…",name,sizeof(name),MenuControls::Emphasis::Neutral',panel)
        self.assertIn('"##types",kinds,kind,scale)',panel)
        self.assertNotIn('kinds,kind,scale,MenuControls::Emphasis::Neutral',panel)
        self.assertNotIn('SetNavCursorVisible(false)',panel)

    def test_shared_controls_keep_ps_tokens_and_keyboard_navigation(self):
        source=(ROOT/'native/MenuControls.h').read_text('utf-8')
        self.assertNotRegex(source,r'IM_COL32\(|ImVec4\(')
        for token in ('FrameRounding','menuControlHeight','menuControlTextInset','menuControlBorder','menuPanelGap'):
            self.assertIn(token,source)
        self.assertIn('ImGuiButtonFlags_EnableNav',source)
        self.assertIn('RenderNavCursor',source)
        self.assertIn('requireText(value)',source)
        build=(ROOT/'tools/build-menu-host.ps1').read_text('utf-8')
        self.assertIn('-DIMGUI_ENABLE_WIN32_DEFAULT_IME_FUNCTIONS',build)
