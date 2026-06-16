#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
float sine (float phase)
{
    return std::sin (phase * juce::MathConstants<float>::twoPi);
}

float softLimit (float x)
{
    return std::tanh (x * 1.35f) * 0.74f;
}

float pctToMs (float value)
{
    return 1.0f + 2499.0f * std::pow (juce::jlimit (0.0f, 1.0f, value), 2.2f);
}

void setParamValue (OrbitfallAudioProcessor::APVTS& apvts, const char* id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        const auto normalised = p->convertTo0to1 (value);
        p->setValueNotifyingHost (normalised);
    }
}

void setChoiceValue (OrbitfallAudioProcessor::APVTS& apvts, const char* id, int index, int count)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (count <= 1 ? 0.0f : (float) index / (float) (count - 1));
}
}

OrbitfallAudioProcessor::OrbitfallAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "OrbitfallParameters", createParameterLayout())
{
    applyFactoryPreset (0);
}

bool OrbitfallAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void OrbitfallAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    predelay.prepare (sampleRate);
    reverb.prepare (sampleRate);
    gate.prepare (sampleRate);
    feedbackInsert.prepare (sampleRate);
    hazy.prepare (sampleRate);
    previousReverbL = previousReverbR = 0.0f;
}

float OrbitfallAudioProcessor::param (const char* id) const
{
    if (auto* p = parameters.getRawParameterValue (id))
        return p->load();
    return 0.0f;
}

int OrbitfallAudioProcessor::choice (const char* id) const
{
    return (int) std::lround (param (id));
}

bool OrbitfallAudioProcessor::toggle (const char* id) const
{
    return param (id) > 0.5f;
}

void OrbitfallAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    if (toggle ("bypass"))
        return;

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);
    const auto sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;

    const auto preTypeValue = choice ("pre_type");
    auto preTime = pctToMs (param ("pre_time"));
    auto preFeedback = param ("pre_feedback");
    auto preCrossfeed = param ("pre_crossfeed");
    const auto preMod = param ("pre_mod");
    const auto halfSpeed = toggle ("pre_half_speed");
    const auto dryBlend = param ("pre_dry_blend");

    const auto algorithmValue = choice ("reverb_algorithm");
    auto reverbDecay = param ("reverb_decay");
    const auto reverbSize = param ("reverb_size");
    auto reverbDiffusion = param ("reverb_diffusion");
    auto reverbLo = param ("reverb_lo_freq");
    auto reverbHi = param ("reverb_hi_freq");
    auto reverbPitch = param ("reverb_pitch");
    const auto reverbPitchMix = param ("reverb_pitch_mix");

    auto gateAttack = param ("gate_attack");
    auto gateHold = param ("gate_hold");
    const auto gateDecay = param ("gate_decay");
    const auto insertTypeValue = choice ("insert_type");
    auto insertAmount = param ("insert_amount");
    const auto insertMix = param ("insert_mix");
    const auto insertParam = param ("insert_param");
    auto hazyAge = param ("hazy_age");
    const auto hazyWarble = param ("hazy_warble");
    const auto hazyDecimate = param ("hazy_decimate");
    const auto hazyMix = param ("hazy_mix");
    const auto mix = param ("mix");
    const auto dryTrim = juce::Decibels::decibelsToGain (juce::jmap (param ("dry_trim"), -24.0f, 6.0f));
    const auto wetTrim = juce::Decibels::decibelsToGain (juce::jmap (param ("wet_trim"), -24.0f, 6.0f));

    const auto inputLevel = buffer.getMagnitude (0, buffer.getNumSamples());
    const auto modAValue = modA.process (choice ("mod_a_type"), param ("mod_a_rate"), param ("mod_a_depth"),
                                         choice ("mod_a_shape"), inputLevel, sr);
    const auto modBValue = modB.process (choice ("mod_b_type"), param ("mod_b_rate"), param ("mod_b_depth"),
                                         choice ("mod_b_shape"), inputLevel, sr);

    auto applyMod = [&] (int target, float value)
    {
        switch (target)
        {
            case 0: reverbDecay = juce::jlimit (0.0f, 1.0f, reverbDecay + value * 0.35f); break;
            case 1: preTime = juce::jlimit (1.0f, 2500.0f, preTime * std::pow (2.0f, value * 2.0f)); break;
            case 2: reverbPitch = juce::jlimit (-12.0f, 12.0f, reverbPitch + value * 12.0f); break;
            case 3: reverbDiffusion = juce::jlimit (0.0f, 1.0f, reverbDiffusion + value * 0.5f); break;
            case 4: reverbLo = juce::jlimit (0.0f, 1.0f, reverbLo + value * 0.5f); break;
            case 5: reverbHi = juce::jlimit (0.0f, 1.0f, reverbHi + value * 0.5f); break;
            case 6: preFeedback = juce::jlimit (0.0f, 1.0f, preFeedback + value * 0.5f); break;
            case 7: preCrossfeed = juce::jlimit (0.0f, 1.0f, preCrossfeed + value * 0.5f); break;
            case 8: gateAttack = juce::jlimit (0.0f, 1.0f, gateAttack + value * 0.5f); break;
            case 9: gateHold = juce::jlimit (0.0f, 1.0f, gateHold + value * 0.5f); break;
            case 10: insertAmount = juce::jlimit (0.0f, 1.0f, insertAmount + value * 0.5f); break;
            case 11: hazyAge = juce::jlimit (0.0f, 1.0f, hazyAge + value * 0.5f); break;
            default: break;
        }
    };
    applyMod (choice ("mod_a_target"), modAValue);
    applyMod (choice ("mod_b_target"), modBValue);

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto dryL = left[sample];
        const auto dryR = right[sample];

        float preL = 0.0f, preR = 0.0f;
        predelay.processSample (dryL, dryR, softLimit (previousReverbL * preFeedback), softLimit (previousReverbR * preFeedback),
                                preL, preR, preTypeValue, preTime, preFeedback, preCrossfeed, preMod, halfSpeed);

        const auto tankInL = (1.0f - dryBlend) * preL + dryBlend * dryL;
        const auto tankInR = (1.0f - dryBlend) * preR + dryBlend * dryR;

        float wetL = 0.0f, wetR = 0.0f;
        if (algorithmValue == 1)
            reverb.processHall78 (tankInL, tankInR, wetL, wetR, reverbDecay * 0.82f, reverbLo * 0.55f,
                                  juce::jlimit (0.0f, 1.0f, reverbHi + 0.18f), reverbSize * 0.35f,
                                  preMod * 0.25f, reverbDiffusion);
        else if (algorithmValue == 2)
            reverb.processGravityPlaceholder (tankInL, tankInR, wetL, wetR, reverbDecay, reverbLo, 0.2f + reverbSize * 4.0f, reverbDiffusion, reverbHi);
        else if (algorithmValue == 3)
            reverb.processHall78 (tankInL, tankInR, wetL, wetR, reverbDecay, reverbLo, reverbHi, reverbSize, preMod, reverbDiffusion);
        else if (algorithmValue == 4)
            reverb.processCathedra (tankInL, tankInR, wetL, wetR, reverbDecay, reverbSize,
                                    juce::jlimit (0.0f, 1.0f, reverbDiffusion + 0.18f),
                                    reverbLo, reverbHi, reverbPitch + 7.0f, juce::jlimit (0.0f, 1.0f, reverbPitchMix + 0.35f));
        else if (algorithmValue == 5)
            reverb.processHall78 (tankInL - tankInR * 0.45f, tankInR - tankInL * 0.45f, wetL, wetR,
                                  reverbDecay * 0.48f, reverbLo, reverbHi * 0.55f,
                                  0.18f + reverbSize * 0.25f, preMod + 0.28f, reverbDiffusion * 0.42f);
        else
            reverb.processCathedra (tankInL, tankInR, wetL, wetR, reverbDecay, reverbSize, reverbDiffusion,
                                    reverbLo, reverbHi, reverbPitch, reverbPitchMix);

        const auto gateGain = gate.process (0.5f * (std::abs (wetL) + std::abs (wetR)), gateAttack, gateHold, gateDecay);
        wetL *= gateGain;
        wetR *= gateGain;

        feedbackInsert.process (wetL, wetR, insertTypeValue, insertAmount, insertMix, insertParam);
        hazy.process (wetL, wetR, hazyAge, hazyWarble, hazyDecimate, hazyMix);

        previousReverbL = wetL;
        previousReverbR = wetR;

        left[sample] = dryL * dryTrim * (1.0f - mix) + wetL * wetTrim * mix;
        right[sample] = dryR * dryTrim * (1.0f - mix) + wetR * wetTrim * mix;
    }
}

juce::AudioProcessorEditor* OrbitfallAudioProcessor::createEditor()
{
    return new OrbitfallAudioProcessorEditor (*this);
}

void OrbitfallAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = parameters.copyState(); auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void OrbitfallAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

