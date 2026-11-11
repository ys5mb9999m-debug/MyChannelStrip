#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class MyChannelStripProcessor : public juce::AudioProcessor
{
public:
    MyChannelStripProcessor();
    ~MyChannelStripProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MyChannelStrip"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateFilters();

    using Filter = juce::dsp::IIR::Filter<float>;
    static constexpr int numCh = 2;
    Filter hp1[numCh], hp2[numCh], lp[numCh], lf[numCh], lmf[numCh], hmf[numCh], hf[numCh];

    double currentSampleRate = 44100.0;
    float outGainLin = 1.0f;

    float getP (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    bool getB (const char* id) const { return getP (id) > 0.5f; }
};
