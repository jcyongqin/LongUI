﻿#pragma once
/**
* Copyright (c) 2014-2016 dustpg   mailto:dustpg@gmail.com
*
* Permission is hereby granted, free of charge, to any person
* obtaining a copy of this software and associated documentation
* files (the "Software"), to deal in the Software without
* restriction, including without limitation the rights to use,
* copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following
* conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
* HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#include "UIContainer.h"
#include "../Component/Element.h"

// LongUI namespace
namespace LongUI {
    // base scroll bar 默认滚动条
    class UIScrollBar : public UIMarginalable {
        // super class
        using Super = UIMarginalable;
    protected:
        // Scroll Bar Type
        enum class ScrollBarType : uint8_t {
            Type_Horizontal = 0,    // 水平
            Type_Vertical = 1,      // 垂直
        };
        // mouse point [0, 1, 2, 3, 4]
        enum class PointType : uint8_t {
            Type_None,      // None
            Type_Arrow1,    // Arrow1
            Type_Arrow2,    // Arrow2
            Type_Thumb,     // Thumb
            Type_Shaft,     // Shaft
        };
    public:
        // update width of marginal
        virtual void UpdateMarginalWidth() noexcept override;
    public:
        // get parent width/height
        auto GetParentWH() noexcept { return 10.f; }
        //auto GetParentWH() noexcept { return this->bartype == ScrollBarType::Type_Horizontal ? this->parent->GetViewWidthByZoomed() : this->parent->GetViewHeightZoomed(); }
        // on page up
        auto OnPageUp() noexcept { return this->SetIndex(m_uiAnimation.end - this->GetParentWH()); }
        // on page down
        auto OnPageDown() noexcept { return this->SetIndex(m_uiAnimation.end + this->GetParentWH()); }
        // on page X
        auto OnPageX(float rate)  noexcept{ return this->SetIndex(m_uiAnimation.end + rate * this->GetParentWH()); }
        // on wheel up
        auto OnWheelUp() noexcept { return this->SetIndex(m_uiAnimation.end - wheel_step); }
        // on wheel down
        auto OnWheelDown() noexcept { return this->SetIndex(m_uiAnimation.end + wheel_step); }
        // on wheel X
        auto OnWheelX(float rate) noexcept { return this->SetIndex(m_uiAnimation.end + rate * wheel_step); }
    public:
        // how size that take up the owner's space
        auto GetIndex() const noexcept { return m_fIndex; }
        // set new index
        void SetIndex(float new_index) noexcept;
    protected:
        // get bar length
        auto get_length() noexcept { return bartype == ScrollBarType::Type_Vertical ? parent->GetContentHeightZoomed() : parent->GetContentWidthZoomed(); }
        // set index
        void set_index(float index) noexcept;
    public:
        // constructor 构造函数
        UIScrollBar(UIContainer* cp) noexcept;
        // deleted function
        UIScrollBar(const UIScrollBar&) = delete;
    protected:
        // something must do before deleted
        void before_deleted() noexcept { Super::before_deleted(); }
        // init
        void initialize(pugi::xml_node node) noexcept;
    public:
        // bartype of scrollbar
        ScrollBarType   const   bartype = ScrollBarType::Type_Vertical;
        // want animation?
        bool                    m_bAnimation = false;
    protected:
        // tpye of mouse pointed
        PointType               m_pointType = PointType::Type_None;
        // tpye of mouse last pointed
        PointType               m_lastPointType = PointType::Type_None;
    public:
        // step distance for whell up/down
        float                   wheel_step = 32.f;
    protected:
        // max index of scroll bar
        float                   m_fMaxIndex = 0.f;
        // max range of scroll bar
        float                   m_fMaxRange = 1.f;
        // now index of scroll bar
        float                   m_fIndex = 0.f;
        // old index of scroll bar
        float                   m_fOldIndex = 0.f;
        // old point of scroll bar
        float                   m_fOldPoint = 0.f;
        // animation
        CUIAnimationOffset      m_uiAnimation;
#ifdef LongUIDebugEvent
    protected:
        // debug infomation
        virtual bool debug_do_event(const LongUI::DebugEventInformation&) const noexcept override;
#endif
    };
    // ComboBox
    class UIComboBox;
    // srcollbar type A
    class UIScrollBarA : public UIScrollBar {
        // super class
        using Super = UIScrollBar;
        // clean this control 清除控件
        virtual void cleanup() noexcept override;
    public:
        // basic size
        static constexpr float BASIC_SIZE = 16.f;
        // arrow for this
        enum Arrow { Arrow_Left, Arrow_Top, Arrow_Right, Arrow_Bottom, ARROW_SIZE };
    private:
        // path geo - Realization
        static ID2D1GeometryRealization*    s_apArrowRealization[ARROW_SIZE];
        // path geo
        static ID2D1PathGeometry*           s_apArrowPathGeometry[ARROW_SIZE];
    public:
        // ctor
        UIScrollBarA(UIContainer* cp) noexcept : Super(cp) { }
        // get arrow realization
        static auto GetArrowRealization(Arrow id) noexcept { return LongUI::SafeAcquire(s_apArrowRealization[id]); }
        // get arrow geometry
        static auto GetArrowGeometry(Arrow id) noexcept { return LongUI::SafeAcquire(s_apArrowPathGeometry[id]); }
        // create this
        static auto CreateControl(CreateEventType, pugi::xml_node) noexcept ->UIControl*;
    protected:
        // something must do before deleted
        void before_deleted() noexcept { Super::before_deleted(); }
        // init
        void initialize(pugi::xml_node node) noexcept;
    public:
        // Render 渲染 
        virtual void Render() const noexcept override;
        // udate 刷新
        virtual void Update() noexcept override;
        // do event 事件处理
        //bool DoEvent(const LongUI::EventArgument& arg) noexcept override;
        // do mouse event 鼠标事件处理
        bool DoMouseEvent(const MouseEventArgument& arg) noexcept override;
        // recreate
        auto Recreate() noexcept ->HRESULT override;
    public:
        // init sb
        //virtual void InitMarginalControl(MarginalControl _type) noexcept override;
        // update width of marginal
        virtual void UpdateMarginalWidth() noexcept override;
    private:
        // set new state
        void set_state(PointType _bartype, ControlState state) noexcept;
    protected:
        // dtor
        ~UIScrollBarA() noexcept;
    private:
        // the rect of arrow 2
        D2D1_RECT_F             m_rtArrow1 = D2D1_RECT_F{0.f};
        // the rect of arrow 2
        D2D1_RECT_F             m_rtArrow2 = D2D1_RECT_F{0.f};
        // the rect of thumb
        D2D1_RECT_F             m_rtThumb = D2D1_RECT_F{0.f};
        // arrow 1 text path geo
        ID2D1PathGeometry*      m_pArrow1Geo = nullptr;
        // arrow 2 text path geo
        ID2D1PathGeometry*      m_pArrow2Geo = nullptr;
        // arrow1
        Component::Element4Bar  m_uiArrow1;
        // arrow2
        Component::Element4Bar  m_uiArrow2;
        // thumb
        Component::Element4Bar  m_uiThumb;
        // step for arrow clicked
        float                   m_fArrowStep = 32.f;
        // captured this
        bool                    m_bCaptured = false;
        // arrow1 use colorrect
        bool                    m_bArrow1InColor = false;
        // arrow2 use colorrect
        bool                    m_bArrow2InColor = false;
        // unused
        bool                    unused_bara = false;
#ifdef LongUIDebugEvent
    protected:
        // debug infomation
        virtual bool debug_do_event(const LongUI::DebugEventInformation&) const noexcept override;
#endif
    };
    // srcollbar type B
    class UIScrollBarB : public UIScrollBar {
        // super class
        using Super = UIScrollBar;
        // close this control 关闭控件
        void cleanup() noexcept override;
    public:
        // render
        void Render() const noexcept override {};
        // update
        void Update() noexcept override;
        // update width of marginal
        virtual void UpdateMarginalWidth() noexcept override {}
        // ctor
        UIScrollBarB(UIContainer* cp) noexcept : Super(cp) { }
        // create this
        static auto CreateControl(CreateEventType, pugi::xml_node) noexcept ->UIControl*;
    protected:
        // something must do before deleted
        void before_deleted() noexcept { Super::before_deleted(); }
        // init
        void initialize(pugi::xml_node node) noexcept { return Super::initialize(node); }
#ifdef LongUIDebugEvent
    protected:
        // debug infomation
        virtual bool debug_do_event(const LongUI::DebugEventInformation&) const noexcept override;
#endif
    };
#ifdef LongUIDebugEvent
    // 重载?特例化 GetIID
    template<> const IID& GetIID<LongUI::UIScrollBar>() noexcept;
    // 重载?特例化 GetIID
    template<> const IID& GetIID<LongUI::UIScrollBarA>() noexcept;
    // 重载?特例化 GetIID
    template<> const IID& GetIID<LongUI::UIScrollBarB>() noexcept;
#endif
}