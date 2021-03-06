﻿#include <Core/luiManager.h>
#include <Graphics/luiGrUtil.h>
#include <Graphics/luiGrHlper.h>
#include <Graphics/luiGrDwrt.h>
#include <Graphics/luiGrWic.h>
#include <LongUI/luiUiXml.h>
#include <LongUI/luiUiMeta.h>
#include <LongUI/luiUiHlper.h>
#include <dwrite_2.h>

// longui::impl 命名空间
namespace LongUI { namespace impl {
    // 字体文件枚举
    class LongUIFontFileEnumerator final : public Helper::ComStatic<
        Helper::QiList<IDWriteFontFileEnumerator>> {
    public:
        // 获取当前字体文件
        HRESULT STDMETHODCALLTYPE GetCurrentFontFile(IDWriteFontFile **ppFontFile) noexcept override {
            if (!ppFontFile) return E_INVALIDARG;
            if (!m_pFilePath || !m_pFactory)  return E_FAIL;
            *ppFontFile = LongUI::SafeAcquire(m_pCurFontFie);
            return m_pCurFontFie ? S_OK : E_FAIL;
        }
        // 移动到下一个文件
        HRESULT STDMETHODCALLTYPE MoveNext(BOOL *pHasCurrentFile) noexcept override {
            if (!pHasCurrentFile)return E_INVALIDARG;
            if (!m_pFilePath || !m_pFactory) return E_FAIL;
            HRESULT hr = S_OK;
            if (*pHasCurrentFile = *m_pFilePathNow) {
                LongUI::SafeRelease(m_pCurFontFie);
                hr = m_pFactory->CreateFontFileReference(m_pFilePathNow, nullptr, &m_pCurFontFie);
                longui_debug_hr(hr, L" m_pFactory->CreateFontFileReference faild");
                if (*pHasCurrentFile = SUCCEEDED(hr)) {
                    m_pFilePathNow += std::wcslen(m_pFilePathNow);
                    ++m_pFilePathNow;
                }
            }
            return hr;
        }
    public:
        // 构造函数
        LongUIFontFileEnumerator(IDWriteFactory* f) :m_pFactory(LongUI::SafeAcquire(f)) {}
        // 析构函数
        ~LongUIFontFileEnumerator() { LongUI::SafeRelease(m_pCurFontFie); LongUI::SafeRelease(m_pFactory); }
        // 初始化
        auto Initialize(const wchar_t* path) { m_pFilePathNow = m_pFilePath = path; };
    private:
        // 文件路径 连续字符串
        const wchar_t*              m_pFilePath = nullptr;
        // 当前文件路径
        const wchar_t*              m_pFilePathNow = nullptr;
        // 当前Direct Write Font File
        IDWriteFontFile*            m_pCurFontFie = nullptr;
        // DWrite 工厂
        IDWriteFactory*             m_pFactory;
    };
    // 字体文件载入器
    class LongUIFontCollectionLoader final : public Helper::ComStatic<
        Helper::QiList<IDWriteFontCollectionLoader >> {
    public:
        // 创建枚举器
        HRESULT STDMETHODCALLTYPE CreateEnumeratorFromKey(
            IDWriteFactory *pFactory,
            const void*collectionKey,
            UINT32 collectionKeySize,
            IDWriteFontFileEnumerator **ppFontFileEnumerator
        ) noexcept override {
            UNREFERENCED_PARAMETER(collectionKeySize);
            if (!pFactory || !ppFontFileEnumerator) return E_INVALIDARG;
            m_enumerator.LongUIFontFileEnumerator::~LongUIFontFileEnumerator();
            m_enumerator.LongUIFontFileEnumerator::LongUIFontFileEnumerator(pFactory);
            m_enumerator.Initialize(reinterpret_cast<const wchar_t*>(collectionKey));
            *ppFontFileEnumerator = &m_enumerator;
            return S_OK;
        }
    public:
        // 构造函数
        LongUIFontCollectionLoader() :m_enumerator(nullptr) {}
        // 析构函数
        ~LongUIFontCollectionLoader() = default;
    private:
        // 枚举器
        LongUIFontFileEnumerator        m_enumerator;
    };
}}

// 创建LongUI的字体集: 本函数会进行I/O, 所以程序开始调用一次即可
auto LongUI::DX::CreateFontCollection(
    const wchar_t * filename,
    const wchar_t * folder,
    bool include_system
) noexcept -> IDWriteFontCollection* {
    IDWriteFontCollection* collection = nullptr;
    constexpr size_t buffer_length = 256 * 256 * 16;
    // 申请足够的空间
    wchar_t* const buffer = LongUI::NormalAllocT<wchar_t>(buffer_length);
    // 内存不足
    if (!buffer) return collection;
    // 初始化
    buffer[0] = 0; auto itr = buffer;
    // 本地路径
    {
        itr = Helper::FindFilesToBuffer(itr, buffer_length, folder, filename);
    }
    // 系统路径
    if (include_system && itr != buffer) {
        wchar_t winpath[MAX_PATH];
        winpath[0] = 0;
        auto len = ::GetWindowsDirectoryW(winpath, lengthof<uint32_t>(winpath));
        assert(len && len < lengthof(winpath) && "buffer to small");
        std::wcscpy(winpath + len, L"\\Fonts");
        itr = Helper::FindFilesToBuffer(itr, buffer_length - size_t(itr - buffer), winpath, L"*.*tf");
        itr = Helper::FindFilesToBuffer(itr, buffer_length - size_t(itr - buffer), winpath, L"*.fon");
    }
    // 当存在符合标准的文件时
    if (itr != buffer) {
        auto hr = S_OK;
        impl::LongUIFontCollectionLoader loader;
        hr = UIManager_DWriteFactory->RegisterFontCollectionLoader(&loader);
        longui_debug_hr(hr, L"UIManager_DWriteFactory->RegisterFontCollectionLoader faild");
        hr = UIManager_DWriteFactory->CreateCustomFontCollection(
            &loader,
            buffer,
            static_cast<uint32_t>(reinterpret_cast<uint8_t*>(itr + 1) - reinterpret_cast<uint8_t*>(buffer)),
            &collection
        );
        longui_debug_hr(hr, L" UIManager_DWriteFactory->CreateCustomFontCollection faild");
        hr = UIManager_DWriteFactory->UnregisterFontCollectionLoader(&loader);
        longui_debug_hr(hr, L" UIManager_DWriteFactory->UnregisterFontCollectionLoader faild");
    }
    LongUI::NormalFree(buffer);
    return collection;
}

// 创建文本格式
auto LongUI::DX::CreateTextFormat(const TextFormatProperties& prop, IDWriteTextFormat** OUT fmt) noexcept -> HRESULT {
    // 参数检查
    assert(fmt && "bad argment"); if (!fmt) return E_INVALIDARG;
#ifdef _DEBUG
    if (*fmt) {
        UIManager << DL_Warning
            << L"pointer 'fmt' pointed a non-nullptr, check it please."
            << LongUI::endl;
    }
#endif
    // 创建
    auto hr = UIManager.CreateTextFormat(
        prop.name,
        static_cast<DWRITE_FONT_WEIGHT>(prop.weight),
        static_cast<DWRITE_FONT_STYLE>(prop.style),
        static_cast<DWRITE_FONT_STRETCH>(prop.stretch),
        prop.size,
        fmt
    );
    // 成功
    if (SUCCEEDED(hr)) {
        auto format = *fmt;
        // 设置 Tab宽度
        hr = format->SetIncrementalTabStop(prop.tab == 0.f ? prop.size * 4.f : prop.tab);
        longui_debug_hr(hr, L"failed format->SetIncrementalTabStop  " << ((prop.tab == 0.f ? prop.size * 4.f : prop.tab)));
        // 设置段落排列方向
        hr = format->SetFlowDirection(static_cast<DWRITE_FLOW_DIRECTION>(prop.flow));
        longui_debug_hr(hr, L"failed format->SetFlowDirection  " << long(prop.flow));
        // 设置段落(垂直)对齐
        hr = format->SetParagraphAlignment(static_cast<DWRITE_PARAGRAPH_ALIGNMENT>(prop.valign));
        longui_debug_hr(hr, L"failed format->SetParagraphAlignment  " << long(prop.valign));
        // 设置文本(水平)对齐
        hr = format->SetTextAlignment(static_cast<DWRITE_TEXT_ALIGNMENT>(prop.halign));
        longui_debug_hr(hr, L"failed format->SetTextAlignmen  t" << long(prop.halign));
        // 设置阅读进行方向
        hr = format->SetReadingDirection(static_cast<DWRITE_READING_DIRECTION>(prop.reading));
        longui_debug_hr(hr, L"failed format->SetReadingDirection  " << long(prop.reading));
        // 设置自动换行
        hr = format->SetWordWrapping(static_cast<DWRITE_WORD_WRAPPING>(prop.wrapping));
        longui_debug_hr(hr, L"failed format->SetWordWrapping  " << long(prop.wrapping));
        // 设置
        hr = S_OK;
    }
    return hr;
}


