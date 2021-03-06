﻿#include "Core/luiManager.h"
#include "LongUI/luiUiHlper.h"
#include "LongUI/luiUiMeta.h"
// 控件
#include "Control/UIComboBox.h"
#include "Control/UIRadioButton.h"
#include "Control/UIScrollBar.h"
#include "Control/UIFloatLayout.h"
#include "Control/UIRamBitmap.h"
#include "Control/UIList.h"
#include "Control/UIEdit.h"
#include "Control/UISlider.h"
#include "Control/UIColor.h"
#include "Control/UIPage.h"
#include "Control/UISingle.h"
#include "Control/UICheckBox.h"

#define LONGUI_D3D_DEBUG
//#define LONGUI_RENDER_IN_STD_THREAD

// longui namespace
namespace LongUI {
#ifdef LONGUI_WITH_DEFAULT_CONFIG
    auto CreateDefaultConfigure(CUIManager&, 
        const char* logfilename) noexcept->IUIConfigure*;
#endif
    // get bitmap
    auto UIManager_GetBitmap(size_t index) noexcept ->ID2D1Bitmap1*{
        return UIManager.GetBitmap(index);
    }
}

// longui::impl
namespace LongUI { namespace impl{
    // create object
    template<class T> inline auto create_object(T& obj) noexcept {
        new(&obj) T(); 
    }
    // destory object
    template<class T> inline auto destory_object(T& obj) noexcept { 
        obj.~T(); 
    }
}}

#ifdef _DEBUG
#define _DEBUG_LL
#endif

/// <summary>
/// Initializes while the specified configuration.
/// 使用指定的配置信息初始化LongUI管理器
/// </summary>
/// <param name="config">The configuration.</param>
/// <returns></returns>
auto LongUI::CUIManager::Initialize(IUIConfigure* config) noexcept ->HRESULT {
    // 检查GUID
#if defined(_DEBUG_LL) && defined(_MSC_VER)
#define CHECK_GUID(x) assert(LongUI::IID_##x == __uuidof(x) && "bad guid")
    CHECK_GUID(IDXGISwapChain2);
    CHECK_GUID(IDWriteFactory1);
    CHECK_GUID(IDWriteTextRenderer);
    CHECK_GUID(IDWriteTextRenderer1);
    CHECK_GUID(IDWriteInlineObject);
    CHECK_GUID(IDWritePixelSnapping);
    CHECK_GUID(IDWriteFontCollection);
    CHECK_GUID(IDWriteFontFileEnumerator);
    CHECK_GUID(IDWriteFontCollectionLoader);
#undef CHECK_GUID
#endif
    m_vDelayCleanup.reserve(16);
    m_vDelayDispose.reserve(16);
    m_vWindows.reserve(16);
    m_vTimeCapsules.reserve(32);
    // 内存不足
    if (!m_vDelayCleanup.isok() 
        || !m_vWindows.isok() 
        || !m_vDelayDispose.isok()
        || !m_vTimeCapsules.isok()
        ) {
        return E_OUTOFMEMORY;
    }
    // 初始化一些东西
    m_szLocaleName[0] = L'\0';
    m_vDelayCleanup.clear();
    m_vWindows.clear();
    m_vDelayDispose.clear();
    // 开始计时
    m_uiTimeMeter.Start();
    this->refresh_display_frequency();
    // 检查
    if (!config) {
#ifdef LONGUI_WITH_DEFAULT_CONFIG
        config = LongUI::CreateDefaultConfigure(*this, nullptr);
        if (!config) return E_OUTOFMEMORY;
#else
        return E_INVALIDARG;
#endif
    }
    // 获取信息
    force_cast(this->configure) = config;
    // 获取flag
    this->flag = this->configure->GetConfigureFlag();
    // 获取资源加载器
    config->CreateInterface(LongUI_IID_PV_ARGS(m_pResourceLoader));
    // 获取脚本
    config->CreateInterface(LongUI_IID_PV_ARGS(force_cast(this->script)));
    // 本地字符集名称
    config->GetLocaleName(m_szLocaleName);
    // 初始化其他
    std::memset(m_apTextRenderer, 0, sizeof(m_apTextRenderer));
    std::memset(m_apSystemBrushes, 0, sizeof(m_apSystemBrushes));
    // 获取实例句柄
    auto hInstance = ::GetModuleHandleW(nullptr);
    HRESULT hr = S_OK;
    // 获取文本格式化器
    if (SUCCEEDED(hr)) {
       hr = config->CreateInterface(LongUI_IID_PV_ARGS(m_pTextFormatter));
       longui_debug_hr(hr, L"Create m_pTextFormatter faild");
    }
    // 创建工具窗口
    if (SUCCEEDED(hr)) {
        // 注册窗口
        WNDCLASSEXW wcex;
        auto code = ::GetClassInfoExW(hInstance, WindowClassNameT, &wcex);
        if (!code) {
            // 处理函数
            auto wndproc = [](HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept ->LRESULT {
                switch (message)
                {
                case WM_INPUT:
                {
                    CUIDataAutoLocker locker;
                    UIManager.m_uiInput.Update(reinterpret_cast<HRAWINPUT>(lParam));
                }
                    return 1;
                case WM_DISPLAYCHANGE:
                    // 显示环境改变
                    UIManager.refresh_display_frequency();
                    UIManager.create_dxgi_output();
#ifdef _DEBUG
                    UIManager << DL_Hint << "WM_DISPLAYCHANGE" << LongUI::endl;
                case WM_CLOSE:
                    // 不能关闭该窗口
#endif
                    return 0;
                default:
                    return ::DefWindowProcW(hwnd, message, wParam, lParam);
                }
                return 0;
            };
            // 注册窗口类
            wcex = { 0 };
            wcex.cbSize = sizeof(WNDCLASSEXW);
            wcex.style = CS_NOCLOSE;
            wcex.cbClsExtra = 0;
            wcex.cbWndExtra = sizeof(void*);
            wcex.hInstance = hInstance;
            wcex.hCursor = nullptr;
            wcex.hbrBackground = nullptr;
            wcex.lpszMenuName = nullptr;
            wcex.lpszClassName = LongUI::WindowClassNameT;
            wcex.hIcon = nullptr;
            wcex.lpfnWndProc = wndproc;
            ::RegisterClassExW(&wcex);
        }
#ifdef _DEBUG
        constexpr int SIX = 50;
        constexpr int SIY = 50;
        constexpr int SIW_ = 256;
        constexpr int SIH_ = 0;
        RECT rect = { SIX, SIY, SIX + SIW_, SIY + SIH_ };
        ::AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
        const int SIW = rect.right - rect.left;
        const int SIH = rect.bottom - rect.top;
#else
        constexpr int SIX = 0;
        constexpr int SIY = 0;
        constexpr int SIW = 0;
        constexpr int SIH = 0;
#endif
        // 创建
        m_hToolWnd = ::CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST, 
            LongUI::WindowClassNameT, L"SystemInvoke",
            0, SIX, SIY, SIW, SIH, nullptr, nullptr, hInstance, nullptr
        );
        // 成功
        if (m_hToolWnd) {
#ifdef _DEBUG
            ::ShowWindow(m_hToolWnd, SW_SHOW);
#endif
        }
        else {
            hr = E_FAIL;
        }
        
    }
    // 初始化输入
    if (SUCCEEDED(hr)) {
        hr = m_uiInput.Init(m_hToolWnd);
    }
    // 位图缓存
    if (SUCCEEDED(hr)) {
        constexpr size_t LENA = LongUIDefaultBitmapSize * LongUIDefaultBitmapSize;
        constexpr size_t LENT = sizeof(RGBQUAD) * LENA;
        m_pBitmap0Buffer = reinterpret_cast<uint8_t*>(LongUI::NormalAlloc(LENT));
        if (!m_pBitmap0Buffer) hr = E_OUTOFMEMORY;
    }
    // 加载控件模板
    if (SUCCEEDED(hr)) {
        m_cCountCtrlTemplate = 1;
        hr = this->load_control_template_string(this->configure->GetTemplateString());
        longui_debug_hr(hr, L"load_control_template_string faild");
    }
    // 资源数据缓存
    if (SUCCEEDED(hr)) {
        // 获取缓存数据
        auto get_buffer_length = [this]() {
            size_t buffer_length =
                sizeof(void*) * m_cCountBmp +
                sizeof(void*) * m_cCountBrs +
                sizeof(void*) * m_cCountTf +
                sizeof(pugi::xml_node) * m_cCountCtrlTemplate +
                (sizeof(HICON) + sizeof(LongUI::Meta)) * m_cCountMt;
            return buffer_length;
        };
        m_cCountBmp = m_cCountBrs = m_cCountTf = m_cCountMt = 1;
        // 检查资源缓存
        if (!m_pResourceBuffer) {
            // 查询资源数量
            if (m_pResourceLoader) {
                m_cCountBmp += static_cast<decltype(m_cCountBmp)>(m_pResourceLoader->GetResourceCount(IUIResourceLoader::Type_Bitmap));
                m_cCountBrs += static_cast<decltype(m_cCountBrs)>(m_pResourceLoader->GetResourceCount(IUIResourceLoader::Type_Brush));
                m_cCountTf += static_cast<decltype(m_cCountTf)>(m_pResourceLoader->GetResourceCount(IUIResourceLoader::Type_TextFormat));
                m_cCountMt += static_cast<decltype(m_cCountMt)>(m_pResourceLoader->GetResourceCount(IUIResourceLoader::Type_Meta));
            }
            // 申请内存
            m_pResourceBuffer = LongUI::NormalAlloc(get_buffer_length());
        }
        // 修改资源
        if (m_pResourceBuffer) {
            std::memset(m_pResourceBuffer, 0, get_buffer_length());
            m_ppBitmaps = reinterpret_cast<decltype(m_ppBitmaps)>(m_pResourceBuffer);
            m_ppBrushes = reinterpret_cast<decltype(m_ppBrushes)>(m_ppBitmaps + m_cCountBmp);
            m_ppTextFormats = reinterpret_cast<decltype(m_ppTextFormats)>(m_ppBrushes + m_cCountBrs);
            m_pMetasBuffer = reinterpret_cast<decltype(m_pMetasBuffer)>(m_ppTextFormats + m_cCountTf);
            m_phMetaIcon = reinterpret_cast<decltype(m_phMetaIcon)>(m_pMetasBuffer + m_cCountMt);
            m_pTemplateNodes = reinterpret_cast<decltype(m_pTemplateNodes)>(m_phMetaIcon + m_cCountMt);
            // 初始化
            for (auto itr = m_pTemplateNodes; itr < m_pTemplateNodes + m_cCountCtrlTemplate; ++itr) {
                impl::create_object(*itr);
            }
        }
        // 内存不足
        else {
            hr = E_OUTOFMEMORY;
        }
    }
    // 设置控件模板
    if (SUCCEEDED(hr)) {
        hr = this->set_control_template_string();
        longui_debug_hr(hr, L"set_control_template_string faild");
    }
    // 创建D2D工厂
    if (SUCCEEDED(hr)) {
        D2D1_FACTORY_OPTIONS options = { D2D1_DEBUG_LEVEL_NONE };
#ifdef _DEBUG
        options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
        hr = LongUI::Dll::D2D1CreateFactory(
            D2D1_FACTORY_TYPE_SINGLE_THREADED,
            IID_ID2D1Factory4,
            &options,
            reinterpret_cast<void**>(&m_pd2dFactory)
        );
        longui_debug_hr(hr, L"D2D1CreateFactory faild");
    }
    // 创建 DirectWrite 工厂.
    if (SUCCEEDED(hr)) {
        hr = ::DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            LongUI_IID_PV_ARGS_Ex(m_pDWriteFactory)
        );
        longui_debug_hr(hr, L"DWriteCreateFactory faild");
    }
    // 创建帮助器
    /*if (SUCCEEDED(hr)) {
        hr = ::CoCreateInstance(
            CLSID_DragDropHelper,
            nullptr,
            CLSCTX_INPROC_SERVER,
            LongUI_IID_PV_ARGS(m_pDropTargetHelper)
        );
        longui_debug_hr(hr, L"CoCreateInstance CLSID_DragDropHelper faild");
    }*/
    // 创建字体集
    if (SUCCEEDED(hr)) {
        // 获取脚本
        config->CreateInterface(LongUI_IID_PV_ARGS(m_pFontCollection));
        // 失败获取系统字体集
        if (!m_pFontCollection) {
            hr = m_pDWriteFactory->GetSystemFontCollection(&m_pFontCollection);
            longui_debug_hr(hr, L"m_pDWriteFactory->GetSystemFontCollection faild");
        }
    }
