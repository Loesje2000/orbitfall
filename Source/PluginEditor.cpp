#include "PluginEditor.h"

namespace
{
juce::Font makeFont (float height, juce::Font::FontStyleFlags style = juce::Font::plain)
{
    return juce::Font (juce::FontOptions { height, style });
}
}

OrbitfallAudioProcessorEditor::LookAndFeel::LookAndFeel (const Palette& p)
    : palette (p)
{
    setColour (juce::Slider::textBoxTextColourId, palette.text);
    setColour (juce::Slider::textBoxBackgroundColourId, palette.dark);
    setColour (juce::Slider::textBoxOutlineColourId, palette.border);
    setColour (juce::Label::textColourId, palette.text);
    setColour (juce::ComboBox::backgroundColourId, palette.dark);
    setColour (juce::ComboBox::textColourId, palette.text);
    setColour (juce::ComboBox::outlineColourId, palette.border);
    setColour (juce::PopupMenu::backgroundColourId, palette.dark);
    setColour (juce::PopupMenu::textColourId, palette.text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, palette.panel);
}

void OrbitfallAudioProcessorEditor::LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                                                   float sliderPos, float rotaryStartAngle,
                                                                   float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto knob = juce::Rectangle<float> (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

    g.setColour (palette.dark);
    g.fillEllipse (knob.expanded (2.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4b3519), knob.getX(), knob.getY(),
                                             juce::Colour (0xff1a1208), knob.getRight(), knob.getBottom(), false));
    g.fillEllipse (knob);
    g.setColour (palette.border);
    g.drawEllipse (knob, 1.4f);

    for (int i = 0; i < 24; ++i)
    {
        const auto tickAngle = rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * ((float) i / 23.0f);
        const auto inner = centre.getPointOnCircumference (radius * 0.80f, tickAngle);
        const auto outer = centre.getPointOnCircumference (radius * 0.94f, tickAngle);
        g.setColour (palette.border.withAlpha (0.55f));
        g.drawLine ({ inner, outer }, 0.8f);
    }

    const auto pointer = centre.getPointOnCircumference (radius * 0.76f, angle);
    g.setColour (palette.accent);
    g.drawLine ({ centre, pointer }, 2.0f);
    g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (pointer));

    if (slider.isMouseOverOrDragging())
    {
        g.setColour (palette.accent.withAlpha (0.18f));
        g.drawEllipse (knob.reduced (1.0f), 2.0f);
    }
}

void OrbitfallAudioProcessorEditor::LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                                       const juce::Colour&, bool over, bool down)
{
    auto area = button.getLocalBounds().toFloat().reduced (0.5f);
    const auto active = button.getToggleState();
    g.setColour (active ? palette.panel.brighter (0.16f) : palette.dark);
    if (down) g.setColour (palette.panel.brighter (0.26f));
    if (over) g.setColour (active ? palette.panel.brighter (0.22f) : palette.panel);
    g.fillRoundedRectangle (area, 5.0f);
    g.setColour (active ? palette.accent : palette.border);
    g.drawRoundedRectangle (area, 5.0f, active ? 1.5f : 1.0f);
}

void OrbitfallAudioProcessorEditor::LookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                                               int, int, int, int, juce::ComboBox&)
{
    auto area = juce::Rectangle<float> (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f);
    g.setColour (palette.dark);
    g.fillRoundedRectangle (area, 5.0f);
    g.setColour (palette.border);
    g.drawRoundedRectangle (area, 5.0f, 1.0f);
    juce::Path arrow;
    arrow.addTriangle ((float) width - 17.0f, (float) height * 0.42f,
                       (float) width - 7.0f, (float) height * 0.42f,
                       (float) width - 12.0f, (float) height * 0.64f);
    g.setColour (palette.accent);
    g.fillPath (arrow);
}

juce::Font OrbitfallAudioProcessorEditor::LookAndFeel::getLabelFont (juce::Label& label)
{
    return makeFont (label.getName() == "section" ? 13.0f : 11.0f,
                     label.getName() == "section" ? juce::Font::bold : juce::Font::plain);
}