const juce::String OrbitfallAudioProcessor::getProgramName (int index)
{
    static const juce::StringArray names { "Cathedra dark", "Spiral hall", "Empty room" };
    return names[index % names.size()];
}

void OrbitfallAudioProcessor::setCurrentProgram (int index)
{
    applyFactoryPreset (juce::jlimit (0, 2, index));
}

void OrbitfallAudioProcessor::applyFactoryPreset (int index)
{
    currentProgram = index;

    setChoiceValue (parameters, "reverb_algorithm", index == 2 ? 3 : 0, 6);
    setChoiceValue (parameters, "pre_type", index == 2 ? 0 : 2, 3);
    setChoiceValue (parameters, "insert_type", index == 1 ? 1 : 0, 5);
    setChoiceValue (parameters, "mod_a_type", index == 1 ? 3 : 0, 4);
    setChoiceValue (parameters, "mod_a_target", index == 1 ? 2 : 0, 12);
    setChoiceValue (parameters, "mod_b_type", 0, 4);
    setChoiceValue (parameters, "mod_b_target", 0, 12);

    if (index == 0)
    {
        setParamValue (parameters, "mix", 0.78f);
        setParamValue (parameters, "dry_trim", 0.17f);
        setParamValue (parameters, "wet_trim", 0.26f);
        setParamValue (parameters, "pre_time", 0.07f);
        setParamValue (parameters, "pre_feedback", 0.33f);
        setParamValue (parameters, "pre_crossfeed", 1.0f);
        setParamValue (parameters, "pre_mod", 0.5f);
        setParamValue (parameters, "pre_dry_blend", 0.5f);
        setParamValue (parameters, "reverb_decay", 0.9f);
        setParamValue (parameters, "reverb_size", 0.72f);
        setParamValue (parameters, "reverb_diffusion", 0.82f);
        setParamValue (parameters, "reverb_lo_freq", 0.55f);
        setParamValue (parameters, "reverb_hi_freq", 0.48f);
        setParamValue (parameters, "reverb_pitch", 2.2f);
        setParamValue (parameters, "reverb_pitch_mix", 0.19f);
        setParamValue (parameters, "gate_attack", 0.39f);
        setParamValue (parameters, "gate_hold", 1.0f);
        setParamValue (parameters, "gate_decay", 0.41f);
        setParamValue (parameters, "spillover", 1.0f);
    }
    else if (index == 1)
    {
        setParamValue (parameters, "mix", 0.24f);
        setParamValue (parameters, "dry_trim", 1.0f);
        setParamValue (parameters, "wet_trim", 0.8f);
        setParamValue (parameters, "pre_time", 0.34f);
        setParamValue (parameters, "pre_feedback", 0.17f);
        setParamValue (parameters, "pre_crossfeed", 0.17f);
        setParamValue (parameters, "pre_mod", 0.46f);
        setParamValue (parameters, "pre_half_speed", 1.0f);
        setParamValue (parameters, "pre_dry_blend", 0.15f);
        setParamValue (parameters, "reverb_decay", 0.97f);
        setParamValue (parameters, "reverb_size", 0.91f);
        setParamValue (parameters, "reverb_diffusion", 0.78f);
        setParamValue (parameters, "reverb_pitch", 3.4f);
        setParamValue (parameters, "reverb_pitch_mix", 1.0f);
        setParamValue (parameters, "gate_hold", 0.11f);
        setParamValue (parameters, "insert_amount", 0.42f);
        setParamValue (parameters, "insert_mix", 0.65f);
        setParamValue (parameters, "insert_param", 0.58f);
        setParamValue (parameters, "mod_a_rate", 0.08f);
        setParamValue (parameters, "mod_a_depth", 0.24f);
    }
    else
    {
        setParamValue (parameters, "mix", 0.3f);
        setParamValue (parameters, "dry_trim", 1.0f);
        setParamValue (parameters, "wet_trim", 1.0f);
        setParamValue (parameters, "pre_time", 0.082f);
        setParamValue (parameters, "pre_feedback", 0.0f);
        setParamValue (parameters, "pre_crossfeed", 0.0f);
        setParamValue (parameters, "pre_mod", 0.2f);
        setParamValue (parameters, "pre_half_speed", 0.0f);
        setParamValue (parameters, "pre_dry_blend", 0.0f);
        setParamValue (parameters, "reverb_decay", 0.4f);
        setParamValue (parameters, "reverb_size", 0.5f);
        setParamValue (parameters, "reverb_diffusion", 0.6f);
        setParamValue (parameters, "gate_attack", 0.0f);
        setParamValue (parameters, "gate_hold", 0.0f);
        setParamValue (parameters, "gate_decay", 0.0f);
    }
}

