﻿#include "Core/luiManager.h"
#include "Control/UIComboBox.h"
#include "Control/UIList.h"
#include "Control/UIScrollBar.h"
#include "Control/UIColor.h"
#include "Control/UIRamBitmap.h"

#ifdef LongUIDebugEvent
#include "Control/UIEdit.h"
#include "Control/UISlider.h"
#include "Control/UIPage.h"
#include "Control/UISingle.h"
#include "Control/UICheckBox.h"
#include "Control/UIRadioButton.h"
#include "Control/UIFloatLayout.h"
#endif

#include <algorithm>

// ----------------------------------------------------------------------------
// **** UIColor
// ----------------------------------------------------------------------------

/// <summary>
/// Render_chain_backgrounds this instance.
/// </summary>
/// <returns></returns>
void LongUI::UIColor::render_chain_background() const noexcept {
    // 渲染父类背景?
    //Super::render_chain_background();
    // 渲染基本色
    D2D1_RECT_F rect; this->GetViewRect(rect);
    // 无需渲染背景
    if (m_color.a < 1.f) {
        // 直接透明
        if (this->is_direct_transparent()) {
            constexpr auto mode = D2D1_ANTIALIAS_MODE_ALIASED;
            UIManager_RenderTarget->PushAxisAlignedClip(&rect, mode);
            UIManager_RenderTarget->Clear(D2D1_COLOR_F{ 0.f,0.f,0.f,0.f });
            UIManager_RenderTarget->PopAxisAlignedClip();
        }
        // 透明表示色
        else {
            UIManager_RenderTarget->FillRectangle(
                &rect, UIManager.RefTransparentBrush()
            );
        }
    }
    // 渲染背景前部分
    auto brush = m_pBrush_SetBeforeUse;
    brush->SetColor(&m_color);
    UIManager_RenderTarget->FillRectangle(&rect, brush);
}


/// <summary>
/// Initializes the specified node.
/// </summary>
/// <param name="node">The node.</param>
/// <returns></returns>
void LongUI::UIColor::initialize(pugi::xml_node node) noexcept {
    // 链式初始化
    Super::initialize(node);
    // 获取颜色
    if (const auto c = node.attribute("color").value()) {
        Helper::MakeColor(c, m_color);
    }
    // 获取颜色
    if (node.attribute("direct").as_bool()) {
        this->SetDirectTransparent();
    }
}

/// <summary>
/// Cleanups this instance.
/// </summary>
/// <returns></returns>
void LongUI::UIColor::cleanup() noexcept {
    this->before_deleted();
    delete this;
}

/// <summary>
/// Renders this instance.
/// </summary>
/// <returns></returns>
void LongUI::UIColor::Render() const noexcept {
    // 背景渲染
    this->render_chain_background();
    // 主景渲染
    this->render_chain_main();
    // 前景渲染
    this->render_chain_foreground();
}

/// <summary>
/// Creates the control.
/// </summary>
/// <param name="">The .</param>
/// <param name="">The .</param>
/// <returns></returns>
auto LongUI::UIColor::CreateControl(
    CreateEventType type, pugi::xml_node node) noexcept -> UIControl* {
    // 分类判断
    UIColor* pControl = nullptr;
    switch (type)
    {
    case LongUI::Type_Initialize:
        break;
    case LongUI::Type_Recreate:
        break;
    case LongUI::Type_Uninitialize:
        break;
    case_LongUI__Type_CreateControl:
        LongUI__CreateWidthCET(UIColor, pControl, type, node);
    }
    return pControl;
}


// ----------------------------------------------------------------------------
// **** UIRamBitmap
// ----------------------------------------------------------------------------


/// <summary>
/// Cleanups this instance.
/// </summary>
/// <returns></returns>
void LongUI::UIRamBitmap::cleanup() noexcept {
    this->before_deleted();
    delete this;
}


/// <summary>
/// Finalizes an instance of the <see cref="UIRamBitmap"/> class.
/// </summary>
/// <returns></returns>
LongUI::UIRamBitmap::~UIRamBitmap() noexcept {
    LongUI::SafeRelease(m_pBitmap);
    if (m_pBitmapData) {
        LongUI::NormalFree(m_pBitmapData);
        m_pBitmapData = nullptr;
    }
}

/// <summary>
/// Initializes the specified node.
/// </summary>
/// <param name="node">The node.</param>
/// <returns></returns>
void LongUI::UIRamBitmap::initialize(pugi::xml_node node) noexcept {
    // 链式初始化
    Super::initialize(node);
    // 获取位图
    if (const auto str = node.attribute("bitmapsize").value()) {
        float size[2];
        Helper::MakeFloats(str, size);
        m_szBitmap.width = uint32_t(size[0]);
        m_szBitmap.height = uint32_t(size[1]);
    }
    // 一般计算
    auto w = std::max(m_szBitmap.width, uint32_t(MIN_SIZE));
    auto h = std::max(m_szBitmap.height, uint32_t(MIN_SIZE));
    w = (w + MIN_PITCH - 1) / MIN_PITCH * MIN_PITCH;
    h = (h + MIN_PITCH - 1) / MIN_PITCH * MIN_PITCH;
    // 申请空间
    m_cPitchWidth = w;
    m_pBitmapData = LongUI::NormalAllocT<RGBA>(w*h);
}

/// <summary>
/// Render chain: render background for this instance.
/// </summary>
/// <returns></returns>
void LongUI::UIRamBitmap::render_chain_background() const noexcept {
    // 父类背景
    Super::render_chain_background();
    // 计算位置
    D2D1_RECT_F des_rect; this->GetViewRect(des_rect);
    D2D1_RECT_F src_rect {
        0.f, 0.f,
        float(m_szBitmap.width),
        float(m_szBitmap.height)
    };
    // 渲染位图
    UIManager_RenderTarget->DrawBitmap(
        m_pBitmap,
        &des_rect,
        1.f,
        D2D1_INTERPOLATION_MODE(m_modeInterpolation),
        &src_rect
    );
}

/// <summary>
/// Renders this instance.
/// </summary>
/// <returns></returns>
void LongUI::UIRamBitmap::Render() const noexcept {
    // 背景渲染
    this->render_chain_background();
    // 主景渲染
    this->render_chain_main();
    // 前景渲染
    this->render_chain_foreground();
}

/// <summary>
/// Writes the data.
/// </summary>
/// <returns></returns>
auto LongUI::UIRamBitmap::write_data() noexcept ->HRESULT {
    assert(m_pBitmap && m_pBitmapData);
    CUIDxgiAutoLocker locker;
    const auto data = m_pBitmapData;
    const auto pitch = m_cPitchWidth * uint32_t(sizeof(m_pBitmapData[0]));
    return m_pBitmap->CopyFromMemory(nullptr, data, pitch);
}

/// <summary>
/// Recreates this instance.
/// </summary>
/// <returns></returns>
auto LongUI::UIRamBitmap::Recreate() noexcept ->HRESULT {
    auto hr = S_OK;
    LongUI::SafeRelease(m_pBitmap);
    // 内存不足
    if (SUCCEEDED(hr)) {
        hr = m_pBitmapData ? S_OK : E_OUTOFMEMORY;
    }
    // 重建父类
    if (SUCCEEDED(hr)) {
        hr = Super::Recreate();
    }
    // 重建位图
    if (SUCCEEDED(hr)) {
        auto w = std::max(m_szBitmap.width, uint32_t(MIN_SIZE));
        auto h = std::max(m_szBitmap.height, uint32_t(MIN_SIZE));
        w = (w + MIN_PITCH - 1) / MIN_PITCH * MIN_PITCH;
        h = (h + MIN_PITCH - 1) / MIN_PITCH * MIN_PITCH;
        hr = UIManager_RenderTarget->CreateBitmap(
            D2D1_SIZE_U{ w, h },
            m_pBitmapData, w * sizeof(m_pBitmapData[0]),
            D2D1::BitmapProperties1(
                D2D1_BITMAP_OPTIONS_NONE,
                D2D1::PixelFormat(
                    DXGI_FORMAT_B8G8R8A8_UNORM, 
                    D2D1_ALPHA_MODE_PREMULTIPLIED
                )
            ),
            &m_pBitmap
        );
    }
    return hr;
}

