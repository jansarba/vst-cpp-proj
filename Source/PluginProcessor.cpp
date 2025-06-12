/*
  ==============================================================================

    implementacja procesora

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::ParameterID createParamID(juce::StringRef paramID)
{
    return { paramID, 1 };
}

//==============================================================================
ProjektAudioProcessor::ProjektAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
    treeState(*this, nullptr, "PARAMETERS",
        {
            std::make_unique<juce::AudioParameterFloat>(createParamID("pitch-12"), "Mix -12", 0.0f, 1.0f, 0.0f),
            std::make_unique<juce::AudioParameterFloat>(createParamID("pitch0"), "Mix 0 (Dry)", 0.0f, 1.0f, 1.0f),
            std::make_unique<juce::AudioParameterFloat>(createParamID("pitch12"), "Mix +12", 0.0f, 1.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>(createParamID("pitch24"), "Mix +24", 0.0f, 1.0f, 0.25f),
            std::make_unique<juce::AudioParameterFloat>(createParamID("reverb_room_size"), "Room Size", 0.0f, 1.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>(createParamID("reverb_damping"), "Damping", 0.0f, 1.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>(createParamID("reverb_wet_level"), "Reverb Wet", 0.0f, 1.0f, 0.33f),
            std::make_unique<juce::AudioParameterFloat>(createParamID("reverb_dry_level"), "Reverb Dry", 0.0f, 1.0f, 0.4f),
            std::make_unique<juce::AudioParameterFloat>(createParamID("delay_time"), "Delay Time", 0.01f, 1.0f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>(createParamID("delay_feedback"), "Feedback", 0.0f, 0.95f, 0.5f),
            std::make_unique<juce::AudioParameterFloat>(createParamID("chorus_rate"), "Chorus Rate", 0.1f, 10.0f, 1.0f),
            std::make_unique<juce::AudioParameterFloat>(createParamID("chorus_depth"), "Chorus Depth", 0.0f, 1.0f, 0.2f),
            std::make_unique<juce::AudioParameterFloat>(createParamID("chorus_mix"), "Chorus Mix", 0.0f, 1.0f, 0.5f)
        })
#endif
{
}

ProjektAudioProcessor::~ProjektAudioProcessor()
{
}

//==============================================================================
const juce::String ProjektAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ProjektAudioProcessor::acceptsMidi() const { return false; }
bool ProjektAudioProcessor::producesMidi() const { return false; }
bool ProjektAudioProcessor::isMidiEffect() const { return false; }
double ProjektAudioProcessor::getTailLengthSeconds() const { return 2.0; }
int ProjektAudioProcessor::getNumPrograms() { return 1; }
int ProjektAudioProcessor::getCurrentProgram() { return 0; }
void ProjektAudioProcessor::setCurrentProgram (int index) { }
const juce::String ProjektAudioProcessor::getProgramName (int index) { return {}; }
void ProjektAudioProcessor::changeProgramName (int index, const juce::String& newName) { }


//==============================================================================
void ProjektAudioProcessor::reset()
{
    // generalne porzadki
    pitchDelayBuffer.clear();
    delayBuffer.clear();
    chorusBuffer.clear();

    pitchWritePosition = 0;
    delayWritePosition = 0;
    chorusWritePosition = 0;
    
    for (int i = 0; i < 4; ++i)
        pitchReadPosition[i] = 0.0f;
    
    chorusPhase = 0.0f;
    
    reverb.reset();
}

void ProjektAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int bufferLength = (int)(2.0 * sampleRate);
    
    pitchDelayBuffer.setSize(getTotalNumOutputChannels(), bufferLength, false, true, true);
    delayBuffer.setSize(getTotalNumOutputChannels(), bufferLength, false, true, true);
    chorusBuffer.setSize(getTotalNumOutputChannels(), bufferLength, false, true, true);
    
    pitchDelayBufferLength = pitchDelayBuffer.getNumSamples();
    delayBufferLength = delayBuffer.getNumSamples();
    chorusBufferLength = chorusBuffer.getNumSamples();
    
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32)samplesPerBlock, (juce::uint32)getTotalNumOutputChannels() };
    reverb.prepare(spec);

    reset();
}

void ProjektAudioProcessor::releaseResources()
{
    // tu w sumie nic nie trzeba
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ProjektAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif
    return true;
  #endif
}
#endif

void ProjektAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    int numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    // --- Pobranie parametrów z interfejsu ---
    float pitchMix[4] = {
        treeState.getRawParameterValue("pitch-12")->load(),
        treeState.getRawParameterValue("pitch0")->load(),
        treeState.getRawParameterValue("pitch12")->load(),
        treeState.getRawParameterValue("pitch24")->load()
    };
    
    reverbParams.roomSize = treeState.getRawParameterValue("reverb_room_size")->load();
    reverbParams.damping = treeState.getRawParameterValue("reverb_damping")->load();
    reverbParams.wetLevel = treeState.getRawParameterValue("reverb_wet_level")->load();
    reverbParams.dryLevel = treeState.getRawParameterValue("reverb_dry_level")->load();
    reverbParams.width = 1.0f;
    reverb.setParameters(reverbParams);
    
    auto delayTimeSamples = treeState.getRawParameterValue("delay_time")->load() * getSampleRate();
    auto delayFeedback = treeState.getRawParameterValue("delay_feedback")->load();
    
    auto chorusRate = treeState.getRawParameterValue("chorus_rate")->load();
    auto chorusDepth = treeState.getRawParameterValue("chorus_depth")->load();
    auto chorusMix = treeState.getRawParameterValue("chorus_mix")->load();

    // Bufor roboczy do przetwarzania efektów
    juce::AudioBuffer<float> processingBuffer(totalNumOutputChannels, numSamples);
    processingBuffer.clear();

    // Pętla przetwarzająca próbka po próbce
    for (int sample = 0; sample < numSamples; ++sample)
    {
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            // Zapisz aktualną próbkę wejściową do bufora pitch shiftera
            pitchDelayBuffer.setSample(channel, pitchWritePosition, buffer.getSample(channel, sample));
        }

        // --- 1. Pitch Shifter (tylko sygnał WET) ---
        float wetPitchedSampleL = 0.0f, wetPitchedSampleR = 0.0f;
        for (int voice = 0; voice < 4; ++voice)
        {
            // Pomijamy głos '0', ponieważ jest to sygnał DRY, który dodamy później
            if (voice == 1)
                continue;

            for (int channel = 0; channel < totalNumInputChannels; ++channel)
            {
                float currentReadPos = pitchReadPosition[voice];
                
                int readPos1 = static_cast<int>(currentReadPos);
                int readPos2 = (readPos1 + 1);
                if (readPos2 >= pitchDelayBufferLength)
                    readPos2 -= pitchDelayBufferLength;
                
                float frac = currentReadPos - readPos1;
                
                float sample1 = pitchDelayBuffer.getSample(channel, readPos1);
                float sample2 = pitchDelayBuffer.getSample(channel, readPos2);
                float pitched = sample1 + frac * (sample2 - sample1);

                if (channel == 0) wetPitchedSampleL += pitched * pitchMix[voice];
                else wetPitchedSampleR += pitched * pitchMix[voice];
            }
        }
        
        // Aktualizacja pozycji odczytu dla każdego głosu pitch shiftera
        for(int voice = 0; voice < 4; ++voice)
        {
            pitchReadPosition[voice] += pitchShiftRatios[voice];
            while (pitchReadPosition[voice] >= pitchDelayBufferLength)
                pitchReadPosition[voice] -= pitchDelayBufferLength;
        }

        // --- 2. Delay (na sygnale WET) ---
        int delayReadPosition = (int)(delayWritePosition - delayTimeSamples + delayBufferLength) % delayBufferLength;
        float delaySampleL = delayBuffer.getSample(0, delayReadPosition);
        float delaySampleR = totalNumOutputChannels > 1 ? delayBuffer.getSample(1, delayReadPosition) : 0.0f;

        // Wyjście z delaya = sygnał z pitch shiftera + echo z pętli zwrotnej
        float wetFromDelayL = wetPitchedSampleL + delaySampleL * delayFeedback;
        float wetFromDelayR = wetPitchedSampleR + delaySampleR * delayFeedback;

        delayBuffer.setSample(0, delayWritePosition, wetFromDelayL);
        if (totalNumOutputChannels > 1) delayBuffer.setSample(1, delayWritePosition, wetFromDelayR);
        
        // --- Łączenie sygnału DRY i WET (przed chorusem) ---
        float drySampleL = buffer.getSample(0, sample) * pitchMix[1];
        float drySampleR = (totalNumInputChannels > 1) ? buffer.getSample(1, sample) * pitchMix[1] : drySampleL;

        float combinedForChorusL = wetFromDelayL + drySampleL;
        float combinedForChorusR = wetFromDelayR + drySampleR;

        // --- 3. Chorus (działa na połączonym sygnale DRY+WET) ---
        chorusPhase += (chorusRate * juce::MathConstants<float>::twoPi) / (float)getSampleRate();
        if (chorusPhase >= juce::MathConstants<float>::twoPi) chorusPhase -= juce::MathConstants<float>::twoPi;
        
        float lfo = 0.5f + 0.5f * sin(chorusPhase);
        float chorusDelay = 10.0f + 15.0f * lfo;
        float chorusDelaySamples = chorusDelay * (getSampleRate() / 1000.0f) * chorusDepth;

        float readPosFloat = (float)chorusWritePosition - chorusDelaySamples;
        while (readPosFloat < 0) readPosFloat += chorusBufferLength;
        while (readPosFloat >= chorusBufferLength) readPosFloat -= chorusBufferLength;
        
        int readPos1 = static_cast<int>(readPosFloat);
        int readPos2 = readPos1 + 1;
        if (readPos2 >= chorusBufferLength)
             readPos2 -= chorusBufferLength;

        float readFrac = readPosFloat - readPos1;
        
        // Zapisz sygnał do bufora chorusa
        chorusBuffer.setSample(0, chorusWritePosition, combinedForChorusL);
        if(totalNumOutputChannels > 1) chorusBuffer.setSample(1, chorusWritePosition, combinedForChorusR);
        
        // Odczytaj sygnał przetworzony przez chorus
        float chorusOutL = chorusBuffer.getSample(0, readPos1) * (1.0f - readFrac) + chorusBuffer.getSample(0, readPos2) * readFrac;
        float chorusOutR = 0.0f;
        if(totalNumOutputChannels > 1)
             chorusOutR = chorusBuffer.getSample(1, readPos1) * (1.0f - readFrac) + chorusBuffer.getSample(1, readPos2) * readFrac;

        // Mieszaj sygnał "przed chorusem" z "przetworzonym przez chorus"
        float finalSampleL = combinedForChorusL * (1.0f - chorusMix) + chorusOutL * chorusMix;
        float finalSampleR = combinedForChorusR * (1.0f - chorusMix) + chorusOutR * chorusMix;

        // Zapisz finalny wynik do bufora, który pójdzie do pogłosu
        processingBuffer.setSample(0, sample, finalSampleL);
        if (totalNumOutputChannels > 1) processingBuffer.setSample(1, sample, finalSampleR);
        
        // --- Inkrementacja pozycji zapisu ---
        pitchWritePosition = (pitchWritePosition + 1) % pitchDelayBufferLength;
        delayWritePosition = (delayWritePosition + 1) % delayBufferLength;
        chorusWritePosition = (chorusWritePosition + 1) % chorusBufferLength;
    }

    // --- 4. Pogłos (na całym zmiksowanym sygnale) ---
    juce::dsp::AudioBlock<float> block(processingBuffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    reverb.process(context);
    
    // --- Kopiowanie do głównego bufora wyjściowego ---
    for (int channel = 0; channel < totalNumOutputChannels; ++channel)
    {
        buffer.copyFrom(channel, 0, processingBuffer, channel, 0, numSamples);
    }
}


//==============================================================================
bool ProjektAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* ProjektAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

//==============================================================================
void ProjektAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = treeState.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void ProjektAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (treeState.state.getType()))
            treeState.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
// tworzy nowa instancje wtyczki
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ProjektAudioProcessor();
}