// 初始化TextFormatProperties
LongUINoinline void LongUI::DX::InitTextFormatProperties(
    TextFormatProperties& prop, size_t name_buf_len) noexcept {
    UNREFERENCED_PARAMETER(name_buf_len);
#ifdef _DEBUG
    auto length = std::wcslen(LongUI::LongUIDefaultTextFontName) + 1;
    assert(name_buf_len > length && "buffer too small");
#endif
    // 复制数据
    prop.size       = LongUIDefaultTextFontSize;
    prop.tab        = 0.f;
    prop.weight     = static_cast<uint16_t>(DWRITE_FONT_WEIGHT_NORMAL);
    prop.style      = static_cast<uint8_t>(DWRITE_FONT_STYLE_NORMAL);
    prop.stretch    = static_cast<uint8_t>(DWRITE_FONT_STRETCH_NORMAL);
    prop.valign     = static_cast<uint8_t>(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    prop.halign     = static_cast<uint8_t>(DWRITE_TEXT_ALIGNMENT_LEADING);
    prop.flow       = static_cast<uint8_t>(DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM);
    prop.reading    = static_cast<uint8_t>(DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    prop.wrapping   = static_cast<uint32_t>(DWRITE_WORD_WRAPPING_NO_WRAP);
    std::wcscpy(prop.name, LongUIDefaultTextFontName);
}

// 做一个文本格式
auto LongUI::DX::MakeTextFormat(
    IN pugi::xml_node node,
    OUT IDWriteTextFormat** fmt,
    IN OPTIONAL IDWriteTextFormat* template_fmt,
    IN OPTIONAL const char* prefix) noexcept -> HRESULT {
    // 参数检查
    assert(fmt && "bad argment"); if (!fmt) return E_INVALIDARG;
#ifdef _DEBUG
    if (*fmt) {
        UIManager << DL_Warning
            << L"pointer 'fmt' pointed a non-nullptr, check it please."
            << LongUI::endl;
    }
#endif
    // 数据
    struct { TextFormatProperties prop; wchar_t buffer[MAX_PATH]; } data;
    assert(static_cast<void*>(data.buffer) > static_cast<void*>(&data.prop) && "bad");
    // 创建新的?
    bool create_a_new_one = false;
    // 存在模板?
    if (template_fmt) {
        // 模板初始化
        auto len = template_fmt->GetFontFamilyNameLength();
        assert(len < MAX_PATH && "buffer too small");
        template_fmt->GetFontFamilyName(data.prop.name, len + 1);
        data.prop.size = template_fmt->GetFontSize();
        data.prop.tab = template_fmt->GetIncrementalTabStop();
        data.prop.weight = static_cast<uint16_t>(template_fmt->GetFontWeight());
        data.prop.style = static_cast<uint8_t>(template_fmt->GetFontStyle());
        data.prop.stretch = static_cast<uint8_t>(template_fmt->GetFontStretch());
        data.prop.valign = static_cast<uint8_t>(template_fmt->GetParagraphAlignment());
        data.prop.halign = static_cast<uint8_t>(template_fmt->GetTextAlignment());
        data.prop.flow = static_cast<uint8_t>(template_fmt->GetFlowDirection());
        data.prop.reading = static_cast<uint8_t>(template_fmt->GetReadingDirection());
        data.prop.wrapping = static_cast<uint32_t>(template_fmt->GetWordWrapping());
    }
    else {
        // 默认初始化
        DX::InitTextFormatProperties(data.prop, MAX_PATH);
        create_a_new_one = true;
    }
    // xml 节点
    {
        auto get_attribute = [=](const char* name) noexcept {
            return Helper::XMLGetValue(node, name, prefix);
        };
        // 字体名称
        auto str = get_attribute("family");
        if (str) {
            // 假设设置字体名称就是修改了
            LongUI::UTF8toWideChar(str, data.prop.name, MAX_PATH)[0] = 0;
            create_a_new_one = true;
        }
        // 字体大小
        if (str = get_attribute("size")) {
            auto tmp = LongUI::AtoF(str);
            create_a_new_one = tmp != data.prop.size || create_a_new_one;
            data.prop.size = tmp;
        }
        // 获取字体粗细
        if (str = get_attribute("weight")) {
            auto tmp = static_cast<uint16_t>(LongUI::AtoI(str));
            create_a_new_one = tmp != data.prop.weight || create_a_new_one;
            data.prop.weight = tmp;
        }
        // 字体风格
        {
            auto tmp = static_cast<uint8_t>(Helper::GetEnumFromXml(node, static_cast<DWRITE_FONT_STYLE>(data.prop.style), "style", prefix));
            create_a_new_one = tmp != data.prop.style || create_a_new_one;
            data.prop.style = tmp;
        }
        // 字体拉伸
        {
            auto tmp = static_cast<uint8_t>(Helper::GetEnumFromXml(node, static_cast<DWRITE_FONT_STRETCH>(data.prop.stretch), "stretch", prefix));
            create_a_new_one = tmp != data.prop.stretch || create_a_new_one;
            data.prop.stretch = tmp;
        }
        // Tab宽度
        float tabstop = data.prop.size * 4.f;
        // 检查Tab宽度
        if (str = get_attribute("tabstop")) {
            // 假设设置字体名称就是修改了
            tabstop = LongUI::AtoF(str);
            create_a_new_one = true;
        }
        // 阅读进行方向
        {
            auto tmp = static_cast<uint8_t>(Helper::GetEnumFromXml(
                node, static_cast<DWRITE_READING_DIRECTION>(data.prop.reading), 
                "readingdirection", prefix)
                );
            create_a_new_one = tmp != data.prop.reading || create_a_new_one;
            data.prop.reading = tmp;
        }
        // 段落排列方向
        {
            auto tmp = static_cast<uint8_t>(Helper::GetEnumFromXml(
                node, static_cast<DWRITE_FLOW_DIRECTION>(data.prop.flow), 
                "flowdirection", prefix)
                );
            create_a_new_one = tmp != data.prop.flow || create_a_new_one;
            data.prop.flow = tmp;
        }
        // 段落(垂直)对齐
        {
            auto tmp = static_cast<uint8_t>(Helper::GetEnumFromXml(
                node, static_cast<DWRITE_PARAGRAPH_ALIGNMENT>(data.prop.valign), 
                "valign", prefix)
                );
            create_a_new_one = tmp != data.prop.valign || create_a_new_one;
            data.prop.valign = tmp;
        }
        // 文本(水平)对齐
        {
            auto tmp = static_cast<uint8_t>(Helper::GetEnumFromXml(
                node, static_cast<DWRITE_TEXT_ALIGNMENT>(data.prop.halign), 
                "align", prefix)
                );
            create_a_new_one = tmp != data.prop.halign || create_a_new_one;
            data.prop.halign = tmp;
        }
        // 设置自动换行
        {
            auto tmp = static_cast<uint32_t>(Helper::GetEnumFromXml(
                node, static_cast<DWRITE_WORD_WRAPPING>(data.prop.wrapping), 
                "wordwrapping", prefix)
                );
            create_a_new_one = tmp != data.prop.wrapping || create_a_new_one;
            data.prop.wrapping = tmp;
        }
    }
    // 创建新的
    if (create_a_new_one) {
        return DX::CreateTextFormat(data.prop, fmt);
    }
    // 使用旧的, 检查逻辑
    assert(template_fmt && "check logic");
    template_fmt->AddRef();
    *fmt = template_fmt;
    return S_FALSE;
}

// 从 文本格式创建几何
auto LongUI::DX::CreateTextPathGeometry(
    IN const char32_t* utf32_string,
    IN uint32_t string_length,
    IN IDWriteTextFormat* format,
    IN ID2D1Factory* factory,
    IN OUT OPTIONAL IDWriteFontFace** _fontface,
    OUT ID2D1PathGeometry** geometry
) noexcept -> HRESULT {
    // 参数检查
    assert(utf32_string && string_length && format && geometry && factory && "bad arugments");
    if (!utf32_string || !string_length || !format || !geometry || !factory) return E_INVALIDARG;
    // 字体集
    IDWriteFontCollection* collection = nullptr;
    IDWriteFontFamily* family = nullptr;
    IDWriteFont* font = nullptr;
    IDWriteFontFace* fontface = nullptr;
    ID2D1PathGeometry* pathgeometry = nullptr;
    if (_fontface) fontface = LongUI::SafeAcquire(*_fontface);
    // 字体名称缓存
    wchar_t fontname_buffer[MAX_PATH]; *fontname_buffer = 0;
    // 必要缓存
    EzContainer::SmallBuffer<uint16_t, 1024> glyph_indices_buffer;
    glyph_indices_buffer.NewSize(string_length);
    // 内存不足?
    HRESULT hr = glyph_indices_buffer.GetCount() < string_length ? E_OUTOFMEMORY : S_OK;
    // 创建字形
    if (!fontface) {
        // 获取字体名称
        if (SUCCEEDED(hr)) {
            hr = format->GetFontFamilyName(fontname_buffer, MAX_PATH);
            longui_debug_hr(hr, L"failed: format->GetFontFamilyName " << fontname_buffer);
        }
        // 获取字体集
        if (SUCCEEDED(hr)) {
            hr = format->GetFontCollection(&collection);
            longui_debug_hr(hr, L"failed: format->GetFontCollection ");
        }
        // 查找索引
        uint32_t index = 0; BOOL exists = FALSE;
        if (SUCCEEDED(hr)) {
            hr = collection->FindFamilyName(fontname_buffer, &index, &exists);
            longui_debug_hr(hr, L"failed: collection->FindFamilyName " << fontname_buffer);
        }
        // 获取字体族
        if (SUCCEEDED(hr)) {
            // 不存在也算错误
            if (exists) {
                hr = collection->GetFontFamily(index, &family);
                longui_debug_hr(hr, L"failed: collection->GetFontFamily " << long(index));
            }
            else {
                hr = E_FAIL;
            }
        }
        // 获取字体
        if (SUCCEEDED(hr)) {
            hr = family->GetFirstMatchingFont(
                format->GetFontWeight(),
                format->GetFontStretch(),
                format->GetFontStyle(),
                &font
            );
            longui_debug_hr(hr, L"failed: family->GetFirstMatchingFont ");

        }
        // 创建字形
        if (SUCCEEDED(hr)) {
            hr = font->CreateFontFace(&fontface);
            longui_debug_hr(hr, L"failed: font->CreateFontFace ");
        }
    }
    // 创建几何
    if (SUCCEEDED(hr)) {
        hr = factory->CreatePathGeometry(&pathgeometry);
        longui_debug_hr(hr, L"failed: factory->CreatePathGeometry ");
        ID2D1GeometrySink* sink = nullptr;
        // 打开Sink
        if (SUCCEEDED(hr)) {
            hr = pathgeometry->Open(&sink);
            longui_debug_hr(hr, L"failed: pathgeometry->Open ");
        }
        // 创建索引编号
        if (SUCCEEDED(hr)) {
            static_assert(sizeof(uint32_t) == sizeof(char32_t), "32 != 32 ?!");
            hr = fontface->GetGlyphIndices(
                reinterpret_cast<const uint32_t*>(utf32_string),
                string_length,
                glyph_indices_buffer.GetData()
            );
            longui_debug_hr(hr, L"failed: fontface->GetGlyphIndices ");
        }
        // 创建轮廓路径几何
        if (SUCCEEDED(hr)) {
            hr = fontface->GetGlyphRunOutline(
                format->GetFontSize(),
                glyph_indices_buffer.GetData(),
                nullptr, nullptr,
                string_length,
                false, false, sink
            );
            longui_debug_hr(hr, L"failed: fontface->GetGlyphRunOutline ");
        }
        // 关闭路径
        if (SUCCEEDED(hr)) {
            hr = sink->Close();
            longui_debug_hr(hr, L"failed: sink->Close ");
        }
        LongUI::SafeRelease(sink);
    }
    // 扫尾
    LongUI::SafeRelease(collection);
    LongUI::SafeRelease(family);
    LongUI::SafeRelease(font);
    if (_fontface && !(*_fontface)) {
        *_fontface = fontface;
    }
    else {
        LongUI::SafeRelease(fontface);
    }
    *geometry = pathgeometry;
#ifdef _DEBUG
    if (pathgeometry) {
        float float_var = 0.f;
        pathgeometry->ComputeLength(nullptr, &float_var);
        pathgeometry->ComputeArea(nullptr, &float_var);
        float_var = 0.f;
    }
#endif
    return hr;
}

// 利用几何体创建网格
auto LongUI::DX::CreateMeshFromGeometry(ID2D1Geometry* geometry, ID2D1Mesh** mesh) noexcept -> HRESULT {
    UNREFERENCED_PARAMETER(geometry);
    assert(mesh && "bad arguemnt"); if (!mesh) return E_INVALIDARG;
#ifdef _DEBUG
    if (*mesh) {
        UIManager << DL_Warning
            << L"pointer 'mesh' pointed a non-nullptr, check it please."
            << LongUI::endl;
    }
#endif
    return E_NOTIMPL;
}

namespace LongUI { namespace impl {
    // 处理状态机
    enum xml_state : uint32_t {
        state_text = 0, // 文本处理
        state_tagb,     // 标签起始
        state_tag1,     // 起始标签
        state_tag2,     // 终结标签
        state_w8av,     // 等待属性
        state_w8rt,     // 等待右标
        state_attr,     // 处理属性
        state_var1,     // 前属性值
        state_var2,     // 后属性值
    };
    // 检查是否为左符号
    inline auto check_ltag(wchar_t ch) noexcept { 
        return ch == '<';  
    };
    // 检查是否为右符号
    inline auto check_rtag(wchar_t ch) noexcept { 
        return ch == '>';  
    };
    // 标签闭合符号
    inline auto check_clz(wchar_t ch) noexcept { 
        return ch == '/';  
    };
    // 等号符号
    inline auto check_equal(wchar_t ch) noexcept { 
        return ch == '=';  
    };
    // 引号符号
    inline auto check_quot(wchar_t ch) noexcept { 
        return ch == '"' ;  
    };
}}

// 仅仅检查Xml有效性
bool LongUI::DX::CheckXmlValidity(const wchar_t* xml) noexcept {
    if (!xml) return false;
    // 最大支持嵌套的层数
    //constexpr size_t MAX_TAG_LEVEL = 2;
    constexpr size_t MAX_TAG_LEVEL = 256;
    struct tag { const wchar_t* begin; const wchar_t* end; };
    tag tags[MAX_TAG_LEVEL]; auto* toptag = tags;
    tag tag2 = { nullptr, nullptr };
    // 处理状态机
    impl::xml_state fsm = impl::state_text;
    // 标签相等
    auto same_tag = [](const tag& a, const tag& b) {
        return (a.end - a.begin) == (b.end - b.begin) &&
            !std::wcsncmp(a.begin, b.begin, b.end - b.begin);
    };
    // 处理栈为空
    auto stack_empty = [&]() { return toptag == tags; };
    // 处理栈溢出
    auto stack_overflow = [&]() { return toptag == tags + MAX_TAG_LEVEL; };
    // 标记标签1起点
    auto mark1_begin = [&](const wchar_t* itr) { toptag->begin = itr; };
    // 标记标签1终点
    auto mark1_end = [&](const wchar_t* itr) { toptag->end = itr; ++toptag; };
    // 标签1弹出
    auto mark1_pop = [&]() { --toptag; };
    // 标记标签2起点
    auto mark2_begin = [&](const wchar_t* itr) { tag2.begin = itr; };
    // 标记标签2终点
    auto mark2_end = [&](const wchar_t* itr) { tag2.end = itr; };
    // 正式处理
    for (auto itr = xml; *itr; ++itr) {
        // 分状态讨论
        switch (fsm)
        {
        case impl::state_text:
            // 等待标签起始符号出现 -> state_tagb
            if (impl::check_ltag(*itr)) fsm = impl::state_tagb;
            break;
        case impl::state_tagb:
            // 标签闭合符号 -> state_tag2
            if (impl::check_clz(*itr)) {
                fsm = impl::state_tag2;
                mark2_begin(itr+1);
            }
            // 否则 -> state_tag1
            else {
                assert(stack_overflow() == false && "stack overflow");
                if (stack_overflow()) return false;
                fsm = impl::state_tag1;
                mark1_begin(itr);
            }
            break;
        case impl::state_tag1:
            // 出现关闭符号
            if (impl::check_clz(*itr)) {
                // 只需要等待关闭标签出现
                fsm = impl::state_w8rt;
            }
            // 出现空格或者右标签符号
            else if (white_space(*itr) || impl::check_rtag(*itr)) {
                // 标记标签结束位置
                mark1_end(itr);
                // 出现右标签 -> state_text
                if (impl::check_rtag(*itr)) fsm = impl::state_text;
                // 出现空格 -> state_w8av
                else fsm = impl::state_w8av;
            }
            break;
        case impl::state_tag2:
            // 出现空格或者右标签符号
            if (white_space(*itr) || impl::check_rtag(*itr)) {
                mark2_end(itr);
                // 标签栈为空 -> 失败
                if (stack_empty()) return false;
                // 弹出栈顶
                mark1_pop();
                // 标签名不匹配 -> 失败
                if (!same_tag(tag2, *toptag)) return false;
                // 出现右标签 -> state_text
                if (impl::check_rtag(*itr)) fsm = impl::state_text;
                // 出现空格 -> state_w8rt
                else fsm = impl::state_w8rt;
            }
            break;
        case impl::state_w8av:
            // 结束标签出现 -> state_w8rt
            if (impl::check_clz(*itr)) {
                fsm = impl::state_w8rt;
                mark1_pop();
            }
            // 结束标签 -> state_text
            else if (impl::check_rtag(*itr)) fsm = impl::state_text;
            // 属性出现(非空格) -> state_attr
            else if (!white_space(*itr)) fsm =impl:: state_attr;
            break;
        case impl::state_w8rt:
            // 等待关闭标签出现 -> state_text
            if (impl::check_rtag(*itr)) fsm = impl::state_text;
            break;
        case impl::state_attr:
            // 等待等号出现 -> state_var1
            if (impl::check_equal(*itr)) fsm = impl::state_var1;
            break;
        case impl::state_var1:
            // 等待引号出现 -> state_var2
            if (impl::check_quot(*itr)) fsm = impl::state_var2;
            break;
        case impl::state_var2:
            // 等待引出现 -> state_w8av
            if (impl::check_quot(*itr)) fsm = impl::state_w8av;
            break;
        }
    }
    // 返回处理结果
    return stack_empty();
}

/*
    只支持文本内转义

    &amp;   &   和
    &lt;    <   小于号
    &gt;    >   大于号
    &apos;  '   单引号
    &quot;  "   双引号

    <b>     粗体
    <i>     斜体
    <u>     下划线
    <s>     删除线
    <font>  字体
            color字体颜色
            size 字体大小
            face 字体名称
    <eval>  字符串替换字符串    -- 需要配置接口支持
            str  字符串
    <ruby>  注音标签
            str  注音字符串
            face 字体名称
            zoom 字体比例 默认0.5


    <img>   显示图片            -- 需要配置接口支持
            src  图片字符串 默认: 接口规定
            clip 剪切矩形   默认: 图片大小
            size 显示大小   默认: 剪切大小

*/


// longui::impl 命名空间
namespace LongUI {
    // 创建RUBY注音内联对象
    auto CreateRubyNotation(IDWriteTextLayout* ruby, 
        IDWriteTextLayout* text) noexcept->IDWriteInlineObject*;
}

// longui::impl 命名空间
namespace LongUI { namespace impl {
    // 缓存长度
    enum : size_t {
#ifdef _DEBUG
        FIXED_BUF_LEN = 1,
#else
        FIXED_BUF_LEN = 1024,
#endif
    };
    // 字符串对
    template<typename T> struct string_pair { const T* begin; const T* end; };
    // 简陋Xml解析器
    struct ez_xml_parser {
        // 字符类型
        using xmlchar_t = wchar_t;
        // strpair
        using strpair = string_pair<xmlchar_t>;
        // 类型
        enum ft : uint8_t { b,i,u,s,font,eval,ruby,img,unk };
        // 单位
        struct unit { 
            uint32_t bgn; 
            uint16_t len; 
            uint16_t fmt;
            union {
                struct { 
                    const xmlchar_t*    name1;
                    const xmlchar_t*    name2;
                    D2D1_COLOR_F        color; 
                    float               size; 
                    float               unused; 
                } font;
                struct {
                    const xmlchar_t*    name1;
                    const xmlchar_t*    name2;
                    D2D1_RECT_F         clip; 
                    D2D1_SIZE_F         size; 
                } img ;
                struct { 
                    const xmlchar_t*    name1;
                    const xmlchar_t*    name2;
                } eval ;
                struct { 
                    const xmlchar_t*    face1;
                    const xmlchar_t*    face2;
                    const xmlchar_t*    name1;
                    const xmlchar_t*    name2;
                    float               zoom; 
                    float               unused; 
                } ruby ;
            };
        };
        // 构造函数
        ez_xml_parser() noexcept = default;
        // 析构函数
        ~ez_xml_parser() noexcept;
        // 标记标签起始
        void mk_tagbegin() noexcept;
        // 标记标签起始
        void mk_tagend() noexcept;
        // 设置属性
        void set_attr() noexcept;
        // 添加字符
        void push_char(xmlchar_t) noexcept;
        // 内存OK
        bool strok() const noexcept { return buffer != nullptr; }
        // 内存ok
        bool initok() const noexcept { return inits != nullptr; }
        // 内存ok
        bool setsok() const noexcept { return sets != nullptr && strok(); }
        // 设置顶层格式
        template<ft fmttype> inline void setfmt() {
            inits_top->fmt = fmttype; ++inits_top;
        }
        // 重新分配字符串缓存
        void reallocstr() noexcept;
        // 缓存指针
        xmlchar_t*  buffer = fixed_buffer;
        // 初始栈栈底
        unit*       inits = init_buffer;
        // 初始栈栈顶
        unit*       inits_top = init_buffer;
        // 设置栈栈底
        unit*       sets = sets_buffer;
        // 设置栈栈顶
        unit*       sets_top = sets_buffer;
        // 属性名字符串
        strpair     attr_pair;
        // 属性值字符串
        strpair     valu_pair;
        // 标签名字符串
        strpair     taag_pair;
        // 缓存长度
        uint32_t    buflen = FIXED_BUF_LEN;
        // 初始栈长度
        uint32_t    inits_len = FIXED_BUF_LEN;
        // 初始栈长度
        uint32_t    sets_len = FIXED_BUF_LEN;
        // 字符串长度
        uint32_t    strlen = 0;
        // 固定缓存大小
        xmlchar_t   fixed_buffer[FIXED_BUF_LEN];
        // 初始栈缓存
        unit        init_buffer[FIXED_BUF_LEN];
        // 设置栈缓存
        unit        sets_buffer[FIXED_BUF_LEN];
    };
    /// <summary>
    /// Finalizes an instance of the <see cref="ez_xml_parser"/> class.
    /// </summary>
    /// <returns></returns>
    ez_xml_parser::~ez_xml_parser() noexcept {
        // 释放缓存
        if (buffer != fixed_buffer) LongUI::NormalFree(buffer);
        if (inits != init_buffer) LongUI::NormalFree(inits);
        if (sets != sets_buffer) LongUI::NormalFree(sets);
    }
    /// <summary>
    /// push the specified char to this
    /// </summary>
    /// <param name="ch">The ch.</param>
    /// <returns></returns>
    void ez_xml_parser::push_char(xmlchar_t ch) noexcept {
        assert(buflen < 1024 * 1024 * 16 && "16MB memory used");
        // 缓存不足
        if (buflen <= strlen) {
            buflen *= 2; this->reallocstr();
            if (!buffer) return;
        }
        // 有效
        if (buffer) buffer[strlen++] = ch;
    }
    // wcsncmp for impl
    template<size_t len, typename T>
    inline auto wcsncmp(const T* __restrict a, const T* __restrict b) noexcept {
        assert(a && b && "you bad bad");
        return std::memcmp(a, b, sizeof(T) * len);
    }
    /// <summary>
    /// mark tag begin
    /// </summary>
    /// <returns></returns>
    void ez_xml_parser::mk_tagbegin() noexcept {
        auto tag1 = taag_pair.begin;
        auto tag2 = taag_pair.end;
        assert(tag1&& tag2 && tag2 > tag1);
        // 保证空间
        if (inits_top == inits + inits_len) {
            auto oldbuf = inits; inits_len *= 2;
            inits = LongUI::NormalAllocT<unit>(inits_len);
            if (inits) std::memcpy(inits, oldbuf, (char*)(inits_top) - (char*)(oldbuf));
            inits_top = inits + (inits_top - oldbuf);
            if (oldbuf != init_buffer) LongUI::NormalFree(oldbuf);
            if (!inits) return;
        }
        // 标记起始位置
        inits_top->bgn = this->strlen;
        std::memset(&inits_top->font, 0, sizeof(inits_top->font));
        // 一个
        if (tag2 - tag1 == 1) {
            switch (*tag1)
            {
            case 'b': this->setfmt<b>(); return;
            case 'i': this->setfmt<i>(); return;
            case 'u': this->setfmt<u>(); return;
            case 's': this->setfmt<s>(); return;
            }
        }
        // 四个
        else if (tag2 - tag1 == 4) {
            if (!impl::wcsncmp<4>(tag1, L"font")) return this->setfmt<font>();
            else if (!impl::wcsncmp<4>(tag1, L"eval")) return this->setfmt<eval>();
            else if (!impl::wcsncmp<4>(tag1, L"ruby")) return this->setfmt<ruby>();
        }
        // 三个
        else if (tag2 - tag1 == 3) {
            if (!impl::wcsncmp<3>(tag1, L"img")) return this->setfmt<img>();
            else {

            }
        }
        // 无效
        assert(!"unknown tag for tag1~tag2");
        this->setfmt<unk>();
    }
    /// <summary>
    /// mark tag end
    /// </summary>
    /// <returns></returns>
    void ez_xml_parser::mk_tagend() noexcept {
        assert(inits && inits_top != inits);
        --inits_top;
        inits_top->len = uint16_t(strlen - inits_top->bgn);
        // 保证空间
        if (sets_top == sets + sets_len) {
            auto oldbuf = sets; sets_len *= 2;
            sets = LongUI::NormalAllocT<unit>(sets_len);
            if (sets) std::memcpy(sets, oldbuf, (char*)(sets_top) - (char*)(oldbuf));
            sets_top = sets + (sets_top - oldbuf);
            if (oldbuf != sets_buffer) LongUI::NormalFree(oldbuf);
            if (!sets) return;
        }
        // 图片占位符添加
        if (inits_top->fmt == ft::img) {
            this->push_char(xmlchar_t('*'));
            inits_top->len++;
        }
        // 字符串计算
        else if (inits_top->fmt == ft::eval) {
            IUITextFormatter::StringPair pair{
                inits_top->eval.name1,
                inits_top->eval.name2
            };
            pair = UIManager.XmlEvalString(pair);
            if (pair.begin) {
                uint32_t pairlen = static_cast<uint32_t>(pair.end - pair.begin);
                if (buflen - strlen < pairlen) {
                    buflen += pairlen * 2; this->reallocstr();
                }
                if (buffer) {
                    std::memcpy(buffer + strlen, pair.begin, pairlen * sizeof(xmlchar_t));
                    strlen += pairlen;
                }
                UIManager.XmlFreeString(pair);
            }
        }
        *sets_top = *inits_top; 
        ++sets_top;
    }
    /// <summary>
    /// Reallocstrs this instance.
    /// </summary>
    /// <returns></returns>
    void ez_xml_parser::reallocstr() noexcept {
        auto oldbuf = buffer; 
        buffer = LongUI::NormalAllocT<xmlchar_t>(buflen);
        if (buffer) std::memcpy(buffer, oldbuf, sizeof(wchar_t) * strlen);
        if (oldbuf != fixed_buffer) LongUI::NormalFree(oldbuf);
    }
    /// <summary>
    /// Set_attrs the specified a.
    /// </summary>
    /// <param name="a">a.</param>
    /// <param name="v">The v.</param>
    /// <returns></returns>
    void ez_xml_parser::set_attr() noexcept {
        assert(attr_pair.begin&& attr_pair.end && attr_pair.end > attr_pair.begin);
        assert(valu_pair.begin&& valu_pair.end && valu_pair.end >=valu_pair.begin);
        assert(inits && inits_top > inits);
        auto& unit = inits_top[-1];
        constexpr size_t VAULE_FIXED_BUFFER_LENTH = 1024;
        assert(size_t(valu_pair.end - valu_pair.begin) < VAULE_FIXED_BUFFER_LENTH);
        if (size_t(valu_pair.end - valu_pair.begin) >= VAULE_FIXED_BUFFER_LENTH) return;
        xmlchar_t strbuffer[VAULE_FIXED_BUFFER_LENTH];
        // 复制字符串
        auto pair = valu_pair;
        auto copy_string_utf16 = [&]() noexcept {
            size_t cpylen = size_t(pair.end - pair.begin) * sizeof(xmlchar_t);
            std::memcpy(strbuffer, pair.begin, cpylen);
            strbuffer[size_t(pair.end - pair.begin)] = 0;
            return strbuffer;
        };
        auto copy_string_utf8 = [&]() noexcept {
            auto u8itr = reinterpret_cast<char*>(strbuffer);
            for (auto itr = pair.begin; itr != pair.end; ++itr) {
                *u8itr = char(*itr); ++u8itr;
            }
            *u8itr = 0; u8itr = reinterpret_cast<char*>(strbuffer);
            return u8itr;
        };
        // 分情况讨论
        switch (unit.fmt)
        {
        case LongUI::impl::ez_xml_parser::b:
            break;
        case LongUI::impl::ez_xml_parser::i:
            break;
        case LongUI::impl::ez_xml_parser::u:
            break;
        case LongUI::impl::ez_xml_parser::s:
            break;
        case LongUI::impl::ez_xml_parser::font:
            // 字体大小
            if (!impl::wcsncmp<4>(attr_pair.begin, L"size")) {
                unit.font.size = LongUI::AtoF(copy_string_utf16());
            }
            // 字体颜色
            else if (!impl::wcsncmp<5>(attr_pair.begin, L"color")) {
                LongUI::Helper::MakeColor(copy_string_utf8(), unit.font.color);
            }
            // 字体名称
            else if (!impl::wcsncmp<4>(attr_pair.begin, L"face")) {
                unit.font.name1 = valu_pair.begin;
                unit.font.name2 = valu_pair.end;
            }
            break;
        case LongUI::impl::ez_xml_parser::eval:
            // 计算字符串
            if (!impl::wcsncmp<3>(attr_pair.begin, L"str")) {
                unit.eval.name1 = valu_pair.begin;
                unit.eval.name2 = valu_pair.end;
            }
            break;
        case LongUI::impl::ez_xml_parser::ruby:
            // 注音字符串
            if (!impl::wcsncmp<3>(attr_pair.begin, L"str")) {
                unit.ruby.name1 = valu_pair.begin;
                unit.ruby.name2 = valu_pair.end;
            }
            // 注音字体大小
            else if (!impl::wcsncmp<4>(attr_pair.begin, L"zoom")) {
                unit.ruby.zoom = LongUI::AtoF(copy_string_utf16());
            }
            // 注音字体名称
            else if (!impl::wcsncmp<4>(attr_pair.begin, L"face")) {
                unit.ruby.face1 = valu_pair.begin;
                unit.ruby.face2 = valu_pair.end;
            }
            break;
        case LongUI::impl::ez_xml_parser::img:
            // 图片源
            if (!impl::wcsncmp<3>(attr_pair.begin, L"src")) {
                unit.img.name1 = valu_pair.begin;
                unit.img.name2 = valu_pair.end;
            }
            // 裁剪区
            else if (!impl::wcsncmp<4>(attr_pair.begin, L"clip")) {
                Helper::MakeFloats(copy_string_utf8(), unit.img.clip);
            }
            // 显示大小
            else if (!impl::wcsncmp<4>(attr_pair.begin, L"size")) {
                Helper::MakeFloats(copy_string_utf8(), unit.img.size);
            }
            break;
        case LongUI::impl::ez_xml_parser::unk:
        default:
            assert(!"UNKNDOWN FORMAT");
            break;
        }
    }
    /// <summary>
    /// XMLs the escape character.
    /// </summary>
    /// <param name="">The .</param>
    /// <param name="ch">The ch.</param>
    /// <returns></returns>
    inline auto xml_escape_char(const wchar_t* str, wchar_t& ch) noexcept {
        // 转义起始
        if (str[0] == '&') {
            // &amp;   &   和
            if (str[1] == 'a' && str[2] == 'm' && str[3] == 'p' && str[4] == ';') {
                ch = '&';
                str += 4;
            }
            // &lt;    <   小于号
            else if (str[1] == 'l' && str[2] == 't' && str[3] == ';') {
                ch = '<';
                str += 3;
            }
            // &gt;    >   大于号
            else if (str[1] == 'g' && str[2] == 't' && str[3] == ';') {
                ch = '>';
                str += 3;
            }
            // &apos;  '   单引号
            else if (str[1] == 'a' && str[2] == 'p' && 
                str[3] == 'o' && str[4] == 's' && str[5] == ';') {
                ch = '\'';
                str += 5;
            }
            // &quot;  "   双引号
            else if (str[1] == 'q' && str[2] == 'u' && 
                str[3] == 'o' && str[4] == 't' && str[5] == ';') {
                ch = '"';
                str += 5;
            }
        }
        // 返回
        return str;
    }
    /// <summary>
    /// Set_fonts the specified unit.
    /// </summary>
    /// <param name="unit">The unit.</param>
    /// <param name="layout">The layout.</param>
    void set_font(const ez_xml_parser::unit& unit, IDWriteTextLayout& layout) {
        constexpr size_t FACENAME_BUFFER_LEN = MAX_PATH * 2;
        DWRITE_TEXT_RANGE range{unit.bgn, unit.len};
        if (unit.font.size > 0.f) {
            layout.SetFontSize(unit.font.size, range);
        }
        if (unit.font.color.a > 0.f) {
            if (const auto c = CUIColorEffect::Create(unit.font.color)) {
                layout.SetDrawingEffect(c, range);
                c->Release();
            }
        }
        if (unit.font.name1) {
            wchar_t facebuf[FACENAME_BUFFER_LEN];
            size_t len = unit.font.name2 - unit.font.name1;
            assert(len < FACENAME_BUFFER_LEN && "buffer too small");
            std::memcpy(facebuf, unit.font.name1, sizeof(facebuf[0])* len);
            facebuf[len] = 0;
            layout.SetFontFamilyName(facebuf, range);
        }
    }
    /// <summary>
    /// Set_rubies the specified unit.
    /// </summary>
    /// <param name="unit">The unit.</param>
    /// <param name="layout">The layout.</param>
    void set_ruby(const ez_xml_parser::unit& unit, 
        IDWriteTextLayout& layout, const wchar_t* str) {
        assert(unit.ruby.name1 && unit.ruby.name2);
        // 注音必须有被注音的部分.....和注音的部分
        if (!(unit.len && unit.ruby.name2 > unit.ruby.name1)) return;
        IDWriteTextLayout* ruby = nullptr, *text = nullptr;
        auto rlen = static_cast<uint32_t>(unit.ruby.name2 - unit.ruby.name1);
        auto tlen = static_cast<uint32_t>(unit.len);
        auto hr = S_OK;
        // 创建注音布局
        if (SUCCEEDED(hr)) {
            hr = UIManager_DWriteFactory->CreateTextLayout(
                unit.ruby.name1, rlen, &layout, 32.f, 32.f, &ruby
            );
        }
        // 创建文本布局
        if (SUCCEEDED(hr)) {
            hr = UIManager_DWriteFactory->CreateTextLayout(
                str + unit.bgn, tlen, &layout, 32.f, 32.f, &text
            );
        }
        // 复制格式
        if (SUCCEEDED(hr)) {
            // 字体效果
            IUnknown* effect = nullptr;
            layout.GetDrawingEffect(unit.bgn, &effect, nullptr);
            ruby->SetDrawingEffect(effect, DWRITE_TEXT_RANGE{ 0, rlen });
            text->SetDrawingEffect(effect, DWRITE_TEXT_RANGE{ 0, tlen });
            // 字体倾斜
            DWRITE_FONT_STYLE style = DWRITE_FONT_STYLE_NORMAL;
            layout.GetFontStyle(unit.bgn, &style, nullptr);
            ruby->SetFontStyle(style, DWRITE_TEXT_RANGE{ 0, rlen });
            text->SetFontStyle(style, DWRITE_TEXT_RANGE{ 0, tlen });
            // 字体粗细
            DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL;
            layout.GetFontWeight(unit.bgn, &weight, nullptr);
            ruby->SetFontWeight(weight, DWRITE_TEXT_RANGE{ 0, rlen });
            text->SetFontWeight(weight, DWRITE_TEXT_RANGE{ 0, tlen });
            // 字体名称
            constexpr uint32_t FACENAME_BUFFER_LEN = MAX_PATH * 2;
            wchar_t facebuf[FACENAME_BUFFER_LEN]; facebuf[0] = 0;
            layout.GetFontFamilyName(unit.bgn, facebuf, FACENAME_BUFFER_LEN, nullptr);
            text->SetFontFamilyName(facebuf, DWRITE_TEXT_RANGE{ 0, tlen });
            size_t len = (const char*)(unit.ruby.face2) - (const char*)(unit.ruby.face1);
            if (len && len < size_t(FACENAME_BUFFER_LEN * 2)) {
                std::memcpy(facebuf, unit.ruby.face1, len);
                facebuf[len / sizeof(wchar_t)] = 0;
            }
            ruby->SetFontFamilyName(facebuf, DWRITE_TEXT_RANGE{ 0, rlen });
            // 字体大小
            float fontsize = 0.f;
            layout.GetFontSize(unit.bgn, &fontsize, nullptr);
            text->SetFontSize(fontsize, DWRITE_TEXT_RANGE{ 0, tlen });
            // 注音部分字体大小
            {
                auto tsize = fontsize * (unit.ruby.zoom == 0.f ? 0.5f : unit.ruby.zoom);
                ruby->SetFontSize(tsize, DWRITE_TEXT_RANGE{ 0, rlen });
            }
            // 创建RUBY内联对象
            if (const auto rubyobj = LongUI::CreateRubyNotation(ruby, text)) {
                DWRITE_TEXT_RANGE range{ unit.bgn , uint32_t(unit.len) };
                layout.SetInlineObject(rubyobj, range);
                rubyobj->Release();
            }
            // 扫尾
            LongUI::SafeRelease(effect);
        }
        // 释放数据
        LongUI::SafeRelease(ruby);
        LongUI::SafeRelease(text);
    }
    /// <summary>
    /// set_img the specified unit.
    /// </summary>
    /// <param name="unit">The unit.</param>
    /// <param name="layout">The layout.</param>
    void set_img(const ez_xml_parser::unit& unit,
        IDWriteTextLayout& layout) noexcept {
        size_t len = (unit.img.name2 - unit.img.name1);
        LongUI::SafeBuffer<wchar_t>(len + 1, [&, len](wchar_t buf[]) noexcept {
            std::memcpy(buf, unit.img.name1, len * sizeof(wchar_t));
            buf[len] = 0;
            DX::InlineImage ii{ unit.img.clip, unit.img.size, buf };
            if (const auto img = UIManager.XmlImgInterface(ii)) {
                layout.SetInlineObject(
                    img, DWRITE_TEXT_RANGE{ unit.bgn, unit.len }
                );
                img->Release();
            }
        });
    }
}}


// 直接使用
auto LongUI::DX::FormatTextXML(
    const FormatTextConfig& config,
    const wchar_t* format
) noexcept ->IDWriteTextLayout* {
#ifdef _DEBUG
    assert(DX::CheckXmlValidity(format) && "bad xml, you should check it");
#endif
    // 简易解析器
    auto xml_parser_ptr = LongUI::NormalAllocT<impl::ez_xml_parser>(1);
    if (!xml_parser_ptr) return nullptr;
    xml_parser_ptr->impl::ez_xml_parser::ez_xml_parser();
    auto& xmlparser = *xml_parser_ptr;
    // 已经假定Xml字符串合法
    IDWriteTextLayout* layout = nullptr;
    // 状态机
    impl::xml_state fsm = impl::state_text;
    // 正式处理
    for (auto itr = format; *itr; ++itr) {
        switch (fsm)
        {
        case impl::state_text:
            // 等待标签起始符号出现 -> state_tagb
            if (impl::check_ltag(*itr)) {
                fsm = impl::state_tagb;
            }
            // 加入文本
            else {
                // 记录
                wchar_t ch = *itr;
                // 转义符
                itr = impl::xml_escape_char(itr, ch);
                // 添加字符
                xmlparser.push_char(ch);
                // OOM
                if (!xmlparser.strok()) goto oom_error;
            }
            break;
        case impl::state_tagb:
            // 标签闭合符号 -> state_tag2
            if (impl::check_clz(*itr)) {
                fsm = impl::state_tag2;
                //mark2_begin(itr+1);
            }
            // 否则 -> state_tag1
            else {
                fsm = impl::state_tag1;
                xmlparser.taag_pair.begin = itr;
                //mark1_begin(itr);
            }
            break;
        case impl::state_tag1:
            // 出现关闭符号
            if (impl::check_clz(*itr)) {
                // 只需要等待关闭标签出现
                fsm = impl::state_w8rt;
            }
            // 出现空格或者右标签符号
            else if (white_space(*itr) || impl::check_rtag(*itr)) {
                // 标记标签结束位置
                xmlparser.taag_pair.end = itr;
                xmlparser.mk_tagbegin();
                if (!xmlparser.initok()) goto oom_error;
                // 出现右标签 -> state_text
                if (impl::check_rtag(*itr)) fsm = impl::state_text;
                // 出现空格 -> state_w8av
                else fsm = impl::state_w8av;
            }
            break;
        case impl::state_tag2:
            // 出现空格或者右标签符号
            if (white_space(*itr) || impl::check_rtag(*itr)) {
                //mark2_end(itr);
                // 弹出栈顶
                //mark1_pop();
                xmlparser.mk_tagend();
                if (!xmlparser.setsok()) goto oom_error;
                // 标签名不匹配 -> 失败
                //if (!same_tag(tag2, *toptag)) return false;

                // 出现右标签 -> state_text
                if (impl::check_rtag(*itr)) fsm = impl::state_text;
                // 出现空格 -> state_w8rt
                else fsm = impl::state_w8rt;
            }
            break;
        case impl::state_w8av:
            // 结束标签出现 -> state_w8rt
            if (impl::check_clz(*itr)) {
                fsm = impl::state_w8rt;
                xmlparser.mk_tagend();
                if (!xmlparser.setsok()) goto oom_error;
            }
            // 结束标签 -> state_text
            else if (impl::check_rtag(*itr)) fsm = impl::state_text;
            // 属性出现(非空格) -> state_attr
            else if (!white_space(*itr)) {
                xmlparser.attr_pair.begin = itr;
                fsm = impl::state_attr;
            }
            break;
        case impl::state_w8rt:
            // 等待关闭标签出现 -> state_text
            if (impl::check_rtag(*itr)) fsm = impl::state_text;
            break;
        case impl::state_attr:
            // 等待等号出现 -> state_var1
            if (impl::check_equal(*itr)) {
                fsm = impl::state_var1;
                xmlparser.attr_pair.end = itr;
            }
            break;
        case impl::state_var1:
            // 等待引号出现 -> state_var2
            if (impl::check_quot(*itr)) {
                xmlparser.valu_pair.begin = itr+1;
                fsm = impl::state_var2;
            }
            break;
        case impl::state_var2:
            // 等待引出现 -> state_w8av
            if (impl::check_quot(*itr)) {
                xmlparser.valu_pair.end = itr;
                xmlparser.set_attr();
                fsm = impl::state_w8av;
            }
            break;
        }
    }
    // 创建布局
    auto hr = UIManager_DWriteFactory->CreateTextLayout(
        xmlparser.buffer,
        xmlparser.strlen,
        config.format,
        config.width, config.height,
        &layout
    );
    // 调试
#ifdef _DEBUG
    xmlparser.push_char(0);
#endif
    // 创建成功
    if (SUCCEEDED(hr)) {
        auto stack = xmlparser.sets_top;
        while (stack != xmlparser.sets) {
            --stack;
            auto& unit = *stack;
            DWRITE_TEXT_RANGE range{unit.bgn, unit.len};
            switch (unit.fmt)
            {
            case LongUI::impl::ez_xml_parser::b:
                layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, range);
                break;
            case LongUI::impl::ez_xml_parser::i:
                layout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, range);
                break;
            case LongUI::impl::ez_xml_parser::u:
                layout->SetUnderline(TRUE, range);
                break;
            case LongUI::impl::ez_xml_parser::s:
                layout->SetStrikethrough(TRUE, range);
                break;
            case LongUI::impl::ez_xml_parser::font:
                impl::set_font(unit, *layout);
                break;
            case LongUI::impl::ez_xml_parser::eval:
                break;
            case LongUI::impl::ez_xml_parser::ruby:
                impl::set_ruby(unit, *layout, xmlparser.buffer);
                break;
            case LongUI::impl::ez_xml_parser::img:
                impl::set_img(unit, *layout);
                break;
            case LongUI::impl::ez_xml_parser::unk:
            default:
                assert(!"UNKNDOWN FORMAT");
                break;
            }
        }
    }
    // 结尾处理
oom_error:
    assert(layout && "error, ignore this meesage");
    xml_parser_ptr->impl::ez_xml_parser::~ez_xml_parser();
    LongUI::NormalFree(xml_parser_ptr);
    return layout;
}