#ifdef _DEBUG
    // 枚举字体
    if (SUCCEEDED(hr) && (this->flag & IUIConfigure::Flag_DbgOutputFontFamily)) {
        auto count = m_pFontCollection->GetFontFamilyCount();
        UIManager << DL_Log << "Font found: " << long(count) << L"\r\n";
        // 遍历所有字体
        for (auto i = 0u; i < count; ++i) {
            IDWriteFontFamily* family = nullptr;
            // 获取字体信息
            if (SUCCEEDED(m_pFontCollection->GetFontFamily(i, &family))) {
                IDWriteLocalizedStrings* string = nullptr;
                // 获取字体名称
                if (SUCCEEDED(family->GetFamilyNames(&string))) {
                    wchar_t buffer[LongUIStringBufferLength];
                    auto tc = string->GetCount();
                    UIManager << DLevel_Log << Formated(L"%4d[%d]: ", int(i), int(tc));
                    // 遍历所有字体名称
#if 0
                    for (auto j = 0u; j < 1u; j++) {
                        string->GetLocaleName(j, buffer, LongUIStringBufferLength);
                        UIManager << DLevel_Log << buffer << " => ";
                        // 有些语言在我的机器上显示不了(比如韩语), 会出现bug略过不少东西, 就显示第一个了
                        string->GetString(j, buffer, LongUIStringBufferLength);
                        UIManager << DLevel_Log << buffer << "; ";
                    }
#else
                    // 显示第一个
                    string->GetLocaleName(0, buffer, LongUIStringBufferLength);
                    UIManager << DLevel_Log << buffer << " => ";
                    string->GetString(0, buffer, LongUIStringBufferLength);
                    UIManager << DLevel_Log << buffer << ";\r\n";
#endif
                }
                LongUI::SafeRelease(string);
            }
            LongUI::SafeRelease(family);
        }
        // 刷新
        UIManager << DL_Log << LongUI::endl;
    }
#endif
    // 注册渲染器
    if (SUCCEEDED(hr)) {
        // 普通渲染器
        if (this->RegisterTextRenderer(&m_normalTRenderer, "normal") != Type_NormalTextRenderer) {
            assert(!"Type_NormalTextRenderer");
            hr = E_FAIL;
        }
    }
    // 注册渲染器
    if (SUCCEEDED(hr)) {
        // 描边渲染器
        if (this->RegisterTextRenderer(&m_outlineTRenderer, "outline") != Type_OutlineTextRenderer) {
            assert(!"Type_OutlineTextRenderer");
            hr = E_FAIL;
        }
    }
    // 添加控件
    if (SUCCEEDED(hr)) {
        // 注册控件类
#define LONGUI_REGISTERCC(name) UI##name::CreateControl, #name
        //this->RegisterControlClass(LONGUI_REGISTERCC(ScrollBarB));
        // 添加默认控件创建函数
        this->RegisterControlClass(CreateNullControl, "Null");
        this->RegisterControlClass(LONGUI_REGISTERCC(Text));
        this->RegisterControlClass(LONGUI_REGISTERCC(List));
        this->RegisterControlClass(LONGUI_REGISTERCC(Edit));
        this->RegisterControlClass(LONGUI_REGISTERCC(Page));
        this->RegisterControlClass(LONGUI_REGISTERCC(Color));
        this->RegisterControlClass(LONGUI_REGISTERCC(Slider));
        this->RegisterControlClass(LONGUI_REGISTERCC(Button));
        this->RegisterControlClass(LONGUI_REGISTERCC(Single));
        this->RegisterControlClass(LONGUI_REGISTERCC(ListLine));
        this->RegisterControlClass(LONGUI_REGISTERCC(CheckBox));
        this->RegisterControlClass(LONGUI_REGISTERCC(ComboBox));
        this->RegisterControlClass(LONGUI_REGISTERCC(RamBitmap));
        this->RegisterControlClass(LONGUI_REGISTERCC(ListHeader));
        this->RegisterControlClass(LONGUI_REGISTERCC(ScrollBarA));
        this->RegisterControlClass(LONGUI_REGISTERCC(RadioButton));
        this->RegisterControlClass(LONGUI_REGISTERCC(FloatLayout));
        this->RegisterControlClass(LONGUI_REGISTERCC(VerticalLayout));
        this->RegisterControlClass(LONGUI_REGISTERCC(HorizontalLayout));
        // 添加自定义控件
        config->RegisterSome();
    }
    // 初始化事件
    if (SUCCEEDED(hr)) {
        hr = this->do_creating_event(LongUI::CreateEventType::Type_Initialize);
        longui_debug_hr(hr, L"do_creating_event(init) faild");
    }
    // 创建资源
    if (SUCCEEDED(hr)) {
        hr = this->RecreateResources();
        longui_debug_hr(hr, L"RecreateResources faild");
    }
    // 检查错误
    if (FAILED(hr)) {
        this->ShowError(hr);
    }
    // 检查当前路径
#ifdef _DEBUG
    wchar_t buffer[MAX_PATH * 4]; buffer[0] = 0;
    ::GetCurrentDirectoryW(lengthof<uint32_t>(buffer), buffer);
    UIManager << DL_Log << L" Current Directory: " << buffer << LongUI::endl;
#endif
    return hr;
}


/// <summary>
/// call this if killed the focus.
/// </summary>
/// <returns></returns>
void LongUI::CUIManager::KillFocus() noexcept {
    m_uiInput.ClearMouseState();
#ifdef _DEBUG
    UIManager << DL_Log << L"called" << LongUI::endl;
#endif
}

/// <summary>
/// Recreates the resources.
/// </summary>
/// <returns></returns>
auto LongUI::CUIManager::RecreateResources() noexcept -> HRESULT {
    CUIDxgiAutoLocker locker;
    this->discard_resources(); 
    return this->create_device_resources(); 
}

/// <summary>
/// Uninitializes this instance.
/// </summary>
/// <returns></returns>
void LongUI::CUIManager::Uninitialize() noexcept {
    // 反初始化事件
    this->do_creating_event(LongUI::CreateEventType::Type_Uninitialize);
    // 释放文本渲染器
    for (auto& renderer : m_apTextRenderer) {
        LongUI::SafeRelease(renderer);
    }
    // 释放时间胶囊
    for (auto cap : m_vTimeCapsules) {
        assert(cap);
        cap->Dispose();
    }
    m_vTimeCapsules.clear();
    // 释放公共设备无关资源
    {
        // 释放文本格式
        for (auto itr = m_ppTextFormats; itr != m_ppTextFormats + m_cCountTf; ++itr) {
            LongUI::SafeRelease(*itr);
        }
        // 摧毁META图标
        for (auto itr = m_phMetaIcon; itr != m_phMetaIcon + m_cCountMt; ++itr) {
            if (*itr) {
                ::DestroyIcon(*itr);
                *itr = nullptr;
            }
        }
        // 控件模板
        for (auto itr = m_pTemplateNodes; itr != m_pTemplateNodes + m_cCountCtrlTemplate; ++itr) {
            impl::destory_object(*itr);
        }
    }
    // 释放资源
    LongUI::SafeRelease(m_pFontCollection);
    LongUI::SafeRelease(m_pDWriteFactory);
    //LongUI::SafeRelease(m_pDropTargetHelper);
    LongUI::SafeRelease(m_pd2dFactory);
    // 释放格式化器
    LongUI::SafeRelease(m_pTextFormatter);
    // 释放脚本
    LongUI::SafeRelease(force_cast(script));
    // 释放读取器
    LongUI::SafeRelease(m_pResourceLoader);
    // 释放设备相关资源
    this->discard_resources();
    // 释放内存
    if (m_pBitmap0Buffer) {
        LongUI::NormalFree(m_pBitmap0Buffer);
        m_pBitmap0Buffer = nullptr;
    }
    // 释放资源缓存
    if (m_pResourceBuffer) {
        LongUI::NormalFree(m_pResourceBuffer);
        m_pResourceBuffer = nullptr;
    }
    m_cCountMt = m_cCountTf = m_cCountBmp = m_cCountBrs = 0;
    // 清理
    m_hashStr2CreateFunc.Clear();
#ifdef _DEBUG
    long time = ::timeGetTime() - m_dbgExitTime;
    UIManager << DL_Log
        << L" took " << time << L"ms to exit here"
        << LongUI::endl;
#endif // _DEBUG
    // 释放配置
    LongUI::SafeRelease(force_cast(this->configure));
    // 关闭窗口
    ::DestroyWindow(m_hToolWnd);
}

// 创建事件
auto LongUI::CUIManager::do_creating_event(CreateEventType type) noexcept ->HRESULT {
    // 类型断言
    assert(type < LongUI::TypeGreater_CreateControl_ReinterpretParentPointer);
    assert(type > Type_CreateControl_NullParentPointer);
    HRESULT hr = S_OK;
    // 遍历hash表
    m_hashStr2CreateFunc.ForEach([type, &hr](StringTable::Unit* unit) noexcept {
        assert(unit);
        if (SUCCEEDED(hr)) {
            auto func = reinterpret_cast<CreateControlEvent>(unit->value);
            void* ptr = func(type, pugi::xml_node(nullptr));
            size_t data = reinterpret_cast<size_t>(ptr);
            hr = static_cast<HRESULT>(data);
        }
    });
    // 返回结果
    return hr;
}

/// <summary>
/// Makes the control tree.
/// 创建控件树
/// </summary>
/// <param name="root">The root.</param>
/// <param name="node">The node.</param>
/// <returns></returns>
void LongUI::CUIManager::MakeControlTree(UIContainer* root, pugi::xml_node node) noexcept {
    // 断言
    assert(root && node && "bad argument");
    // 队列 -- 顺序遍历树
    EzContainer::FixedCirQueue<pugi::xml_node, LongUIMaxControlInited> xml_queue;
    EzContainer::FixedCirQueue<UIContainer*, LongUIMaxControlInited> parents_queue;
    UIControl* control = nullptr;
    UIContainer* parent = root;
    // 遍历算法: 1.压入所有子结点 2.依次弹出 3.重复1
    bool noskip = true;
    while (true) {
        // 压入/入队 所有子结点
        node = node.first_child();
        while (noskip && node) {
            xml_queue.Push(node);
            parents_queue.Push(parent);
            node = node.next_sibling();
        }
        noskip = true;
        // 为空则退出
        if (xml_queue.IsEmpty()) break;
        // 弹出/出队 第一个结点
        node = xml_queue.Front();  xml_queue.Pop();
        parent = parents_queue.Front(); parents_queue.Pop();
        assert(node && "bad xml node");
        // 根据名称创建控件
        if (!(control = this->CreateControl(parent, node, nullptr))) {
            // 错误
            parent = nullptr;
            UIManager << DL_Error
                << L" control class not found: "
                << node.name()
                << L".or OOM"
                << LongUI::endl;
            continue;
        }
        // 添加子结点
        parent->Push(control);
        // 去除引用
        control->Release();
        // 设置结点为下次父结点
        parent = static_cast<UIContainer*>(control);
        // 不是容器的话直接跳过
        noskip = !!(parent->flags & Flag_UIContainer);
    }
}

/// <summary>
/// Gets the create function pointer.
/// 获取创建控件函数指针
/// </summary>
/// <param name="clname">The clname.</param>
/// <returns></returns>
auto LongUI::CUIManager::GetCreateFunc(const char* clname) noexcept -> CreateControlEvent {
    // 检查 !white_space(clname[0]) && 
    assert(clname && clname[0] && "bad argment");
    // 查找
    auto result = m_hashStr2CreateFunc.Find(clname);
    // 检查
    assert(result && "class not found");
    // 返回
    return reinterpret_cast<CreateControlEvent>(*result);
}