OrbitfallAudioProcessor::APVTS::ParameterLayout OrbitfallAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    auto addFloat = [&] (const char* id, const char* name, float min, float max, float def)
    {
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, min, max, def));
    };
    auto addBool = [&] (const char* id, const char* name, bool def)
    {
        params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
    };
    auto addChoice = [&] (const char* id, const char* name, juce::StringArray items, int def)
    {
        params.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def));
    };

    addChoice ("reverb_algorithm", "Reverb Algorithm", { "cathedra", "ultraplate", "gravity", "78 hall", "prism", "spring" }, 0);
    addFloat ("reverb_decay", "Decay", 0.0f, 1.0f, 0.9f);
    addFloat ("reverb_size", "Size", 0.0f, 1.0f, 0.72f);
    addFloat ("reverb_diffusion", "Diffusion", 0.0f, 1.0f, 0.82f);
    addFloat ("reverb_lo_freq", "Lo Freq", 0.0f, 1.0f, 0.55f);
    addFloat ("reverb_hi_freq", "Hi Freq", 0.0f, 1.0f, 0.48f);
    addFloat ("reverb_pitch", "Pitch", -12.0f, 12.0f, 2.2f);
    addFloat ("reverb_pitch_mix", "Pitch Mix", 0.0f, 1.0f, 0.19f);

    addChoice ("pre_type", "Predelay Type", { "tape", "BBD", "digital" }, 2);
    addFloat ("pre_time", "Predelay Time", 0.0f, 1.0f, 0.07f);
    addFloat ("pre_feedback", "Predelay Feedback", 0.0f, 1.0f, 0.33f);
    addFloat ("pre_crossfeed", "Predelay Crossfeed", 0.0f, 1.0f, 1.0f);
    addFloat ("pre_mod", "Predelay Mod", 0.0f, 1.0f, 0.5f);
    addBool ("pre_half_speed", "Half Speed", false);
    addFloat ("pre_dry_blend", "Dry Blend", 0.0f, 1.0f, 0.5f);

    addFloat ("gate_attack", "Gate Attack", 0.0f, 1.0f, 0.39f);
    addFloat ("gate_hold", "Gate Hold", 0.0f, 1.0f, 1.0f);
    addFloat ("gate_decay", "Gate Decay", 0.0f, 1.0f, 0.41f);

    addChoice ("insert_type", "Feedback Insert", { "none", "lo-fi pitch", "ladder filter", "hazy", "diffusion" }, 0);
    addFloat ("insert_amount", "Insert Amount", 0.0f, 1.0f, 0.0f);
    addFloat ("insert_mix", "Insert Mix", 0.0f, 1.0f, 0.0f);
    addFloat ("insert_param", "Insert Param", 0.0f, 1.0f, 0.5f);

    addChoice ("mod_a_type", "Modifier A Type", { "LFO", "env", "S+H", "seq" }, 0);
    addFloat ("mod_a_rate", "Modifier A Rate", 0.01f, 20.0f, 0.08f);
    addFloat ("mod_a_depth", "Modifier A Depth", 0.0f, 1.0f, 0.0f);
    addChoice ("mod_a_shape", "Modifier A Shape", { "sine", "tri", "ramp up", "ramp down", "square", "3 step", "4 step" }, 0);
    addChoice ("mod_a_target", "Modifier A Target", { "decay", "pre time", "pitch", "diffusion", "lo freq", "hi freq", "feedback", "crossfeed", "gate attack", "gate hold", "insert amount", "hazy age" }, 0);
    addChoice ("mod_b_type", "Modifier B Type", { "LFO", "env", "S+H", "seq" }, 0);
    addFloat ("mod_b_rate", "Modifier B Rate", 0.01f, 20.0f, 0.1f);
    addFloat ("mod_b_depth", "Modifier B Depth", 0.0f, 1.0f, 0.0f);
    addChoice ("mod_b_shape", "Modifier B Shape", { "sine", "tri", "ramp up", "ramp down", "square", "3 step", "4 step" }, 0);
    addChoice ("mod_b_target", "Modifier B Target", { "decay", "pre time", "pitch", "diffusion", "lo freq", "hi freq", "feedback", "crossfeed", "gate attack", "gate hold", "insert amount", "hazy age" }, 0);

    addFloat ("hazy_age", "Hazy Age", 0.0f, 1.0f, 0.0f);
    addFloat ("hazy_warble", "Hazy Warble", 0.0f, 1.0f, 0.0f);
    addFloat ("hazy_decimate", "Hazy Decimate", 0.0f, 1.0f, 0.0f);
    addFloat ("hazy_mix", "Hazy Mix", 0.0f, 1.0f, 0.0f);

    addFloat ("mix", "Mix", 0.0f, 1.0f, 0.78f);
    addFloat ("dry_trim", "Dry Trim", 0.0f, 1.0f, 0.17f);
    addFloat ("wet_trim", "Wet Trim", 0.0f, 1.0f, 0.26f);
    addBool ("spillover", "Spillover", true);
    addBool ("trails", "Trails", true);
    addBool ("bypass", "Bypass", false);

    return { params.begin(), params.end() };
}