// 格式化文字
/*
control char    C-Type      Infomation                                   StringParamSupported

%%              [none]      As '%' Character(like %% in std::printf)             ---

%a         [const wchar_t*] string add(like %ls in std::printf)                  ---

%c            [float4*]   new font color range start, with alpha                 Yes

%e           [IUnknown*]    new drawing effect range start                       ---

%s             [float]     new font size range start                             Yes

%f        [const wchar_t*] new font family name range start            Yes but without "," char

%h             [enum]      new font stretch range start                          Yes

%y             [enum]      new font style range start                            Yes

%w             [enum]      new font weight range start                           Yes

%u             [---]      new underline range start                              ---

%d             [---]      new strikethrough(delete line) range start             ---

%i            [IDIO*]     new inline object range start                          ---

%]             [---]       end of the last range                                 ---

%t            [IDWT*]      new typography range start                            ---

%l       [const wchar_t*]    new locale name range start                         YES

//  Unsupported
%f %F   [UNSPT]  [IDFC*]     new font collection range start                     ---
IDWriteFontCollection*

FORMAT IN STRING
the va_list(ap) can be nullptr while string format include the PARAMETERS,
use %p to mark PARAMETERS start

*/

// longui::dx namespace
namespace LongUI { namespace DX {
    // 范围类型
    enum class RANGE_TYPE : size_t { F, W, Y, H, S, U, D, E, I, T, L };
    // 范围数据
    struct RANGE_DATA {
        // 具体数据
        union {
            const wchar_t*          wstr;       // FL
            IUnknown*               effect;     // E
            IDWriteInlineObject*    inlineobj;  // I
            IDWriteTypography*      typography; // T
            DWRITE_FONT_WEIGHT      weight;     // W
            DWRITE_FONT_STYLE       style;      // Y
            DWRITE_FONT_STRETCH     stretch;    // D
            float                   size;       // S
            //BOOL                    underline;  // U
            //BOOL                    strikethr;  // T
        };
        // 类型
        RANGE_TYPE                  type;
        // 范围
        DWRITE_TEXT_RANGE           range;
    };
    // C参数
    struct CoreMLParamC {
        // 构造函数
        CoreMLParamC(va_list va) noexcept : list(va) {};
        // 获取刻画效果
        auto GetEffect() noexcept { auto p = va_arg(list, IUnknown*); assert(p); p->AddRef(); return p; }
        // 获取内联对象
        auto GetInlineObject() noexcept { auto p va_arg(list, IDWriteInlineObject*);  assert(p); p->AddRef(); return p; }
        // 获取版式功能
        auto GetTypography() noexcept { auto p va_arg(list, IDWriteTypography*);  assert(p); p->AddRef(); return p; }
        // 获取字符串
        auto GetString() noexcept { return va_arg(list, const wchar_t*); }
        // 获取字体名称
        auto GetStringEx() noexcept { return va_arg(list, const wchar_t*); }
        // 获取颜色
        void GetColor(D2D1_COLOR_F& color) noexcept { color = *(va_arg(list, D2D1_COLOR_F*)); }
        // 获取浮点, float 经过可变参数会提升至double
        auto GetFloat() noexcept { return static_cast<float>(va_arg(list, double)); }
        // 获取字体粗细
        auto GetFontWeight() noexcept { return va_arg(list, DWRITE_FONT_WEIGHT); }
        // 获取字体风格
        auto GetFontStyle() noexcept { return va_arg(list, DWRITE_FONT_STYLE); }
        // 获取字体伸缩
        auto GetFontStretch() noexcept { return va_arg(list, DWRITE_FONT_STRETCH); }
        // 可变参数列表
        va_list             list;
    };
    // 字符串参数
    struct CoreMLParamString {
        // 构造函数
        CoreMLParamString(const wchar_t* p) noexcept;
        // 获取刻画效果
        auto GetEffect() noexcept -> IUnknown* { assert(!"unsupported for string param!"); return nullptr; }
        // 获取内联对象
        auto GetInlineObject() noexcept -> IDWriteInlineObject* { assert(!"unsupported for string param!"); return nullptr; }
        // 获取版式功能
        auto GetTypography() noexcept->IDWriteTypography*;
        // 获取字符串
        auto GetString() noexcept -> const wchar_t* { assert(!"unsupported for string param!"); return nullptr; }
        // 获取字符串Ex
        auto GetStringEx() noexcept -> const wchar_t*;
        // 获取颜色
        void GetColor(D2D1_COLOR_F& color) noexcept;
        // 获取浮点
        auto GetFloat() noexcept ->float;
        // 获取字体粗细
        auto GetFontWeight() noexcept->DWRITE_FONT_WEIGHT;
        // 获取字体风格
        auto GetFontStyle() noexcept->DWRITE_FONT_STYLE;
        // 获取字体伸缩
        auto GetFontStretch() noexcept->DWRITE_FONT_STRETCH;
        // 参数地址
        const wchar_t*              param = nullptr;
        // 字体名迭代器
        wchar_t*                    family_itr = family_buffer;
        // 字体名缓存
        wchar_t                     family_buffer[1024];
    private:
        // 复制字符串, 返回最后非空白字符串数据
        template<typename T> auto copy_string_sp(T* __restrict des) noexcept {
            // 获取数据
            const wchar_t* __restrict src = this->param;
            // 跳过空白
            while (white_space(*src)) ++src;
            // 复制数据
            while ((*src) && ((*src)) != ',') { *des = static_cast<T>(*src); ++src; ++des; }
            // 检查最后有效字符串
            auto last = des; while (white_space(last[-1])) --last;
            // 写入
            this->param = src + 1;
#ifdef _DEBUG
            // 调试
            if (!src[0]) this->param = nullptr;
#endif
            // 返回
            return last;
        }
    };
    // CoreMLParamString 构造函数
    inline CoreMLParamString::CoreMLParamString(const wchar_t* p) noexcept {
        while (*p) {
            if (p[0] == '%' && p[1] == 'p') {
                this->param = p + 2;
                break;
            }
            ++p;
        }
    }
    // 获取浮点数
    inline auto CoreMLParamString::GetFloat() noexcept -> float {
        assert(this->param && "bad param, ungiven parameters, nor wrong number of parameters");
        char buffer[1024];
        auto end = this->copy_string_sp(buffer); end[0] = 0;
        assert((size_t(end - buffer) < lengthof(buffer)) && "buffer to small");
        return LongUI::AtoF(buffer);
    }
    // 获取字体粗细
    inline auto CoreMLParamString::GetFontWeight() noexcept -> DWRITE_FONT_WEIGHT {
        assert(this->param && "bad param, ungiven parameters, nor wrong number of parameters");
        char buffer[1024];
        auto end = this->copy_string_sp(buffer); end[0] = 0;
        assert((size_t(end - buffer) < lengthof(buffer)) && "buffer to small");
        return static_cast<DWRITE_FONT_WEIGHT>(LongUI::AtoI(buffer));
    }
    // 获取字体风格
    inline auto CoreMLParamString::GetFontStyle() noexcept -> DWRITE_FONT_STYLE {
        assert(this->param && "bad param, ungiven parameters, nor wrong number of parameters");
        char buffer[1024];
        auto end = this->copy_string_sp(buffer); end[0] = 0;
        assert((size_t(end - buffer) < lengthof(buffer)) && "buffer to small");
        return Helper::GetEnumFromString(buffer, DWRITE_FONT_STYLE_NORMAL);
    }
    // 获取字体伸缩
    inline auto CoreMLParamString::GetFontStretch() noexcept -> DWRITE_FONT_STRETCH {
        assert(this->param && "bad param, ungiven parameters, nor wrong number of parameters");
        char buffer[1024];
        auto end = this->copy_string_sp(buffer); end[0] = 0;
        assert((size_t(end - buffer) < lengthof(buffer)) && "buffer to small");
        return Helper::GetEnumFromString(buffer, DWRITE_FONT_STRETCH_NORMAL);
    }
    // 获取颜色
    inline void CoreMLParamString::GetColor(D2D1_COLOR_F& color) noexcept {
        assert(this->param && "bad param, ungiven parameters, nor wrong number of parameters");
        char buffer[1024];
        auto end = this->copy_string_sp(buffer); end[0] = 0;
        assert((size_t(end - buffer) < lengthof(buffer)) && "buffer to small");
        Helper::MakeColor(buffer, color);
    }
    // 获取字体名称
    auto CoreMLParamString::GetStringEx() noexcept -> const wchar_t* {
        auto old = this->family_itr;
        auto end = this->copy_string_sp(old); end[0] = 0;
        this->family_itr = end + 1;
        return old;
    }
    // 获取排版样式
    inline auto CoreMLParamString::GetTypography() noexcept -> IDWriteTypography* {
        IDWriteTypography* typography = nullptr;
        // 设置参数
        assert(this->param && "bad param, ungiven parameters, nor wrong number of parameters");
        char buffer[1024];
        auto end = this->copy_string_sp(buffer);
        end[0] = ' '; end[1] = 0;
        assert((size_t(end - buffer) < lengthof(buffer)) && "buffer to small");
        // 创建 Typography
        auto hr = UIManager_DWriteFactory->CreateTypography(&typography);
        longui_debug_hr(hr, L"failed: UIManager_DWriteFactory->CreateTypography ");
        assert(std::strlen(buffer) % 5 == 0 && "bad font feature tag");
        // CPU 大小端检查
        static_assert(uint32_t(DWRITE_FONT_FEATURE_TAG_CASE_SENSITIVE_FORMS) == uint32_t("case"_longui32), "check cpu type");
        static_assert(sizeof(uint32_t) == sizeof(DWRITE_FONT_FEATURE_TAG), "check enum type");
        // 添加 OpenTypoe 特性
        if (SUCCEEDED(hr)) {
            DWRITE_FONT_FEATURE feature;
            feature.parameter = 5;
            // 遍历字符串
            for (auto itr = buffer; *itr; itr += 5) {
                // 稍微检查一下
                assert(itr[0] && itr[1] && itr[2] && itr[3] && itr[4] == ' ' && "bad argments");
                // 一般视为二进制数据
                auto tmp = *reinterpret_cast<int32_t*>(itr);
                feature.nameTag = static_cast<DWRITE_FONT_FEATURE_TAG>(tmp);
                auto thr = typography->AddFontFeature(feature);
                UNREFERENCED_PARAMETER(thr);
                longui_debug_hr(thr, L"failed: typography->AddFontFeature " << long(feature.nameTag));
            }
        }
        return typography;
    }
    // 创建格式文本
    template<typename T>
    auto FormatTextViaCoreML(const FormatTextConfig& cfg, const wchar_t* fmt, T& param) noexcept {
        using cctype = wchar_t;
        cctype ch = 0;
        constexpr int CoreML_BUFFER_LENGTH = 2048;
        cctype text[CoreML_BUFFER_LENGTH]; auto text_itr = text;
        EzContainer::FixedStack<RANGE_DATA, 1024> stack_check, stack_set;
        // 遍历字符串
        while ((ch = *fmt)) {
            // 出现%标记
            if (ch == '%') {
                switch ((ch = fmt[1]))
                {
                case '%':
                    // %% --> 添加字符"%"
                    *text_itr = '%';
                    ++text_itr;
                    break;
                case 'p':
                    // %p --> 结束
                    goto force_break;
                case ']':
                    // %] --> 结束一个范围
                    // 检查栈弹出
                    stack_check.pop();
                    // 计算长度
                    stack_check.top->range.length = static_cast<UINT32>(text_itr - text) - stack_check.top->range.startPosition;
                    // 压入设置栈
                    stack_set.push_back(*stack_check.top);
                    break;
                case 'a':
                    // %a --> 添加字符串
                    // 直接写入字符串
                    for (auto str = param.GetString(); *str; ++str, ++text_itr) {
                        *text_itr = *str;
                    }
                    break;
                case 'c': case 'e':
                    // %c --> 添加颜色
                    // %e --> 添加效果
                {
                    RANGE_DATA range;
                    if (ch == 'c') {
                        D2D1_COLOR_F color; param.GetColor(color);
                        range.effect = CUIColorEffect::Create(color);
                    }
                    else {
                        range.effect = param.GetEffect();
                    }
                    assert(range.effect && "OOM or bad action");
                    range.range.startPosition = static_cast<UINT32>(text_itr - text);
                    range.type = RANGE_TYPE::E;
                    stack_check.push_back(range);
                    break;
                }
                case 'i':
                    // %i --> 添加内联对象
                {
                    RANGE_DATA range;
                    range.effect = param.GetInlineObject();
                    assert(range.effect && "OOM or bad action");
                    range.range.startPosition = static_cast<UINT32>(text_itr - text);
                    range.type = RANGE_TYPE::I;
                    stack_check.push_back(range);
                    break;
                }
                case 't':
                    // %t --> 添加版式功能
                {
                    RANGE_DATA range;
                    range.typography = param.GetTypography();
                    assert(range.effect && "OOM or bad action");
                    range.range.startPosition = static_cast<UINT32>(text_itr - text);
                    range.type = RANGE_TYPE::T;
                    stack_check.push_back(range);
                    break;
                }
                case 'f': case 'l':
                    // %f --> 字体名称
                    // %l --> 本地名称
                {
                    RANGE_DATA range;
                    range.range.startPosition = static_cast<UINT32>(text_itr - text);
                    range.type = ch == 'f' ? RANGE_TYPE::F : RANGE_TYPE::L;
                    range.wstr = param.GetStringEx();
                    stack_check.push_back(range);
                    break;
                }
                case 's':
                    // %s --> 字体大小
                {
                    RANGE_DATA range;
                    range.range.startPosition = static_cast<UINT32>(text_itr - text);
                    range.type = RANGE_TYPE::S;
                    range.size = param.GetFloat();
                    stack_check.push_back(range);
                    break;
                }
                case 'w':
                    // %w --> 字体粗细
                {
                    RANGE_DATA range;
                    range.range.startPosition = static_cast<UINT32>(text_itr - text);
                    range.type = RANGE_TYPE::W;
                    range.weight = param.GetFontWeight();
                    stack_check.push_back(range);
                    break;
                }
                case 'y':
                    // %y --> 字体风格
                {
                    RANGE_DATA range;
                    range.range.startPosition = static_cast<UINT32>(text_itr - text);
                    range.type = RANGE_TYPE::Y;
                    range.style = param.GetFontStyle();
                    stack_check.push_back(range);
                    break;
                }
                case 'h':
                    // %h --> 字体伸缩
                {
                    RANGE_DATA range;
                    range.range.startPosition = static_cast<UINT32>(text_itr - text);
                    range.type = RANGE_TYPE::H;
                    range.stretch = param.GetFontStretch();
                    stack_check.push_back(range);
                    break;
                }
                case 'u':
                    // %u --> 设置下划线
                {
                    RANGE_DATA range;
                    range.range.startPosition = static_cast<UINT32>(text_itr - text);
                    range.type = RANGE_TYPE::U;
                    stack_check.push_back(range);
                    break;
                }
                case 'd':
                    // %d --> 设置删除线
                {
                    RANGE_DATA range;
                    range.range.startPosition = static_cast<UINT32>(text_itr - text);
                    range.type = RANGE_TYPE::D;
                    stack_check.push_back(range);
                    break;
                }
                }
                // 写入数据
                fmt += 2;
            }
            else {
                // 写入数据
                assert((size_t(text_itr - text) < lengthof(text)) && "buffer too small");
                *text_itr = *fmt;
                ++fmt;
                ++text_itr;
            }
        }
    force_break:
        auto hr = S_OK;
        assert(stack_check.empty() == true && "unmatched maker");
        // 计算
        IDWriteTextLayout* layout = nullptr;
        auto length = static_cast<UINT32>(text_itr - text);
        auto needed = static_cast<uint32_t>(static_cast<float>(length + 1) * cfg.progress);
        if (needed > length) needed = length;
        // 创建布局
        if (SUCCEEDED(hr)) {
            hr = UIManager_DWriteFactory->CreateTextLayout(
                text,
                length,
                cfg.format,
                cfg.width, cfg.height,
                &layout
            );
#ifdef _DEBUG
            text[length] = 0;
            longui_debug_hr(hr, L"failed: UIManager_DWriteFactory->CreateTextLayout  " << text);
#endif
        }
        // 数据末尾
        auto setend = stack_set.top;
        // 正式创建
        if (SUCCEEDED(hr)) {
            // 创建
            while (!stack_set.empty()) {
                stack_set.pop();
                // 检查进度(progress)范围 释放数据
                auto end = stack_set.top->range.startPosition + stack_set.top->range.length;
                if (end > needed) continue;
                // 检查类型
                switch (stack_set.top->type)
                {
                case RANGE_TYPE::F:
                    layout->SetFontFamilyName(stack_set.top->wstr, stack_set.top->range);
                    break;
                case RANGE_TYPE::W:
                    layout->SetFontWeight(stack_set.top->weight, stack_set.top->range);
                    break;
                case RANGE_TYPE::Y:
                    layout->SetFontStyle(stack_set.top->style, stack_set.top->range);
                    break;
                case RANGE_TYPE::H:
                    layout->SetFontStretch(stack_set.top->stretch, stack_set.top->range);
                    break;
                case RANGE_TYPE::S:
                    layout->SetFontSize(stack_set.top->size, stack_set.top->range);
                    break;
                case RANGE_TYPE::U:
                    layout->SetUnderline(TRUE, stack_set.top->range);
                    break;
                case RANGE_TYPE::D:
                    layout->SetStrikethrough(TRUE, stack_set.top->range);
                    break;
                case RANGE_TYPE::E:
                    layout->SetDrawingEffect(stack_set.top->effect, stack_set.top->range);
                    break;
                case RANGE_TYPE::I:
                    layout->SetInlineObject(stack_set.top->inlineobj, stack_set.top->range);
                    break;
                case RANGE_TYPE::T:
                    layout->SetTypography(stack_set.top->typography, stack_set.top->range);
                    break;
                case RANGE_TYPE::L:
                    layout->SetLocaleName(stack_set.top->wstr, stack_set.top->range);
                    break;
                }
            }
        }
        // 错误信息
        if (FAILED(hr)) {
            UIManager << DL_Error << L"HR Code: " << long(hr) << LongUI::endl;
        }
        // 释放数据
        for (auto itr = stack_set.data; itr != setend; ++itr) {
            if (itr->type == RANGE_TYPE::E || itr->type == RANGE_TYPE::I ||
                itr->type == RANGE_TYPE::T) {
                LongUI::SafeRelease(itr->effect);
            }
        }
        return layout;
    }
    // 创建格式文本
    auto FormatTextCoreC(const FormatTextConfig& cfg, const wchar_t* fmt, ...) noexcept ->IDWriteTextLayout* {
        va_list ap;
        va_start(ap, fmt);
        CoreMLParamC param(ap);
        auto lyt = DX::FormatTextViaCoreML(cfg, fmt, param);
        va_end(ap);
        return lyt;
    }
    // 创建格式文本
    auto FormatTextCore(const FormatTextConfig& cfg, const wchar_t* fmt, va_list) noexcept ->IDWriteTextLayout* {
        CoreMLParamString param(fmt);
        return DX::FormatTextViaCoreML(cfg, fmt, param);
    }
}}