OrbitfallAudioProcessorEditor::OrbitfallAudioProcessorEditor (OrbitfallAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), lookAndFeel (colours)
{
    setLookAndFeel (&lookAndFeel);
    setResizable (true, true);
    setResizeLimits (820, 600, 1400, 900);

    logo.setText ("Rubblesonic", juce::dontSendNotification);
    logo.setColour (juce::Label::textColourId, colours.sage);
    logo.setJustificationType (juce::Justification::centredLeft);
    logo.setFont (makeFont (17.0f, juce::Font::bold));
    addAndMakeVisible (logo);

    title.setText ("orbitfall", juce::dontSendNotification);
    title.setColour (juce::Label::textColourId, colours.text);
    title.setJustificationType (juce::Justification::centredLeft);
    title.setFont (makeFont (18.0f));
    addAndMakeVisible (title);

    hint.setText ("click feedback insert type to expand routing", juce::dontSendNotification);
    hint.setColour (juce::Label::textColourId, colours.muted);
    hint.setJustificationType (juce::Justification::centred);
    hint.setFont (makeFont (11.0f));
    addAndMakeVisible (hint);

    addProgramButton ("<", 0);
    addProgramButton ("Cathedra dark", 0);
    addProgramButton (">", 1);

    algorithm = &addChoice ("reverb_algorithm", { "cathedra", "ultraplate", "gravity", "78 hall", "prism", "spring" });
    addKnob ("decay", "reverb_decay", 56.0f);
    addKnob ("size", "reverb_size", 56.0f);
    addKnob ("diffusion", "reverb_diffusion", 56.0f);
    addKnob ("lo freq", "reverb_lo_freq", 34.0f);
    addKnob ("hi freq", "reverb_hi_freq", 34.0f);
    addKnob ("pitch", "reverb_pitch", 34.0f);
    addKnob ("pitch mix", "reverb_pitch_mix", 34.0f);

    preType = &addChoice ("pre_type", { "tape", "BBD", "digital" });
    addKnob ("time", "pre_time", 44.0f);
    addKnob ("feedback", "pre_feedback", 44.0f);
    addKnob ("crossfeed", "pre_crossfeed", 44.0f);
    addKnob ("mod", "pre_mod", 44.0f);
    addToggle ("half speed", "pre_half_speed");
    addKnob ("dry blend", "pre_dry_blend", 34.0f);

    addKnob ("attack", "gate_attack", 44.0f);
    addKnob ("hold", "gate_hold", 44.0f);
    addKnob ("decay", "gate_decay", 44.0f);
    insertType = &addChoice ("insert_type", { "none", "lo-fi pitch", "ladder filter", "hazy", "diffusion" });
    addKnob ("amount", "insert_amount", 34.0f);
    addKnob ("mix", "insert_mix", 34.0f);
    addKnob ("param", "insert_param", 34.0f);

    modAType = &addChoice ("mod_a_type", { "LFO", "env", "S+H", "seq" });
    addKnob ("rate", "mod_a_rate", 34.0f);
    addKnob ("depth", "mod_a_depth", 34.0f);
    modATarget = &addChoice ("mod_a_target", { "decay", "pre time", "pitch", "diffusion", "lo freq", "hi freq", "feedback", "crossfeed", "gate attack", "gate hold", "insert amount", "hazy age" });
    modBType = &addChoice ("mod_b_type", { "LFO", "env", "S+H", "seq" });
    addKnob ("rate", "mod_b_rate", 34.0f);
    addKnob ("depth", "mod_b_depth", 34.0f);
    modBTarget = &addChoice ("mod_b_target", { "decay", "pre time", "pitch", "diffusion", "lo freq", "hi freq", "feedback", "crossfeed", "gate attack", "gate hold", "insert amount", "hazy age" });
    addKnob ("age", "hazy_age", 34.0f);
    addKnob ("warble", "hazy_warble", 34.0f);
    addKnob ("decimate", "hazy_decimate", 34.0f);
    addKnob ("mix", "hazy_mix", 34.0f);

    addKnob ("mix", "mix", 56.0f);
    addKnob ("dry trim", "dry_trim", 56.0f);
    addKnob ("wet trim", "wet_trim", 56.0f);
    addToggle ("spillover", "spillover");
    addToggle ("trails", "trails");

    setSize (980, 660);
}