/// <summary>
/// Creates the control.
/// </summary>
/// <param name="type">The type.</param>
/// <param name="node">The node.</param>
/// <returns></returns>
auto LongUI::UIRamBitmap::CreateControl(
    CreateEventType type, pugi::xml_node node) noexcept -> UIControl * {
    // 分类判断
    UIRamBitmap* pControl = nullptr;
    switch (type)
    {
    case LongUI::Type_Initialize:
        break;
    case LongUI::Type_Recreate:
        break;
    case LongUI::Type_Uninitialize:
        break;
    case_LongUI__Type_CreateControl:
        LongUI__CreateWidthCET(UIRamBitmap, pControl, type, node);
    }
    return pControl;
}

// ----------------------------------------------------------------------------
// **** UIText
// ----------------------------------------------------------------------------

// 前景渲染
void LongUI::UIText::render_chain_foreground() const noexcept {
    // 文本算前景
    m_text.Render(UIManager_RenderTarget, D2D1_POINT_2F{});
    // 父类
    Super::render_chain_foreground();
}


// UI文本: 渲染
void LongUI::UIText::Render() const noexcept {
    // 背景渲染
    this->render_chain_background();
    // 主景渲染
    this->render_chain_main();
    // 前景渲染
    this->render_chain_foreground();
}

// UIText: 刷新
void LongUI::UIText::Update() noexcept {
    // 改变了大小
    if (this->IsControlLayoutChanged()) {
        // 设置大小
        m_text.Resize(this->view_size.width, this->view_size.height);
        // 已经处理
        this->ControlLayoutChangeHandled();
    }
    return Super::Update();
}

/// <summary>
/// refresh auto-size for this control
/// </summary>
/// <returns></returns>
void LongUI::UIText::refresh_auto_size() noexcept {
    // 自动调整大小
    if (this->flags & (Flag_AutoWidth | Flag_AutoHeight)) {
        RectLTWH_F rect; m_text.GetTextBox(rect);
        // 自动调整宽度
        if (this->flags & Flag_AutoWidth) {
            auto exw = this->margin_rect.left + this->margin_rect.right;
            this->SetContentWidth(rect.width + exw);
        }
        // 自动调整高度
        if (this->flags & Flag_AutoHeight) {
            auto exh = this->margin_rect.bottom + this->margin_rect.top;
            this->SetContentHeight(rect.height + exh);
        }
    }
}


// UIText: 事件响应
bool LongUI::UIText::DoEvent(const LongUI::EventArgument& arg) noexcept {
    assert(arg.sender && "bad argument");
    // LONGUI 事件
    switch (arg.event)
    {
    case LongUI::Event::Event_SetText:
        m_text = arg.stt.text;
        this->refresh_auto_size();
        this->InvalidateThis();
        __fallthrough;
    case LongUI::Event::Event_GetText:
        arg.str = m_text.c_str();
        return true;
    case LongUI::Event::Event_SetEnabled:
        // 修改状态
        m_text.SetState(arg.ste.enabled ? State_Normal : State_Disabled);
    }
    return Super::DoEvent(arg);
}


/// <summary>
/// Initializes the specified node.
/// </summary>
/// <param name="node">The node.</param>
/// <returns></returns>
void LongUI::UIText::initialize(pugi::xml_node node) noexcept { 
    // 基本初始化
    Super::initialize(node); m_text.Init(node);
    // 保留旧标记
    auto flag = this->flags;
    // 自动调整宽度
    if (this->flags & Flag_AutoWidth) flag |= Flag_WidthFixed;
    // 自动调整高度
    if (this->flags & Flag_AutoHeight) flag |= Flag_HeightFixed;
    // 更新标记
    force_cast(this->flags) = flag;
    // 自动调整大小
    this->refresh_auto_size();
}

/*/ UIText 构造函数
LongUI::UIText::UIText(pugi::xml_node node) noexcept: Super(node), m_text(node) {
    //m_bInitZoneChanged = true;
}
*/

/// <summary>
/// Gets the basic color of the text.
/// </summary>
/// <param name="s">The s.</param>
/// <param name="color">The color.</param>
/// <returns></returns>
void LongUI::UIText::GetBasicColor(ControlState s, D2D1_COLOR_F& color) const noexcept {
    assert(s < STATE_COUNT && "out of range");
    color = m_text.color[s];
}


/// <summary>
/// Sets the basic color of the text.
/// </summary>
/// <param name="s">The s.</param>
/// <param name="color">The color.</param>
/// <returns></returns>
void LongUI::UIText::SetBasicColor(ControlState s, const D2D1_COLOR_F& color) noexcept {
    assert(s < STATE_COUNT && "out of range");
    {
        // CUIDxgiAutoLocker locker;
        m_text.color[s] = color;
    }
    this->InvalidateThis();
}


// UIText::CreateControl 函数
auto LongUI::UIText::CreateControl(CreateEventType type, pugi::xml_node node) noexcept ->UIControl* {
    // 分类判断
    UIText* pControl = nullptr;
    switch (type)
    {
    case LongUI::Type_Initialize:
        break;
    case LongUI::Type_Recreate:
        break;
    case LongUI::Type_Uninitialize:
        break;
    case_LongUI__Type_CreateControl:
        LongUI__CreateWidthCET(UIText, pControl, type, node);
    }
    return pControl;
}

// close this control 关闭控件
void LongUI::UIText::cleanup() noexcept {
    // 删前调用
    this->before_deleted();
    // 删除对象
    delete this;
}

// ----------------------------------------------------------------------------
// **** UIButton
// ----------------------------------------------------------------------------

// UIButton: 前景渲染
void LongUI::UIButton::render_chain_background() const noexcept {
    // UI部分算作前景
    D2D1_RECT_F draw_rect;
    this->GetViewRect(draw_rect);
    m_uiElement.Render(draw_rect);
    // 父类前景
    Super::render_chain_background();
}

// Render 渲染 
void LongUI::UIButton::Render() const noexcept {
    // 背景渲染
    this->render_chain_background();
    // 主景渲染
    this->render_chain_main();
    // 前景渲染
    this->render_chain_foreground();
}

// UI按钮: 刷新
void LongUI::UIButton::Update() noexcept {
    // 更新计时器
    m_uiElement.Update();
    return Super::Update();
}

// UIButton 构造函数
void LongUI::UIButton::initialize(pugi::xml_node node) noexcept {
    // 链式初始化
    Super::initialize(node);
    m_uiElement.Init(this->check_state(), 0, node);
    // 允许键盘焦点
    auto flag = this->flags | Flag_Focusable;
    // 初始化
    Helper::SetBorderColor(node, m_aBorderColor);
    // 修改
    force_cast(this->flags) = flag;
}


