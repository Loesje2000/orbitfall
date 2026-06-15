#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

class OrbitfallAudioProcessor final : public juce::AudioProcessor
{
public:
    using APVTS = juce::AudioProcessorValueTreeState;

    OrbitfallAudioProcessor();
    ~OrbitfallAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return 3; }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static APVTS::ParameterLayout createParameterLayout();
    APVTS parameters;

private:
    struct DelayLine
    {
        void prepare (double newSampleRate, double maxSeconds);
        void reset();
        void write (float sample);
        float read (float delaySamples, float modulationSamples, bool halfSpeed);

        juce::AudioBuffer<float> buffer;
        double sampleRate = 48000.0;
        int writeIndex = 0;
        bool halfWritePhase = false;

    private:
        float getSampleWrapped (int index) const;
        static float hermite (float xm1, float x0, float x1, float x2, float frac);
    };

    struct PredelayEngine
    {
        enum Type { tape = 0, bbd, digital };

        void prepare (double newSampleRate);
        void reset();
        void processSample (float inL, float inR, float fbL, float fbR,
                            float& outL, float& outR, int type, float timeMs,
                            float feedback, float crossfeed, float mod, bool halfSpeed);

        DelayLine left, right;
        double sampleRate = 48000.0;
        float lfoPhaseA = 0.0f;
        float lfoPhaseB = 0.31f;
        float jitter = 0.0f;
        juce::Random random;
        float bbdLpL = 0.0f, bbdLpR = 0.0f;
        juce::SmoothedValue<float> halfBlend { 0.0f };
        bool lastHalfSpeed = false;
    };

    struct ReverbGate
    {
        void prepare (double newSampleRate);
        void reset();
        float process (float inputLevel, float attack, float hold, float decay);

        enum State { closed, attacking, holding, decaying } state = closed;
        double sampleRate = 48000.0;
        float envelope = 0.0f;
        int holdSamplesRemaining = 0;
    };

    struct SimpleReverb
    {
        void prepare (double newSampleRate);
        void reset();
        void processCathedra (float inL, float inR, float& outL, float& outR,
                              float decay, float size, float diffusion,
                              float loFreq, float hiFreq, float pitch, float pitchMix);
        void processHall78 (float inL, float inR, float& outL, float& outR,
                            float mids, float bass, float treble, float cross, float tankMod, float diffusion);
        void processGravityPlaceholder (float inL, float inR, float& outL, float& outR,
                                        float decay, float tilt, float modSpeed, float modDepth, float gain);

        double sampleRate = 48000.0;
        juce::Reverb reverbA, reverbB;
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> shimmerL { 4096 }, shimmerR { 4096 };
        float shimmerPhase = 0.0f;
        float gravityL = 0.0f, gravityR = 0.0f, gravityPhase = 0.0f;
    };

    struct FeedbackInsert
    {
        void prepare (double newSampleRate);
        void reset();
        void process (float& left, float& right, int type, float amount, float mix, float param);

        double sampleRate = 48000.0;
        juce::dsp::StateVariableTPTFilter<float> svfL, svfR;
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> pitchL { 4096 }, pitchR { 4096 };
        float pitchPhase = 0.0f;
        float hazyPhase = 0.0f;
        float decimateHoldL = 0.0f, decimateHoldR = 0.0f, decimateCounter = 0.0f;
    };

    struct HazyProcessor
    {
        void prepare (double newSampleRate);
        void reset();
        void process (float& left, float& right, float age, float warble, float decimate, float mix);

        double sampleRate = 48000.0;
        float phase = 0.0f;
        float holdL = 0.0f, holdR = 0.0f, counter = 0.0f;
        juce::Random random;
    };

    struct Modifier
    {
        float process (int type, float rate, float depth, int shape, float inputLevel, double sampleRate);
        float phase = 0.0f;
        float envelope = 0.0f;
        float held = 0.0f;
        juce::Random random;
    };

    void applyFactoryPreset (int index);
    float param (const char* id) const;
    int choice (const char* id) const;
    bool toggle (const char* id) const;

    PredelayEngine predelay;
    SimpleReverb reverb;
    ReverbGate gate;
    FeedbackInsert feedbackInsert;
    HazyProcessor hazy;
    Modifier modA, modB;

    float previousReverbL = 0.0f;
    float previousReverbR = 0.0f;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrbitfallAudioProcessor)
};
