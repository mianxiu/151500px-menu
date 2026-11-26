#pragma once
#include <d2d1.h>
#include <dwrite_3.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <array>
#include <filesystem>
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
    std::filesystem::path photoshopFonts;
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
    bool findPhotoshopFace(const std::wstring& name,const DWRITE_FONT_PROPERTY& identity,Ptr<IDWriteFontSet>& match){
        if(photoshopFonts.empty() || name.find_first_not_of(L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.")!=std::wstring::npos)
            return false;
        for(const auto* extension:{L".otf",L".ttf"}){
            const auto path=photoshopFonts/(name+extension);
            Ptr<IDWriteFontFaceReference> reference;
            Ptr<IDWriteFontSetBuilder> builder;
            Ptr<IDWriteFontSet> privateFonts;
            if(SUCCEEDED(write->CreateFontFaceReference(path.c_str(),nullptr,0,DWRITE_FONT_SIMULATIONS_NONE,reference.GetAddressOf())) &&
               SUCCEEDED(write->CreateFontSetBuilder(builder.GetAddressOf())) &&
               SUCCEEDED(builder->AddFontFaceReference(reference.Get())) &&
               SUCCEEDED(builder->CreateFontSet(privateFonts.GetAddressOf())) &&
               SUCCEEDED(privateFonts->GetMatchingFonts(&identity,1,match.ReleaseAndGetAddressOf())) && match->GetFontCount()==1)
                return true;
        }
        return false;
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
        if((FAILED(fonts->GetMatchingFonts(&identity,1,match.GetAddressOf())) || match->GetFontCount()!=1) &&
           !findPhotoshopFace(name,identity,match))return face;
        if(FAILED(write->CreateFontCollectionFromFontSet(match.Get(),face.collection.GetAddressOf())) ||
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
    void setPhotoshopOwner(HWND owner){
        DWORD pid=0;
        GetWindowThreadProcessId(owner,&pid);
        std::wstring path(32768,L'\0');
        DWORD length=DWORD(path.size());
        const HANDLE process=pid?OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid):nullptr;
        const bool found=process && QueryFullProcessImageNameW(process,0,path.data(),&length);
        if(process)CloseHandle(process);
        std::filesystem::path directory;
        if(found){
            path.resize(length);
            const std::filesystem::path exe(path);
            if(_wcsicmp(exe.filename().c_str(),L"Photoshop.exe")==0)
                directory=exe.parent_path()/L"Required"/L"PDFL"/L"Resource"/L"Fonts";
        }
        if(directory!=photoshopFonts){photoshopFonts=std::move(directory);draftFaces.clear();}
    }
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
        UINT32 markerEnd=0;
        size_t listLevel=0;
        DWRITE_TEXT_ALIGNMENT align=DWRITE_TEXT_ALIGNMENT_LEADING;
    };
    struct DraftFragment {
        Ptr<IDWriteTextLayout> text;
        UINT32 start=0,length=0;
        float x=0,y=0,height=0;
    };
    struct DraftLayout : DraftFragment {
        DraftFragment marker;
        float firstLineBottom=0,bottom=0;
    };
    bool layoutDraft(const std::vector<DraftParagraph>& paragraphs,float width,float height,float em,bool wrap,
                     ID2D1RenderTarget& target,std::vector<DraftLayout>& positions){
        if(!initialize() || paragraphs.empty() || width<=0 || height<=0)return false;
        std::map<std::array<float,3>,Ptr<ID2D1SolidColorBrush>> inks;
        float y=0;
        UINT32 start=0;
        const auto fragment=[&](const DraftParagraph& paragraph,UINT32 from,UINT32 to,bool marker,float layoutWidth){
            DraftFragment result;
            Ptr<IDWriteTextFormat> format;
            if(FAILED(write->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,
                                              DWRITE_FONT_STRETCH_NORMAL,em,L"",format.GetAddressOf())))return result;
            format->SetTextAlignment(marker?DWRITE_TEXT_ALIGNMENT_LEADING:paragraph.align);
            format->SetWordWrapping(wrap&&!marker?DWRITE_WORD_WRAPPING_WRAP:DWRITE_WORD_WRAPPING_NO_WRAP);
            if(paragraph.leading>0)format->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM,paragraph.leading,std::min(paragraph.leading,em*.85f));
            Ptr<IDWriteTextLayout> layout;
            auto text=paragraph.text.substr(from,to-from);
            if(marker)text.back()=L' ';
            if(text.empty())text=L" ";
            if(FAILED(write->CreateTextLayout(text.c_str(),UINT32(text.size()),format.Get(),
                                              layoutWidth,height,layout.GetAddressOf())))return result;
            Ptr<IDWriteTextLayout1> spacing;layout.As(&spacing);
            for(const auto& run:paragraph.runs){
                const UINT32 first=std::max(from,run.range.startPosition);
                const UINT32 last=std::min(to,run.range.startPosition+run.range.length);
                if(first>=last)continue;
                const DWRITE_TEXT_RANGE range{first-from,last-first};
                const auto& face=draftFace(run.font);
                if(!face.family.empty()){
                    layout->SetFontCollection(face.collection.Get(),range);
                    layout->SetFontFamilyName(face.family.c_str(),range);
                    layout->SetFontWeight(run.bold?DWRITE_FONT_WEIGHT_BOLD:face.weight,range);
                    layout->SetFontStyle(run.italic?DWRITE_FONT_STYLE_ITALIC:face.style,range);
                    layout->SetFontStretch(face.stretch,range);
                }else{
                    if(run.bold)layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD,range);
                    if(run.italic)layout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC,range);
                }
                layout->SetFontSize(run.em,range);
                layout->SetUnderline(run.underline,range);layout->SetStrikethrough(run.strike,range);
                if(spacing && run.spacing)spacing->SetCharacterSpacing(0,run.spacing,0,range);
                Ptr<IDWriteTypography> typography;
                if(!run.features.empty() && SUCCEEDED(write->CreateTypography(typography.GetAddressOf()))){
                    for(const auto& feature:run.features)typography->AddFontFeature(feature);
                    layout->SetTypography(typography.Get(),range);
                }
                auto [ink,inserted]=inks.try_emplace(run.rgb);
                if(inserted && FAILED(target.CreateSolidColorBrush(D2D1::ColorF(run.rgb[0],run.rgb[1],run.rgb[2]),ink->second.GetAddressOf())))return result;
                layout->SetDrawingEffect(ink->second.Get(),range);
            }
            DWRITE_TEXT_METRICS metrics{};
            if(FAILED(layout->GetMetrics(&metrics)))return result;
            return DraftFragment{layout,from,to-from,0,0,metrics.height};
        };
        std::array<float,6> columns{};
        positions.clear();positions.resize(paragraphs.size());
        for(size_t i=0;i<paragraphs.size();++i){
            const auto& paragraph=paragraphs[i];
            if(!paragraph.markerEnd)continue;
            auto& marker=positions[i].marker;
            marker=fragment(paragraph,0,paragraph.markerEnd,true,width);
            DWRITE_TEXT_METRICS metrics{};
            if(!marker.text || FAILED(marker.text->GetMetrics(&metrics)))return false;
            auto& column=columns[paragraph.listLevel];
            column=std::max(column,metrics.widthIncludingTrailingWhitespace);
        }
        std::vector<DWRITE_LINE_METRICS> lines;
        const auto firstLine=[&](IDWriteTextLayout& layout,DWRITE_LINE_METRICS& line){
            UINT32 count=0;
            if(SUCCEEDED(layout.GetLineMetrics(&line,1,&count)))return count!=0;
            lines.resize(count);
            if(!count || FAILED(layout.GetLineMetrics(lines.data(),count,&count)))return false;
            line=lines.front();return true;
        };
        for(size_t i=0;i<paragraphs.size();++i){
            const auto& paragraph=paragraphs[i];
            auto& row=positions[i];
            const auto length=UINT32(paragraph.text.size());
            y+=paragraph.before;
            const float firstX=std::max(0.f,paragraph.left+paragraph.first);
            const float bodyX=row.marker.text?std::max(paragraph.left,firstX+columns[paragraph.listLevel]):firstX;
            auto body=fragment(paragraph,paragraph.markerEnd,length,false,std::max(.01f,width-bodyX-paragraph.right));
            if(!body.text)return false;
            body.start+=start;body.x=bodyX;body.y=y;
            if(row.marker.text){
                auto& marker=row.marker;
                DWRITE_LINE_METRICS markerLine{},bodyLine{};
                if(!firstLine(*marker.text.Get(),markerLine)||!firstLine(*body.text.Get(),bodyLine))return false;
                marker.start=start;marker.x=firstX;
                marker.y=y+std::max(0.f,bodyLine.baseline-markerLine.baseline);
                body.y=y+std::max(0.f,markerLine.baseline-bodyLine.baseline);
                row.firstLineBottom=std::max(marker.y+markerLine.height,body.y+bodyLine.height);
            }
            row.bottom=std::max(body.y+body.height,row.marker.y+row.marker.height);
            static_cast<DraftFragment&>(row)=std::move(body);
            start+=length+1;
            y=row.bottom+paragraph.after;
        }
        return true;
    }
};