void OrbitfallAudioProcessor::DelayLine::prepare (double newSampleRate, double maxSeconds)
{
    sampleRate = newSampleRate;
    buffer.setSize (1, (int) std::ceil (sampleRate * maxSeconds) + 8);
    reset();
}

void OrbitfallAudioProcessor::DelayLine::reset()
{
    buffer.clear();
    writeIndex = 0;
    halfWritePhase = false;
}

void OrbitfallAudioProcessor::DelayLine::write (float sample)
{
    buffer.setSample (0, writeIndex, sample);
    writeIndex = (writeIndex + 1) % buffer.getNumSamples();
}

float OrbitfallAudioProcessor::DelayLine::read (float delaySamples, float modulationSamples, bool halfSpeed)
{
    auto effectiveDelay = delaySamples * (halfSpeed ? 2.0f : 1.0f) + modulationSamples;
    auto readPosition = (float) writeIndex - effectiveDelay;
    while (readPosition < 0.0f)
        readPosition += (float) buffer.getNumSamples();

    const auto i0 = (int) std::floor (readPosition);
    const auto frac = readPosition - (float) i0;
    return hermite (getSampleWrapped (i0 - 1), getSampleWrapped (i0), getSampleWrapped (i0 + 1), getSampleWrapped (i0 + 2), frac);
}

float OrbitfallAudioProcessor::DelayLine::getSampleWrapped (int index) const
{
    const auto size = buffer.getNumSamples();
    while (index < 0) index += size;
    return buffer.getSample (0, index % size);
}

float OrbitfallAudioProcessor::DelayLine::hermite (float xm1, float x0, float x1, float x2, float frac)
{
    const auto c = (x1 - xm1) * 0.5f;
    const auto v = x0 - x1;
    const auto w = c + v;
    const auto a = w + v + (x2 - x0) * 0.5f;
    const auto bNeg = w + a;
    return (((a * frac) - bNeg) * frac + c) * frac + x0;
}

void OrbitfallAudioProcessor::PredelayEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    left.prepare (sampleRate, 5.6);
    right.prepare (sampleRate, 5.6);
    halfBlend.reset (sampleRate, 0.05);
    reset();
}

void OrbitfallAudioProcessor::PredelayEngine::reset()
{
    left.reset();
    right.reset();
    lfoPhaseA = 0.0f;
    lfoPhaseB = 0.31f;
    jitter = 0.0f;
    bbdLpL = bbdLpR = 0.0f;
}