// 保存图片
auto LongUI::DX::SaveAsImageFile(
    ID2D1Bitmap1* bitmap, IWICImagingFactory* factory,
    const wchar_t* file_name, const GUID* container_format
) noexcept -> HRESULT {
    // 参数检查
    assert(bitmap && factory && file_name && file_name[0]);
    if (!(bitmap && factory && file_name && file_name[0])) {
        return E_INVALIDARG;
    }
    // 初始化
    D2D1_MAPPED_RECT rect = { 0 };
    HRESULT hr = S_OK;
    ID2D1Bitmap1* readable_bitmap = nullptr;
    auto bitmap_size = bitmap->GetPixelSize();
    auto dxgi_format = bitmap->GetPixelFormat();
    // 创建CPU可读位图
    if (SUCCEEDED(hr)) {
        // CPU 可读?
        if (bitmap->GetOptions() & D2D1_BITMAP_OPTIONS_CPU_READ) {
            readable_bitmap = LongUI::SafeAcquire(bitmap);
        }
        else {
            D2D1_BITMAP_PROPERTIES1 prop;
            prop.pixelFormat = dxgi_format;
            bitmap->GetDpi(&prop.dpiX, &prop.dpiY);
            prop.colorContext = nullptr;
            prop.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
            // 创建位图
            hr = UIManager_RenderTarget->CreateBitmap(
                bitmap_size,
                nullptr,
                0,
                &prop,
                &readable_bitmap
            );
#ifdef _DEBUG
            D2D1_POINT_2F ppppt;
            ppppt.x = float(bitmap_size.width);
            ppppt.y = float(bitmap_size.height);
            longui_debug_hr(hr, L"failed: UIManager_RenderTarget->CreateBitmap " << ppppt);
#endif
        }
    }
    // 复制位图
    if (SUCCEEDED(hr)) {
        hr = readable_bitmap->CopyFromBitmap(nullptr, bitmap, nullptr);
        longui_debug_hr(hr, L"failed: readable_bitmap->CopyFromBitmap");
    }
    // 映射数据
    if (SUCCEEDED(hr)) {
        hr = readable_bitmap->Map(D2D1_MAP_OPTIONS_READ, &rect);
        longui_debug_hr(hr, L"failed: readable_bitmap->Map");
    }
    // 处理
    if (SUCCEEDED(hr)) {
        SaveAsImageFileProperties prop;
        prop.bits = rect.bits;
        prop.factory = factory;
        prop.data_format = LongUI::DX::DXGIToWIC(dxgi_format.format);
        prop.container_format = container_format;
        prop.width = bitmap_size.width;
        prop.height = bitmap_size.height;
        prop.pitch = rect.pitch;
        auto hr1 = LongUI::DX::SaveAsImageFile(prop, file_name);
        longui_debug_hr(hr1, L"failed: DX::SaveAsImageFile");
        auto hr2 = readable_bitmap->Unmap();
        longui_debug_hr(hr2, L"failed: readable_bitmap->Unmap");
        // 检查错误
        if (SUCCEEDED(hr1)) {
            hr = SUCCEEDED(hr2) ? S_OK : hr2;
        }
        else {
            hr = hr1;
        }
    }
    // 扫尾处理
    LongUI::SafeRelease(readable_bitmap);
    // 返回结果
    return hr;
}