OrbitfallAudioProcessorEditor::Knob& OrbitfallAudioProcessorEditor::addKnob (const juce::String& text,
                                                                             const juce::String& paramID,
                                                                             float diameter)
{
    auto knob = std::make_unique<Knob>();
    knob->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::jmax (42, (int) diameter + 8), 16);
    knob->slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    knob->slider.setName (juce::String (diameter));
    knob->label.setText (text, juce::dontSendNotification);
    knob->label.setJustificationType (juce::Justification::centred);
    knob->label.setColour (juce::Label::textColourId, diameter < 40.0f ? colours.muted : colours.text);
    knob->label.setFont (makeFont (11.0f));
    knob->attachment = std::make_unique<SliderAttachment> (audioProcessor.parameters, paramID, knob->slider);
    addAndMakeVisible (knob->slider);
    addAndMakeVisible (knob->label);
    knobs.push_back (std::move (knob));
    return *knobs.back();
}

OrbitfallAudioProcessorEditor::Toggle& OrbitfallAudioProcessorEditor::addToggle (const juce::String& text,
                                                                                 const juce::String& paramID)
{
    auto toggle = std::make_unique<Toggle>();
    toggle->button.setButtonText (text);
    toggle->button.setColour (juce::ToggleButton::textColourId, colours.text);
    toggle->attachment = std::make_unique<ButtonAttachment> (audioProcessor.parameters, paramID, toggle->button);
    addAndMakeVisible (toggle->button);
    toggles.push_back (std::move (toggle));
    return *toggles.back();
}

OrbitfallAudioProcessorEditor::Choice& OrbitfallAudioProcessorEditor::addChoice (const juce::String& paramID,
                                                                                 const juce::StringArray& items)
{
    auto choice = std::make_unique<Choice>();
    choice->box.addItemList (items, 1);
    choice->box.setJustificationType (juce::Justification::centred);
    choice->attachment = std::make_unique<ComboAttachment> (audioProcessor.parameters, paramID, choice->box);
    addAndMakeVisible (choice->box);
    choices.push_back (std::move (choice));
    return *choices.back();
}

juce::TextButton& OrbitfallAudioProcessorEditor::addProgramButton (const juce::String& text, int programIndex)
{
    auto button = std::make_unique<juce::TextButton> (text);
    button->setColour (juce::TextButton::textColourOffId, colours.text);
    button->setColour (juce::TextButton::buttonColourId, colours.dark);
    button->onClick = [this, programIndex]
    {
        const auto next = programIndex == 1 ? (audioProcessor.getCurrentProgram() + 1) % audioProcessor.getNumPrograms() : programIndex;
        audioProcessor.setCurrentProgram (next);
        if (programButtons.size() > 1)
            programButtons[1]->setButtonText (audioProcessor.getProgramName (audioProcessor.getCurrentProgram()));
    };
    addAndMakeVisible (*button);
    programButtons.push_back (std::move (button));
    return *programButtons.back();
}

void OrbitfallAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours.background);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRect (getLocalBounds().withTrimmedTop (48));

    paintSection (g, reverbArea, "reverb");
    paintSection (g, predelayArea, "predelay");
    paintSection (g, gateArea, "gate + feedback insert");
    paintSection (g, modArea, "modulation");
    paintSection (g, outputArea, "output");
}

void OrbitfallAudioProcessorEditor::paintSection (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& titleText)
{
    g.setColour (colours.panel);
    g.fillRoundedRectangle (area.toFloat(), 7.0f);
    g.setColour (colours.border);
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 7.0f, 1.0f);
    g.setColour (colours.muted);
    g.setFont (makeFont (12.0f, juce::Font::bold));
    g.drawText (titleText, area.removeFromTop (22).reduced (12, 0), juce::Justification::centredLeft);
}

