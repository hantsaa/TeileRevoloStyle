#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using APC = juce::AudioParameterChoice;
using PID = juce::ParameterID;
using Attr = juce::AudioParameterFloatAttributes;

// Divisions de la synchro tempo : cycles de trémolo par noire
static const char* kDivisionNames[] = { "1/2", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
static const double kCyclesPerBeat[] = { 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 6.0, 8.0 };

juce::AudioProcessorValueTreeState::ParameterLayout RevoloProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto unit = [] { return juce::NormalisableRange<float> (0.f, 1.f); };
    auto pct = [] (float v, int) { return juce::String (juce::roundToInt (v * 100.f)) + " %"; };
    auto seconds = [] (float v, int)
    {
        const float s = 0.25f * std::pow (40.f, v);   // même loi que le moteur : 0,25 s … 10 s
        return juce::String (s, s < 10.f ? 2 : 1) + " s";
    };
    auto hz = [] (float v, int)
    {
        return v >= 1000.f ? juce::String (v / 1000.f, v >= 10000.f ? 1 : 2) + " kHz"
                           : juce::String (v, v < 100.f ? 1 : 0) + " Hz";
    };
    auto rate = [] (float v, int) { return juce::String (v, v < 10.f ? 2 : 1) + " Hz"; };

    p.push_back (std::make_unique<APF> (PID { "reverb", 1 },    "Reverb",    unit(), 1.0f,  Attr().withStringFromValueFunction (pct)));
    p.push_back (std::make_unique<APF> (PID { "character", 1 }, "Character", unit(), 1.0f,  Attr().withStringFromValueFunction (pct)));
    p.push_back (std::make_unique<APF> (PID { "length", 1 },    "Length",    unit(), 0.72f, Attr().withStringFromValueFunction (seconds)));
    p.push_back (std::make_unique<APF> (PID { "air", 1 },       "Air",       unit(), 0.5f,  Attr().withStringFromValueFunction (pct)));
    p.push_back (std::make_unique<APF> (PID { "noise", 1 },     "Noise",     unit(), 0.0f,  Attr().withStringFromValueFunction (pct)));
    p.push_back (std::make_unique<APF> (PID { "tremolo", 1 },   "Tremolo",   unit(), 0.0f,  Attr().withStringFromValueFunction (pct)));

    juce::NormalisableRange<float> filt (20.f, 20000.f);
    filt.setSkewForCentre (632.f);
    p.push_back (std::make_unique<APF> (PID { "filter", 1 }, "Filter", filt, 150.f, Attr().withStringFromValueFunction (hz)));

    juce::NormalisableRange<float> tempo (0.2f, 40.f);
    tempo.setSkewForCentre (2.8f);
    p.push_back (std::make_unique<APF> (PID { "tempo", 1 }, "Tempo", tempo, 2.0f, Attr().withStringFromValueFunction (rate)));

    juce::StringArray divisions;
    for (auto* d : kDivisionNames) divisions.add (d);
    p.push_back (std::make_unique<APC> (PID { "division", 1 }, "Tremolo Division", divisions, 3));

    p.push_back (std::make_unique<APB> (PID { "sync", 1 },   "Tremolo Host Sync", false));
    p.push_back (std::make_unique<APB> (PID { "mix", 1 },    "Mix (off = Wet)", false));
    p.push_back (std::make_unique<APB> (PID { "mom", 1 },    "Momentary (off = ON/OFF)", false));
    p.push_back (std::make_unique<APB> (PID { "active", 1 }, "Active", true));
    return { p.begin(), p.end() };
}

