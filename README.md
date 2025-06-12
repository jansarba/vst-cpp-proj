# vst-cpp

<sub><sup> Note: the VST3 technology requires quite a bit of boilerplate and JUCE provides some abstraction. This isn't exactly a low-level project. </sub></sup>

A simple multi-effect audio plugin developed using the JUCE framework in C++. The plugin features a signal chain that includes a pitch shifter, a delay, a chorus, and a reverb effect. VSTs are mostly used in Digital Audio Workstations (DAWs) and video editing software. The core idea is block-based processing - you have to finish handling one before you're given another, so memory allocation in the processing function is very risky.

## Table of Contents

- [Features](#features)
- [Signal Flow](#signal-flow)
- [Setup and Building](#setup-and-building)
- [Code Highlights](#code-highlights)
  - [Parameters](#parameters)
  - [Audio Processing Chain](#audio-processing-chain)
  - [Pitch Shifter](#pitch-shifter)
  - [Delay](#delay)
  - [Chorus](#chorus)
  - [Reverb](#reverb)

## Features

*   **Four-Voice Pitch Shifter**: Shifts the pitch of the incoming audio signal down one octave, and up one and two octaves, with a mix control for each voice and the original dry signal.
*   **Delay**: A feedback delay line applied to the wet signal from the pitch shifter.
*   **Chorus**: A classic chorus effect applied to the combined dry and wet signal.
*   **Reverb**: A configurable reverb effect applied to the final mixed signal.
*   **Generic UI**: Utilizes a generic JUCE editor for controlling all plugin parameters.

## Signal Flow

The audio signal is processed through the effects in the following order:

1.  The input signal is fed into a circular buffer for the **Pitch Shifter**.
2.  The wet (pitched) signal is then passed to the **Delay** effect.
3.  The output of the delay is combined with the dry signal (un-pitched).
4.  This combined signal is then processed by the **Chorus** effect.
5.  Finally, the entire mixed signal is sent to the **Reverb** effect.

## Setup and Building

This project is built using the JUCE framework. To build the plugin, you will need to have a working JUCE development environment set up.

**Prerequisites:**

*   A C++ compiler (like GCC, Clang, or MSVC)
*   The JUCE framework. You can download it from the official [JUCE website](https://juce.com/get-juce/download).

**Building Instructions:**

1.  Clone this repository to your local machine.
2.  Open the `Projekt.jucer` file in the Projucer.
3.  Set up your export targets (e.g., VST3, AU, AAX) and save the project. This will generate the necessary project files for your chosen IDE (e.g., Visual Studio, Xcode).
4.  Open the generated project in your IDE and build the plugin.

## Code Highlights

### Parameters

All plugin parameters are managed using `juce::AudioProcessorValueTreeState`. See the wonderful [JUCE docs](https://docs.juce.com/master/classAudioProcessorValueTreeState.html) for more info.

**`PluginProcessor.cpp`**
```cpp
// ...
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
// ...
```

### Audio Processing Chain

The core audio processing happens in the `processBlock` function. The code iterates through each sample, applying the effects in sequence. A separate `processingBuffer` is used to hold the output of the effects chain before it's passed to the final reverb stage.

### Pitch Shifter

The pitch shifter is implemented using a delay line with multiple read pointers. Each read pointer moves at a different speed (`pitchShiftRatios`) to create the pitch-shifted voices. Linear interpolation is used to read fractional positions from the delay buffer, which helps to reduce aliasing artifacts.

**`PluginProcessor.h`**
```cpp
// ...
float pitchShiftRatios[4] = { 0.5f, 1.0f, 2.0f, 4.0f };
juce::AudioBuffer<float> pitchDelayBuffer;
int pitchDelayBufferLength = 1;
int pitchWritePosition = 0;
float pitchReadPosition[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
// ...
```

**`PluginProcessor.cpp`**
```cpp
// ...
// --- 1. Pitch Shifter (tylko sygnał WET) ---
float wetPitchedSampleL = 0.0f, wetPitchedSampleR = 0.0f;
for (int voice = 0; voice < 4; ++voice)
{
    // ...
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
    // ...
}
// ...
```

### Delay

A simple feedback delay is implemented using a circular buffer. The delayed signal is read from the buffer and fed back into the delay line, controlled by the `delayFeedback` parameter.

**`PluginProcessor.cpp`**
```cpp
// ...
// --- 2. Delay (na sygnale WET) ---
int delayReadPosition = (int)(delayWritePosition - delayTimeSamples + delayBufferLength) % delayBufferLength;
float delaySampleL = delayBuffer.getSample(0, delayReadPosition);
float delaySampleR = totalNumOutputChannels > 1 ? delayBuffer.getSample(1, delayReadPosition) : 0.0f;

// Wyjście z delaya = sygnał z pitch shiftera + echo z pętli zwrotnej
float wetFromDelayL = wetPitchedSampleL + delaySampleL * delayFeedback;
float wetFromDelayR = wetPitchedSampleR + delaySampleR * delayFeedback;

delayBuffer.setSample(0, delayWritePosition, wetFromDelayL);
if (totalNumOutputChannels > 1) delayBuffer.setSample(1, delayWritePosition, wetFromDelayR);
// ...
```

### Chorus

The chorus effect is created by modulating a short delay time with a Low-Frequency Oscillator (LFO). This implementation uses a sine wave LFO to modulate the delay time, creating the characteristic shimmering sound of a chorus.

**`PluginProcessor.cpp`**
```cpp
// ...
// --- 3. Chorus (działa na połączonym sygnale DRY+WET) ---
chorusPhase += (chorusRate * juce::MathConstants<float>::twoPi) / (float)getSampleRate();
if (chorusPhase >= juce::MathConstants<float>::twoPi) chorusPhase -= juce::MathConstants<float>::twoPi;

float lfo = 0.5f + 0.5f * sin(chorusPhase);
float chorusDelay = 10.0f + 15.0f * lfo;
float chorusDelaySamples = chorusDelay * (getSampleRate() / 1000.0f) * chorusDepth;
// ...
```

### Reverb

The plugin utilizes the `juce::dsp::Reverb` class for a high-quality reverb effect. The reverb parameters (room size, damping, wet/dry levels) are controlled by the user and applied to the processed signal at the end of the chain. I would not recommend implementing your own reverb unless masochistic.

**`PluginProcessor.h`**
```cpp
// ...
// poglos
juce::dsp::Reverb reverb;
juce::dsp::Reverb::Parameters reverbParams;
// ...
```

**`PluginProcessor.cpp`**
```cpp
// ...
// --- 4. Pogłos (na całym zmiksowanym sygnale) ---
juce::dsp::AudioBlock<float> block(processingBuffer);
juce::dsp::ProcessContextReplacing<float> context(block);
reverb.process(context);
// ...
```