void OrbitfallAudioProcessor::PredelayEngine::processSample (float inL, float inR, float fbL, float fbR,
                                                             float& outL, float& outR, int type, float timeMs,
                                                             float feedback, float crossfeed, float mod, bool halfSpeed)
{
    juce::ignoreUnused (feedback);
    if (halfSpeed != lastHalfSpeed)
    {
        halfBlend.setTargetValue (halfSpeed ? 1.0f : 0.0f);
        lastHalfSpeed = halfSpeed;
    }

    const auto depthSamples = mod * 0.004f * (float) sampleRate;
    const auto tapeLfo = 0.7f * sine (lfoPhaseA) + 0.3f * std::tanh (1.6f * sine (lfoPhaseB));
    const auto bbdLfo = sine (lfoPhaseA) + jitter * 0.15f;
    const auto digitalLfo = sine (lfoPhaseA) * 0.35f;
    const auto lfo = type == tape ? tapeLfo : (type == bbd ? bbdLfo : digitalLfo);
    const auto delaySamples = timeMs * 0.001f * (float) sampleRate;

    auto rawL = left.read (delaySamples, depthSamples * lfo, halfSpeed);
    auto rawR = right.read (delaySamples * 1.013f, -depthSamples * lfo, halfSpeed);

    if (type == bbd)
    {
        const auto coeff = 0.18f;
        bbdLpL += coeff * (rawL - bbdLpL);
        bbdLpR += coeff * (rawR - bbdLpR);
        rawL = bbdLpL;
        rawR = bbdLpR;
    }
    else if (type == tape)
    {
        const auto coeff = 0.12f;
        bbdLpL += coeff * (rawL - bbdLpL);
        bbdLpR += coeff * (rawR - bbdLpR);
        rawL = juce::jmap (mod, rawL, bbdLpL);
        rawR = juce::jmap (mod, rawR, bbdLpR);
    }

    outL = (1.0f - crossfeed) * rawL + crossfeed * rawR;
    outR = crossfeed * rawL + (1.0f - crossfeed) * rawR;

    const auto inputL = inL + fbL;
    const auto inputR = inR + fbR;
    if (! halfSpeed || left.halfWritePhase)
    {
        left.write (inputL);
        right.write (inputR);
    }
    left.halfWritePhase = ! left.halfWritePhase;
    right.halfWritePhase = left.halfWritePhase;

    lfoPhaseA += (type == tape ? 0.3f : 0.08f) / (float) sampleRate;
    lfoPhaseB += 0.7f / (float) sampleRate;
    if (lfoPhaseA >= 1.0f)
    {
        lfoPhaseA -= 1.0f;
        jitter = random.nextFloat() * 2.0f - 1.0f;
    }
    if (lfoPhaseB >= 1.0f)
        lfoPhaseB -= 1.0f;
}

void OrbitfallAudioProcessor::ReverbGate::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    reset();
}

void OrbitfallAudioProcessor::ReverbGate::reset()
{
    state = closed;
    envelope = 0.0f;
    holdSamplesRemaining = 0;
}

float OrbitfallAudioProcessor::ReverbGate::process (float inputLevel, float attack, float hold, float decay)
{
    if (attack <= 0.01f && hold <= 0.01f && decay <= 0.01f)
        return 1.0f;

    if (inputLevel > 0.0008f && state == closed)
        state = attacking;

    const auto attackSamples = (float) sampleRate * juce::jmap (attack, 0.005f, 3.0f);
    const auto decaySamples = (float) sampleRate * juce::jmap (decay, 0.02f, 8.0f);

    if (state == attacking)
    {
        envelope += 1.0f / juce::jmax (1.0f, attackSamples);
        if (envelope >= 1.0f)
        {
            envelope = 1.0f;
            state = holding;
            holdSamplesRemaining = hold >= 0.99f ? std::numeric_limits<int>::max() : (int) (hold * 8.0f * sampleRate);
        }
    }
    else if (state == holding)
    {
        if (holdSamplesRemaining != std::numeric_limits<int>::max() && --holdSamplesRemaining <= 0)
            state = decaying;
    }
    else if (state == decaying)
    {
        envelope -= 1.0f / juce::jmax (1.0f, decaySamples);
        if (envelope <= 0.0f)
        {
            envelope = 0.0f;
            state = closed;
        }
    }

    return juce::jlimit (0.0f, 1.0f, envelope);
}

void OrbitfallAudioProcessor::SimpleReverb::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, 1, 1 };
    reverbA.setSampleRate (sampleRate);
    reverbB.setSampleRate (sampleRate);
    shimmerL.prepare (spec);
    shimmerR.prepare (spec);
    reset();
}

void OrbitfallAudioProcessor::SimpleReverb::reset()
{
    reverbA.reset();
    reverbB.reset();
    shimmerL.reset();
    shimmerR.reset();
    gravityL = gravityR = 0.0f;
    shimmerPhase = gravityPhase = 0.0f;
}