// UIButton::CreateControl 函数
auto LongUI::UIButton::CreateControl(CreateEventType type, pugi::xml_node node) noexcept ->UIControl* {
    UIButton* pControl = nullptr;
    switch (type)
    {
    case LongUI::Type_Initialize:
        break;
    case LongUI::Type_Recreate:
        break;
    case LongUI::Type_Uninitialize:
        break;
    case_LongUI__Type_CreateControl:
        LongUI__CreateWidthCET(UIButton, pControl, type, node);
    }
    return pControl;
}


// do event 事件处理
bool LongUI::UIButton::DoEvent(const LongUI::EventArgument& arg) noexcept {
    assert(arg.sender && "bad argument");
    // longui 消息
    switch (arg.event)
    {
    case LongUI::Event::Event_SetFocus:
        // 设置焦点: 要求焦点
        return true;
    case LongUI::Event::Event_KillFocus:
        // 释放焦点:
        m_tarStateClick = LongUI::State_Normal;
        return true;
    case LongUI::Event::Event_SetEnabled:
        // 修改状态
        m_uiElement.SetBasicState(arg.ste.enabled ? State_Normal : State_Disabled);
    }
    return Super::DoEvent(arg);
}

// 设置控件状态
LongUINoinline void LongUI::UIButton::SetControlState(ControlState state) noexcept {
    m_text.SetState(state);
    this->StartRender(m_uiElement.SetBasicState(state));
}

// 鼠标事件处理
bool LongUI::UIButton::DoMouseEvent(const MouseEventArgument& arg) noexcept {
    // 禁用状态禁用鼠标消息
    if (!this->GetEnabled()) return true;
    // 鼠标 消息
    switch (arg.event)
    {
    case LongUI::MouseEvent::Event_MouseEnter:
        // 鼠标移进: 设置UI元素状态
        m_colorBorderNow = m_aBorderColor[LongUI::State_Hover];
        this->SetControlState(LongUI::State_Hover);
        return true;
    case LongUI::MouseEvent::Event_MouseLeave:
        // 鼠标移出: 设置UI元素状态
        m_colorBorderNow = m_aBorderColor[LongUI::State_Normal];
        this->SetControlState(LongUI::State_Normal);
        return true;
    case LongUI::MouseEvent::Event_LButtonDown:
        // 左键按下:
        m_pWindow->SetCapture(this);
        m_colorBorderNow = m_aBorderColor[LongUI::State_Pushed];
        this->SetControlState(LongUI::State_Pushed);
        return true;
    case LongUI::MouseEvent::Event_LButtonUp:
        m_tarStateClick = LongUI::State_Hover;
        // 左键弹起:
        if (m_pWindow->IsControlCaptured(this)) {
            // 范围内弹起
            D2D1_POINT_2F pt{ arg.ptx, arg.pty };
            if (LongUI::IsPointInRect(this->visible_rect, pt)) {
                bool rec = this->CallUiEvent(m_event, SubEvent::Event_ItemClicked);
                // TODO: rec
                rec = false;
            }
            // 设置状态
            this->SetControlState(m_tarStateClick);
            m_colorBorderNow = m_aBorderColor[m_tarStateClick];
            m_pWindow->ReleaseCapture();
        }
        return true;
    }
    // 未处理的消息
    return false;
}

// recreate 重建
auto LongUI::UIButton::Recreate() noexcept ->HRESULT {
    // 重建元素
    m_uiElement.Recreate();
    // 父类处理
    return Super::Recreate();
}

// 添加事件监听器(雾)
bool LongUI::UIButton::uniface_addevent(SubEvent sb, UICallBack&& call) noexcept {
    // 点击
    if (sb == SubEvent::Event_ItemClicked) {
        m_event += std::move(call);
        return true;
    }
    return Super::uniface_addevent(sb, std::move(call));
}

// 关闭控件
void LongUI::UIButton::cleanup() noexcept {
    // 删前调用
    this->before_deleted();
    // 删除对象
    delete this;
}

// ----------------------------------------------------------------------------
// **** UIComboBox
// ----------------------------------------------------------------------------

// Render 渲染 
void LongUI::UIComboBox::Render() const noexcept {
    // 背景渲染
    this->render_chain_background();
    // 主景渲染
    this->render_chain_main();
    // 前景渲染
    this->render_chain_foreground();
}

/// <summary>
/// Recreates this instance.
/// </summary>
/// <returns></returns>
auto LongUI::UIComboBox::Recreate() noexcept ->HRESULT {
    auto arrow = UIScrollBarA::GetArrowRealization(UIScrollBarA::Arrow_Bottom);
    LongUI::SafeRelease(arrow);
    return Super::Recreate();
}

// UI文本: 渲染前景
void LongUI::UIComboBox::render_chain_foreground() const noexcept {
    // 父类渲染
    Super::render_chain_foreground();
    // 渲染下拉箭头
    if (m_bDrawDownArrow || !m_uiElement.IsExtraInterfaceValid()) {
        // 几何体
        auto arrow = UIScrollBarA::GetArrowRealization(UIScrollBarA::Arrow_Bottom);
#if 0
        // 目标矩形
        D2D1_RECT_F rect;
        rect.right = this->view_size.width;
        rect.left = rect.right - UIScrollBarA::BASIC_SIZE;
        rect.top = (this->view_size.height - UIScrollBarA::BASIC_SIZE) * 0.5f;
        rect.bottom = (this->view_size.height + UIScrollBarA::BASIC_SIZE) * 0.5f;
        // 颜色同文本
        m_pBrush_SetBeforeUse->SetColor(m_text.GetColor());
        // 渲染
        auto render_geo = [](ID2D1RenderTarget* const target, ID2D1Brush* const bush,
            ID2D1Geometry* const geo, const D2D1_RECT_F& rect) noexcept {
            D2D1_MATRIX_3X2_F matrix; target->GetTransform(&matrix);
            target->SetTransform(
                DX::Matrix3x2F::Translation(rect.left, rect.top) * matrix
            );
            target->DrawGeometry(geo, bush, 2.33333f);
            // 修改
            target->SetTransform(&matrix);
        };
        // 渲染
        render_geo(UIManager_RenderTarget, m_pBrush_SetBeforeUse, arrow, rect);
#else
        // 目标坐标
        D2D1_POINT_2F pt;
        pt.x = this->view_size.width - UIScrollBarA::BASIC_SIZE;
        pt.y = (this->view_size.height - UIScrollBarA::BASIC_SIZE) * 0.5f;
        // 颜色同文本
        m_pBrush_SetBeforeUse->SetColor(m_text.GetColor());
        // 渲染
        auto render_arrow = [&pt, arrow](ID2D1DeviceContext1* target, ID2D1Brush* brush) noexcept {
            D2D1_MATRIX_3X2_F matrix; target->GetTransform(&matrix);
            target->SetTransform(DX::Matrix3x2F::Translation(pt.x, pt.y) * matrix);
            target->DrawGeometryRealization(arrow, brush);
            target->SetTransform(&matrix);
        };
        // 渲染
        render_arrow(UIManager_RenderTarget, m_pBrush_SetBeforeUse);
#endif
        LongUI::SafeRelease(arrow);
    }
}

// UIComboBox: 添加事件监听器(雾)
bool LongUI::UIComboBox::uniface_addevent(SubEvent sb, UICallBack&& call) noexcept {
    // 点击
    if (sb == SubEvent::Event_ValueChanged) {
        m_eventChanged += std::move(call);
        return true;
    }
    return Super::uniface_addevent(sb, std::move(call));
}


// 同步列表
void LongUI::UIComboBox::sync_list() noexcept {
    if (m_pItemList) {
        m_pItemList->ClearList();
        for (const auto* item : m_vItems) {
            m_pItemList->PushLineElement(item);
        }
        m_pItemList->SetControlLayoutChanged();
        m_pItemList->InvalidateThis();
        assert(m_pItemList->GetChildrenCount() == m_vItems.size());
    }
}