/// <summary>
/// Creates the text format.
/// 创建文本格式
/// </summary>
/// <param name="fontFamilyName">Name of the font family.</param>
/// <param name="fontWeight">The font weight.</param>
/// <param name="fontStyle">The font style.</param>
/// <param name="fontStretch">The font stretch.</param>
/// <param name="fontSize">Size of the font.</param>
/// <param name="textFormat">The text format.</param>
/// <returns></returns>
auto LongUI::CUIManager::CreateTextFormat(
    WCHAR const * fontFamilyName,
    DWRITE_FONT_WEIGHT fontWeight,
    DWRITE_FONT_STYLE fontStyle,
    DWRITE_FONT_STRETCH fontStretch,
    FLOAT fontSize,
    IDWriteTextFormat ** textFormat) noexcept -> HRESULT {
    auto hr = S_OK;
#ifdef _DEBUG
    // 检查字体名称
    UINT32 index = 0; BOOL exist = FALSE;
    hr = m_pFontCollection->FindFamilyName(fontFamilyName, &index, &exist);
    if (SUCCEEDED(hr)) {
        // 字体不存在, 则给予警告
        if (!exist) {
            UIManager << DL_Hint
                << Formated(L"font family(%ls) not found", fontFamilyName)
                << LongUI::endl;
            int bk; bk = 9;
        }
    }
    longui_debug_hr(hr, L"m_pFontCollection->FindFamilyName failed");
#endif
    // 创建文本格式
    hr = m_pDWriteFactory->CreateTextFormat(
        fontFamilyName,
        m_pFontCollection,
        fontWeight,
        fontStyle,
        fontStretch,
        fontSize,
        m_szLocaleName,
        textFormat
    );
    // 检查错误
    longui_debug_hr(hr, Formated(L"CreateTextFormat(%ls) faild", fontFamilyName));
    return hr;
}


#ifdef _DEBUG
/// <summary>
/// Exits this instance.
/// </summary>
/// <returns></returns>
void LongUI::CUIManager::Exit() noexcept {
    m_dbgExitTime = ::timeGetTime();
    UIManager << DL_Log << L" CALLED" << LongUI::endl;
    this->exit();
}
namespace LongUI { namespace impl { void dbg_cal_fps() { } }}

#endif

#ifdef LONGUI_RENDER_IN_STD_THREAD
#include <thread>
#else
#include <process.h>
#endif


/// <summary>
/// Runs this instance.
/// </summary>
/// <returns></returns>
void LongUI::CUIManager::Run() noexcept {
    // 开始!
    m_dwWaitVSStartTime = ::timeGetTime();
    UIManager.m_uiTimeMeter.RefreshFrequency();
    // 渲染线程函数
    auto render_thread_func = [](void*) noexcept ->unsigned {
        // 更新
        UIManager.m_cStartTick = UIManager.m_cNowTick = ::timeGetTime();
        // 循环
        while (true) {
            // 刷新
            {
                // 数据锁
                CUIDataAutoLocker locker;
#ifdef _DEBUG
                ++UIManager.frame_id;
#endif
                // 更新计时器
                UIManager.m_fDeltaTime = UIManager.m_uiTimeMeter.Delta_s<float>();
                UIManager.m_uiTimeMeter.MovStartEnd();
                auto old = UIManager.m_cNowTick;
                auto now = ::timeGetTime(); UIManager.m_cNowTick = now;
                // 大概每隔2分钟, 刷新一次
                constexpr uint32_t REFRESH_RATE = 1024 * 128 - 1;
                constexpr uint32_t REFRESH_MASK = ~REFRESH_RATE;
                if ((old & REFRESH_MASK) != (now & REFRESH_MASK) ) {
                    UIManager.m_uiTimeMeter.RefreshFrequency();
#ifdef _DEBUG
                    UIManager << DL_Log
                        << L"m_uiTimeMeter.RefreshFrequency"
                        << LongUI::endl;
#endif
                }
                // 更新时间胶囊
                UIManager.update_time_capsules(UIManager.m_fDeltaTime);
                // 刷新窗口
                for (auto window : UIManager.m_vWindows) {
                    window->Update();
                }
                // 更新输入
                UIManager.m_uiInput.AfterUpdate();
            }
            // 渲染
            {
                // 渲染锁
                CUIDxgiAutoLocker locker;
                // 渲染窗口
                for (auto window : UIManager.m_vWindows) {
                    window->Render();
                }
            }
            // 帧尾处理
            {
                // 数据锁
                CUIDataAutoLocker locker;
                // 延迟清理
                UIManager.cleanup_delay_cleanup_chain();
#ifdef _DEBUG
                // 计算平均FPS
                auto& fpsc = UIManager.m_vFpsCalculator;
                fpsc.push_back(UIManager.m_fDeltaTime);
                size_t frame = size_t(UIManager.m_dDisplayFrequency / 2);
                // 固定时间刷新一次
                if (fpsc.size() >= size_t(frame)) {
                    float time = 0.f;
                    for (auto t : fpsc) time += t;
                    time /= float(frame);
                    wchar_t buffer[1024];
                    std::swprintf(
                        buffer, lengthof(buffer),
                        L"delta: %.2fms -- %2.2f fps",
                        time * 1000.f, 1.f / time
                    );
                    auto hwnd = UIManager.m_hToolWnd;
                    UIManager.DataUnlock();
                    ::SetWindowTextW(hwnd, buffer);
                    UIManager.DataLock();
                    fpsc.clear();
                }
#endif
            }
            // 退出检查
            if (UIManager.m_exitFlag) break;
            // 等待垂直同步
            UIManager.wait_for_vblank();
        }
        return 0;
    };
    // 需要std::thread?
#ifdef LONGUI_RENDER_IN_STD_THREAD
    std::thread thread(render_thread_func, nullptr);
#else
    auto thread = reinterpret_cast<HANDLE>(
        ::_beginthreadex(nullptr, 0, render_thread_func, nullptr, 0, nullptr)
        );
    assert(thread && "failed to create thread");
#endif
    // 消息响应
    MSG msg;
    while (::GetMessageW(&msg, nullptr, 0, 0)) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
    // 等待线程
#ifdef LONGUI_RENDER_IN_STD_THREAD
    try { if (thread.joinable()) { thread.join(); } }
    catch (...) {}
#else
    if (thread) {
        ::WaitForSingleObject(thread, INFINITE);
        ::CloseHandle(thread);
        thread = nullptr;
    }
#endif
    // 再次清理
    this->cleanup_delay_cleanup_chain();
    // 尝试强行关闭
    /*if (m_cCountWindow) {
        UIViewport* windows[LongUIMaxWindow];
        std::memcpy(windows, m_apWindows, sizeof(m_apWindows));
        auto count = m_cCountWindow;
        // 清理窗口
        for (auto i = 0u; i < count; ++i) {
            windows[count - i - 1]->cleanup();
        }
    }*/
    while (m_vWindows.size()) {
        m_vWindows.back()->Dispose();
    }
    assert(m_vWindows.empty() && "bad");
}

/// <summary>
/// Wait_for_vblanks this instance.
/// 等待垂直同步
/// </summary>
/// <returns></returns>
void LongUI::CUIManager::wait_for_vblank() noexcept {
    // 自行等待垂直同步以方便游戏窗口不等待垂直同步的实现
    if (this->flag & IUIConfigure::Flag_RenderInAnytime) return;
    // 存在DXGI输出?
    if (m_pDxgiOutput) return void(m_pDxgiOutput->WaitForVBlank());
    // 保留刷新时间点
    auto f = static_cast<uint32_t>(m_dDisplayFrequency);
    auto c = ((++m_dwWaitVSCount) * 1000ui32);
    auto end_time_of_sleep = m_dwWaitVSStartTime + c / f;
    // 保证等待
    while (::timeGetTime() < end_time_of_sleep) ::Sleep(1);
}


/// <summary>
/// Creates the control.
/// 利用模板ID创建控件
/// </summary>
/// <param name="cp">The cp.</param>
/// <param name="templateid">The templateid.</param>
/// <param name="function">The function.</param>
/// <returns></returns>
auto LongUI::CUIManager::CreateControl(
    UIContainer* cp, 
    size_t templateid, 
    CreateControlEvent function
) noexcept ->UIControl* {
    // 检查参数
    assert(function && "function must be specified");
    //assert(templateid && "template id must be specified");
    // 检查模板ID
    assert(templateid < m_cCountCtrlTemplate && "out of range");
    // 越界检查
    if (templateid >= m_cCountCtrlTemplate) templateid = 0;
    // 创建
    return function(cp->GetCET(), m_pTemplateNodes[templateid]);
}

/// <summary>
/// Create_controls the specified cp.
/// 利用现有资源创建控件
/// </summary>
/// <param name="cp">The cp.</param>
/// <param name="function">The function.</param>
/// <param name="node">The node.</param>
/// <param name="tid">The tid.</param>
/// <returns></returns>
auto LongUI::CUIManager::create_control(
    UIContainer* cp, 
    CreateControlEvent function, 
    pugi::xml_node node, 
    size_t tid
) noexcept -> UIControl * {
    // TODO: NODE
    assert(node && "call another method if no xml-node");
    // 检查参数 function
    if (!function) {
        function = this->GetCreateFunc(node.name());
    }
    // 结点有效并且没有指定模板ID则尝试获取
    if (!tid) {
        tid = static_cast<decltype(tid)>(LongUI::AtoI(
            node.attribute(LongUI::XmlAttribute::TemplateID).value())
            );
    }
    // 利用id查找模板控件
    if (tid) {
        assert(tid < m_cCountCtrlTemplate && "out of range");
        if (tid >= m_cCountCtrlTemplate) tid = 0;
        // 结点有效->添加属性
        {
            auto attribute = m_pTemplateNodes[tid].first_attribute();
            // 遍历属性
            while (attribute) {
                // 添加属性
                auto name = attribute.name();
                if (!node.attribute(name)) {
                    node.insert_attribute_after(name, node.last_attribute()).set_value(attribute.value());
                }
                // 推进
                attribute = attribute.next_attribute();
            }
        }
    }
    // 检查
    assert(function && "bad idea");
    return function ? function(cp->GetCET(), node) : nullptr;
}

/// <summary>
/// Create_ui_windows the specified node.
/// 创建UI窗口
/// </summary>
/// <param name="node">The node.</param>
/// <param name="parent">The parent.</param>
/// <param name="call">The call.</param>
/// <returns></returns>
auto LongUI::CUIManager::create_ui_window(
    pugi::xml_node node,
    XUIBaseWindow* parent,
    callback_create_viewport call) noexcept -> XUIBaseWindow* {
    assert(node && call && "bad arguments");
    // 初始化
    HRESULT hr;
    Config::Window config;
    std::memset(&config, 0, sizeof(config));
    config.node = node;
    config.parent = parent;
    config.system = true;
    // 创建窗口
    auto window = LongUI::CreateBuiltinWindow(config);
    assert(window && "create system window failed");
    if (!window) return nullptr;
    // 创建视口
    auto viewport = call(node, window);
    assert(viewport && "create viewport failed");
    if (!viewport) {
        window->Dispose();
        return nullptr;
    }
    // 链接视口
    window->InitializeViewport(viewport);
#ifdef _DEBUG
    //::Sleep(5000);
    CUITimeMeterH dbg_tmtr; dbg_tmtr.Start();
    UIManager << DL_Log << window << LongUI::endl;
#endif
    // 重建资源
    if (FAILED(hr = window->Recreate())) {
        constexpr auto id = String_FailRecreate;
        auto substring = this->configure->GetString(id);
        this->ShowError(hr, substring);
    }
#ifdef _DEBUG
    //::Sleep(500);
    auto time = dbg_tmtr.Delta_ms<double>();
    UIManager << DL_Log
        << Formated(L" took %.3lfms to recreate.", time)
        << LongUI::endl;
    dbg_tmtr.MovStartEnd();
#endif
    // 创建控件树
    this->MakeControlTree(viewport, node);
    // 完成创建
#ifdef _DEBUG
    time = dbg_tmtr.Delta_ms<double>();
    UIManager << DL_Log
        << Formated(L" took %.3lfms to make tree.", time)
        << LongUI::endl;
    dbg_tmtr.MovStartEnd();
#endif
    // 发送消息
    viewport->DoLongUIEvent(Event::Event_TreeBuildingFinished);
#ifdef _DEBUG
    time = dbg_tmtr.Delta_ms<double>();
    UIManager << DL_Log
        << Formated(L" took %.3lfms to send finished event.", time)
        << LongUI::endl;
    //::Sleep(500);
#endif
    return window;
}

