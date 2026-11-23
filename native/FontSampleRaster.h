#pragma once
#include <d2d1.h>
#include <dwrite_3.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <array>
#include <map>
#include <vector>
#include <string>

// Lookup by the same PostScript face identity used by Photoshop. Restrict
// the system-font sample to local resources and keep the host proof separate.
class FontSampleRaster {
    template<class T> using Ptr=Microsoft::WRL::ComPtr<T>;
    Ptr<IDWriteFactory3> write;
    Ptr<IDWriteFontSet> fonts;
    Ptr<ID2D1Factory> draw;
    Ptr<IWICImagingFactory> wic;
    struct Face {
        Ptr<IDWriteFontCollection1> collection;
        std::wstring family;
        DWRITE_FONT_WEIGHT weight=DWRITE_FONT_WEIGHT_NORMAL;
        DWRITE_FONT_STYLE style=DWRITE_FONT_STYLE_NORMAL;
        DWRITE_FONT_STRETCH stretch=DWRITE_FONT_STRETCH_NORMAL;
    };
    std::map<std::wstring,Face> draftFaces;
    bool initialize(){
        if(fonts)return true;
        if(FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory3),reinterpret_cast<IUnknown**>(write.ReleaseAndGetAddressOf()))) ||
           FAILED(write->GetSystemFontSet(fonts.ReleaseAndGetAddressOf())) ||
           FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,draw.ReleaseAndGetAddressOf())) ||
           FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(wic.ReleaseAndGetAddressOf())))){
            fonts.Reset();return false;
        }
        return true;
    }
    const Face& draftFace(const std::wstring& name) {
        if(draftFaces.size()>=128)draftFaces.clear();
        auto [at,inserted]=draftFaces.try_emplace(name);
        auto& face=at->second;
        if(!inserted || name.empty())return face;
        const DWRITE_FONT_PROPERTY identity{DWRITE_FONT_PROPERTY_ID_POSTSCRIPT_NAME,name.c_str(),L"en-us"};
        Ptr<IDWriteFontSet> match;
        Ptr<IDWriteFontFamily> family;
        Ptr<IDWriteLocalizedStrings> names;
        Ptr<IDWriteFont> font;
        if(FAILED(fonts->GetMatchingFonts(&identity,1,match.GetAddressOf())) || match->GetFontCount()!=1 ||
           FAILED(write->CreateFontCollectionFromFontSet(match.Get(),face.collection.GetAddressOf())) ||
           FAILED(static_cast<IDWriteFontCollection*>(face.collection.Get())->GetFontFamily(0,family.GetAddressOf())) ||
           FAILED(family->GetFamilyNames(names.GetAddressOf())) || FAILED(family->GetFont(0,font.GetAddressOf())))return face;
        UINT32 length=0;
        if(FAILED(names->GetStringLength(0,&length)) || length>512)return face;
        face.family.resize(length+1);
        if(FAILED(names->GetString(0,face.family.data(),length+1))){face.family.clear();return face;}
        face.family.resize(length);
        face.weight=font->GetWeight();face.style=font->GetStyle();face.stretch=font->GetStretch();
        return face;
    }