// initialize, maybe you want call v-method
void LongUI::UIComboBox::initialize(pugi::xml_node node) noexcept {
    m_vItems.reserve(32);
    assert(!m_pItemList);
    // 链式初始化
    Super::initialize(node);
    // 渲染下箭头
    m_bDrawDownArrow = node.attribute("drawdownarrow").as_bool(false);
    // 创建列表
    auto list = node.first_child();
    if (!list) list = node.append_child("List");
    assert(list && "bad action");
    auto tmp = UIManager.CreateControl(nullptr, list, nullptr);
    if (tmp) {
        // 转换为List
        m_pItemList = longui_cast<UIList*>(tmp);
        // 存在子节点尝试创建控件树
        if (list.first_child()) {
            UIManager.MakeControlTree(m_pItemList, list);
        }
    }
    // 添加事件回调
    if (m_pItemList) {
        auto list = m_pItemList;
        m_pItemList->AddEventCall([list, this](UIControl* unused) noexcept {
            UNREFERENCED_PARAMETER(unused);
            auto& selected = list->GetSelectedIndices();
            // 检查选项
            if (selected.size() && m_vItems.isok()) {
                // 选择
                uint32_t index = uint32_t(selected.front());
                auto old = this->GetSelectedIndex();
                this->SetSelectedIndex(index);
                // 是否选择
                if (this->GetSelectedIndex() != old) {
                    this->CallUiEvent(m_eventChanged, SubEvent::Event_ValueChanged);
                }
            }
            // 点击选项关闭
            ::PostMessageW(list->GetWindow()->GetHwnd(), WM_CLOSE, 0, 0);
            return true;
        }, SubEvent::Event_ItemClicked);
    }
    // 点击事件
    auto call = [this](UIControl* ccb) noexcept {
        // 同步显示
        if (m_bChanged) {
            // zero window pointer because there is no window
            // right now, will crashed if insert new child
            m_pItemList->ZeroWindow();
            this->sync_list();
            m_bChanged = false;
        }
        // 清零
        m_pItemList->ZeroAllLinesContentWidth();
        // 坐标转换
        D2D1_POINT_2F p1 = { 0.f, 0.f };
        D2D1_POINT_2F p2 = { this->view_size.width,  this->view_size.height };
        p1 = LongUI::TransformPoint(this->world, p1);
        p2 = LongUI::TransformPoint(this->world, p2);
        D2D1_RECT_L rect;
        // 上界限
        rect.top  = LONG(p1.y);
        // 下界限
        rect.bottom = LONG(p2.y);
        // 左界限
        rect.left = LONG(p1.x);
        // 宽度
        rect.right = LONG(p2.x);
        // 高度
        float height = 0.f;
        {
            uint32_t count = std::min(uint32_t(m_uMaxLine), m_pItemList->GetChildrenCount());
            float zoomy = ccb->GetWindow()->GetViewport()->GetZoomY();
            height = zoomy * m_pItemList->GetLineHeight() * static_cast<float>(count);
        }
        // 创建弹出窗口
        auto popup = m_pWindow->CreatePopup(rect, LONG(height), m_pItemList);
        // 成功
        if (popup) {
            // 选择
            m_pItemList->SelectChild(m_indexSelected);
            // 显示
            popup->ShowWindow(SW_SHOW);
        }
        // 链接
        return true;
    };
    // 添加事件
    this->AddEventCall(call, SubEvent::Event_ItemClicked);
    // 成功
    if (m_vItems.isok() && m_pItemList) {
        // 添加项目
        for (auto line : m_pItemList->GetContainer()) {
            this->PushItem(line->GetFirstChildText());
        }
        // 已经同步过了
        m_bChanged = false;
        // 获取索引
        auto index = uint32_t(LongUI::AtoI(node.attribute("select").value()));
        // 设置显示
        this->SetSelectedIndex(index);
    }
}

// 插入物品
LongUINoinline void LongUI::UIComboBox::InsertItem(uint32_t index, const wchar_t* item) noexcept {
    assert(item && index <= m_vItems.size() && "bad argument");
    auto copy = m_strAllocator.CopyString(item);
    // 有效
    if (copy && index <= m_vItems.size()) {
        const auto oldsize = m_vItems.size();
        m_vItems.insert(index, copy);
        // 标记已修改
        m_bChanged = true;
        // 校正索引
        if (m_indexSelected >= index && m_indexSelected < oldsize) {
            // 选择后面那个
            this->SetSelectedIndex(m_indexSelected + 1);
        }
        this->InvalidateThis();
    }
#ifdef _DEBUG
    else {
        assert(!"error");
    }
#endif
}

// 设置选择索引
void LongUI::UIComboBox::SetSelectedIndex(uint32_t index) noexcept {
    // 在选择范围
    if (index >= 0 && index < m_vItems.size()) {
        m_text = m_vItems[index];
        m_indexSelected = index;
    }
    // 取消显示
    else {
        m_text = L"";
        m_indexSelected = static_cast<decltype(m_indexSelected)>(-1);
    }
    // 下帧刷新
    this->InvalidateThis();
}

// 移除物品
LongUINoinline void LongUI::UIComboBox::RemoveItem(uint32_t index) noexcept {
    assert(index < m_vItems.size() && "bad argument");
    // 有效
    if (index < m_vItems.size()) {
        // 移除item列表
        const auto oldsize = m_vItems.size();
        m_vItems.erase(index);
        // 标记已修改
        m_bChanged = true;
        // 校正索引
        if (m_indexSelected >= index && m_indexSelected < oldsize) {
            this->SetSelectedIndex(m_indexSelected > 0 ? m_indexSelected - 1 : m_indexSelected);
        }
        this->InvalidateThis();
    }
}

/// <summary>
/// Pushes the item to string list and item list.
/// </summary>
/// <param name="item">The utf-8 string. cannot be null</param>
/// <returns></returns>
void LongUI::UIComboBox::PushItem(const char* item) noexcept {
    assert(item && "bad argument");
    LongUI::SafeUTF8toWideChar(item, [this](const wchar_t* begin, void*) {
        this->PushItem(begin);
    });
}

/// <summary>
/// Pushes the item to string list and item list.
/// </summary>
/// <param name="item">The wchar string. cannot be null</param>
/// <returns></returns>
void LongUI::UIComboBox::PushItem(const wchar_t* item) noexcept {
    assert(item && "bad argument");
    this->InsertItem(this->GetItemCount(), item);
}

// UIComboBox: 析构函数
inline LongUI::UIComboBox::~UIComboBox() noexcept {
    LongUI::SafeRelease(m_pItemList);
}

// UIComboBox: 关闭控件
void LongUI::UIComboBox::cleanup() noexcept {
    // 删前调用
    this->before_deleted();
    // 删除对象
    delete this;
}

// UIComboBox::CreateControl 函数
auto LongUI::UIComboBox::CreateControl(CreateEventType type, pugi::xml_node node) noexcept ->UIControl* {
    UIComboBox* pControl = nullptr;
    switch (type)
    {
    case LongUI::Type_Initialize:
        break;
    case LongUI::Type_Recreate:
        break;
    case LongUI::Type_Uninitialize:
        break;
    case_LongUI__Type_CreateControl:
        LongUI__CreateWidthCET(UIComboBox, pControl, type, node);
    }
    return pControl;
}