/// <summary>
/// Gets the color of the theme.
/// 获取主题颜色
/// </summary>
/// <param name="colorf">The colorf.</param>
/// <returns></returns>
auto LongUI::CUIManager::GetThemeColor(D2D1_COLOR_F& colorf) noexcept -> HRESULT {
    union { DWORD color; uint8_t argb[4]; };
    color = DWORD(-1); auto hr = S_OK; DWORD buffer_size = sizeof(DWORD);
    // 获取Colorization颜色
    ::RegGetValueA(
        HKEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\DWM", "ColorizationColor",
        RRF_RT_DWORD, nullptr, &color, &buffer_size
    );
    DWORD balance = 50; buffer_size = sizeof(DWORD);
    // 获取Colorization混合标准
    ::RegGetValueA(
        HKEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\DWM", "ColorizationColorBalance",
        RRF_RT_DWORD, nullptr, &balance, &buffer_size
    );
    {
        // 混合通道
        auto blend_channel = [](float ch1, float ch2, float prec) {
            auto data = ch1 + (ch2 - ch1) * prec;
            return data > 1.f ? 1.f : (data < 0.f ? 0.f : data);
        };
        colorf.a = 1.f; auto prec = 1.f - float(balance) / 100.f;
        constexpr float basegrey = float(217) / 255.f;
        colorf.r = blend_channel(float(argb[2]) / 255.f, basegrey, prec);
        colorf.g = blend_channel(float(argb[1]) / 255.f, basegrey, prec);
        colorf.b = blend_channel(float(argb[0]) / 255.f, basegrey, prec);
    }
    return hr;
}

/// <summary>
/// Initializes a new instance of the <see cref="CUIManager"/> class.
///  <see cref="CUIManager"/> 构造函数
/// </summary>
LongUI::CUIManager::CUIManager() noexcept  {

}

/// <summary>
/// Finalizes an instance of the <see cref="CUIManager"/> class.
///  <see cref="CUIManager"/> 析构函数
/// </summary>
/// <returns></returns>
LongUI::CUIManager::~CUIManager() noexcept {
    this->discard_resources();
}

/// <summary>
/// Registers the control class.
/// 注册控件类
/// </summary>
/// <param name="func">The create event function.</param>
/// <param name="clname">The class name.</param>
/// <returns></returns>
auto LongUI::CUIManager::RegisterControlClass(
    CreateControlEvent func, const char* clname) noexcept ->HRESULT {
    assert(clname && clname[0] && "bad argument");
    // 插入
    auto result = m_hashStr2CreateFunc.Insert(m_oStringAllocator.CopyString(clname), func);
    // 插入失败的原因只有一个->OOM
    return result ? S_OK : E_OUTOFMEMORY;
}

/// <summary>
/// Determines whether [is registerd control class] [the specified clname].
/// 检查控件类是否注册
/// </summary>
/// <param name="clname">The class name.</param>
/// <returns></returns>
bool LongUI::CUIManager::IsRegisteredControlClass(const char* clname) noexcept {
    assert(clname && clname[0] && "bad argument");
    return !!m_hashStr2CreateFunc.Find(clname);
}

/// <summary>
/// Unregisters the control class.
/// 反注册控件类
/// </summary>
/// <param name="clname">The class name.</param>
/// <returns></returns>
void LongUI::CUIManager::UnregisterControlClass(const char* clname) noexcept {
    assert(clname && clname[0] && "bad argument");
    // 移除
    if (this->IsRegisteredControlClass(clname)) {
        m_hashStr2CreateFunc.Remove(clname);
    }
#ifdef _DEBUG
    else {
        UIManager << DL_Error
            << L"class name # "
            << clname
            << L" # not found"
            << LongUI::endl;
        assert(!"class not found");
    }
#endif
}

/// <summary>
/// Shows the error.
/// 显示错误代码
/// </summary>
/// <param name="hr">The hr.</param>
/// <param name="str_b">The STR_B.</param>
/// <returns></returns>
void LongUI::CUIManager::ShowError(HRESULT hr, const wchar_t* str_b) noexcept {
    constexpr uint32_t BUFFER_SIZE = 512;
    wchar_t buffer[BUFFER_SIZE];
    // 格式化
    if (!::FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM,
        nullptr, hr,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
        buffer, BUFFER_SIZE,
        nullptr)) {
        auto s = this->configure->GetString(String_FaildHR);
        // 处理
        std::swprintf(buffer, BUFFER_SIZE, L"%ls: 0x%08X", s, hr);
    }
    // 错误
    this->ShowError(buffer, str_b);
}

/// <summary>
/// Registers the text renderer.
/// 注册文本渲染器
/// </summary>
/// <param name="renderer">The renderer.</param>
/// <param name="name">The name.</param>
/// <returns></returns>
auto LongUI::CUIManager::RegisterTextRenderer(
    XUIBasicTextRenderer* renderer, 
    const char name[LongUITextRendererNameMaxLength]
) noexcept -> int32_t {
    assert(m_uTextRenderCount < lengthof(m_apTextRenderer) && "buffer too small");
    assert(!white_space(name[0]) && "name cannot begin with white space");
    // 满了
    if (m_uTextRenderCount == lengthof(m_apTextRenderer)) {
        return -1;
    }
    const auto count = m_uTextRenderCount;
    assert((std::strlen(name) + 1) < LongUITextRendererNameMaxLength && "buffer too small");
    std::strcpy(m_aszTextRendererName[count].name, name);
    m_apTextRenderer[count] = LongUI::SafeAcquire(renderer);
    ++m_uTextRenderCount;
    return count;
}

/// <summary>
/// Create_indexzero_resourceses this instance.
/// 创建0索引资源
/// </summary>
/// <returns></returns>
auto LongUI::CUIManager::create_indexzero_resources() noexcept ->HRESULT {
    assert(m_pResourceBuffer && "bad alloc");
    HRESULT hr = S_OK;
    // 索引0位图: 可MAP位图
    if (SUCCEEDED(hr)) {
        assert(m_ppBitmaps[LongUIDefaultBitmapIndex] == nullptr && "bad action");
        hr = m_pd2dDeviceContext->CreateBitmap(
            D2D1_SIZE_U{ LongUIDefaultBitmapSize, LongUIDefaultBitmapSize },
            nullptr, LongUIDefaultBitmapSize * 4,
            D2D1::BitmapProperties1(
                static_cast<D2D1_BITMAP_OPTIONS>(LongUIDefaultBitmapOptions),
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
            ),
            m_ppBitmaps + LongUIDefaultBitmapIndex
        );
        longui_debug_hr(hr, L"_pd2dDeviceContext->CreateBitmap failed");
    }
    // 索引0笔刷: 全控件共享用前写纯色笔刷
    if (SUCCEEDED(hr)) {
        assert(m_ppBrushes[LongUICommonSolidColorBrushIndex] == nullptr && "bad action");
        ID2D1SolidColorBrush* brush = nullptr;
        D2D1_COLOR_F color = D2D1::ColorF(D2D1::ColorF::Black);
        hr = m_pd2dDeviceContext->CreateSolidColorBrush(&color, nullptr, &brush);
        m_ppBrushes[LongUICommonSolidColorBrushIndex] = LongUI::SafeAcquire(brush);
        LongUI::SafeRelease(brush);
        longui_debug_hr(hr, L"_pd2dDeviceContext->CreateSolidColorBrush failed");
    }
    // 索引0文本格式: 默认格式, 属于设备无关资源，仅需要重建一次
    if (SUCCEEDED(hr) && !(m_ppTextFormats[LongUIDefaultTextFormatIndex])) {
        hr = this->CreateTextFormat(
            LongUIDefaultTextFontName,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            LongUIDefaultTextFontSize,
            //12.f,
            m_ppTextFormats + LongUIDefaultTextFormatIndex
        );
        longui_debug_hr(hr, L"this->CreateTextFormat failed");
    }
    // 设置
    if (SUCCEEDED(hr)) {
        m_ppTextFormats[LongUIDefaultTextFormatIndex]->SetParagraphAlignment(
            DWRITE_PARAGRAPH_ALIGNMENT(LongUIDefaultTextVAlign)
        );
        m_ppTextFormats[LongUIDefaultTextFormatIndex]->SetTextAlignment(
            DWRITE_TEXT_ALIGNMENT(LongUIDefaultTextHAlign)
        );
    }
    // 索引0图元: 暂无
    if (SUCCEEDED(hr)) {

    }
    // 索引0 xml节点
    if (SUCCEEDED(hr) && !(m_pTemplateNodes[0])) {
        m_pTemplateNodes[0] = m_docTemplate.append_child("Control");
    }
    return hr;
}

/// <summary>
/// Cleanup_delay_cleanup_chains this instance.
/// 清理延迟清理链
/// </summary>
/// <returns></returns>
void LongUI::CUIManager::cleanup_delay_cleanup_chain() noexcept {
    for (auto ctrl : m_vDelayCleanup) {
        ctrl->Release();
    }
    for (auto wnd : m_vDelayDispose) {
        wnd->Dispose();
    }
    m_vDelayCleanup.clear();
    m_vDelayDispose.clear();
}

/// <summary>
/// Load_control_template_strings the specified string.
/// 载入模板字符串
/// </summary>
/// <param name="str">The string.</param>
/// <returns></returns>
auto LongUI::CUIManager::load_control_template_string(const char* str) noexcept ->HRESULT {
    // 检查参数
    if (str && *str) {
        // 载入字符串
        auto code = m_docTemplate.load_string(str);
        if (code.status) {
            assert(!"load error");
            ::MessageBoxA(nullptr, code.description(), "<LongUI::CUIManager::load_control_template_string>: Failed to Parse/Load XML", MB_ICONERROR);
            return E_FAIL;
        }
        // 获取子结点数量
        auto get_children_count = [](pugi::xml_node node) {
            node = node.first_child();
            auto count = 0ui16;
            while (node) { node = node.next_sibling(); ++count; }
            return count;
        };
        m_cCountCtrlTemplate = 1 + get_children_count(m_docTemplate.root().first_child());
        // 解析
        return S_OK;
    }
    else {
        return S_FALSE;
    }
}


/// <summary>
/// Set_control_template_strings this instance.
/// 设置模板字符串
/// </summary>
/// <returns></returns>
auto LongUI::CUIManager::set_control_template_string() noexcept ->HRESULT {
    // 有效情况
    if (m_cCountCtrlTemplate > 1) {
        auto itr = m_pTemplateNodes + 1;
        auto node = m_docTemplate.root().first_child().first_child();
        // 写入索引
        while (node) {
            *itr = node;
            node = node.next_sibling();
            ++itr;
        }
    }
    return S_OK;
}



/// <summary>
/// create dxgi output for this instance.
/// </summary>
/// <returns></returns>
auto LongUI::CUIManager::create_dxgi_output() noexcept -> HRESULT {
    assert(m_pDxgiFactory && "bad action");
    LongUI::SafeRelease(m_pDxgiOutput);
    // 调试
#ifdef _DEBUG
    m_vFpsCalculator.reserve(m_dDisplayFrequency);
    if (!m_vFpsCalculator.isok()) return E_OUTOFMEMORY;
    m_vFpsCalculator.clear();
#endif
    // 初始化
    IDXGIAdapter1* pDxgiAdapter = nullptr;
    UINT ia = 0;
    // 枚举适配器
    while (m_pDxgiFactory->EnumAdapters1(ia, &pDxgiAdapter) != DXGI_ERROR_NOT_FOUND) {
        assert(pDxgiAdapter && "bad action");
#ifdef _DEBUG
        DXGI_ADAPTER_DESC1 desca;
        pDxgiAdapter->GetDesc1(&desca);
#endif
        // 枚举显示输出
        while (pDxgiAdapter->EnumOutputs(0, &m_pDxgiOutput) != DXGI_ERROR_NOT_FOUND) {
            assert(m_pDxgiOutput && "bad action");
            DXGI_OUTPUT_DESC desco;
            m_pDxgiOutput->GetDesc(&desco);
            LongUI::GetMonitorDpi(desco.Monitor, m_uMainDpiX, m_uMainDpiY);
            return S_OK;
        }
        ++ia;
        pDxgiAdapter->Release();
    }
    // 检查
    assert(!pDxgiAdapter && "bad action");
    return S_FALSE;
}

