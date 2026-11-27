#include "VintageDigital.h"

#include "IControls.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <functional>
#include <numeric>
#include <string>

using namespace iplug;
using namespace igraphics;

namespace
{
constexpr float kMargin = 12.f;
constexpr float kRowHeight = 28.f;
constexpr float kGap = 6.f;
constexpr float kToggleHeight = 24.f;
constexpr float kToggleWidth = 64.f;
constexpr float kLabelWidth = 150.f;
constexpr float kValueWidth = 96.f;

enum ETags
{
    kDepopTag = 1,
    kPageTag,
    kEffectMenuTag // + unit
};

const IColor kBackground (255, 50, 62, 68);
const IColor kFieldColor (255, 32, 40, 44);
const IColor kTextColor (255, 235, 235, 235);
const IColor kDimTextColor (255, 150, 160, 165);
const IColor kAccentColor (255, 66, 162, 200);

const IText kLabelText (15.f, kTextColor, nullptr, EAlign::Near);
const IText kFieldText = IText (15.f, kTextColor).WithTEColors (kFieldColor, kTextColor);

const IVStyle kSliderStyle = DEFAULT_STYLE.WithShowLabel (false)
                                 .WithShowValue (false)
                                 .WithDrawShadows (false)
                                 .WithDrawFrame (false)
                                 .WithColor (kFG, kTextColor)
                                 .WithColor (kPR, COLOR_WHITE)
                                 .WithColor (kSH, kFieldColor)
                                 .WithColor (kX1, kAccentColor);

const IVStyle kToggleStyle = DEFAULT_STYLE.WithShowLabel (false)
                                 .WithValueText (kFieldText)
                                 .WithDrawShadows (false)
                                 .WithEmboss (false)
                                 .WithColor (kFG, kFieldColor)
                                 .WithColor (kPR, kAccentColor)
                                 .WithColor (kFR, kFieldColor);

class Field : public ICaptionControl
{
public:
    Field (const IRECT& bounds, int paramIdx)
        : ICaptionControl (bounds, paramIdx, kFieldText, kFieldColor, false)
    {
        mTriangleColor = kDimTextColor;
        mTriangleMouseOverColor = kTextColor;
    }
};

// A unit's effects; the G-Major 2's in one submenu per family, as all 36 in one list would not
// fit the editor.
class EffectMenu final : public Field
{
public:
    explicit EffectMenu (const IRECT& bounds, int unitToList)
        : Field (bounds, kG2EffectParam + unitToList), unit (unitToList)
    {
    }

    void OnMouseDown (float, float, const IMouseMod&) override
    {
        menu.Clear();
        const int current = GetParam()->Int();
        IPopupMenu* familyMenu = nullptr;
        const char* family = nullptr;

        for (int effect = 0; effect < getEffectCount (unit); ++effect)
        {
            const auto& info = getEffectInfo (unit, effect);
            const int flags = effect == current ? IPopupMenu::Item::kChecked : IPopupMenu::Item::kNoFlags;

            if (info.family == nullptr)
            {
                menu.AddItem (new IPopupMenu::Item (info.name, flags, effect));
                continue;
            }

            if (family == nullptr || std::strcmp (family, info.family) != 0)
            {
                familyMenu = new IPopupMenu();
                menu.AddItem (info.family, familyMenu)
                    ->SetChecked (std::strcmp (getEffectInfo (unit, current).family, info.family) == 0);
                family = info.family;
            }

            familyMenu->AddItem (new IPopupMenu::Item (info.name, flags, effect));
        }

        GetUI()->CreatePopupMenu (*this, menu, mRECT);
    }

    void OnPopupMenuSelection (IPopupMenu* selected, int) override
    {
        if (selected != nullptr && selected->GetChosenItem() != nullptr)
            SetValueFromUserInput (GetParam()->ToNormalized (selected->GetChosenItem()->GetTag()));
    }

private:
    const int unit;
    IPopupMenu menu;
};

// The Intellifex effect's FUNCTION SELECT pages. The page is editor state, not a parameter.
class PageMenu final : public ICaptionControl
{
public:
    PageMenu (const IRECT& bounds, std::vector<int>& effectPages, std::function<void()> onChange)
        : ICaptionControl (bounds, kNoParameter, kFieldText, kFieldColor, false),
          pages (effectPages),
          changed (std::move (onChange))
    {
        mTriangleColor = kDimTextColor;
        mTriangleMouseOverColor = kTextColor;
    }

    void Draw (IGraphics& g) override
    {
        const auto& info = effect();
        mStr.Set (info.pages[page (info)].name);
        ICaptionControl::Draw (g);
    }

