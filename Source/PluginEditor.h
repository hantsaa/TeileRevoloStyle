#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace ui { constexpr int kW = 640, kH = 548; }   // taille de conception ; l'éditeur se redimensionne à ce ratio

// Pédale au pied (ONF ou TAP) avec sa LED
struct FootSwitch : public juce::Component
{
    std::function<void()> onDown, onUp;
    std::function<bool()> ledOn;
    juce::Colour ledColour { juce::Colours::red };

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refreshLed();   // ne repeint que si l'état de la LED a changé

    bool down = false, lastLed = false;
};

// Interrupteur à deux positions lié à un paramètre booléen (WET|MIX, ONF|MOM, FREE|SYNC)
struct TwoWaySwitch : public juce::Button
{
    TwoWaySwitch (const juce::String& off, const juce::String& on)
        : juce::Button (off + on), offText (off), onText (on) { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
    juce::String offText, onText;
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
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    struct KnobLook : public juce::LookAndFeel_V4
    {
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    };

    void timerCallback() override;
    void addKnob (Knob&, const juce::String& paramId, const juce::String& name, int cx, int cy);
    void attachTempoKnob (bool sync);
    void showReadoutFor (const juce::MouseEvent&);
    juce::Rectangle<int> S (int x, int y, int w, int h) const;
    juce::Rectangle<int> lcdArea() const { return S (396, 14, 220, 28); }

    KnobLook look;
    RevoloProcessor& proc;
    Knob reverb, character, length, air, noise, tremolo, tempo, filter;
    std::vector<Knob*> knobs;
    TwoWaySwitch mixSwitch { "WET", "MIX" }, momSwitch { "ONF", "MOM" }, syncSwitch { "FREE", "SYNC" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> mixAtt, momAtt, syncAtt;
    FootSwitch onf, tapBtn;

    float scale = 1.f;
    bool tempoIsSync = false, lastMom = false;
    juce::String lcdTitle, lcdValue;
    juce::uint32 lcdUntil = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RevoloEditor)
};