/// <summary>
/// Create_device_resourceses this instance.
/// UIManager 创建设备相关资源
/// </summary>
/// <returns></returns>
auto LongUI::CUIManager::create_device_resources() noexcept ->HRESULT {
    // 重新获取flag
    this->flag = this->configure->GetConfigureFlag();
    // 待用适配器
    IDXGIAdapter1* adapter = nullptr;
    // 枚举显示适配器
    if (!(this->flag & IUIConfigure::Flag_RenderByCPU)) {
        IDXGIFactory1* dxgifactory = nullptr;
        auto idd = IID_IDXGIFactory1;
        auto add = reinterpret_cast<void**>(&dxgifactory);
        // 创建一个临时工厂
        if (SUCCEEDED(LongUI::Dll::CreateDXGIFactory1(idd, add))) {
            IDXGIAdapter1* apAdapters[LongUIMaxAdaptersSize];
            DXGI_ADAPTER_DESC1 descs[LongUIMaxAdaptersSize];
            uint32_t adnum = 0;
            // 枚举适配器
            for (adnum = 0; adnum < lengthof(apAdapters); ++adnum) {
                constexpr HRESULT nf = DXGI_ERROR_NOT_FOUND;
                if (dxgifactory->EnumAdapters1(adnum, apAdapters + adnum) == nf) {
                    break;
                }
                apAdapters[adnum]->GetDesc1(descs + adnum);
            }
            // 选择适配器
            auto index = this->configure->ChooseAdapter(descs, adnum);
            if (index < adnum) {
                adapter = LongUI::SafeAcquire(apAdapters[index]);
            }
            // 释放适配器
            for (size_t i = 0; i < adnum; ++i) {
                LongUI::SafeRelease(apAdapters[i]);
            }
        }
        LongUI::SafeRelease(dxgifactory);
    }
    // 创建设备资源
    HRESULT hr /*= m_docResource.Error() ? E_FAIL :*/ S_OK;
    // 创建 D3D11设备与设备上下文 
    if (SUCCEEDED(hr)) {
        // D3D11 创建flag 
        // 一定要有D3D11_CREATE_DEVICE_BGRA_SUPPORT
        // 否则创建D2D设备上下文会失败
        UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG) && defined(LONGUI_D3D_DEBUG)
        // Debug状态 有D3D DebugLayer就可以取消注释
        creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
        auto tmpflag = D3D11_CREATE_DEVICE_FLAG(creationFlags);
        tmpflag = D3D11_CREATE_DEVICE_FLAG(0);
#endif
        D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_3,
            D3D_FEATURE_LEVEL_9_2,
            D3D_FEATURE_LEVEL_9_1
        };
        // 根据情况检查驱动类型
        D3D_DRIVER_TYPE dtype = (this->flag & IUIConfigure::Flag_RenderByCPU) ? D3D_DRIVER_TYPE_WARP :
            (adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE);
        // 创建设备
        hr = ::D3D11CreateDevice(
            // 设置为渲染
            adapter,
            // 驱动类型
            dtype,
            // 没有软件接口
            nullptr,
            // 创建flag
            creationFlags,
            // 欲使用的特性等级列表
            featureLevels,
            // 特性等级列表长度
            static_cast<UINT>(lengthof(featureLevels)),
            // SDK 版本
            D3D11_SDK_VERSION,
            // 返回的D3D11设备指针
            &m_pd3dDevice,
            // 返回的特性等级
            &m_featureLevel,
            // 返回的D3D11设备上下文指针
            &m_pd3dDeviceContext
        );
        // 检查
        if (FAILED(hr)) {
            UIManager << DL_Warning
                << L"create d3d11 device failed, now, try to create in warp mode"
                << LongUI::endl;
        }
        // 创建失败则尝试使用软件
        if (FAILED(hr)) {
            hr = ::D3D11CreateDevice(
                // 设置为渲染
                nullptr,
                // 根据情况选择类型
                D3D_DRIVER_TYPE_WARP,
                // 没有软件接口
                nullptr,
                // 创建flag
                creationFlags,
                // 欲使用的特性等级列表
                featureLevels,
                // 特性等级列表长度
                static_cast<UINT>(lengthof(featureLevels)),
                // SDK 版本
                D3D11_SDK_VERSION,
                // 返回的D3D11设备指针
                &m_pd3dDevice,
                // 返回的特性等级
                &m_featureLevel,
                // 返回的D3D11设备上下文指针
                &m_pd3dDeviceContext
            );
        }
        // 再次检查错误
        if (FAILED(hr)) {
            UIManager << DL_Error
                << L" create d3d11-device in warp modd, but failed."
                << LongUI::endl;
            this->ShowError(hr);
        }
    }
    LongUI::SafeRelease(adapter);
    // 创建 ID3D11Debug对象
#if defined(_DEBUG) && defined(LONGUI_D3D_DEBUG)
    if (SUCCEEDED(hr)) {
        hr = m_pd3dDevice->QueryInterface(LongUI_IID_PV_ARGS(m_pd3dDebug));
        longui_debug_hr(hr, L"m_pd3dDevice->QueryInterface(m_pd3dDebug) faild");
    }
#endif
    // 创建 IDXGIDevice
    if (SUCCEEDED(hr)) {
        hr = m_pd3dDevice->QueryInterface(LongUI_IID_PV_ARGS(m_pDxgiDevice));
        longui_debug_hr(hr, L"m_pd3dDevice->QueryInterface(m_pd3dDebug) faild");
    }
    // 创建 D2D设备
    if (SUCCEEDED(hr)) {
        hr = m_pd2dFactory->CreateDevice(m_pDxgiDevice, &m_pd2dDevice);
        longui_debug_hr(hr, L"m_pd2dFactory->CreateDevice faild");
    }
    // 创建 D2D设备上下文
    if (SUCCEEDED(hr)) {
        hr = m_pd2dDevice->CreateDeviceContext(
            D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
            &m_pd2dDeviceContext
        );
        longui_debug_hr(hr, L"m_pd2dDevice->CreateDeviceContext faild");
    }
    // 获取 Dxgi适配器 可以获取该适配器信息
    if (SUCCEEDED(hr)) {
        // 顺带使用像素作为单位
        m_pd2dDeviceContext->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
        // 获取
        hr = m_pDxgiDevice->GetAdapter(&m_pDxgiAdapter);
        longui_debug_hr(hr, L"m_pDxgiDevice->GetAdapter faild");
    }
#ifdef _DEBUG
    // 输出显卡信息
    if (SUCCEEDED(hr)) {
        DXGI_ADAPTER_DESC desc = { 0 };
        m_pDxgiAdapter->GetDesc(&desc);
        UIManager << DL_Log << desc << LongUI::endl;
    }
#endif
    // 获取 Dxgi工厂
    if (SUCCEEDED(hr)) {
        hr = m_pDxgiAdapter->GetParent(LongUI_IID_PV_ARGS(m_pDxgiFactory));
    }
#ifdef LONGUI_WITH_MMFVIDEO
    UINT token = 0;
    // 多线程
    if (SUCCEEDED(hr)) {
        ID3D10Multithread* mt = nullptr;
        hr = m_pd3dDevice->QueryInterface(LongUI_IID_PV_ARGS(mt));
        longui_debug_hr(hr, L"m_pd3dDevice->QueryInterface ID3D10Multithread faild");
        // 保护
        if (SUCCEEDED(hr)) {
            mt->SetMultithreadProtected(TRUE);
        }
        LongUI::SafeRelease(mt);
    }
    // 设置 MF
    if (SUCCEEDED(hr)) {
        hr = ::MFStartup(MF_VERSION);
        longui_debug_hr(hr, L"MFStartup faild");
    }
    // 创建 MF Dxgi 设备管理器
    if (SUCCEEDED(hr)) {
        hr = ::MFCreateDXGIDeviceManager(&token, &m_pMFDXGIManager);
        longui_debug_hr(hr, L"MFCreateDXGIDeviceManager faild");
    }
    // 重置设备
    if (SUCCEEDED(hr)) {
        hr = m_pMFDXGIManager->ResetDevice(m_pd3dDevice, token);
        longui_debug_hr(hr, L"m_pMFDXGIManager->ResetDevice faild");
    }
    // 创建 MF媒体类工厂
    if (SUCCEEDED(hr)) {
        hr = ::CoCreateInstance(
            CLSID_MFMediaEngineClassFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            LongUI_IID_PV_ARGS(m_pMediaEngineFactory)
        );
        longui_debug_hr(hr, L"CoCreateInstance CLSID_MFMediaEngineClassFactory faild");
    }
#endif
    // 创建 Dxgi 输出
    if (SUCCEEDED(hr)) {
        hr = this->create_dxgi_output();
    }
    // 创建系统笔刷
    if (SUCCEEDED(hr)) {
        hr = this->create_system_brushes();
        longui_debug_hr(hr, L"create_system_brushes faild");
    }
    // 创建资源描述资源
    if (SUCCEEDED(hr)) {
        hr = this->create_indexzero_resources();
        longui_debug_hr(hr, L"create_indexzero_resources faild");
    }
    // 事件
    if (SUCCEEDED(hr)) {
        hr = this->do_creating_event(LongUI::CreateEventType::Type_Recreate);
        longui_debug_hr(hr, L"do_creating_event(recreate) faild");
    }
    // 设置文本渲染器数据
    if (SUCCEEDED(hr)) {
        for (uint32_t i = 0u; i < m_uTextRenderCount; ++i) {
            hr = m_apTextRenderer[i]->ChangeTarget(m_pd2dDeviceContext);
            longui_debug_hr(hr, L"ChangeTarget failed: " << long(i));
            if (FAILED(hr)) break;
        }
    }
    // 重建所有窗口
    for (auto window : m_vWindows) {
        if (SUCCEEDED(hr)) {
            hr = window->Recreate();
            longui_debug_hr(hr, window << L"wnd->Recreate");
        }
    }
    return hr;
}

// longui namespace
namespace LongUI {
    // built-in brush
    enum : uint32_t {
        // size of bitmap
        TARGET_BITMAP_SIZE = 64,
        // transparent size
        TRANSPARENT_SIZE = 16,
        // transparent count
        TRANSPARENT_COUNT = 2,
        // transparent offset x
        TRANSPARENT_OFFSET_X = 0,
        // transparent offset y
        TRANSPARENT_OFFSET_Y = 0,
        // transparent color 1
        TRANSPARENT_COLOR1 = 0xFFFFFFFFui32,
        // transparent color 2
        TRANSPARENT_COLOR2 = 0xFFCCCCCCui32,
        // focused size
        FOCUSED_SIZE = 32,
        // focused offset x
        FOCUSED_OFFSET_X = 0,
        // focused offset y
        FOCUSED_OFFSET_Y = 32,
        // focused color 1
        FOCUSED_COLOR1 = 0xFFFFFFFFui32,
        // focused color 2
        FOCUSED_COLOR2 = 0xFF000000ui32,
    };
}