void OrbitfallAudioProcessor::SimpleReverb::processCathedra (float inL, float inR, float& outL, float& outR,
                                                             float decay, float size, float diffusion,
                                                             float loFreq, float hiFreq, float pitch, float pitchMix)
{
    juce::Reverb::Parameters rp;
    rp.roomSize = juce::jlimit (0.0f, 1.0f, 0.55f + 0.45f * size);
    rp.damping = 1.0f - hiFreq * 0.75f;
    rp.wetLevel = 1.0f;
    rp.dryLevel = 0.0f;
    rp.width = 0.95f;
    rp.freezeMode = decay > 0.995f ? 1.0f : 0.0f;
    reverbA.setParameters (rp);

    auto l = inL;
    auto r = inR;
    reverbA.processStereo (&l, &r, 1);

    const auto pitchRatio = std::pow (2.0f, pitch / 12.0f);
    const auto modDelay = 18.0f + 12.0f * sine (shimmerPhase);
    shimmerL.setDelay (juce::jlimit (1.0f, 3800.0f, modDelay * pitchRatio));
    shimmerR.setDelay (juce::jlimit (1.0f, 3800.0f, (modDelay + 3.0f) * pitchRatio));
    const auto shL = shimmerL.popSample (0);
    const auto shR = shimmerR.popSample (0);
    shimmerL.pushSample (0, l + shR * 0.42f * diffusion);
    shimmerR.pushSample (0, r + shL * 0.42f * diffusion);

    shimmerPhase += (0.03f + 0.08f * diffusion) / (float) sampleRate;
    if (shimmerPhase >= 1.0f)
        shimmerPhase -= 1.0f;

    const auto lowGain = juce::Decibels::decibelsToGain (juce::jmap (loFreq, -3.0f, 2.0f));
    const auto highGain = juce::Decibels::decibelsToGain (juce::jmap (hiFreq, -7.0f, 3.0f));
    const auto toneGain = 0.5f * (lowGain + highGain);

    outL = juce::jmap (pitchMix, l, shL) * toneGain;
    outR = juce::jmap (pitchMix, r, shR) * toneGain;
}

void OrbitfallAudioProcessor::SimpleReverb::processHall78 (float inL, float inR, float& outL, float& outR,
                                                           float mids, float bass, float treble, float cross, float tankMod, float diffusion)
{
    juce::Reverb::Parameters rp;
    rp.roomSize = juce::jlimit (0.0f, 1.0f, 0.38f + 0.62f * mids);
    rp.damping = 1.0f - treble * 0.85f;
    rp.wetLevel = 1.0f;
    rp.dryLevel = 0.0f;
    rp.width = 0.82f + 0.18f * diffusion;
    reverbB.setParameters (rp);

    auto l = inL * (0.65f + 0.35f * bass);
    auto r = inR * (0.65f + 0.35f * bass);
    reverbB.processStereo (&l, &r, 1);
    const auto tilt = juce::jmap (cross, -0.18f, 0.18f);
    const auto mod = 1.0f + tankMod * 0.04f * sine (shimmerPhase);
    shimmerPhase += 0.11f / (float) sampleRate;
    if (shimmerPhase >= 1.0f)
        shimmerPhase -= 1.0f;
    outL = (l + tilt * r) * mod;
    outR = (r - tilt * l) * mod;
}

void OrbitfallAudioProcessor::SimpleReverb::processGravityPlaceholder (float inL, float inR, float& outL, float& outR,
                                                                       float decay, float tilt, float modSpeed, float modDepth, float gain)
{
    const auto envelope = 0.995f + decay * 0.0045f;
    const auto wobble = 0.25f + 0.75f * std::abs (sine (gravityPhase));
    gravityL = gravityL * envelope + inL * (0.002f + 0.018f * modDepth) * wobble;
    gravityR = gravityR * envelope + inR * (0.002f + 0.018f * modDepth) * (1.0f - 0.35f * wobble);
    gravityPhase += modSpeed / (float) sampleRate;
    if (gravityPhase >= 1.0f)
        gravityPhase -= 1.0f;
    outL = gravityL * juce::jmap (gain, 0.5f, 2.2f) * (1.0f - tilt * 0.25f);
    outR = gravityR * juce::jmap (gain, 0.5f, 2.2f) * (1.0f + tilt * 0.25f);
}

void OrbitfallAudioProcessor::FeedbackInsert::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, 1, 1 };
    svfL.prepare (spec);
    svfR.prepare (spec);
    pitchL.prepare (spec);
    pitchR.prepare (spec);
    reset();
}

void OrbitfallAudioProcessor::FeedbackInsert::reset()
{
    svfL.reset();
    svfR.reset();
    pitchL.reset();
    pitchR.reset();
    pitchPhase = hazyPhase = 0.0f;
}