// 调试区域
#ifdef LongUIDebugEvent
// GUID 信息
namespace LongUI {
    // 重载?特例化 GetIID
    template<> const IID& GetIID<LongUI::UIRamBitmap>() noexcept {
        // {763F1F35-E13A-4548-8D14-594FC11FB3ED}
        static const GUID IID_LongUI_UIRamBitmap = { 
            0x763f1f35, 0xe13a, 0x4548, { 0x8d, 0x14, 0x59, 0x4f, 0xc1, 0x1f, 0xb3, 0xed } 
        };
        return IID_LongUI_UIRamBitmap;
    }
    // 重载?特例化 GetIID
    template<> const IID& GetIID<LongUI::UIColor>() noexcept {
        // {BDB1D144-C511-4FAB-912F-2B9716AB2C36}
        static const GUID IID_LongUI_UIColor = { 
            0xbdb1d144, 0xc511, 0x4fab, { 0x91, 0x2f, 0x2b, 0x97, 0x16, 0xab, 0x2c, 0x36 } 
        };
        return IID_LongUI_UIColor;
    }
    // 重载?特例化 GetIID
    template<> const IID& GetIID<LongUI::UISingle>() noexcept {
        // {B3C3CDEB-21A7-48E3-8E6C-693212ED7619}
        static const GUID IID_LongUI_UISingle = {
            0xb3c3cdeb, 0x21a7, 0x48e3, { 0x8e, 0x6c, 0x69, 0x32, 0x12, 0xed, 0x76, 0x19 }
        };
        return IID_LongUI_UISingle;
    }
    // 重载?特例化 GetIID
    template<> const IID& GetIID<LongUI::UIPage>() noexcept {
        // {26E98A66-C52F-40BB-AAD8-FAA0A549D899}
        static const GUID IID_LongUI_UIPage = {
            0x26e98a66, 0xc52f, 0x40bb, { 0xaa, 0xd8, 0xfa, 0xa0, 0xa5, 0x49, 0xd8, 0x99 }
        };
        return IID_LongUI_UIPage;
    }
    // 重载?特例化 GetIID
    template<> const IID& GetIID<LongUI::UIListLine>() noexcept {
        // {E5CF04FC-1221-4E06-B6F3-315D45B1F2E6}
        static const GUID IID_LongUI_UIListLine= {
            0x83b86af2, 0x6755, 0x47a8, { 0xba, 0x7d, 0x69, 0x3c, 0x2b, 0xdb, 0xf, 0xbc } 
        };
        return IID_LongUI_UIListLine;
    }
    // 重载?特例化 GetIID
    template<> const IID& GetIID<LongUI::UIList>() noexcept {
        // {E5CF04FC-1221-4E06-B6F3-315D45B1F2E6}
        static const GUID IID_LongUI_UIList = {
            0xe5cf04fc, 0x1221, 0x4e06,{ 0xb6, 0xf3, 0x31, 0x5d, 0x45, 0xb1, 0xf2, 0xe6 } 
        };
        return IID_LongUI_UIList;
    }
    // 重载?特例化 GetIID
    template<> const IID& GetIID<LongUI::UIListHeader>() noexcept {
        // {E5CF04FC-1221-4E06-B6F3-315D45B1F2E6}
        static const GUID IID_LongUI_UIListHeader = { 
            0x6db3aac2, 0xf4cf, 0x4301, { 0x92, 0x91, 0xa5, 0x18, 0x1b, 0x22, 0xa0, 0x39 } 
        };
        return IID_LongUI_UIListHeader;
    }
    // 重载?特例化 GetIID
    template<> const IID& GetIID<LongUI::UIHorizontalLayout>() noexcept {
        // {E5CF04FC-1221-4E06-B6F3-315D45B1F2E6}
        static const GUID IID_LongUI_UIHorizontalLayout = {
            0xe5cf04fc, 0x1221, 0x4e06,{ 0xb6, 0xf3, 0x31, 0x5d, 0x45, 0xb1, 0xf2, 0xe6 } 
        };
        return IID_LongUI_UIHorizontalLayout;
    }
    // 重载?特例化 GetIID
    template<> const IID& GetIID<LongUI::UIVerticalLayout>() noexcept {
        // {3BE5198C-B922-4C99-827E-F0D08875B045}
        static const GUID IID_LongUI_UIVerticalLayout = {
            0x3be5198c, 0xb922, 0x4c99,{ 0x82, 0x7e, 0xf0, 0xd0, 0x88, 0x75, 0xb0, 0x45 } 
        };
        return IID_LongUI_UIVerticalLayout;
    }
    // 重载?特例化 GetIID
    template<> const IID& GetIID<LongUI::UIFloatLayout>() noexcept {
        // {8EE34AFA-7FC5-4F06-85B3-6B834979B3AF}
        static const GUID IID_LongUI_UIFloatLayout = {
            0x8ee34afa, 0x7fc5, 0x4f06, { 0x85, 0xb3, 0x6b, 0x83, 0x49, 0x79, 0xb3, 0xaf } 
        };
        return IID_LongUI_UIFloatLayout;
    }
    // 重载?特例化 GetIID
    template<> const IID& GetIID<LongUI::UIButton>() noexcept {
        // {90098AB1-4C9E-4F16-BF5E-9179B2B29570}
        static const GUID IID_LongUI_UIButton = {
            0x90098ab1, 0x4c9e, 0x4f16,{ 0xbf, 0x5e, 0x91, 0x79, 0xb2, 0xb2, 0x95, 0x70 } 
        };
        return IID_LongUI_UIButton;
    }
    // 重载?特例化 GetIID
    template<> const IID& GetIID<LongUI::UICheckBox>() noexcept {
        // {B5B59701-F9CB-4B12-9FCB-8AA4780B4061}
        static const GUID IID_LongUI_UICheckBox = {
            0xb5b59701, 0xf9cb, 0x4b12, { 0x9f, 0xcb, 0x8a, 0xa4, 0x78, 0xb, 0x40, 0x61 } 
        };
        return IID_LongUI_UICheckBox;
    }
    // 重载?特例化 GetIID
    template<> const IID& GetIID<LongUI::UIComboBox>() noexcept {
        // {01C8A079-4BF6-4F04-BE48-04E47C056CCE}
        static const GUID IID_LongUI_UIComboBox = { 
            0x1c8a079, 0x4bf6, 0x4f04, { 0xbe, 0x48, 0x4, 0xe4, 0x7c, 0x5, 0x6c, 0xce } 
        };
        return IID_LongUI_UIComboBox;
    }
    // UIContainer GUID 信息
    template<> const IID& GetIID<LongUI::UIContainer>() noexcept {
        // {30523BF0-4170-4F2F-9FF6-7946A2B8BEEB}
        static const GUID IID_LongUI_UIContainer = {
            0x30523bf0, 0x4170, 0x4f2f,{ 0x9f, 0xf6, 0x79, 0x46, 0xa2, 0xb8, 0xbe, 0xeb }
        };
        return IID_LongUI_UIContainer;
    }
    // UIContainerBuiltIn GUID 信息
    template<> const IID& GetIID<LongUI::UIContainerBuiltIn>() noexcept {
        // {D88DC85F-4903-42C7-93A4-DE6EBEC38576}
        static const GUID IID_LongUI_UIContainerBuiltIn = {
            0xd88dc85f, 0x4903, 0x42c7, { 0x93, 0xa4, 0xde, 0x6e, 0xbe, 0xc3, 0x85, 0x76 }
        };
        return IID_LongUI_UIContainerBuiltIn;
    }
    // UIEdit GUID 信息 
    template<> const IID& GetIID<LongUI::UIEdit>() noexcept {
        // {D60826F0-4AF1-48F9-A63A-58117943CE66}
        static const GUID IID_LongUI_UIEdit = { 
            0xd60826f0, 0x4af1, 0x48f9, { 0xa6, 0x3a, 0x58, 0x11, 0x79, 0x43, 0xce, 0x66 } 
        };
        return IID_LongUI_UIEdit;
    }
    // UIControl GUID 信息
    template<> const IID& GetIID<LongUI::UIControl>() noexcept { 
        // {87EB711F-3B53-4B21-ABCD-C907E5C43F8D}
        static const GUID IID_LongUI_UIControl = { 
            0x87eb711f, 0x3b53, 0x4b21,{ 0xab, 0xcd, 0xc9, 0x7, 0xe5, 0xc4, 0x3f, 0x8d } 
        };
        return IID_LongUI_UIControl;
    }
    // UIRadioButton GUID 信息
    template<> const IID& GetIID<LongUI::UIMarginalable>() noexcept {
        // {6CF3853D-6740-4635-AF7E-F8A42AEBA6C9}
        static const GUID IID_LongUI_UIMarginalControl = { 
            0x6cf3853d, 0x6740, 0x4635,{ 0xaf, 0x7e, 0xf8, 0xa4, 0x2a, 0xeb, 0xa6, 0xc9 } 
        };
        return IID_LongUI_UIMarginalControl;
    }
    // UIRadioButton GUID 信息
    template<> const IID& GetIID<LongUI::UIRadioButton>() noexcept {
        // {AC3EDEE4-BF70-434D-8F73-E7F8EA702F0E}
        static const GUID IID_LongUI_UIRadioButton = { 
            0xac3edee4, 0xbf70, 0x434d, { 0x8f, 0x73, 0xe7, 0xf8, 0xea, 0x70, 0x2f, 0xe } 
        };
        return IID_LongUI_UIRadioButton;
    }
    // UIScrollBar GUID 信息
    template<> const IID& GetIID<LongUI::UIScrollBar>() noexcept {
        // {AD925DE3-636D-44DD-A01E-A2C180DEA98D}
        static const GUID IID_LongUI_UIScrollBar = { 
            0xad925de3, 0x636d, 0x44dd,{ 0xa0, 0x1e, 0xa2, 0xc1, 0x80, 0xde, 0xa9, 0x8d } 
        };
        return IID_LongUI_UIScrollBar;
    }
    // UIScrollBarA GUID 信息
    template<> const IID& GetIID<LongUI::UIScrollBarA>() noexcept {
        // {AD925DE3-636D-44DD-A01E-A2C180DEA98D}
        static const GUID IID_LongUI_UIScrollBarA = {
            0x30af626, 0x1958, 0x4bdf,{ 0x86, 0x3e, 0x19, 0x2b, 0xdb, 0x1a, 0x49, 0x46 }
        };
        return IID_LongUI_UIScrollBarA;
    }
    