/// <summary>
/// Create_system_brusheses this instance.
/// 创建系统笔刷
/// </summary>
/// <returns></returns>
auto LongUI::CUIManager::create_system_brushes() noexcept -> HRESULT {
    HRESULT hr = S_OK;
    // 透明表示笔刷 - 杂色间隔
    if (SUCCEEDED(hr)) {
        // 使用位图
        ID2D1Bitmap* pBitmap = nullptr;
        // 创建位图呈现器
        if (SUCCEEDED(hr)) {
            uint32_t buffer[TARGET_BITMAP_SIZE * TARGET_BITMAP_SIZE];
            static_assert(sizeof(buffer) < 32 * 1024, "buffer too large, set it to heap");
            // 填充缓存
            {
                // A. 透明表示色
                constexpr uint32_t size1 = TRANSPARENT_SIZE*TRANSPARENT_COUNT;
                constexpr uint32_t count1 = size1*size1;
                for (uint32_t i = 0; i < count1; ++i) {
                    uint32_t x = i % size1;
                    uint32_t y = i / size1;
                    constexpr uint32_t ox = TRANSPARENT_OFFSET_X;
                    constexpr uint32_t oy = TRANSPARENT_OFFSET_Y;
                    uint32_t index = (x + ox) + (y + oy) *  TARGET_BITMAP_SIZE;
                    index[buffer] = (x / TRANSPARENT_SIZE == y / TRANSPARENT_SIZE) ?
                        TRANSPARENT_COLOR1 : TRANSPARENT_COLOR2;
                }
                // B. 焦点表示色
                constexpr uint32_t count2 = FOCUSED_SIZE*FOCUSED_SIZE;
                for (uint32_t i = 0; i < count2; ++i) {
                    uint32_t x = i % FOCUSED_SIZE;
                    uint32_t y = i / FOCUSED_SIZE;
                    constexpr uint32_t ox = FOCUSED_OFFSET_X;
                    constexpr uint32_t oy = FOCUSED_OFFSET_Y;
                    uint32_t index = (x + ox) + (y + oy) *  TARGET_BITMAP_SIZE;
                    index[buffer] = ((x/2) & 1) == ((y/2) & 1)  ?
                        FOCUSED_COLOR1 : FOCUSED_COLOR2;
                }
            }
            // 创建w位图
            hr = m_pd2dDeviceContext->CreateBitmap(
                D2D1_SIZE_U { TARGET_BITMAP_SIZE, TARGET_BITMAP_SIZE },
                buffer, TARGET_BITMAP_SIZE * sizeof(buffer[0]),
                D2D1::BitmapProperties1(
                    D2D1_BITMAP_OPTIONS_NONE,
                    D2D1::PixelFormat(
                        DXGI_FORMAT_B8G8R8A8_UNORM, 
                        D2D1_ALPHA_MODE_PREMULTIPLIED
                    )
                ),
                &m_pd2dBrushTarget
            );
        }
        // 创建透明显示色笔刷
        if (SUCCEEDED(hr)) {
            // 设置参数
            D2D1_IMAGE_BRUSH_PROPERTIES bbp;
            bbp.sourceRectangle = { 
                0.f, 0.f,
                float(TRANSPARENT_SIZE * TRANSPARENT_COUNT),
                float(TRANSPARENT_SIZE * TRANSPARENT_COUNT)
            };
            bbp.extendModeX = D2D1_EXTEND_MODE_WRAP;
            bbp.extendModeY = D2D1_EXTEND_MODE_WRAP;
            bbp.interpolationMode = D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
            // 正式创建
            if (SUCCEEDED(hr)) {
                bbp.sourceRectangle.left   += float(TRANSPARENT_OFFSET_X);
                bbp.sourceRectangle.top    += float(TRANSPARENT_OFFSET_Y);
                bbp.sourceRectangle.right  += float(TRANSPARENT_OFFSET_X);
                bbp.sourceRectangle.bottom += float(TRANSPARENT_OFFSET_Y);
                hr = m_pd2dDeviceContext->CreateImageBrush(
                    m_pd2dBrushTarget, &bbp, nullptr, &m_pTransparentBrush
                );
            }
            // 正式创建
            if (SUCCEEDED(hr)) {
                bbp.sourceRectangle.left   += float(FOCUSED_OFFSET_X - TRANSPARENT_OFFSET_X);
                bbp.sourceRectangle.top    += float(FOCUSED_OFFSET_Y - TRANSPARENT_OFFSET_Y);
                bbp.sourceRectangle.right  += float(FOCUSED_OFFSET_X - TRANSPARENT_OFFSET_X);
                bbp.sourceRectangle.bottom += float(FOCUSED_OFFSET_Y - TRANSPARENT_OFFSET_Y);
                bbp.interpolationMode = D2D1_INTERPOLATION_MODE_LINEAR;
                hr = m_pd2dDeviceContext->CreateImageBrush(
                    m_pd2dBrushTarget, &bbp, nullptr, &m_pFocusedBrush
                );
            }
            /*// 正式创建
            if (SUCCEEDED(hr)) {
                m_pFocusedBrush->SetTransform(D2D1::Matrix3x2F::Scale(2.f, 2.f));
            }*/
        }
    }
    /*
    焦点: 0x3399FF 矩形描边, 并且内边有虚线矩形
        0. 禁用: 0xBF灰度 矩形描边; 中心 0xCC灰色
        1. 普通: 0xAC灰度 矩形描边; 中心 从上到下0xF0灰色到0xE5灰色渐变
        2. 移上: 0x7EB4EA 矩形描边; 中心 从上到下0xECF4FC到0xDCECFC渐变
        3. 按下: 0x569DE5 矩形描边; 中心 从上到下0xDAECFC到0xC4E0FC渐变
    */
    // 禁用
    if (SUCCEEDED(hr)) {
        hr = m_pd2dDeviceContext->CreateSolidColorBrush(
            D2D1::ColorF(0xCCCCCC),
            reinterpret_cast<ID2D1SolidColorBrush**>(m_apSystemBrushes + State_Disabled)
        );
    }
    // 普通
    if (SUCCEEDED(hr)) {
        ID2D1GradientStopCollection* collection = nullptr;
        D2D1_GRADIENT_STOP stops[] = {
            { 0.f, D2D1::ColorF(0xF0F0F0) },
            { 1.f, D2D1::ColorF(0xE5E5E5) }
        };
        // 渐变关键点集
        if (SUCCEEDED(hr)) {
            hr = m_pd2dDeviceContext->CreateGradientStopCollection(
                stops, lengthof<uint32_t>(stops), &collection
            );
        }
        // 创建笔刷
        if (SUCCEEDED(hr)) {
            hr = m_pd2dDeviceContext->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(
                    D2D1_POINT_2F{0.f}, D2D1_POINT_2F{0.f, 1.f}
                ),
                collection,
                reinterpret_cast<ID2D1LinearGradientBrush**>(m_apSystemBrushes + State_Normal)
            );
        }
        LongUI::SafeRelease(collection);
    }
    // 移上
    if (SUCCEEDED(hr)) {
        ID2D1GradientStopCollection* collection = nullptr;
        D2D1_GRADIENT_STOP stops[] = {
            { 0.f, D2D1::ColorF(0xECF4FC) },
            { 1.f, D2D1::ColorF(0xDCECFC) }
        };
        // 渐变关键点集
        if (SUCCEEDED(hr)) {
            hr = m_pd2dDeviceContext->CreateGradientStopCollection(
                stops, lengthof<uint32_t>(stops), &collection
            );
        }
        // 创建笔刷
        if (SUCCEEDED(hr)) {
            hr = m_pd2dDeviceContext->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(
                    D2D1_POINT_2F{0.f}, D2D1_POINT_2F{0.f, 1.f}
                ),
                collection,
                reinterpret_cast<ID2D1LinearGradientBrush**>(m_apSystemBrushes + State_Hover)
            );
        }
        LongUI::SafeRelease(collection);
    }
    // 按下
    if (SUCCEEDED(hr)) {
        ID2D1GradientStopCollection* collection = nullptr;
        D2D1_GRADIENT_STOP stops[] = {
            { 0.f, D2D1::ColorF(0xDAECFC) } ,
            { 1.f, D2D1::ColorF(0xC4E0FC) } ,
        };
        // 渐变关键点集
        if (SUCCEEDED(hr)) {
            hr = m_pd2dDeviceContext->CreateGradientStopCollection(
                stops, lengthof<uint32_t>(stops), &collection
            );
        }
        // 创建笔刷
        if (SUCCEEDED(hr)) {
            hr = m_pd2dDeviceContext->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(
                    D2D1_POINT_2F{0.f}, D2D1_POINT_2F{0.f, 1.f}
                ),
                collection,
                reinterpret_cast<ID2D1LinearGradientBrush**>(m_apSystemBrushes + State_Pushed)
            );
        }
        LongUI::SafeRelease(collection);
    }
    return hr;
}

#if defined(_DEBUG) && defined(_MSC_VER)
extern ID3D11Debug*    g_pd3dDebug_longui;
#endif

/// <summary>
/// Discard_resourceses this instance.
/// 丢弃设备资源
/// </summary>
/// <returns></returns>
void LongUI::CUIManager::discard_resources() noexcept {
    // 释放系统笔刷
    for (auto& brush : m_apSystemBrushes) {
        LongUI::SafeRelease(brush);
    }
    LongUI::SafeRelease(m_pTransparentBrush);
    LongUI::SafeRelease(m_pd2dBrushTarget);
    LongUI::SafeRelease(m_pFocusedBrush);
    // 释放公共设备相关资源
    {
        // 释放 位图
        for (auto itr = m_ppBitmaps; itr != m_ppBitmaps + m_cCountBmp; ++itr) {
            LongUI::SafeRelease(*itr);
        }
        // 释放 笔刷
        for (auto itr = m_ppBrushes; itr != m_ppBrushes + m_cCountBrs; ++itr) {
            LongUI::SafeRelease(*itr);
        }
        // META
        for (auto itr = m_pMetasBuffer; itr != m_pMetasBuffer + m_cCountMt; ++itr) {
            impl::destory_object(*itr);
            itr->bitmap = nullptr;
        }
    }
    // 清除
    if (m_pd2dDevice) {
        m_pd2dDevice->ClearResources();
    }
    // 释放 设备
    LongUI::SafeRelease(m_pDxgiFactory);
    LongUI::SafeRelease(m_pd2dDeviceContext);
    LongUI::SafeRelease(m_pd2dDevice);
    LongUI::SafeRelease(m_pDxgiAdapter);
    LongUI::SafeRelease(m_pDxgiOutput);
    LongUI::SafeRelease(m_pDxgiDevice);
    LongUI::SafeRelease(m_pd3dDevice);
    LongUI::SafeRelease(m_pd3dDeviceContext);
#ifdef LONGUI_WITH_MMFVIDEO
    LongUI::SafeRelease(m_pMFDXGIManager);
    LongUI::SafeRelease(m_pMediaEngineFactory);
    ::MFShutdown();
#endif
#ifdef _DEBUG
    if (m_pd3dDebug) {
        if (const auto count = m_pd3dDebug->Release()) {
            m_pd3dDebug->ReportLiveDeviceObjects(D3D11_RLDO_SUMMARY);
        }
        m_pd3dDebug = nullptr;
    }
#endif
}

/// <summary>
/// Gets the bitmap via index.
/// 获取位图
/// </summary>
/// <param name="index">The index.</param>
/// <returns></returns>
auto LongUI::CUIManager::GetBitmap(size_t index) noexcept ->ID2D1Bitmap1* {
    // 越界
    if (index >= m_cCountBmp) {
        UIManager << DL_Warning
            << L"[index @ " << long(index)
            << L"]is out of range \t\tNow set to 0"
            << LongUI::endl;
        index = 0;
    }
    auto bitmap = m_ppBitmaps[index];
    // 没有数据则载入
    if (!bitmap) {
        // 没有数据并且没有资源加载器则?
        assert(m_pResourceLoader);
        // 载入资源
        m_ppBitmaps[index] = static_cast<ID2D1Bitmap1*>(
            m_pResourceLoader->GetResourcePointer(m_pResourceLoader->Type_Bitmap, index - 1)
            );
        bitmap = m_ppBitmaps[index];
    }
    // 再没有数据则报错
    if (!bitmap) {
        UIManager << DL_Error << L"index @ " << long(index) << L"bitmap is null" << LongUI::endl;
    }
    return LongUI::SafeAcquire(bitmap);
}

/// <summary>
/// Gets the brush.
/// 获取笔刷
/// </summary>
/// <param name="index">The index.</param>
/// <returns></returns>
auto LongUI::CUIManager::GetBrush(size_t index) noexcept -> ID2D1Brush* {
    // 越界
    if (index >= m_cCountBrs) {
        UIManager << DL_Warning
            << L"[index @ " << long(index)
            << L"]is out of range \t\tNow set to 0"
            << LongUI::endl;
        index = 0;
    }
    auto brush = m_ppBrushes[index];
    // 没有数据则载入
    if (!brush) {
        // 没有数据并且没有资源加载器则?
        assert(m_pResourceLoader);
        // 载入资源
        m_ppBrushes[index] = static_cast<ID2D1Brush*>(
            m_pResourceLoader->GetResourcePointer(m_pResourceLoader->Type_Brush, index - 1)
            );
        brush = m_ppBrushes[index];
    }
    // 再没有数据则报错
    if (!brush) {
        UIManager << DL_Error << L"index @ " << long(index) << L"brush is null" << LongUI::endl;
    }
    return LongUI::SafeAcquire(brush);
}