void OrbitfallAudioProcessor::FeedbackInsert::process (float& left, float& right, int type, float amount, float mix, float p)
{
    if (type == 0 || amount <= 0.001f || mix <= 0.001f)
        return;

    auto wetL = left;
    auto wetR = right;
    if (type == 1)
    {
        const auto semitones = juce::jmap (p, -5.0f, 7.0f);
        const auto ratio = std::pow (2.0f, semitones / 12.0f);
        pitchL.setDelay (juce::jlimit (1.0f, 3800.0f, 240.0f + 80.0f * sine (pitchPhase) * ratio));
        pitchR.setDelay (juce::jlimit (1.0f, 3800.0f, 255.0f - 80.0f * sine (pitchPhase) * ratio));
        wetL = pitchL.popSample (0);
        wetR = pitchR.popSample (0);
        pitchL.pushSample (0, left);
        pitchR.pushSample (0, right);
        pitchPhase += 2.5f / (float) sampleRate;
        if (pitchPhase >= 1.0f) pitchPhase -= 1.0f;
    }
    else if (type == 2)
    {
        const auto cutoff = 80.0f * std::pow (200.0f, p);
        svfL.setCutoffFrequency (cutoff);
        svfR.setCutoffFrequency (cutoff * (1.0f + amount * 0.08f));
        svfL.setResonance (0.7f + amount * 4.0f);
        svfR.setResonance (0.7f + amount * 4.0f);
        svfL.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        svfR.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        wetL = svfL.processSample (0, left);
        wetR = svfR.processSample (0, right);
    }
    else if (type == 3)
    {
        const auto noise = (std::sin (hazyPhase * 91.7f) * 0.003f * amount);
        wetL = std::tanh ((left + noise) * (1.0f + amount));
        wetR = std::tanh ((right - noise) * (1.0f + amount));
        hazyPhase += (0.07f + p * 1.4f) / (float) sampleRate;
        if (hazyPhase >= 1.0f) hazyPhase -= 1.0f;
    }
    else if (type == 4)
    {
        wetL = 0.72f * left + 0.28f * right;
        wetR = 0.72f * right + 0.28f * left;
    }

    left = juce::jmap (mix, left, softLimit (juce::jmap (amount, left, wetL)));
    right = juce::jmap (mix, right, softLimit (juce::jmap (amount, right, wetR)));
}

void OrbitfallAudioProcessor::HazyProcessor::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    reset();
}

void OrbitfallAudioProcessor::HazyProcessor::reset()
{
    phase = 0.0f;
    holdL = holdR = counter = 0.0f;
}

void OrbitfallAudioProcessor::HazyProcessor::process (float& left, float& right, float age, float warble, float decimate, float mix)
{
    if (mix <= 0.001f)
        return;

    const auto dryL = left;
    const auto dryR = right;
    const auto noise = (random.nextFloat() * 2.0f - 1.0f) * age * 0.001f;
    const auto wobble = 1.0f + warble * 0.015f * sine (phase);

    counter += juce::jmap (decimate, 1.0f, 0.08f);
    if (counter >= 1.0f)
    {
        counter -= 1.0f;
        holdL = std::tanh ((left + noise) * wobble);
        holdR = std::tanh ((right - noise) / wobble);
    }

    phase += (0.1f + warble * 1.9f) / (float) sampleRate;
    if (phase >= 1.0f)
        phase -= 1.0f;

    left = juce::jmap (mix, dryL, holdL);
    right = juce::jmap (mix, dryR, holdR);
}

float OrbitfallAudioProcessor::Modifier::process (int type, float rate, float depth, int shape, float inputLevel, double sampleRate)
{
    if (depth <= 0.001f)
        return 0.0f;

    auto value = 0.0f;
    if (type == 1)
    {
        envelope = inputLevel > 0.01f ? juce::jmin (1.0f, envelope + 0.002f) : envelope * 0.9995f;
        value = envelope * 2.0f - 1.0f;
    }
    else if (type == 2)
    {
        if (phase < rate / (float) sampleRate)
            held = random.nextFloat() * 2.0f - 1.0f;
        value = held;
    }
    else if (type == 3)
    {
        static constexpr float steps[] { 0.14f, 0.28f, 0.43f, 0.57f, 0.72f, 0.86f, 1.0f };
        value = steps[(int) (phase * 7.0f) % 7] * 2.0f - 1.0f;
    }
    else
    {
        if (shape == 1) value = 1.0f - 4.0f * std::abs (phase - 0.5f);
        else if (shape == 2) value = phase * 2.0f - 1.0f;
        else if (shape == 3) value = 1.0f - phase * 2.0f;
        else if (shape == 4) value = phase < 0.5f ? 1.0f : -1.0f;
        else value = sine (phase);
    }

    phase += rate / (float) sampleRate;
    while (phase >= 1.0f)
        phase -= 1.0f;

    return value * depth;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OrbitfallAudioProcessor();
}