    // UIScrollBarB GUID 信息
    template<> const IID& GetIID<LongUI::UIScrollBarB>() noexcept {
        // {820DACDF-5B99-4291-A9B2-9010BE28D12D}
        static const GUID IID_LongUI_UIScrollBarB = { 
            0x820dacdf, 0x5b99, 0x4291,{ 0xa9, 0xb2, 0x90, 0x10, 0xbe, 0x28, 0xd1, 0x2d } 
        };
        return IID_LongUI_UIScrollBarB;
    }
    // UISlider GUID 信息
    template<> const IID& GetIID<LongUI::UISlider>() noexcept {
        // {3BE5198C-B922-4C99-827E-F0D08875B045}
        static const GUID IID_LongUI_UISlider = {
            0xcc64ee29, 0x3be1, 0x4b24,{ 0x9b, 0x27, 0x99, 0xbf, 0x98, 0xaa, 0x3c, 0x15 } 
        };
        return IID_LongUI_UISlider;
    }
    // UIViewport GUID信息
    template<> const IID& GetIID<LongUI::UIViewport>() noexcept {
        // {64F7B3E5-621E-4864-9535-7E6A29F670C1}
        static const GUID IID_LongUI_UIViewport = { 
            0x64f7b3e5, 0x621e, 0x4864,{ 0x95, 0x35, 0x7e, 0x6a, 0x29, 0xf6, 0x70, 0xc1 } 
        };
        return IID_LongUI_UIViewport;
    }
    // UIText GUID信息
    template<> const IID& GetIID<LongUI::UIText>() noexcept {
        // {47F83436-2D1F-413B-BBAD-9322EFF18185}
        static const GUID IID_LongUI_UIText = {
            0x47f83436, 0x2d1f, 0x413b,{ 0xbb, 0xad, 0x93, 0x22, 0xef, 0xf1, 0x81, 0x85 } 
        };
        return IID_LongUI_UIText;
    }
}


// UI控件: 调试信息
bool LongUI::UIControl::debug_do_event(const LongUI::DebugEventInformation& info) const noexcept {
    switch (info.infomation)
    {
    case LongUI::DebugInformation::Information_GetClassName:
        info.str = L"UIControl";
        return true;
    case LongUI::DebugInformation::Information_GetFullClassName:
        info.str = L"::LongUI::UIControl";
        return true;
    case LongUI::DebugInformation::Information_CanbeCasted:
        // 类型转换
        return *info.iid == LongUI::GetIID<::LongUI::UIControl>();
    default:
        break;
    }
    return false;
}

// 类型转换断言
void LongUI::UIControl::AssertTypeCasting(const IID& iid) const noexcept {
    LongUI::DebugEventInformation info;
    info.infomation = LongUI::DebugInformation::Information_CanbeCasted;
    info.iid = &iid; info.id = 0;
    assert(this->debug_do_event(info) && "bad casting");
}

// 类型转换判断
auto LongUI::UIControl::IsCanbeCastedTo(const IID& iid) const noexcept ->bool {
    LongUI::DebugEventInformation info;
    info.infomation = LongUI::DebugInformation::Information_CanbeCasted;
    info.iid = &iid; info.id = 0;
    return this->debug_do_event(info);
}

// 获取控件类名
auto LongUI::UIControl::GetControlClassName(bool full) const noexcept ->const wchar_t* {
    LongUI::DebugEventInformation info;
    info.infomation = full ? LongUI::DebugInformation::Information_GetFullClassName
        : LongUI::DebugInformation::Information_GetClassName;
    info.iid = nullptr; info.str = L"";
    this->debug_do_event(info);
    return info.str;
}

// UI颜色: 调试信息
bool LongUI::UIColor::debug_do_event(const LongUI::DebugEventInformation& info) const noexcept {
    switch (info.infomation)
    {
    case LongUI::DebugInformation::Information_GetClassName:
        info.str = L"UIColor";
        return true;
    case LongUI::DebugInformation::Information_GetFullClassName:
        info.str = L"::LongUI::UIColor";
        return true;
    case LongUI::DebugInformation::Information_CanbeCasted:
        // 类型转换
        return *info.iid == LongUI::GetIID<::LongUI::UIColor>()
            || Super::debug_do_event(info);
    default:
        break;
    }
    return false;
}

// UI系统内存位图: 调试信息
bool LongUI::UIRamBitmap::debug_do_event(const LongUI::DebugEventInformation& info) const noexcept {
    switch (info.infomation)
    {
    case LongUI::DebugInformation::Information_GetClassName:
        info.str = L"UIRamBitmap";
        return true;
    case LongUI::DebugInformation::Information_GetFullClassName:
        info.str = L"::LongUI::UIRamBitmap";
        return true;
    case LongUI::DebugInformation::Information_CanbeCasted:
        // 类型转换
        return *info.iid == LongUI::GetIID<::LongUI::UIRamBitmap>()
            || Super::debug_do_event(info);
    default:
        break;
    }
    return false;
}