/// <summary>
/// Gets the text format.
/// 获取文本格式
/// </summary>
/// <param name="index">The index.</param>
/// <returns></returns>
auto LongUI::CUIManager::GetTextFormat(size_t index) noexcept ->IDWriteTextFormat* {
    // 越界
    if (index >= m_cCountTf) {
        UIManager << DL_Warning
            << L"[index @ " << long(index)
            << L"]is out of range \t\tNow set to 0"
            << LongUI::endl;
        index = 0;
    }
    auto format = m_ppTextFormats[index];
    // 没有数据则载入
    if (!format) {
        // 没有数据并且没有资源加载器则?
        assert(m_pResourceLoader);
        // 载入资源
        m_ppTextFormats[index] = static_cast<IDWriteTextFormat*>(
            m_pResourceLoader->GetResourcePointer(m_pResourceLoader->Type_TextFormat, index - 1)
            );
        format = m_ppTextFormats[index];
    }
    // 再没有数据则报错
    if (!format) {
        UIManager << DL_Error << L"index @ " << long(index) << L"text format is null" << LongUI::endl;
    }
    return LongUI::SafeAcquire(format);
}

/// <summary>
/// Gets the text renderer.
/// 利用名称获取
/// </summary>
/// <param name="name">The name.</param>
/// <returns></returns>
auto LongUI::CUIManager::GetTextRenderer(const char* name) const noexcept -> XUIBasicTextRenderer* {
    int index = 0;
    // 字符串有效情况下
    if (name && name[0]) {
        // 跳过空白
        while (white_space(*name)) ++name;
        // 检查数字
        if (*name >= '0' && *name <= '9') {
            index = LongUI::AtoI(name);
        }
        // 线性查找
        else {
            for (int i = 0; i < int(m_uTextRenderCount); ++i) {
                if (!std::strcmp(m_aszTextRendererName[i].name, name)) {
                    index = i;
                    break;
                }
            }
        }
    }
    return this->GetTextRenderer(index);
}

/// <summary>
/// Gets the meta.
/// 获取图元
/// </summary>
/// <param name="index">The index.</param>
/// <param name="meta">The meta.</param>
/// <returns></returns>
void LongUI::CUIManager::GetMeta(size_t index, LongUI::Meta& meta) noexcept {
    // 越界
    if (index >= m_cCountMt) {
        UIManager << DL_Warning
            << L"[index @ " << long(index)
            << L"]is out of range \t\tNow set to 0"
            << LongUI::endl;
        index = 0;
        std::memset(&meta, 0, sizeof(meta));
        return;
    }
    meta = m_pMetasBuffer[index];
    // 没有位图数据则载入
    if (!meta.bitmap) {
        // 没有数据并且没有资源加载器则?
        assert(m_pResourceLoader);
        DeviceIndependentMeta meta_raw;
        ::std::memset(&meta_raw, 0, sizeof(meta_raw));
        // 载入资源
        m_pResourceLoader->GetMeta(index - 1, meta_raw);
        meta.interpolation = meta_raw.interpolation;
        meta.src_rect = meta_raw.src_rect;
        meta.rule = meta_raw.rule;
        meta.bitmap = this->GetBitmap(meta_raw.bitmap_index);
        // 减少计数
        if (meta.bitmap) {
            meta.bitmap->Release();
        }
    }
    // 再没有数据则报错
    if (!meta.bitmap) {
        UIManager << DL_Error << L"index @ " << long(index) << L"meta is null" << LongUI::endl;
    }
}

/// <summary>
/// Gets the meta hicon.
/// 获取Meta的图标句柄
/// </summary>
/// <param name="index">The index.</param>
/// <returns></returns>
auto LongUI::CUIManager::GetMetaHICON(size_t index) noexcept -> HICON {
    // 越界
    if (index >= m_cCountMt) {
        UIManager << DL_Warning
            << L"[index @ " << long(index)
            << L"]is out of range \t\tNow set to 0"
            << LongUI::endl;
        index = 0;
    }
    // 有就直接返回
    if (m_phMetaIcon[index]) return m_phMetaIcon[index];
    LongUI::Meta meta; this->GetMeta(index, meta);
    assert(meta.bitmap);
    ID2D1Bitmap1* bitmap = this->GetBitmap(LongUIDefaultBitmapIndex);
    D2D1_RECT_U src_rect = {
        static_cast<uint32_t>(meta.src_rect.left),
        static_cast<uint32_t>(meta.src_rect.top),
        static_cast<uint32_t>(meta.src_rect.right),
        static_cast<uint32_t>(meta.src_rect.bottom)
    };
    HRESULT hr = S_OK;
    // 大小保证
    if (SUCCEEDED(hr)) {
        if (src_rect.right - src_rect.left > LongUIDefaultBitmapSize ||
            src_rect.bottom - src_rect.top > LongUIDefaultBitmapSize) {
            assert(!"width/height is too large");
            hr = E_FAIL;
        }
    }
    // 检查错误
    if (SUCCEEDED(hr)) {
        if (!(bitmap && meta.bitmap)) {
            hr = E_POINTER;
        }
    }
    // 复制数据
    if (SUCCEEDED(hr)) {
        hr = bitmap->CopyFromBitmap(nullptr, meta.bitmap, &src_rect);
        longui_debug_hr(hr, L"bitmap->CopyFromBitmap failed");
    }
    // 映射数据
    if (SUCCEEDED(hr)) {
        D2D1_MAPPED_RECT mapped_rect = {
            LongUIDefaultBitmapSize * sizeof(RGBQUAD) ,
            m_pBitmap0Buffer
        };
        hr = bitmap->Map(D2D1_MAP_OPTIONS_READ, &mapped_rect);
        longui_debug_hr(hr, L"bitmap->Map failed");
    }
    // 取消映射
    if (SUCCEEDED(hr)) {
        hr = bitmap->Unmap();
        longui_debug_hr(hr, L"bitmap->Unmap failed");
    }
    // 转换数据
    HICON hAlphaIcon = nullptr;
    if (SUCCEEDED(hr)) {
        auto meta_width = src_rect.right - src_rect.left;
        auto meta_height = src_rect.bottom - src_rect.top;
#if 1
        BITMAPV5HEADER bi; std::memset(&bi, 0, sizeof(BITMAPV5HEADER));
        bi.bV5Size = sizeof(BITMAPV5HEADER);
        bi.bV5Width = meta_width;
        bi.bV5Height = meta_height;
        bi.bV5Planes = 1;
        bi.bV5BitCount = 32;
        bi.bV5Compression = BI_BITFIELDS;
        // 掩码填写
        bi.bV5RedMask = 0x00FF0000;
        bi.bV5GreenMask = 0x0000FF00;
        bi.bV5BlueMask = 0x000000FF;
        bi.bV5AlphaMask = 0xFF000000;
        HDC hdc = ::GetDC(nullptr);
        uint8_t* pTargetBuffer = nullptr;
        // 创建带Alpha通道DIB
        auto hBitmap = ::CreateDIBSection(
            hdc, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS,
            reinterpret_cast<void **>(&pTargetBuffer), nullptr,
            (DWORD)0
        );
        // 错误
        if (!hBitmap) {
            hr = LongUI::WinCode2HRESULT(::GetLastError());
            longui_debug_hr(hr, L"CreateDIBSection failed");
        }
        // 成功
        else {
            auto hMemDC = ::CreateCompatibleDC(hdc);
            ::ReleaseDC(nullptr, hdc);
            // 写入数据
            auto hOldBitmap = static_cast<HBITMAP*>(::SelectObject(hMemDC, hBitmap));
            ::PatBlt(hMemDC, 0, 0, meta_width, meta_height, WHITENESS);
            ::SelectObject(hMemDC, hOldBitmap);
            ::DeleteDC(hMemDC);
            // 创建掩码位图
            HBITMAP hMonoBitmap = ::CreateBitmap(meta_width, meta_height, 1, 1, nullptr);
            // 输入
            auto lpdwPixel = reinterpret_cast<DWORD*>(pTargetBuffer);
            for (auto y = 0u; y < meta_height; ++y) {
                auto src_buffer = m_pBitmap0Buffer + LongUIDefaultBitmapSize * sizeof(RGBQUAD) * y;
                for (auto x = 0u; x < meta_width; ++x) {
                    *lpdwPixel = *src_buffer;
                    src_buffer++;
                    lpdwPixel++;
                }
            }
            // 填写
            ICONINFO ii;
            ii.fIcon = TRUE; ii.xHotspot = 0; ii.yHotspot = 0;
            ii.hbmMask = hMonoBitmap; ii.hbmColor = hBitmap;
            // 创建图标
            hAlphaIcon = ::CreateIconIndirect(&ii);
            ::DeleteObject(hBitmap);
            ::DeleteObject(hMonoBitmap);
#else
        assert(!"CreateIcon just AND & XOR, no alpha channel");
#endif
        }
    }
    // 错误
    if (FAILED(hr)) {
        this->ShowError(hr);
    }
    LongUI::SafeRelease(bitmap);
    return m_phMetaIcon[index] = hAlphaIcon;
}



/// <summary>
/// Adds the window.
/// 添加窗口
/// </summary>
/// <param name="wnd">The WND.</param>
/// <returns></returns>
void LongUI::CUIManager::AddWindow(XUISystemWindow* wnd) noexcept {
    assert(wnd && "bad argument");
    // 检查是否已经存在
#ifdef _DEBUG
    {
        auto end = m_vWindows.end();
        if (std::find(m_vWindows.begin(), m_vWindows.end(), wnd) != end) {
            assert(!"target window has been registered.");
        }
    }
#endif
    // 添加窗口
    m_vWindows.push_back(wnd);
}

/// <summary>
/// Refresh_display_frequencies this instance.
/// 刷新屏幕刷新率
/// </summary>
/// <returns></returns>
void LongUI::CUIManager::refresh_display_frequency() noexcept {
    // 获取屏幕刷新率
    DEVMODEW mode; std::memset(&mode, 0, sizeof(mode));
    ::EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &mode);
    m_dDisplayFrequency = static_cast<uint16_t>(mode.dmDisplayFrequency);
    // 稍微检查
    if (!m_dDisplayFrequency) {
        UIManager << DL_Error
            << L"EnumDisplaySettingsW failed: got zero for DEVMODEW::dmDisplayFrequency"
            << L", now assume as 60Hz"
            << LongUI::endl;
        assert(!"TODO");
        m_dDisplayFrequency = 60;
    }
}


/// <summary>
/// 刷新时间胶囊
/// </summary>
/// <param name="time">The time.</param>
/// <returns></returns>
void LongUI::CUIManager::update_time_capsules(float time) noexcept {
    uint32_t deleted = 0;
    // 遍历时间胶囊
    for (auto& capsule : m_vTimeCapsules) {
        assert(capsule && "bad time capsule");
        if (capsule->Update(time)) {
            capsule->Dispose();
            capsule = nullptr;
            ++deleted;
        }
    }
    // 检查释放
    if (deleted) {
        uint32_t count = m_vTimeCapsules.size();
        // 全部删除
        if (count == deleted) {
            m_vTimeCapsules.clear();
        }
        // 部分删除
        else {
            const auto data = m_vTimeCapsules.data();
            auto last = data + (count - 1);
            auto itr = data;
            do {
                // 空 -> 与最后一个交换
                if (!(*itr)) {
                    std::swap(*itr, *last);
                    --last;
                }
                // 非空 -> 继续
                else {
                    ++itr;
                }
            } while (itr != last);
            // 删除后面的
            m_vTimeCapsules.resize(count - deleted);
        }
    }
}

/// <summary>
/// Remove_time_capsules the specified identifier.
/// 移除时间胶囊
/// </summary>
/// <param name="id">The identifier.</param>
/// <returns></returns>
void LongUI::CUIManager::remove_time_capsule(void* id) noexcept {
    size_t realid = reinterpret_cast<size_t>(id);
    for (auto itr = m_vTimeCapsules.begin(); itr != m_vTimeCapsules.end(); ++itr) {
        auto capsule = *itr;
        if (capsule->GetId() == realid) {
            capsule->Dispose();
            m_vTimeCapsules.erase(itr);
            return;
        }
    }
}