#include <Wincodec.h>

// longui::dx namespace
LONGUI_NAMESPACE_BEGIN namespace DX {
    // 保存数据为图片格式
    auto SaveAsImageFile(const SaveAsImageFileProperties& prop, const wchar_t* file_name) noexcept -> HRESULT {
        // 参数检查
        assert(prop.bits && prop.factory && file_name && file_name[0]);
        if (!(prop.bits && prop.factory && file_name && file_name[0])) {
            return E_INVALIDARG;
        }
        // 初始化
        HRESULT hr = S_OK;
        IWICBitmapEncoder *pEncoder = nullptr;
        IWICBitmapFrameEncode *pFrameEncode = nullptr;
        IWICStream *pStream = nullptr;
        IWICBitmap *pWICBitmap = nullptr;
        // 创建WIC位图
        if (SUCCEEDED(hr)) {
            hr = prop.factory->CreateBitmapFromMemory(
                prop.width,
                prop.height,
                prop.data_format ? *prop.data_format : GUID_WICPixelFormat32bppBGRA,
                prop.pitch,
                prop.pitch * prop.height,
                prop.bits,
                &pWICBitmap
            );
            longui_debug_hr(hr, L"failed: prop.factory->CreateBitmapFromMemory");
        }
        // 创建流
        if (SUCCEEDED(hr)) {
            hr = prop.factory->CreateStream(&pStream);
            longui_debug_hr(hr, L"failed: prop.factory->CreateStream");
        }
        // 从文件初始化
        if (SUCCEEDED(hr)) {
            hr = pStream->InitializeFromFilename(file_name, GENERIC_WRITE);
            longui_debug_hr(hr, L"failed: pStream->InitializeFromFilename  " << file_name);
        }
        // 创建编码器
        if (SUCCEEDED(hr)) {
            hr = prop.factory->CreateEncoder(
                prop.container_format ? *prop.container_format : GUID_ContainerFormatPng,
                nullptr,
                &pEncoder
            );
            longui_debug_hr(hr, L"failed: prop.factory->CreateEncoder");
        }
        // 初始化编码器
        if (SUCCEEDED(hr)) {
            hr = pEncoder->Initialize(pStream, WICBitmapEncoderNoCache);
            longui_debug_hr(hr, L"failed: pEncoder->Initialize");
        }
        // 创建新的一帧
        if (SUCCEEDED(hr)) {
            hr = pEncoder->CreateNewFrame(&pFrameEncode, nullptr);
            longui_debug_hr(hr, L"failed: pEncoder->CreateNewFrame");
        }
        // 初始化帧编码器
        if (SUCCEEDED(hr)) {
            hr = pFrameEncode->Initialize(nullptr);
            longui_debug_hr(hr, L"failed: pFrameEncode->Initialize");
        }
        // 设置大小
        if (SUCCEEDED(hr)) {
            hr = pFrameEncode->SetSize(prop.width, prop.height);
            longui_debug_hr(hr, L"failed: pFrameEncode->SetSize " << long(prop.width) << L", " << long(prop.height));
        }
        // 设置格式
        WICPixelFormatGUID format = GUID_WICPixelFormatDontCare;
        if (SUCCEEDED(hr)) {
            hr = pFrameEncode->SetPixelFormat(&format);
            longui_debug_hr(hr, L"failed: pFrameEncode->SetPixelFormat");
        }
        // 写入源数据
        if (SUCCEEDED(hr)) {
            hr = pFrameEncode->WriteSource(pWICBitmap, nullptr);
            longui_debug_hr(hr, L"failed: pFrameEncode->WriteSource");
        }
        // 提交帧编码器
        if (SUCCEEDED(hr)) {
            hr = pFrameEncode->Commit();
            longui_debug_hr(hr, L"failed: pFrameEncode->Commit");
        }
        // 提交编码
        if (SUCCEEDED(hr)) {
            hr = pEncoder->Commit();
            longui_debug_hr(hr, L"failed: pEncoder->Commit");
        }
        // 扫尾处理
        LongUI::SafeRelease(pWICBitmap);
        LongUI::SafeRelease(pStream);
        LongUI::SafeRelease(pFrameEncode);
        LongUI::SafeRelease(pEncoder);
        // 返回结果
        return hr;
    }
    // WIC GUID <--> DXGI_FORMAT
    struct WICTranslate { GUID wic; DXGI_FORMAT format; };
    // data
    static const WICTranslate s_WICFormats[] = {
        { GUID_WICPixelFormat128bppRGBAFloat,       DXGI_FORMAT_R32G32B32A32_FLOAT },

        { GUID_WICPixelFormat64bppRGBAHalf,         DXGI_FORMAT_R16G16B16A16_FLOAT },
        { GUID_WICPixelFormat64bppRGBA,             DXGI_FORMAT_R16G16B16A16_UNORM },

        { GUID_WICPixelFormat32bppRGBA,             DXGI_FORMAT_R8G8B8A8_UNORM },
        { GUID_WICPixelFormat32bppBGRA,             DXGI_FORMAT_B8G8R8A8_UNORM },
        { GUID_WICPixelFormat32bppBGR,              DXGI_FORMAT_B8G8R8X8_UNORM },

        { GUID_WICPixelFormat32bppRGBA1010102XR,    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM },
        { GUID_WICPixelFormat32bppRGBA1010102,      DXGI_FORMAT_R10G10B10A2_UNORM },
        { GUID_WICPixelFormat32bppRGBE,             DXGI_FORMAT_R9G9B9E5_SHAREDEXP },

        { GUID_WICPixelFormat16bppBGRA5551,         DXGI_FORMAT_B5G5R5A1_UNORM },
        { GUID_WICPixelFormat16bppBGR565,           DXGI_FORMAT_B5G6R5_UNORM },

        { GUID_WICPixelFormat32bppGrayFloat,        DXGI_FORMAT_R32_FLOAT },
        { GUID_WICPixelFormat16bppGrayHalf,         DXGI_FORMAT_R16_FLOAT },
        { GUID_WICPixelFormat16bppGray,             DXGI_FORMAT_R16_UNORM },
        { GUID_WICPixelFormat8bppGray,              DXGI_FORMAT_R8_UNORM },

        { GUID_WICPixelFormat8bppAlpha,             DXGI_FORMAT_A8_UNORM },

        { GUID_WICPixelFormatBlackWhite,            DXGI_FORMAT_R1_UNORM },

        { GUID_WICPixelFormat96bppRGBFloat,         DXGI_FORMAT_R32G32B32_FLOAT },
    };
    // DXGI格式转换为 WIC GUID 格式
    auto DXGIToWIC(DXGI_FORMAT format) noexcept ->CPGUID {
        const GUID* outformat = nullptr;
        // 检查
        for (const auto& data : s_WICFormats) {
            if (data.format == format) {
                outformat = &data.wic;
                break;
            }
        }
        // 特殊
        if (!outformat) {
            switch (format)
            {
            case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
                outformat = &GUID_WICPixelFormat32bppRGBA;
                break;
            case DXGI_FORMAT_D32_FLOAT:
                outformat = &GUID_WICPixelFormat32bppGrayFloat;
                break;
            case DXGI_FORMAT_D16_UNORM:
                outformat = &GUID_WICPixelFormat16bppGray;
                break;
            case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
                outformat = &GUID_WICPixelFormat32bppBGRA;
                break;
            case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
                outformat = &GUID_WICPixelFormat32bppBGR;
                break;
            }
        }
        // 返回数据
        return outformat;
    }
    // d2d1 ------------------------
    void LongUINoinline D2D1MakeRotateMatrix(
        float angle, 
        D2D1_POINT_2F center, 
        D2D1_MATRIX_3X2_F& matrix) noexcept {
        constexpr float pi = 3.141592654f;
        const float theta = angle * (pi / 180.0f);
        const float sin_theta = std::sin(theta);
        const float cos_theta = std::cos(theta);
        matrix._11 = cos_theta;
        matrix._12 = sin_theta;
        matrix._21 = -sin_theta;
        matrix._22 = cos_theta;
        matrix._31 = center.x - center.x * cos_theta + center.y * sin_theta;
        matrix._32 = center.y - center.x * sin_theta - center.y * cos_theta;
    }
    // make skew
    void LongUINoinline D2D1MakeSkewMatrixEx(
        float x, float y, 
        D2D1_POINT_2F center,
        D2D1_MATRIX_3X2_F& matrix) noexcept {
        constexpr float pi = 3.141592654f;
        float theta_x = x * (pi / 180.0f);
        float theta_y = y * (pi / 180.0f);
        float tan_x = std::tan(theta_x);
        float tan_y = std::tan(theta_y);
        matrix._11 = 1.f;
        matrix._12 = tan_y;
        matrix._21 = tan_x;
        matrix._22 = 1.f;
        matrix._31 =-center.y * tan_x;
        matrix._32 = center.x * tan_y ;
    }
#ifdef _DEBUG
    long g_dbg_product_counter = 0;
#endif
    // 设置乘
    void Matrix3x2F::SetProduct(const D2D1_MATRIX_3X2_F& a, const D2D1_MATRIX_3X2_F& b) noexcept {
#ifdef _DEBUG
        ++g_dbg_product_counter;
#endif
        this->_11 = a._11 * b._11 + a._12 * b._21;
        this->_12 = a._11 * b._12 + a._12 * b._22;
        this->_21 = a._21 * b._11 + a._22 * b._21;
        this->_22 = a._21 * b._12 + a._22 * b._22;
        this->_31 = a._31 * b._11 + a._32 * b._21 + b._31;
        this->_32 = a._31 * b._12 + a._32 * b._22 + b._32;
    }
}
LONGUI_NAMESPACE_END


