#pragma once

#include <JuceHeader.h>

//==============================================================================
class ProjektAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    ProjektAudioProcessor();
    ~ProjektAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState treeState;

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjektAudioProcessor)

    // poglos
    juce::dsp::Reverb reverb;
    juce::dsp::Reverb::Parameters reverbParams;

    // zmiana tonacji
    float pitchShiftRatios[4] = { 0.5f, 1.0f, 2.0f, 4.0f };
    juce::AudioBuffer<float> pitchDelayBuffer;
    int pitchDelayBufferLength = 1;
    int pitchWritePosition = 0;
    float pitchReadPosition[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    // delay
    juce::AudioBuffer<float> delayBuffer;
    int delayBufferLength = 1;
    int delayWritePosition = 0;

    // chorus
    juce::AudioBuffer<float> chorusBuffer;
    int chorusBufferLength = 1;
    int chorusWritePosition = 0;
    float chorusPhase = 0.0f;
    
    void reset();
};