// UI文本: 调试信息
bool LongUI::UIText::debug_do_event(const LongUI::DebugEventInformation& info) const noexcept {
    switch (info.infomation)
    {
    case LongUI::DebugInformation::Information_GetClassName:
        info.str = L"UIText";
        return true;
    case LongUI::DebugInformation::Information_GetFullClassName:
        info.str = L"::LongUI::UIText";
        return true;
    case LongUI::DebugInformation::Information_CanbeCasted:
        // 类型转换
        return *info.iid == LongUI::GetIID<::LongUI::UIText>()
            || Super::debug_do_event(info);
    default:
        break;
    }
    return false;
}

// UI按钮: 调试信息
bool LongUI::UIButton::debug_do_event(const LongUI::DebugEventInformation& info) const noexcept {
    switch (info.infomation)
    {
    case LongUI::DebugInformation::Information_GetClassName:
        info.str = L"UIButton";
        return true;
    case LongUI::DebugInformation::Information_GetFullClassName:
        info.str = L"::LongUI::UIButton";
        return true;
    case LongUI::DebugInformation::Information_CanbeCasted:
        // 类型转换
        return *info.iid == LongUI::GetIID<::LongUI::UIButton>()
            || Super::debug_do_event(info);
    default:
        break;
    }
    return false;
}

// UI组合框: 调试信息
bool LongUI::UIComboBox::debug_do_event(const LongUI::DebugEventInformation& info) const noexcept {
    switch (info.infomation)
    {
    case LongUI::DebugInformation::Information_GetClassName:
        info.str = L"UIComboBox";
        return true;
    case LongUI::DebugInformation::Information_GetFullClassName:
        info.str = L"::LongUI::UIComboBox";
        return true;
    case LongUI::DebugInformation::Information_CanbeCasted:
        // 类型转换
        return *info.iid == LongUI::GetIID<::LongUI::UIComboBox>()
            || Super::debug_do_event(info);
    default:
        break;
    }
    return false;
}

// UI单选按钮: 调试信息
bool LongUI::UIRadioButton::debug_do_event(const LongUI::DebugEventInformation& info) const noexcept {
    switch (info.infomation)
    {
    case LongUI::DebugInformation::Information_GetClassName:
        info.str = L"UIRadioButton";
        return true;
    case LongUI::DebugInformation::Information_GetFullClassName:
        info.str = L"::LongUI::UIRadioButton";
        return true;
    case LongUI::DebugInformation::Information_CanbeCasted:
        // 类型转换
        return *info.iid == LongUI::GetIID<::LongUI::UIRadioButton>()
            || Super::debug_do_event(info);
    default:
        break;
    }
    return false;
}

// UI复选框: 调试信息
bool LongUI::UICheckBox::debug_do_event(const LongUI::DebugEventInformation& info) const noexcept {
    switch (info.infomation)
    {
    case LongUI::DebugInformation::Information_GetClassName:
        info.str = L"UICheckBox";
        return true;
    case LongUI::DebugInformation::Information_GetFullClassName:
        info.str = L"::LongUI::UICheckBox";
        return true;
    case LongUI::DebugInformation::Information_CanbeCasted:
        // 类型转换
        return *info.iid == LongUI::GetIID<::LongUI::UICheckBox>()
            || Super::debug_do_event(info);
    default:
        break;
    }
    return false;
}

// UI边缘控件: 调试信息
bool LongUI::UIMarginalable::debug_do_event(const LongUI::DebugEventInformation& info) const noexcept {
    switch (info.infomation)
    {
    case LongUI::DebugInformation::Information_GetClassName:
        info.str = L"UIMarginalable";
        return true;
    case LongUI::DebugInformation::Information_GetFullClassName:
        info.str = L"::LongUI::UIMarginalable";
        return true;
    case LongUI::DebugInformation::Information_CanbeCasted:
        // 类型转换
        return *info.iid == LongUI::GetIID<::LongUI::UIMarginalable>()
            || Super::debug_do_event(info);
    default:
        break;
    }
    return false;
}

// UI滚动条: 调试信息
bool LongUI::UIScrollBar::debug_do_event(const LongUI::DebugEventInformation& info) const noexcept {
    switch (info.infomation)
    {
    case LongUI::DebugInformation::Information_GetClassName:
        info.str = L"UIScrollBar";
        return true;
    case LongUI::DebugInformation::Information_GetFullClassName:
        info.str = L"::LongUI::UIScrollBar";
        return true;
    case LongUI::DebugInformation::Information_CanbeCasted:
        // 类型转换
        return *info.iid == LongUI::GetIID<::LongUI::UIScrollBar>()
            || Super::debug_do_event(info);
    default:
        break;
    }
    return false;
}

// UI滚动条-A型: 调试信息
bool LongUI::UIScrollBarA::debug_do_event(const LongUI::DebugEventInformation& info) const noexcept {
    switch (info.infomation)
    {
    case LongUI::DebugInformation::Information_GetClassName:
        info.str = L"UIScrollBarA";
        return true;
    case LongUI::DebugInformation::Information_GetFullClassName:
        info.str = L"::LongUI::UIScrollBarA";
        return true;
    case LongUI::DebugInformation::Information_CanbeCasted:
        // 类型转换
        return *info.iid == LongUI::GetIID<::LongUI::UIScrollBarA>()
            || Super::debug_do_event(info);
    default:
        break;
    }
    return false;
}

// UI滚动条-A型: 调试信息
bool LongUI::UIScrollBarB::debug_do_event(const LongUI::DebugEventInformation& info) const noexcept {
    switch (info.infomation)
    {
    case LongUI::DebugInformation::Information_GetClassName:
        info.str = L"UIScrollBarB";
        return true;
    case LongUI::DebugInformation::Information_GetFullClassName:
        info.str = L"::LongUI::UIScrollBarB";
        return true;
    case LongUI::DebugInformation::Information_CanbeCasted:
        // 类型转换
        return *info.iid == LongUI::GetIID<::LongUI::UIScrollBarB>()
            || Super::debug_do_event(info);
    default:
        break;
    }
    return false;
}

// UI容器: 调试信息
bool LongUI::UIContainer::debug_do_event(const LongUI::DebugEventInformation& info) const noexcept {
    switch (info.infomation)
    {
    case LongUI::DebugInformation::Information_GetClassName:
        info.str = L"UIContainer";
        return true;
    case LongUI::DebugInformation::Information_GetFullClassName:
        info.str = L"::LongUI::UIContainer";
        return true;
    case LongUI::DebugInformation::Information_CanbeCasted:
        // 类型转换
        return *info.iid == LongUI::GetIID<::LongUI::UIContainer>()
            || Super::debug_do_event(info);
    default:
        break;
    }
    return false;
}

// LongUI内建容器: 调试信息
bool LongUI::UIContainerBuiltIn::debug_do_event(const LongUI::DebugEventInformation& info) const noexcept {
    switch (info.infomation)
    {
    case LongUI::DebugInformation::Information_GetClassName:
        info.str = L"UIContainerBuiltIn";
        return true;
    case LongUI::DebugInformation::Information_GetFullClassName:
        info.str = L"::LongUI::UIContainerBuiltIn";
        return true;
    case LongUI::DebugInformation::Information_CanbeCasted:
        // 类型转换
        return *info.iid == LongUI::GetIID<::LongUI::UIContainerBuiltIn>()
            || Super::debug_do_event(info);
    default:
        break;
    }
    return false;
}