/// <summary>
/// float4 color ---> 32-bit ARGB uint color
/// 将浮点颜色转换成32位ARGB排列整型
/// </summary>
/// <param name="color">The d2d color</param>
/// <returns>32-bit ARGB 颜色</returns>
LongUINoinline auto LongUI::PackTheColorARGB(D2D1_COLOR_F& IN color) noexcept -> uint32_t {
    // 常量
    constexpr uint32_t ALPHA_SHIFT = CHAR_BIT * 3;
    constexpr uint32_t RED_SHIFT = CHAR_BIT * 2;
    constexpr uint32_t GREEN_SHIFT = CHAR_BIT * 1;
    constexpr uint32_t BLUE_SHIFT = CHAR_BIT * 0;
    // 写入
    uint32_t colorargb =
        ((uint32_t(color.a * 255.f) & 0xFF) << ALPHA_SHIFT) |
        ((uint32_t(color.r * 255.f) & 0xFF) << RED_SHIFT) |
        ((uint32_t(color.g * 255.f) & 0xFF) << GREEN_SHIFT) |
        ((uint32_t(color.b * 255.f) & 0xFF) << BLUE_SHIFT);
    return colorargb;
}

/// <summary>
/// 32-bit ARGB uint color ---> float4 color
/// 将32位ARGB排列整型转换成浮点颜色
/// </summary>
/// <param name="color32">The 32-bit color.</param>
/// <param name="color4f">The float4 color.</param>
/// <returns>void</returns>
LongUINoinline auto LongUI::UnpackTheColorARGB(uint32_t IN color32, D2D1_COLOR_F& OUT color4f) noexcept ->void {
    // 位移量
    constexpr uint32_t ALPHA_SHIFT = CHAR_BIT * 3;
    constexpr uint32_t RED_SHIFT = CHAR_BIT * 2;
    constexpr uint32_t GREEN_SHIFT = CHAR_BIT * 1;
    constexpr uint32_t BLUE_SHIFT = CHAR_BIT * 0;
    // 掩码
    constexpr uint32_t ALPHA_MASK = 0xFFU << ALPHA_SHIFT;
    constexpr uint32_t RED_MASK = 0xFFU << RED_SHIFT;
    constexpr uint32_t GREEN_MASK = 0xFFU << GREEN_SHIFT;
    constexpr uint32_t BLUE_MASK = 0xFFU << BLUE_SHIFT;
    // 计算
    color4f.r = static_cast<float>((color32 & RED_MASK) >> RED_SHIFT) / 255.f;
    color4f.g = static_cast<float>((color32 & GREEN_MASK) >> GREEN_SHIFT) / 255.f;
    color4f.b = static_cast<float>((color32 & BLUE_MASK) >> BLUE_SHIFT) / 255.f;
    color4f.a = static_cast<float>((color32 & ALPHA_MASK) >> ALPHA_SHIFT) / 255.f;
}