void OrbitfallAudioProcessorEditor::resized()
{
    if (programButtons.size() < 3 || knobs.size() < 29 || toggles.size() < 3)
        return;

    auto area = getLocalBounds().reduced (18);
    auto header = area.removeFromTop (40);
    logo.setBounds (header.removeFromLeft (124));
    logo.setTransform (juce::AffineTransform::rotation (juce::degreesToRadians (-6.0f), 42.0f, 20.0f));
    title.setBounds (header.removeFromLeft (140));

    auto preset = header.removeFromRight (290);
    programButtons[0]->setBounds (preset.removeFromLeft (34).reduced (2));
    programButtons[1]->setBounds (preset.removeFromLeft (190).reduced (2));
    programButtons[2]->setBounds (preset.removeFromLeft (34).reduced (2));

    reverbArea = area.removeFromTop (144).reduced (0, 4);
    auto middle = area.removeFromTop (156);
    predelayArea = middle.removeFromLeft (middle.getWidth() / 2).reduced (0, 4);
    gateArea = middle.reduced (8, 4);
    modArea = area.removeFromTop (140).reduced (0, 4);
    outputArea = area.removeFromTop (118).reduced (0, 4);
    hint.setBounds (area.reduced (0, 2));

    auto r = reverbArea.reduced (14, 28);
    algorithm->box.setBounds (r.removeFromTop (28));
    auto primary = r.removeFromTop (64);
    layoutKnobs (primary, { knobs[0].get(), knobs[1].get(), knobs[2].get() });
    layoutKnobs (r, { knobs[3].get(), knobs[4].get(), knobs[5].get(), knobs[6].get() });

    auto p = predelayArea.reduced (14, 28);
    preType->box.setBounds (p.removeFromTop (26).removeFromLeft (210));
    layoutKnobs (p.removeFromTop (70), { knobs[7].get(), knobs[8].get(), knobs[9].get(), knobs[10].get() });
    toggles[0]->button.setBounds (p.removeFromLeft (120).reduced (0, 8));
    layoutKnobs (p.removeFromLeft (92), { knobs[11].get() });

    auto g = gateArea.reduced (14, 28);
    layoutKnobs (g.removeFromTop (68), { knobs[12].get(), knobs[13].get(), knobs[14].get() });
    insertType->box.setBounds (g.removeFromTop (26).removeFromLeft (180));
    layoutKnobs (g, { knobs[15].get(), knobs[16].get(), knobs[17].get() });

    auto m = modArea.reduced (14, 28);
    auto colW = m.getWidth() / 3;
    auto ma = m.removeFromLeft (colW).reduced (0, 2);
    auto mb = m.removeFromLeft (colW).reduced (10, 2);
    auto hz = m.reduced (10, 2);
    modAType->box.setBounds (ma.removeFromTop (26));
    layoutKnobs (ma.removeFromLeft (126), { knobs[18].get(), knobs[19].get() });
    modATarget->box.setBounds (ma.removeFromTop (26).reduced (4, 0));
    modBType->box.setBounds (mb.removeFromTop (26));
    layoutKnobs (mb.removeFromLeft (126), { knobs[20].get(), knobs[21].get() });
    modBTarget->box.setBounds (mb.removeFromTop (26).reduced (4, 0));
    layoutKnobs (hz, { knobs[22].get(), knobs[23].get(), knobs[24].get(), knobs[25].get() });

    auto o = outputArea.reduced (14, 28);
    auto switches = o.removeFromRight (170);
    layoutKnobs (o, { knobs[26].get(), knobs[27].get(), knobs[28].get() });
    toggles[1]->button.setBounds (switches.removeFromTop (34).reduced (6));
    toggles[2]->button.setBounds (switches.removeFromTop (34).reduced (6));
}

void OrbitfallAudioProcessorEditor::layoutKnobs (juce::Rectangle<int> area, std::initializer_list<Knob*> items)
{
    const auto count = (int) items.size();
    if (count == 0)
        return;

    const auto cellW = area.getWidth() / count;
    int index = 0;
    for (auto* knob : items)
    {
        auto cell = area.withX (area.getX() + index * cellW).withWidth (cellW).reduced (4, 0);
        const auto diameter = knob->slider.getName().getFloatValue();
        auto sliderArea = cell.removeFromTop ((int) diameter + 20).withSizeKeepingCentre ((int) diameter + 18, (int) diameter + 20);
        knob->slider.setBounds (sliderArea);
        knob->label.setBounds (cell.removeFromTop (18));
        ++index;
    }
}