    void OnResize() override
    {
        const float width = mText.mSize;
        mTriangleRect = mRECT.GetFromRight (width).GetCentredInside (IRECT (0.f, 0.f, width * 0.5f, width * 0.33f));
        SetDirty (false);
    }

    void OnMouseDown (float, float, const IMouseMod&) override
    {
        menu.Clear();
        const auto& info = effect();

        for (int p = 0; p < info.pageCount; ++p)
            menu.AddItem (new IPopupMenu::Item (info.pages[p].name,
                                                p == page (info) ? IPopupMenu::Item::kChecked
                                                                 : IPopupMenu::Item::kNoFlags,
                                                p));

        GetUI()->CreatePopupMenu (*this, menu, mRECT);
    }

    void OnPopupMenuSelection (IPopupMenu* selected, int) override
    {
        if (selected == nullptr || selected->GetChosenItem() == nullptr)
            return;

        pages[(size_t) effectIndex()] = selected->GetChosenItem()->GetTag();
        changed();
    }

private:
    int effectIndex()
    {
        return std::clamp (GetDelegate()->GetParam (kIfxEffectParam)->Int(), 0,
                           getEffectCount (kUnitIntellifex) - 1);
    }

    const EffectInfo& effect()
    {
        return getEffectInfo (kUnitIntellifex, effectIndex());
    }

    int page (const EffectInfo& info)
    {
        return std::clamp (pages[(size_t) effectIndex()], 0, info.pageCount - 1);
    }

    std::vector<int>& pages;
    std::function<void()> changed;
    IPopupMenu menu;
};

// Typed values are read in the knob's units, e.g. "2 kHz".
class KnobValue final : public ITextControl
{
public:
    KnobValue (const IRECT& bounds, int paramIdx, const KnobDef& knobDef, std::function<int()> linkedValue)
        : ITextControl (bounds, "", kFieldText, kFieldColor), knob (knobDef), linked (std::move (linkedValue))
    {
        // iPlug2's text entry only accepts the keys that the value-0 parameter's type allows,
        // digits for an Int knob, so the parameter is linked to value 1.
        SetNVals (2);
        SetParamIdx (paramIdx, 1);
        mIgnoreMouse = false;
        mDblAsSingleClick = true;
    }

    void Draw (IGraphics& g) override
    {
        GetParam (1)->GetDisplay (mStr);
        ITextControl::Draw (g);
    }

    void OnMouseDown (float, float, const IMouseMod&) override
    {
        GetUI()->CreateTextEntry (*this, mText, mRECT, mStr.Get(), kNoValIdx);
    }

    void OnTextEntryCompletion (const char* text, int) override
    {
        SetNonNormalizedValueFromUserInput (1, parseKnobValue (knob, text, linked()));
    }

private:
    const KnobDef& knob;
    std::function<int()> linked;
};
} // namespace