RevoloProcessor::RevoloProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pReverb    = apvts.getRawParameterValue ("reverb");
    pCharacter = apvts.getRawParameterValue ("character");
    pLength    = apvts.getRawParameterValue ("length");
    pAir       = apvts.getRawParameterValue ("air");
    pNoise     = apvts.getRawParameterValue ("noise");
    pFilter    = apvts.getRawParameterValue ("filter");
    pTremolo   = apvts.getRawParameterValue ("tremolo");
    pTempo     = apvts.getRawParameterValue ("tempo");
    pMix       = apvts.getRawParameterValue ("mix");
    pActive    = apvts.getRawParameterValue ("active");
    pSync      = apvts.getRawParameterValue ("sync");
    pDivision  = apvts.getRawParameterValue ("division");
}

bool RevoloProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto& in = l.getMainInputChannelSet();
    const auto& out = l.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

void RevoloProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

void RevoloProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int nCh = buffer.getNumChannels();
    const int n = buffer.getNumSamples();
    if (nCh == 0) return;

    revolo::Params p;
    p.reverb = pReverb->load();     p.character = pCharacter->load();
    p.length = pLength->load();     p.air = pAir->load();
    p.noise = pNoise->load();       p.filterHz = pFilter->load();
    p.tremolo = pTremolo->load();   p.tempoHz = pTempo->load();
    p.mixMode = pMix->load() > 0.5f;
    p.active = pActive->load() > 0.5f;

    if (resetPhaseRequest.exchange (false))
        engine.setLfoPhase (0.f);

    // Tempo de l'hôte (FL Studio le fournit) : vitesse du trémolo calée sur le projet
    juce::Optional<double> bpm, ppq;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            bpm = pos->getBpm();
            ppq = pos->getPpqPosition();
            playing = pos->getIsPlaying();
        }
    hostBpm.store (bpm ? (float) *bpm : 0.f);

    if (pSync->load() > 0.5f && bpm && *bpm > 1.0)
    {
        const int div = juce::jlimit (0, 7, (int) std::lround (pDivision->load()));
        const double cpb = kCyclesPerBeat[div];
        p.tempoHz = juce::jlimit (0.2f, 40.f, (float) (*bpm / 60.0 * cpb));

        if (playing && ppq)   // verrouillage de phase sur la position musicale
        {
            const double cyc = *ppq * cpb;
            const float expected = (float) (cyc - std::floor (cyc));
            const float cur = engine.getLfoPhase();
            float d = expected - cur;
            d -= std::round (d);
            engine.setLfoPhase (std::abs (d) > 0.2f ? expected : cur + 0.15f * d);
        }
    }
    engine.setParams (p);

    float* l = buffer.getWritePointer (0);
    float* r = nCh > 1 ? buffer.getWritePointer (1) : nullptr;
    for (int i = 0; i < n; ++i)
    {
        const float inL = l[i];
        const float inR = r ? r[i] : inL;
        float oL, oR;
        engine.processSample (inL, inR, oL, oR);
        l[i] = r ? oL : 0.5f * (oL + oR);
        if (r) r[i] = oR;
    }
    lfoPhase.store (engine.getLfoPhase());
}

void RevoloProcessor::tap()
{
    if (pSync->load() > 0.5f) return;   // en synchro hôte, le tap n'a pas de sens

    const double now = juce::Time::getMillisecondCounterHiRes();
    if (tapCount > 0 && now - lastTapMs > 2000.0) tapCount = 0;   // nouvelle série
    lastTapMs = now;
    if (tapCount < 4) tapTimes[tapCount++] = now;
    else { for (int i = 0; i < 3; ++i) tapTimes[i] = tapTimes[i + 1]; tapTimes[3] = now; }
    resetPhaseRequest.store (true);   // le trémolo repart du temps fort au tap
    if (tapCount < 2) return;
    const double avgMs = (tapTimes[tapCount - 1] - tapTimes[0]) / (double) (tapCount - 1);
    const float hz = juce::jlimit (0.2f, 40.f, (float) (1000.0 / avgMs));
    if (auto* prm = apvts.getParameter ("tempo"))
        prm->setValueNotifyingHost (prm->convertTo0to1 (hz));
}

void RevoloProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void RevoloProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* RevoloProcessor::createEditor() { return new RevoloEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RevoloProcessor(); }
