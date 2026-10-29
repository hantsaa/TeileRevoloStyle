#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Taille de conception ; l'éditeur se redimensionne en gardant ce ratio.
// La face avant (600 x 560) reprend la disposition du dessin du manuel du TEIL2 Revolo.
namespace ui { constexpr int kW = 640, kH = 640; }

// Grosse pédale au pied (ONF ou TAP)
struct FootSwitch : public juce::Component
{
    std::function<void()> onDown, onUp;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool down = false;
};

// Interrupteur à glissière lié à un paramètre booléen.
//   Horizontal : légende gauche | glissière | légende droite  (MIX | WET, FREE | SYNC)
//   Vertical   : légende haut / glissière / légende bas       (MOM / ONF)
struct SlideSwitch : public juce::Button
{
    enum Orientation { Horizontal, Vertical };
    SlideSwitch (const juce::String& first, const juce::String& second, bool onIsFirst_, Orientation o)
        : juce::Button (first + second), firstText (first), secondText (second), onIsFirst (onIsFirst_), orient (o)
    { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
    juce::String firstText, secondText;
    bool onIsFirst;
    Orientation orient;
};

class RevoloEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RevoloEditor (RevoloProcessor&);
    ~RevoloEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent& e) override     { showReadoutFor (e); }
    void mouseDown (const juce::MouseEvent& e) override      { showReadoutFor (e); }
    void mouseDrag (const juce::MouseEvent& e) override      { showReadoutFor (e); }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails&) override { showReadoutFor (e); }

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
        juce::String name;
        int cx = 0, cy = 0;
        bool labelBelow = false;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    struct KnobLook : public juce::LookAndFeel_V4
    {
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    };

    void timerCallback() override;
    void addKnob (Knob&, const juce::String& paramId, const juce::String& name, int cx, int cy, bool labelBelow = false);
    void attachTempoKnob (bool sync);
    void showReadoutFor (const juce::MouseEvent&);
    void drawLogo (juce::Graphics&, juce::Point<float> centre);
    void drawLed (juce::Graphics&, juce::Point<float> c, juce::Colour colour, bool on);
    juce::Rectangle<int> S (int x, int y, int w, int h) const;
    juce::Rectangle<int> lcdArea() const { return S (396, 14, 224, 32); }

    KnobLook look;
    RevoloProcessor& proc;
    Knob reverb, character, length, air, noise, tremolo, tempo, filter;
    std::vector<Knob*> knobs;
    SlideSwitch mixSwitch  { "MIX", "WET", true,  SlideSwitch::Horizontal };
    SlideSwitch momSwitch  { "MOM", "ONF", true,  SlideSwitch::Vertical };
    SlideSwitch syncSwitch { "FREE", "SYNC", false, SlideSwitch::Horizontal };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> mixAtt, momAtt, syncAtt;
    FootSwitch onf, tapBtn;
    juce::Image grain;

    float scale = 1.f;
    bool tempoIsSync = false, lastOnfLed = false, lastTapLed = false;
    juce::String lcdTitle, lcdValue;
    juce::uint32 lcdUntil = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RevoloEditor)
};