public:
    enum class Result { pixels, noFace, missingGlyph, unavailable };
    Result render(const std::wstring& name,const std::wstring& sample,UINT width,UINT height,float em,std::vector<unsigned char>& pixels){
        if(!initialize())return Result::unavailable;
        if(name.empty() || sample.empty() || width<8 || width>2048 || height<8 || height>256)return Result::unavailable;
        const DWRITE_FONT_PROPERTY identity{DWRITE_FONT_PROPERTY_ID_POSTSCRIPT_NAME,name.c_str(),L"en-us"};
        Ptr<IDWriteFontSet> match;
        if(FAILED(fonts->GetMatchingFonts(&identity,1,match.GetAddressOf())) || match->GetFontCount()!=1)return Result::noFace;
        Ptr<IDWriteFontFaceReference> reference;
        Ptr<IDWriteFontFace3> face;
        if(FAILED(match->GetFontFaceReference(0,reference.GetAddressOf())) || reference->GetLocality()!=DWRITE_LOCALITY_LOCAL ||
           FAILED(reference->CreateFontFace(face.GetAddressOf())))return Result::noFace;
        for(size_t index=0;index<sample.size();++index){
            UINT32 code=sample[index];
            if(code>=0xd800 && code<=0xdbff && index+1<sample.size()){
                const UINT32 low=sample[index+1];
                if(low>=0xdc00 && low<=0xdfff){code=0x10000+((code-0xd800)<<10)+low-0xdc00;++index;}
            }
            if(!face->HasCharacter(code))return Result::missingGlyph;
        }
        Ptr<IDWriteFontCollection1> collection;
        Ptr<IDWriteFontFamily> family;
        Ptr<IDWriteLocalizedStrings> names;
        Ptr<IDWriteFont> font;
        if(FAILED(write->CreateFontCollectionFromFontSet(match.Get(),collection.GetAddressOf())) ||
           FAILED(static_cast<IDWriteFontCollection*>(collection.Get())->GetFontFamily(0,family.GetAddressOf())) ||
           FAILED(family->GetFamilyNames(names.GetAddressOf())) ||
           FAILED(family->GetFont(0,font.GetAddressOf())))return Result::unavailable;
        UINT32 familyLength=0;
        if(FAILED(names->GetStringLength(0,&familyLength)) || familyLength>512)return Result::unavailable;
        std::wstring familyName(familyLength+1,L'\0');
        if(FAILED(names->GetString(0,familyName.data(),familyLength+1)))return Result::unavailable;
        Ptr<IDWriteTextFormat> format;
        Ptr<IDWriteTextLayout> layout;
        if(FAILED(write->CreateTextFormat(familyName.c_str(),collection.Get(),font->GetWeight(),font->GetStyle(),font->GetStretch(),em,L"",format.GetAddressOf())) ||
           FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP)) ||
           FAILED(format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER)) ||
           FAILED(write->CreateTextLayout(sample.data(),UINT32(sample.size()),format.Get(),float(width)-2,float(height),layout.GetAddressOf())))return Result::unavailable;
        Ptr<IWICBitmap> bitmap;
        Ptr<ID2D1RenderTarget> target;
        Ptr<ID2D1SolidColorBrush> white;
        const auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),96,96);
        if(FAILED(wic->CreateBitmap(width,height,GUID_WICPixelFormat32bppPBGRA,WICBitmapCacheOnLoad,bitmap.GetAddressOf())) ||
           FAILED(draw->CreateWicBitmapRenderTarget(bitmap.Get(),properties,target.GetAddressOf())) ||
           FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White),white.GetAddressOf())))return Result::unavailable;
        target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        target->BeginDraw();target->Clear(D2D1::ColorF(0,0));target->DrawTextLayout({2,0},layout.Get(),white.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
        if(FAILED(target->EndDraw()))return Result::unavailable;
        pixels.resize(width*height*4);
        if(FAILED(bitmap->CopyPixels(nullptr,width*4,UINT(pixels.size()),pixels.data())))return Result::unavailable;
        for(size_t offset=0;offset<pixels.size();offset+=4) pixels[offset]=pixels[offset+1]=pixels[offset+2]=255;
        return Result::pixels;
    }
    struct DraftRun {
        DWRITE_TEXT_RANGE range;
        std::wstring font;
        float em,spacing;
        std::array<float,3> rgb;
        bool bold,italic,underline,strike;
        std::vector<DWRITE_FONT_FEATURE> features;
    };
    struct DraftParagraph {
        std::wstring text;
        std::vector<DraftRun> runs;
        float left=0,right=0,before=0,after=0,first=0,leading=0;
        DWRITE_TEXT_ALIGNMENT align=DWRITE_TEXT_ALIGNMENT_LEADING;
    };
    struct DraftLayout {
        Ptr<IDWriteTextLayout> text;
        UINT32 start,length;
        float x,y,height;
    };
    bool renderDraft(const std::vector<DraftParagraph>& paragraphs,UINT width,UINT height,float em,bool wrap,
                     std::vector<unsigned char>& pixels,std::vector<DraftLayout>& positions){
        if(!initialize() || paragraphs.empty() || width<1 || height<1 || width>1800 || height>1800)return false;
        Ptr<IWICBitmap> bitmap;
        Ptr<ID2D1RenderTarget> target;
        const auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),96,96);
        if(FAILED(wic->CreateBitmap(width,height,GUID_WICPixelFormat32bppPBGRA,WICBitmapCacheOnLoad,bitmap.GetAddressOf())) ||
           FAILED(draw->CreateWicBitmapRenderTarget(bitmap.Get(),properties,target.GetAddressOf())))return false;
        Ptr<ID2D1SolidColorBrush> defaultInk;
        if(FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black),defaultInk.GetAddressOf())))return false;
        target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        target->BeginDraw();target->Clear(D2D1::ColorF(0,0));
        float y=0;
        UINT32 start=0;
        positions.clear();
        for(const auto& paragraph:paragraphs){
            y+=paragraph.before;
            Ptr<IDWriteTextFormat> format;
            if(FAILED(write->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,
                                              DWRITE_FONT_STRETCH_NORMAL,em,L"",format.GetAddressOf())))return false;
            format->SetTextAlignment(paragraph.align);
            format->SetWordWrapping(wrap?DWRITE_WORD_WRAPPING_WRAP:DWRITE_WORD_WRAPPING_NO_WRAP);
            if(paragraph.leading>0)format->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM,paragraph.leading,std::min(paragraph.leading,em*.85f));
            const float x=std::max(0.f,paragraph.left+paragraph.first);
            const float layoutWidth=std::max(1.f,float(width)-x-paragraph.right);
            Ptr<IDWriteTextLayout> layout;
            const auto text=paragraph.text.empty()?L" ":paragraph.text.c_str();
            if(FAILED(write->CreateTextLayout(text,UINT32(std::max<size_t>(1,paragraph.text.size())),format.Get(),
                                              layoutWidth,float(height),layout.GetAddressOf())))return false;
            Ptr<IDWriteTextLayout1> spacing;layout.As(&spacing);
            for(const auto& run:paragraph.runs){
                const auto& face=draftFace(run.font);
                if(!face.family.empty()){
                    layout->SetFontCollection(face.collection.Get(),run.range);
                    layout->SetFontFamilyName(face.family.c_str(),run.range);
                    layout->SetFontWeight(run.bold?DWRITE_FONT_WEIGHT_BOLD:face.weight,run.range);
                    layout->SetFontStyle(run.italic?DWRITE_FONT_STYLE_ITALIC:face.style,run.range);
                    layout->SetFontStretch(face.stretch,run.range);
                }else{
                    if(run.bold)layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD,run.range);
                    if(run.italic)layout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC,run.range);
                }
                layout->SetFontSize(run.em,run.range);
                layout->SetUnderline(run.underline,run.range);layout->SetStrikethrough(run.strike,run.range);
                if(spacing && run.spacing)spacing->SetCharacterSpacing(0,run.spacing,0,run.range);
                Ptr<IDWriteTypography> typography;
                if(!run.features.empty() && SUCCEEDED(write->CreateTypography(typography.GetAddressOf()))){
                    for(const auto& feature:run.features)typography->AddFontFeature(feature);
                    layout->SetTypography(typography.Get(),run.range);
                }
                Ptr<ID2D1SolidColorBrush> ink;
                if(SUCCEEDED(target->CreateSolidColorBrush(D2D1::ColorF(run.rgb[0],run.rgb[1],run.rgb[2]),ink.GetAddressOf())))
                    layout->SetDrawingEffect(ink.Get(),run.range);
            }
            if(y<height)target->DrawTextLayout({x,y},layout.Get(),defaultInk.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
            DWRITE_TEXT_METRICS metrics{};
            if(FAILED(layout->GetMetrics(&metrics)))return false;
            const auto length=UINT32(paragraph.text.size());
            positions.push_back({layout,start,length,x,y,metrics.height});
            start+=length+1;
            y+=metrics.height+paragraph.after;
        }
        if(FAILED(target->EndDraw()))return false;
        pixels.resize(width*height*4);
        if(FAILED(bitmap->CopyPixels(nullptr,width*4,UINT(pixels.size()),pixels.data())))return false;
        for(size_t at=0;at<pixels.size();at+=4) {
            std::swap(pixels[at],pixels[at+2]);
            const auto alpha=pixels[at+3];
            if(alpha && alpha<255)for(int channel=0;channel<3;++channel)
                pixels[at+channel]=static_cast<unsigned char>(std::min(255,(pixels[at+channel]*255+alpha/2)/alpha));
        }
        return true;
    }
};