void VintageDigital::layoutEditor (IGraphics& graphics)
{
    graphics.AttachPanelBackground (kBackground);
    graphics.LoadFont ("Roboto-Regular", ROBOTO_FN);
    graphics.EnableTooltips (true);

    // iPlug2's native Linux menus go through GTK, which crashes the host when GTK runs on Wayland.
    graphics.AttachPopupMenuControl (kFieldText);
    graphics.AttachTextEntryControl();
    IPopupMenuControl* popup = graphics.GetPopupMenuControl();
    popup->SetPanelColor (kFieldColor);
    popup->SetCellBackgroundColor (kAccentColor);
    popup->SetItemColor (kTextColor);
    popup->SetItemMouseoverColor (COLOR_WHITE);
    popup->SetDisabledItemColor (kDimTextColor);
    popup->SetSeparatorColor (kDimTextColor);

    const float right = graphics.GetBounds().R - kMargin;
    const float controlLeft = kMargin + kLabelWidth + 8.f;
    const auto row = [&] (float top) { return IRECT (kMargin, top, right, top + kRowHeight); };
    float y = kMargin;

    graphics.AttachControl (new Field (row (y), kUnitParam));
    y += kRowHeight + kGap;

    for (int unit = 0; unit < kUnitCount; ++unit)
        graphics.AttachControl (new EffectMenu (row (y), unit), kEffectMenuTag + unit);
    y += kRowHeight + kGap;

    graphics.AttachControl (new ITextControl (IRECT (kMargin, y, kMargin + kLabelWidth, y + kRowHeight),
                                              "De-pop", kLabelText));
    const float toggleTop = y + 0.5f * (kRowHeight - kToggleHeight);
    graphics
        .AttachControl (new IVToggleControl (IRECT (controlLeft, toggleTop, controlLeft + kToggleWidth,
                                                    toggleTop + kToggleHeight),
                                             kDepopParam, "", kToggleStyle, "Off", "On"),
                        kDepopTag)
        ->SetTooltip ("Removes the clicks the device makes when this effect's knobs turn fast; "
                      "once they settle the output is the device's.");
    y += kRowHeight + kGap;

    // The G Major 2's effects have no pages; the Intellifex's page menu takes a row of its own.
    const float g2KnobTop = y;
    graphics.AttachControl (new PageMenu (row (y), ifxPage, [this] { showEffect (*GetUI()); }), kPageTag);
    y += kRowHeight + kGap;
    const float ifxKnobTop = y;

    const auto addRows = [&] (int unit, int effect, const std::string& group, const int* knobs, int count) {
        const auto& info = getEffectInfo (unit, effect);
        const int base = knobBase[(size_t) unit][(size_t) effect];
        float rowTop = unit == kUnitIntellifex ? ifxKnobTop : g2KnobTop;

        for (int i = 0; i < count; ++i, rowTop += kRowHeight + kGap)
        {
            const KnobDef& knob = info.knobs[knobs[i]];
            const int param = base + knobs[i];
            const IRECT label (kMargin, rowTop, kMargin + kLabelWidth, rowTop + kRowHeight);
            const IRECT control (controlLeft, rowTop, right, rowTop + kRowHeight);
            assert (control.B <= graphics.GetBounds().B - kMargin);

            graphics.AttachControl (new ITextControl (label, knob.name, kLabelText), kNoTag, group.c_str());

            if (knob.kind == KnobKind::Choice)
            {
                graphics.AttachControl (new Field (control, param), kNoTag, group.c_str());
            }
            else
            {
                const int linkParam = paramKnob[(size_t) param].linkParam;
                graphics.AttachControl (new IVSliderControl (control.GetReducedFromRight (kValueWidth + kGap),
                                                             param, "", kSliderStyle, false,
                                                             EDirection::Horizontal),
                                        kNoTag, group.c_str());
                graphics.AttachControl (new KnobValue (control.GetFromRight (kValueWidth), param, knob,
                                                       [this, linkParam] { return linkedValue (linkParam); }),
                                        kNoTag, group.c_str());
            }
        }
    };

    for (int unit = 0; unit < kUnitCount; ++unit)
    {
        for (int effect = 0; effect < getEffectCount (unit); ++effect)
        {
            const auto& info = getEffectInfo (unit, effect);

            if (info.pageCount == 0)
            {
                std::vector<int> all ((size_t) info.knobCount);
                std::iota (all.begin(), all.end(), 0);
                addRows (unit, effect, info.key, all.data(), info.knobCount);
            }

            for (int page = 0; page < info.pageCount; ++page)
                addRows (unit, effect, std::string (info.key) + "#" + std::to_string (page),
                         info.pages[page].knobs, info.pages[page].knobCount);
        }
    }

    showEffect (graphics);
}

void VintageDigital::showEffect (IGraphics& graphics)
{
    const int unit = currentUnit();
    const int effect = currentEffect (unit);
    const auto& info = getEffectInfo (unit, effect);

    std::string group = info.key;
    if (info.pageCount > 0)
        group += "#" + std::to_string (std::clamp (ifxPage[(size_t) effect], 0, info.pageCount - 1));

    graphics.ForAllControlsFunc ([&group] (IControl* control) {
        const char* controlGroup = control->GetGroup();
        if (controlGroup[0] != '\0')
            control->Hide (group != controlGroup);
    });

    for (int menuUnit = 0; menuUnit < kUnitCount; ++menuUnit)
        graphics.GetControlWithTag (kEffectMenuTag + menuUnit)->Hide (menuUnit != unit);

    graphics.GetControlWithTag (kDepopTag)->SetDisabled (! info.depop);
    graphics.GetControlWithTag (kPageTag)->Hide (info.pageCount == 0);
    graphics.GetControlWithTag (kPageTag)->SetDirty (false);
}

void VintageDigital::OnParamChangeUI (int paramIdx, EParamSource)
{
    if (paramIdx == kUnitParam)
    {
        const int latency = latencyFor (currentUnit());
        if (latency != GetLatency())
            SetLatency (latency);
    }

    IGraphics* graphics = GetUI();
    if (graphics == nullptr)
        return;

    if (paramIdx == kUnitParam || paramIdx == kG2EffectParam || paramIdx == kIfxEffectParam)
        showEffect (*graphics);
    else if (paramIdx >= 0 && paramIdx < NParams() && paramKnob[(size_t) paramIdx].linkTarget)
        graphics->SetAllControlsDirty();
}
