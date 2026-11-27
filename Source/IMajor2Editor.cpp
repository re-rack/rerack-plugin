#include "IMajor2.h"

#include "IControls.h"

#include <cassert>
#include <cstring>

using namespace iplug;
using namespace igraphics;

namespace
{
constexpr float kMargin = 12.f;
constexpr float kRowHeight = 28.f;
constexpr float kGap = 6.f;
constexpr float kComboHeight = 28.f;
constexpr float kToggleHeight = 24.f;
constexpr float kToggleWidth = 64.f;
constexpr float kLabelWidth = 150.f;
constexpr float kValueWidth = 96.f;
constexpr int kDepopTag = 1;

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

// The effects in one submenu per family: all 36 in one list would not fit the editor.
class EffectMenu final : public Field
{
public:
    explicit EffectMenu (const IRECT& bounds) : Field (bounds, kEffectParam) {}

    void OnMouseDown (float, float, const IMouseMod&) override
    {
        menu.Clear();
        const int current = GetParam()->Int();
        IPopupMenu* familyMenu = nullptr;
        const char* family = nullptr;

        for (int effect = 0; effect < getEffectCount(); ++effect)
        {
            const auto& info = getEffectInfo (effect);

            if (family == nullptr || std::strcmp (family, info.family) != 0)
            {
                familyMenu = new IPopupMenu();
                menu.AddItem (info.family, familyMenu)
                    ->SetChecked (std::strcmp (getEffectInfo (current).family, info.family) == 0);
                family = info.family;
            }

            const int flags = effect == current ? IPopupMenu::Item::kChecked : IPopupMenu::Item::kNoFlags;
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
    IPopupMenu menu;
};

// Typed values are read in the knob's units, e.g. "2 kHz".
class KnobValue final : public ITextControl
{
public:
    KnobValue (const IRECT& bounds, int paramIdx, const KnobDef& knobDef)
        : ITextControl (bounds, "", kFieldText, kFieldColor), knob (knobDef)
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
        SetNonNormalizedValueFromUserInput (1, parseKnobValue (knob, text));
    }

private:
    const KnobDef& knob;
};
} // namespace

void IMajor2::layoutEditor (IGraphics& graphics)
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
    float y = kMargin;

    graphics.AttachControl (new EffectMenu (IRECT (kMargin, y, right, y + kComboHeight)));
    y += kComboHeight + kGap;

    graphics.AttachControl (new ITextControl (IRECT (kMargin, y, kMargin + kLabelWidth, y + kToggleHeight),
                                              "De-pop", kLabelText));
    graphics
        .AttachControl (new IVToggleControl (IRECT (controlLeft, y, controlLeft + kToggleWidth, y + kToggleHeight),
                                             kDepopParam, "", kToggleStyle, "Off", "On"),
                        kDepopTag)
        ->SetTooltip ("Removes the clicks the device makes when this effect's knobs turn fast; "
                      "once they settle the output is the device's.");
    y += kToggleHeight + kGap;

    for (int effect = 0; effect < getEffectCount(); ++effect)
    {
        const auto& info = getEffectInfo (effect);
        float rowTop = y;

        for (int knobIndex = 0; knobIndex < info.knobCount; ++knobIndex, rowTop += kRowHeight + kGap)
        {
            const KnobDef& knob = info.knobs[knobIndex];
            const int param = knobBase[(size_t) effect] + knobIndex;
            const IRECT label (kMargin, rowTop, kMargin + kLabelWidth, rowTop + kRowHeight);
            const IRECT control (controlLeft, rowTop, right, rowTop + kRowHeight);
            assert (control.B <= graphics.GetBounds().B - kMargin);

            graphics.AttachControl (new ITextControl (label, knob.name, kLabelText), kNoTag, info.key);

            if (knob.kind == KnobKind::Choice)
            {
                graphics.AttachControl (new Field (control, param), kNoTag, info.key);
            }
            else
            {
                graphics.AttachControl (new IVSliderControl (control.GetReducedFromRight (kValueWidth + kGap),
                                                             param, "", kSliderStyle, false,
                                                             EDirection::Horizontal),
                                        kNoTag, info.key);
                graphics.AttachControl (new KnobValue (control.GetFromRight (kValueWidth), param, knob),
                                        kNoTag, info.key);
            }
        }
    }

    showEffect (graphics);
}

void IMajor2::showEffect (IGraphics& graphics)
{
    const auto& info = getEffectInfo (currentEffect());

    graphics.ForAllControlsFunc ([&info] (IControl* control) {
        const char* group = control->GetGroup();
        if (group[0] != '\0')
            control->Hide (std::strcmp (group, info.key) != 0);
    });

    graphics.GetControlWithTag (kDepopTag)->SetDisabled (! info.depop);
}

void IMajor2::OnParamChangeUI (int paramIdx, EParamSource)
{
    if (paramIdx == kEffectParam && GetUI() != nullptr)
        showEffect (*GetUI());
}