/// <summary>
/// Push_time_capsules the specified call.
/// 添加时间胶囊
/// </summary>
/// <param name="call">The call.</param>
/// <param name="id">The identifier.</param>
/// <param name="time">The time.</param>
/// <returns></returns>
void LongUI::CUIManager::push_time_capsule(TimeCapsuleCall && call, void* id, float time) noexcept {
    const size_t realid = reinterpret_cast<size_t>(id);
    // 有效情况下
    if (m_vTimeCapsules.isok()) {
        // 创建胶囊
        auto capsule = CUITimeCapsule::Create(std::move(call), realid, time);
        // 创建失败
        if (!capsule) return;
        CUITimeCapsule** included = nullptr;
        // 遍历检查是否已经拥有
        for (auto& capsule : m_vTimeCapsules) {
            if (capsule->GetId() == realid) {
                included = &capsule;
                break;
            }
        }
        // 有的情况
        if (included) {
            (*included)->Dispose();
            *(included) = capsule;
            UIManager << DL_Log << "new capsule insteaded" << LongUI::endl;
        }
        else {
            m_vTimeCapsules.push_back(capsule);
        }
    }
}

/// <summary>
/// Removes the window.
/// 移除窗口
/// </summary>
/// <param name="wnd">The WND.</param>
/// <returns></returns>
void LongUI::CUIManager::RemoveWindow(XUISystemWindow* wnd) noexcept {
    // 检查时是不是在本数组中
    // 正式移除
    {
        auto end = m_vWindows.end();
        auto itr = std::find(m_vWindows.begin(), end, wnd);
        if (itr != end) m_vWindows.erase(itr);
    }
    // 再次检查时是不是在本数组中
#ifdef _DEBUG
    {
        auto end = m_vWindows.end();
        if (std::find(m_vWindows.begin(), end, wnd) != end) {
            assert(!"target window not in windows vector!");
            return;
        }
    }
#endif
}

/*/ 是否以管理员权限运行
bool LongUI::CUIManager::IsRunAsAdministrator() noexcept {
    BOOL fIsRunAsAdmin = FALSE;
    PSID pAdministratorsGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    ::AllocateAndInitializeSid(
        &NtAuthority,
        2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0,
        &pAdministratorsGroup
    );
    // 成功? 清理资源
    if (pAdministratorsGroup) {
        ::CheckTokenMembership(nullptr, pAdministratorsGroup, &fIsRunAsAdmin);
        ::FreeSid(pAdministratorsGroup);
        pAdministratorsGroup = nullptr;
    }
    // 返回结果
    return fIsRunAsAdmin != 0;
}

// 提升权限
bool LongUI::CUIManager::TryElevateUACNow(const wchar_t* parameters, bool exit) noexcept {
    if (!CUIManager::IsRunAsAdministrator()) {
        wchar_t szPath[MAX_PATH];
        // 获取实例句柄
        if (::GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath))) {
            // Launch itself as admin
            SHELLEXECUTEINFOW sei = { 0 };
            sei.cbSize = sizeof(sei);
            sei.lpVerb = L"runas";
            sei.lpFile = szPath;
            sei.lpParameters = parameters;
            sei.hwnd = nullptr;
            sei.nShow = SW_NORMAL;
            // 执行
            if (!::ShellExecuteExW(&sei)) {
#ifdef _DEBUG
                DWORD dwError = ::GetLastError();
                assert(dwError == ERROR_CANCELLED && "anyelse?");
#endif
                return false;
            }
            else if (exit) {
                // 退出
                UIManager.Exit();
            }
        }
    }
    return true;
}*/

#ifdef _DEBUG

/// <summary>
/// Formateds the specified format.
/// 传递可视化东西
/// </summary>
/// <param name="format">The format.</param>
/// <param name="">The .</param>
/// <returns></returns>
auto LongUI::Formated(const wchar_t* format, ...) noexcept -> const wchar_t* {
    static thread_local wchar_t buffer[2048];
    va_list ap;
    va_start(ap, format);
    std::vswprintf(buffer, lengthof(buffer), format, ap);
    va_end(ap);
    return buffer;
}

/// <summary>
/// Interfmts the specified format.
/// 传递可视化东西
/// </summary>
/// <param name="format">The format.</param>
/// <param name="">The .</param>
/// <returns></returns>
auto LongUI::Interfmt(const wchar_t* format, ...) noexcept -> const wchar_t* {
    static thread_local wchar_t buffer[2048];
    va_list ap;
    va_start(ap, format);
    std::vswprintf(buffer, lengthof(buffer), format, ap);
    va_end(ap);
    return buffer;
}

/// <summary>
/// Operators the specified .
/// 换行刷新重载
/// </summary>
/// <param name="">The .</param>
/// <returns></returns>
auto LongUI::CUIManager::operator<<(const LongUI::EndL&) noexcept ->CUIManager& {
    wchar_t chs[3] = { L'\r',L'\n', 0 };
    this->Output(m_lastLevel, chs);
    return *this;
}

/// <summary>
/// Operators the specified desc.
/// </summary>
/// <param name="desc">The desc.</param>
/// <returns></returns>
auto LongUI::CUIManager::operator<<(const DXGI_ADAPTER_DESC& desc) noexcept ->CUIManager& {
    CUIString str;
    str.Format(
        L"Adapter:   { \r\n\t Description: %ls\r\n\t VendorId: 0x%08X"
        L"\t\t DeviceId: 0x%08X\r\n\t SubSysId: 0x%08X\t\t Revision: 0x%08X\r\n"
        L"\t DedicatedVideoMemory: %.3lfMB\r\n"
        L"\t DedicatedSystemMemory: %.3lfMB\r\n"
        L"\t SharedSystemMemory: %.3lfMB\r\n"
        L"\t AdapterLuid: 0x%08X%08X\r\n }",
        desc.Description,
        desc.VendorId,
        desc.DeviceId,
        desc.SubSysId,
        desc.Revision,
        static_cast<double>(desc.DedicatedVideoMemory) / (1024.*1024.),
        static_cast<double>(desc.DedicatedSystemMemory) / (1024.*1024.),
        static_cast<double>(desc.SharedSystemMemory) / (1024.*1024.),
        desc.AdapterLuid.HighPart,
        desc.AdapterLuid.LowPart
    );
    this->OutputNoFlush(m_lastLevel, str.c_str());
    return *this;
}

/// <summary>
/// Operators the specified rect.
/// </summary>
/// <param name="rect">The rect.</param>
/// <returns></returns>
auto LongUI::CUIManager::operator<<(const RectLTWH_F& rect) noexcept ->CUIManager& {
    CUIString str;
    str.Format(
        L"RECT_WH(%7.2f, %7.2f, %7.2f, %7.2f)",
        rect.left, rect.top, rect.width, rect.height
    );
    this->OutputNoFlush(m_lastLevel, str.c_str());
    return *this;
}

/// <summary>
/// Operators the specified matrix.
/// </summary>
/// <param name="matrix">The matrix.</param>
/// <returns></returns>
auto LongUI::CUIManager::operator<<(const D2D1_MATRIX_3X2_F& matrix) noexcept ->CUIManager& {
    CUIString str;
    str.Format(
        L"MATRIX (%7.2f, %7.2f, %7.2f, %7.2f, %7.2f, %7.2f)",
        matrix._11, matrix._12,
        matrix._21, matrix._22,
        matrix._31, matrix._32
    );
    this->OutputNoFlush(m_lastLevel, str.c_str());
    return *this;
}

/// <summary>
/// Operators the specified rect.
/// </summary>
/// <param name="rect">The rect.</param>
/// <returns></returns>
auto LongUI::CUIManager::operator<<(const D2D1_RECT_F& rect) noexcept ->CUIManager& {
    CUIString str;
    str.Format(
        L"RECT_RB(%7.2f, %7.2f, %7.2f, %7.2f)",
        rect.left, rect.top, rect.right, rect.bottom
    );
    this->OutputNoFlush(m_lastLevel, str.c_str());
    return *this;
}

/// <summary>
/// Operators the specified pt.
/// </summary>
/// <param name="pt">The pt.</param>
/// <returns></returns>
auto LongUI::CUIManager::operator<<(const D2D1_POINT_2F& pt) noexcept ->CUIManager& {
    CUIString str;
    str.Format(L"POINT(%7.2f, %7.2f)", pt.x, pt.y);
    this->OutputNoFlush(m_lastLevel, str.c_str());
    return *this;
}

/// <summary>
/// Outputs the specified l.
/// 输出UTF-8字符串 并刷新
/// </summary>
/// <param name="l">The l.</param>
/// <param name="s">The s.</param>
/// <returns></returns>
void LongUI::CUIManager::Output(DebugStringLevel l, const char* s) noexcept {
    LongUI::SafeUTF8toWideChar(s, [this, l](const wchar_t* begin, const wchar_t*) {
        this->Output(l, begin);
    });
}

/// <summary>
/// Outputs the no flush.
/// 输出UTF-8字符串
/// </summary>
/// <param name="l">The l.</param>
/// <param name="s">The s.</param>
/// <returns></returns>
void LongUI::CUIManager::OutputNoFlush(DebugStringLevel l, const char* s) noexcept {
    LongUI::SafeUTF8toWideChar(s, [this, l](const wchar_t* begin, const wchar_t*) {
        this->OutputNoFlush(l, begin);
    });
}

/// <summary>
/// Operators the specified f.
/// 浮点重载
/// </summary>
/// <param name="f">The f.</param>
/// <returns></returns>
auto LongUI::CUIManager::operator<<(const float f) noexcept ->CUIManager& {
    CUIString str;
    str.Format(L"%.2f", f);
    this->OutputNoFlush(m_lastLevel, str.c_str());
    return *this;
}

/// <summary>
/// Operators the specified control.
/// 指针
/// </summary>
/// <param name="ctrl">The control.</param>
/// <returns></returns>
auto LongUI::CUIManager::operator<<(const void* ctrl) noexcept ->CUIManager& {
    CUIString str;
    str.Format(L"[0x%p] ", ctrl);
    this->OutputNoFlush(m_lastLevel, str.c_str());
    return *this;
}

/// <summary>
/// Operators the specified control.
/// 控件
/// </summary>
/// <param name="ctrl">The control.</param>
/// <returns></returns>
auto LongUI::CUIManager::operator<<(const UIControl* ctrl) noexcept ->CUIManager& {
    CUIString str;
    if (ctrl) str.Format(
            L"[0x%p{%S}%ls] ",
            ctrl,
            ctrl->name.c_str(),
            ctrl->GetControlClassName(false)
        );
    else str = L"[null]";
    this->OutputNoFlush(m_lastLevel, str.c_str());
    return *this;
}

/// <summary>
/// Operators the specified CTRLS.
/// 控件数组
/// </summary>
/// <param name="ctrls">The CTRLS.</param>
/// <returns></returns>
auto LongUI::CUIManager::operator<<(const ControlVector& ctrls) noexcept ->CUIManager& {
    CUIString str;
    int index = 0;
    for (auto ctrl : ctrls) {
        str.Format(
            L"\r\n\t\t[%4d][0x%p{%S}%ls] ",
            index,
            ctrl,
            ctrl->name.c_str(),
            ctrl->GetControlClassName(false)
        );
        this->OutputNoFlush(m_lastLevel, str.c_str());
        ++index;
    }
    return *this;
}

/// <summary>
/// Operators the specified l.
/// 整型输出重载
/// </summary>
/// <param name="l">The l.</param>
/// <returns></returns>
auto LongUI::CUIManager::operator<<(const long l) noexcept ->CUIManager& {
    CUIString str;
    str.Format(L"%ld", l);
    this->OutputNoFlush(m_lastLevel, str.c_str());
    return *this;
}

/// <summary>
/// Operators the specified b.
/// 布尔输出重载
/// </summary>
/// <param name="b">The b.</param>
/// <returns></returns>
auto LongUI::CUIManager::operator<<(const bool b) noexcept ->CUIManager& {
    this->OutputNoFlush(m_lastLevel, b ? "true" : "false");
    return *this;
}

#endif