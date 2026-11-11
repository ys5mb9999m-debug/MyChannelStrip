#include "PluginProcessor.h"

using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using Range = juce::NormalisableRange<float>;

static Range freqRange (float lo, float hi)
{
    Range r (lo, hi);
    r.setSkewForCentre (std::sqrt (lo * hi));
    return r;
}

juce::AudioProcessorValueTreeState::ParameterLayout MyChannelStripProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto pid = [] (const char* s) { return juce::ParameterID { s, 1 }; };

    // Filtri
    l.add (std::make_unique<APB> (pid ("filt_on"), "Filters In", true));
    l.add (std::make_unique<APF> (pid ("hp_freq"), "HP Freq", freqRange (16.0f, 350.0f), 16.0f));
    l.add (std::make_unique<APB> (pid ("hp_on"), "HP On", false));
    l.add (std::make_unique<APF> (pid ("lp_freq"), "LP Freq", freqRange (3000.0f, 22000.0f), 22000.0f));
    l.add (std::make_unique<APB> (pid ("lp_on"), "LP On", false));

    // EQ
    l.add (std::make_unique<APB> (pid ("eq_on"), "EQ In", true));

    l.add (std::make_unique<APF> (pid ("lf_gain"), "LF Gain", Range (-16.0f, 16.0f, 0.1f), 0.0f));
    l.add (std::make_unique<APF> (pid ("lf_freq"), "LF Freq", freqRange (30.0f, 450.0f), 100.0f));
    l.add (std::make_unique<APB> (pid ("lf_bell"), "LF Bell", false));

    l.add (std::make_unique<APF> (pid ("lmf_gain"), "LMF Gain", Range (-16.0f, 16.0f, 0.1f), 0.0f));
    l.add (std::make_unique<APF> (pid ("lmf_freq"), "LMF Freq", freqRange (250.0f, 6000.0f), 1000.0f));
    l.add (std::make_unique<APF> (pid ("lmf_q"), "LMF Q", freqRange (0.5f, 2.5f), 1.0f));

    l.add (std::make_unique<APF> (pid ("hmf_gain"), "HMF Gain", Range (-16.0f, 16.0f, 0.1f), 0.0f));
    l.add (std::make_unique<APF> (pid ("hmf_freq"), "HMF Freq", freqRange (600.0f, 7000.0f), 3000.0f));
    l.add (std::make_unique<APF> (pid ("hmf_q"), "HMF Q", freqRange (0.5f, 2.5f), 1.0f));

    l.add (std::make_unique<APF> (pid ("hf_gain"), "HF Gain", Range (-16.0f, 16.0f, 0.1f), 0.0f));
    l.add (std::make_unique<APF> (pid ("hf_freq"), "HF Freq", freqRange (1500.0f, 16000.0f), 8000.0f));
    l.add (std::make_unique<APB> (pid ("hf_bell"), "HF Bell", false));

    l.add (std::make_unique<APF> (pid ("out_gain"), "Output", Range (-12.0f, 12.0f, 0.1f), 0.0f));
    return l;
}

MyChannelStripProcessor::MyChannelStripProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool MyChannelStripProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void MyChannelStripProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    for (int c = 0; c < numCh; ++c)
    {
        hp1[c].reset(); hp2[c].reset(); lp[c].reset();
        lf[c].reset(); lmf[c].reset(); hmf[c].reset(); hf[c].reset();
    }
    updateFilters();
}

void MyChannelStripProcessor::updateFilters()
{
    const double sr = currentSampleRate;
    const double nyq = sr * 0.49;
    auto clampF = [nyq] (float f) { return (float) juce::jmin ((double) f, nyq); };
    auto dbToG = [] (float db) { return juce::Decibels::decibelsToGain (db); };

    // HP 18 dB/ott = 1° ordine + 2° ordine (Butterworth)
    auto hpA = juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sr, clampF (getP ("hp_freq")));
    auto hpB = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, clampF (getP ("hp_freq")), 0.7071f);
    // LP 12 dB/ott = 2° ordine
    auto lpA = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, clampF (getP ("lp_freq")), 0.7071f);

    const float shelfQ = 0.7071f;
    const float bellQLow = 1.0f; // Q fisso per LF/HF in modalita Bell (da affinare con le misure)

    auto lfC = getB ("lf_bell")
        ? juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, clampF (getP ("lf_freq")), bellQLow, dbToG (getP ("lf_gain")))
        : juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, clampF (getP ("lf_freq")), shelfQ, dbToG (getP ("lf_gain")));

    auto lmfC = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, clampF (getP ("lmf_freq")), getP ("lmf_q"), dbToG (getP ("lmf_gain")));
    auto hmfC = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, clampF (getP ("hmf_freq")), getP ("hmf_q"), dbToG (getP ("hmf_gain")));

    auto hfC = getB ("hf_bell")
        ? juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, clampF (getP ("hf_freq")), bellQLow, dbToG (getP ("hf_gain")))
        : juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, clampF (getP ("hf_freq")), shelfQ, dbToG (getP ("hf_gain")));

    for (int c = 0; c < numCh; ++c)
    {
        hp1[c].coefficients = hpA;
        hp2[c].coefficients = hpB;
        lp[c].coefficients = lpA;
        lf[c].coefficients = lfC;
        lmf[c].coefficients = lmfC;
        hmf[c].coefficients = hmfC;
        hf[c].coefficients = hfC;
    }

    outGainLin = dbToG (getP ("out_gain"));
}

void MyChannelStripProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    updateFilters();

    const bool filtersIn = getB ("filt_on");
    const bool hpOn = filtersIn && getB ("hp_on");
    const bool lpOn = filtersIn && getB ("lp_on");
    const bool eqOn = getB ("eq_on");

    const int chans = juce::jmin (buffer.getNumChannels(), numCh);
    const int n = buffer.getNumSamples();

    for (int c = 0; c < chans; ++c)
    {
        auto* d = buffer.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            float x = d[i];
            if (hpOn) { x = hp1[c].processSample (x); x = hp2[c].processSample (x); }
            if (lpOn) { x = lp[c].processSample (x); }
            if (eqOn)
            {
                x = lf[c].processSample (x);
                x = lmf[c].processSample (x);
                x = hmf[c].processSample (x);
                x = hf[c].processSample (x);
            }
            d[i] = x * outGainLin;
        }
    }
}

juce::AudioProcessorEditor* MyChannelStripProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void MyChannelStripProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MyChannelStripProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MyChannelStripProcessor();
}