/// <summary>
/// render the meta 渲染Meta
/// </summary>
/// <param name="target">The render target.</param>
/// <param name="des_rect">The des_rect.</param>
/// <param name="opacity">The opacity.</param>
/// <returns></returns>
void LongUI::Meta::Render(ID2D1DeviceContext* target, const D2D1_RECT_F& des_rect, float opacity) const noexcept {
    // 无需渲染
    //if (opacity < 0.001f) return;
    // 无效位图
    if (!this->bitmap) {
        UIManager << DL_Warning << "bitmap->null" << LongUI::endl;
        return;
    }
    switch (this->rule)
    {
    case LongUI::BitmapRenderRule::Rule_Scale:
        __fallthrough;
    default:
    case LongUI::BitmapRenderRule::Rule_ButtonLike:
        // 直接缩放:
        target->DrawBitmap(
            this->bitmap,
            des_rect, opacity,
            static_cast<D2D1_INTERPOLATION_MODE>(this->interpolation),
            this->src_rect,
            nullptr
        );
        break;
        /*case LongUI::BitmapRenderRule::Rule_ButtonLike:
            // 类按钮
            // - 原矩形, 宽 > 高 ->
            // - 原矩形, 宽 < 高 ->
        {
            constexpr float MARKER = 0.25f;
            auto width = meta.src_rect.right - meta.src_rect.left;
            auto bilibili = width * MARKER / (meta.src_rect.bottom - meta.src_rect.top) *
                (des_rect.bottom - des_rect.top);
            D2D1_RECT_F des_rects[3]; D2D1_RECT_F src_rects[3]; D2D1_RECT_F clip_rects[3];
            // ---------------------------------------
            des_rects[0] = {
                des_rect.left, des_rect.top,
                des_rect.left + bilibili, des_rect.bottom
            };
            des_rects[1] = {
                des_rects[0].right, des_rect.top,
                des_rect.right - bilibili, des_rect.bottom
            };
            des_rects[2] = {
                des_rects[1].right, des_rect.top,
                des_rect.right, des_rect.bottom
            };
            // ---------------------------------------
            std::memcpy(clip_rects, des_rects, sizeof(des_rects));
            if (clip_rects[1].left > des_rects[1].right) {
                std::swap(clip_rects[1].right, des_rects[1].left);
                std::swap(des_rects[1].right, des_rects[1].left);
                clip_rects[0].right = des_rects[1].left;
                clip_rects[2].left = des_rects[1].right;
            }
            // ---------------------------------------
            src_rects[0] = {
                meta.src_rect.left, meta.src_rect.top,
                meta.src_rect.left + width * MARKER, meta.src_rect.bottom
            };
            src_rects[1] = {
                src_rects[0].right, meta.src_rect.top,
                meta.src_rect.right - width * MARKER, meta.src_rect.bottom
            };
            src_rects[2] = {
                src_rects[1].right, meta.src_rect.top,
                meta.src_rect.right, meta.src_rect.bottom
            };
            // 正式渲染
            for (auto i = 0u; i < lengthof(src_rects); ++i) {
                target->PushAxisAlignedClip(clip_rects + i, D2D1_ANTIALIAS_MODE_ALIASED);
                target->DrawBitmap(
                    meta.bitmap,
                    des_rects[i], opacity,
                    static_cast<D2D1_INTERPOLATION_MODE>(meta.interpolation),
                    src_rects[i],
                    nullptr
                    );
                target->PopAxisAlignedClip();
            }
        }
        break;*/
    }
}

// Render Common Brush
void LongUI::FillRectWithCommonBrush(ID2D1RenderTarget* target, ID2D1Brush* brush, const D2D1_RECT_F& rect) noexcept {
    assert(target && brush && "bad arguments");
    // 保存状态
    D2D1_MATRIX_3X2_F matrix; target->GetTransform(&matrix);
    auto height = rect.bottom - rect.top;
    D2D1_RECT_F unit_rect = {
        0.f, 0.f, (rect.right - rect.left) / height , 1.f
    };
    // 新的
    target->SetTransform(
        DX::Matrix3x2F::Scale(height, height) *
        DX::Matrix3x2F::Translation(rect.left, rect.top) *
        matrix
    );
    // 填写
    target->FillRectangle(unit_rect, brush);
    // 恢复
    target->SetTransform(&matrix);
}