// LongUI单独容器: 调试信息
bool LongUI::UISingle::debug_do_event(const LongUI::DebugEventInformation& info) const noexcept {
    switch (info.infomation)
    {
    case LongUI::DebugInformation::Information_GetClassName:
        info.str = L"UISingle";
        return true;
    case LongUI::DebugInformation::Information_GetFullClassName:
        info.str = L"::LongUI::UISingle";
        return true;
    case LongUI::DebugInformation::Information_CanbeCasted:
        // 类型转换
        return *info.iid == LongUI::GetIID<::LongUI::UISingle>()
            || Super::debug_do_event(info);
    default:
        break;
    }
    return false;
}

// LongUI页面容器: 调试信息
bool LongUI::UIPage::debug_do_event(const LongUI::DebugEventInformation& info) const noexcept {
    switch (info.infomation)
    {
    case LongUI::DebugInformation::Information_GetClassName:
        info.str = L"UIPage";
        return true;
    case LongUI::DebugInformation::Information_GetFullClassName:
        info.str = L"::LongUI::UIPage";
        return true;
    case LongUI::DebugInformation::Information_CanbeCasted:
        // 类型转换
        return *info.iid == LongUI::GetIID<::LongUI::UIPage>()
            || Super::debug_do_event(info);
    default:
        break;
    }
    return false;
}

// UI 基本编辑控件: 调试信息
bool LongUI::UIEdit::debug_do_event(const LongUI::DebugEventInformation& info) const noexcept {
    switch (info.infomation)
    {
    case LongUI::DebugInformation::Information_GetClassName:
        info.str = L"UIEdit";
        return true;
    case LongUI::DebugInformation::Information_GetFullClassName:
        info.str = L"::LongUI::UIEdit";
        return true;
    case LongUI::DebugInformation::Information_CanbeCasted:
        // 类型转换
        return *info.iid == LongUI::GetIID<::LongUI::UIEdit>()
            || Super::debug_do_event(info);
    default:
        break;
    }
    return false;
}

// UI水平布局: 调试信息
bool LongUI::UIHorizontalLayout::debug_do_event(const LongUI::DebugEventInformation& info) const noexcept {
    switch (info.infomation)
    {
    case LongUI::DebugInformation::Information_GetClassName:
        info.str = L"UIHorizontalLayout";
        return true;
    case LongUI::DebugInformation::Information_GetFullClassName:
        info.str = L"::LongUI::UIHorizontalLayout";
        return true;
    case LongUI::DebugInformation::Information_CanbeCasted:
        // 类型转换
        return *info.iid == LongUI::GetIID<::LongUI::UIHorizontalLayout>()
            || Super::debug_do_event(info);
    default:
        break;
    }
    return false;
}

// UI垂直布局: 调试信息
bool LongUI::UIVerticalLayout::debug_do_event(const LongUI::DebugEventInformation& info) const noexcept {
    switch (info.infomation)
    {
    case LongUI::DebugInformation::Information_GetClassName:
        info.str = L"UIVerticalLayout";
        return true;
    case LongUI::DebugInformation::Information_GetFullClassName:
        info.str = L"::LongUI::UIVerticalLayout";
        return true;
    case LongUI::DebugInformation::Information_CanbeCasted:
        // 类型转换
        return *info.iid == LongUI::GetIID<::LongUI::UIVerticalLayout>()
            || Super::debug_do_event(info);
    default:
        break;
    }
    return false;
}

// UI浮动布局: 调试信息
bool LongUI::UIFloatLayout::debug_do_event(const LongUI::DebugEventInformation& info) const noexcept {
    switch (info.infomation)
    {
    case LongUI::DebugInformation::Information_GetClassName:
        info.str = L"UIFloatLayout";
        return true;
    case LongUI::DebugInformation::Information_GetFullClassName:
        info.str = L"::LongUI::UIFloatLayout";
        return true;
    case LongUI::DebugInformation::Information_CanbeCasted:
        // 类型转换
        return *info.iid == LongUI::GetIID<::LongUI::UIFloatLayout>()
            || Super::debug_do_event(info);
    default:
        break;
    }
    return false;
}


// UI窗口: 调试信息
bool LongUI::UIViewport::debug_do_event(const LongUI::DebugEventInformation& info) const noexcept {
    switch (info.infomation)
    {
    case LongUI::DebugInformation::Information_GetClassName:
        info.str = L"UIViewport";
        return true;
    case LongUI::DebugInformation::Information_GetFullClassName:
        info.str = L"::LongUI::UIViewport";
        return true;
    case LongUI::DebugInformation::Information_CanbeCasted:
        // 类型转换
        return *info.iid == LongUI::GetIID<::LongUI::UIViewport>()
            || Super::debug_do_event(info);
    default:
        break;
    }
    return false;
}


// --------------------------- 单独 ---------------------
// UI滑动条: 调试信息
bool LongUI::UISlider::debug_do_event(const LongUI::DebugEventInformation& info) const noexcept {
    switch (info.infomation)
    {
    case LongUI::DebugInformation::Information_GetClassName:
        info.str = L"UISlider";
        return true;
    case LongUI::DebugInformation::Information_GetFullClassName:
        info.str = L"::LongUI::UISlider";
        return true;
    case LongUI::DebugInformation::Information_CanbeCasted:
        // 类型转换
        return *info.iid == LongUI::GetIID<::LongUI::UISlider>()
            || Super::debug_do_event(info);
    default:
        break;
    }
    return false;
}

// UI列表: 调试信息
bool LongUI::UIList::debug_do_event(const LongUI::DebugEventInformation& info) const noexcept {
    switch (info.infomation)
    {
    case LongUI::DebugInformation::Information_GetClassName:
        info.str = L"UIList";
        return true;
    case LongUI::DebugInformation::Information_GetFullClassName:
        info.str = L"::LongUI::UIList";
        return true;
    case LongUI::DebugInformation::Information_CanbeCasted:
        // 类型转换
        return *info.iid == LongUI::GetIID<::LongUI::UIList>()
            || Super::debug_do_event(info);
    default:
        break;
    }
    return false;
}

// UI列表元素: 调试信息
bool LongUI::UIListLine::debug_do_event(const LongUI::DebugEventInformation& info) const noexcept {
    switch (info.infomation)
    {
    case LongUI::DebugInformation::Information_GetClassName:
        info.str = L"UIListLine";
        return true;
    case LongUI::DebugInformation::Information_GetFullClassName:
        info.str = L"::LongUI::UIListLine";
        return true;
    case LongUI::DebugInformation::Information_CanbeCasted:
        // 类型转换
        return *info.iid == LongUI::GetIID<::LongUI::UIListLine>()
            || Super::debug_do_event(info);
    default:
        break;
    }
    return false;
}

// UI列表头: 调试信息
bool LongUI::UIListHeader::debug_do_event(const LongUI::DebugEventInformation& info) const noexcept {
    switch (info.infomation)
    {
    case LongUI::DebugInformation::Information_GetClassName:
        info.str = L"UIListHeader";
        return true;
    case LongUI::DebugInformation::Information_GetFullClassName:
        info.str = L"::LongUI::UIListHeader";
        return true;
    case LongUI::DebugInformation::Information_CanbeCasted:
        // 类型转换
        return *info.iid == LongUI::GetIID<::LongUI::UIListHeader>()
            || Super::debug_do_event(info);
    default:
        break;
    }
    return false;
}
#endif