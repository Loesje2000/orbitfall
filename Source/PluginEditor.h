#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class OrbitfallAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit OrbitfallAudioProcessorEditor (OrbitfallAudioProcessor&);
    ~OrbitfallAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Palette
    {
        juce::Colour background { 0xff2c1f0e };
        juce::Colour accent { 0xffc8a840 };
        juce::Colour text { 0xffe8d5a3 };
        juce::Colour muted { 0xffa07840 };
        juce::Colour border { 0xff5a3e1e };
        juce::Colour panel { 0xff3d2a0f };
        juce::Colour dark { 0xff1a1208 };
        juce::Colour sage { 0xff8fa37a };
    } colours;

    class LookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        explicit LookAndFeel (const Palette& p);
        void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
        void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
        juce::PopupMenu::Options getOptionsForComboBoxPopupMenu (juce::ComboBox&, juce::Label&) override;
        juce::Font getLabelFont (juce::Label&) override;

    private:
        Palette palette;
    };

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    struct Toggle
    {
        juce::ToggleButton button;
        std::unique_ptr<ButtonAttachment> attachment;
    };

    struct Choice
    {
        juce::ComboBox box;
        std::unique_ptr<ComboAttachment> attachment;
    };

    Knob& addKnob (const juce::String& text, const juce::String& paramID, float diameter);
    Toggle& addToggle (const juce::String& text, const juce::String& paramID);
    Choice& addChoice (const juce::String& paramID, const juce::StringArray& items);
    juce::TextButton& addProgramButton (const juce::String& text, int programIndex);
    void layoutKnobs (juce::Rectangle<int> area, std::initializer_list<Knob*> knobs);
    void paintSection (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title);

    OrbitfallAudioProcessor& audioProcessor;
    LookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 650 };

    juce::Label logo;
    juce::Label title;
    juce::Label hint;
    juce::ToggleButton bypassButton;
    std::unique_ptr<ButtonAttachment> bypassAttachment;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Toggle>> toggles;
    std::vector<std::unique_ptr<Choice>> choices;
    std::vector<std::unique_ptr<juce::TextButton>> programButtons;

    Choice* algorithm = nullptr;
    Choice* preType = nullptr;
    Choice* insertType = nullptr;
    Choice* modAType = nullptr;
    Choice* modATarget = nullptr;
    Choice* modBType = nullptr;
    Choice* modBTarget = nullptr;

    juce::Rectangle<int> reverbArea, predelayArea, gateArea, modArea, outputArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrbitfallAudioProcessorEditor)
};
