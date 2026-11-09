// Semantic text roles. DPI is applied once when the font atlas is built.
enum class MenuTextRole { Body, Control, Secondary, Shortcut, PresetDimension, PresetTag, PresetUnit };
static constexpr float menuFontEms[]={11.f,12.f,13.f,10.f,10.75f,11.5f,8.5f,8.5f,
    11.f*figmaBasicTextScale,12.f*figmaBasicTextScale,13.f*figmaBasicTextScale,9.f};
static ImFont* menuFonts[std::size(menuFontEms)]{};
static int fontIndex() {return uiFontSize=="large"?2:uiFontSize=="medium"?1:0;}
static int menuFontIndex(MenuTextRole role) {
    switch(role) {
    case MenuTextRole::Control:return 8+fontIndex();
    case MenuTextRole::Secondary:return fontIndex()?fontIndex()-1:3;
    case MenuTextRole::Shortcut:return fontIndex()==0?11:fontIndex()==1?3:0;
    case MenuTextRole::PresetDimension:return 3+fontIndex();
    case MenuTextRole::PresetTag:return 6;
    case MenuTextRole::PresetUnit:return 7;
    default:return fontIndex();
    }
}
static ImFont* menuFont(MenuTextRole role) {
    auto* font=menuFonts[menuFontIndex(role)];return font?font:ImGui::GetFont();
}
struct MenuTextStyle {
    explicit MenuTextStyle(MenuTextRole role){ImGui::PushFont(menuFont(role));}
    ~MenuTextStyle(){ImGui::PopFont();}
};
