#include "AudioEngine.h"

#include <algorithm>

namespace pianolume
{

namespace
{
    constexpr int numOutputChannels = 2;
    constexpr int maxMidiBufferBytes = 4096;
    constexpr size_t maxScheduledEvents = 8192;

    // 0 dB のときの FluidSynth のゲイン。FluidSynth 既定の 0.2 では小さすぎるため、聴きやすい大きさに合わせた
    constexpr float referenceSynthGain = 1.5f;

    constexpr float limiterThresholdDb = -0.5f;
    constexpr float limiterReleaseMs = 50.0f;
}

AudioEngine::AudioEngine()
{
    // デバイス開始前に届いたMIDIも受け取れるよう、仮のサンプルレートで初期化しておく
    midiCollector.reset (44100.0);
    scheduledEvents.reserve (maxScheduledEvents);

    limiter.setThreshold (limiterThresholdDb);
    limiter.setRelease (limiterReleaseMs);

    setMasterVolumeDb (masterVolumeDb);
}

AudioEngine::~AudioEngine()
{
    deviceManager.removeAudioCallback (this);
    deviceManager.closeAudioDevice();
}

void AudioEngine::initialise (const juce::XmlElement* savedState)
{
    const auto error = deviceManager.initialise (0, numOutputChannels, savedState, true);

    if (error.isNotEmpty())
        juce::Logger::writeToLog ("Audio device error: " + error);

    deviceManager.addAudioCallback (this);
}

std::unique_ptr<juce::XmlElement> AudioEngine::createStateXml() const
{
    return deviceManager.createStateXml();
}

void AudioEngine::setSynthEnabled (bool shouldBeEnabled)
{
    synthEnabled = shouldBeEnabled;

    if (! shouldBeEnabled)
        synth.allNotesOff();
}

void AudioEngine::setMasterVolumeDb (float newVolumeDb)
{
    masterVolumeDb = juce::jlimit (minVolumeDb, maxVolumeDb, newVolumeDb);
    synth.setGain (referenceSynthGain * juce::Decibels::decibelsToGain (masterVolumeDb));
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const*,
                                                    int,
                                                    float* const* outputChannelData,
                                                    int numOutputChannelsActive,
                                                    int numSamples,
                                                    const juce::AudioIODeviceCallbackContext&)
{
    // 曲の再生位置は、音を鳴らさない場合でも進める（表示が止まらないように）
    scheduledEvents.clear();

    midiCollector.removeNextBlockOfMessages (incomingMidi, numSamples);

    for (const auto metadata : incomingMidi)
    {
        if (metadata.numBytes > 3)
            continue;

        const auto message = metadata.getMessage();
        appendEvent (scheduledEvents, metadata.samplePosition, message, SynthChannels::live, false);

        if (message.isNoteOn())
            songPlayer.handleLiveNoteOn (message.getNoteNumber());
    }

    incomingMidi.clear();

    songPlayer.processBlock (numSamples, scheduledEvents);

    for (int i = 0; i < numOutputChannelsActive; ++i)
        if (outputChannelData[i] != nullptr)
            juce::FloatVectorOperations::clear (outputChannelData[i], numSamples);

    if (! synthEnabled.load() || numOutputChannelsActive == 0 || outputChannelData[0] == nullptr
        || numSamples > scratchBuffer.getNumSamples())
        return;

    // 出力がモノラルの場合は右チャンネルを作業用バッファに描いて捨てる
    auto* left = outputChannelData[0];
    auto* right = (numOutputChannelsActive > 1 && outputChannelData[1] != nullptr) ? outputChannelData[1]
                                                                                   : scratchBuffer.getWritePointer (0);

    std::sort (scheduledEvents.begin(), scheduledEvents.end(), [] (const ScheduledMidiEvent& a, const ScheduledMidiEvent& b)
    {
        return a.samplePosition != b.samplePosition ? a.samplePosition < b.samplePosition : a.order < b.order;
    });

    // MIDIイベントのサンプル位置でブロックを区切って描画し、発音タイミングを正確にする
    int position = 0;

    for (const auto& event : scheduledEvents)
    {
        const auto eventPosition = juce::jlimit (0, numSamples, event.samplePosition);

        if (eventPosition > position)
        {
            synth.render (left + position, right + position, eventPosition - position);
            position = eventPosition;
        }

        synth.handleMidiMessage (event.message, event.synthChannel, event.allowProgramChange);
    }

    if (position < numSamples)
        synth.render (left + position, right + position, numSamples - position);

    float* channels[] = { left, right };
    juce::dsp::AudioBlock<float> block (channels, 2, (size_t) numSamples);
    limiter.process (juce::dsp::ProcessContextReplacing<float> (block));
}

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    const auto sampleRate = device->getCurrentSampleRate();
    const auto bufferSize = device->getCurrentBufferSizeSamples();

    synth.prepare (sampleRate);
    songPlayer.prepare (sampleRate);
    midiCollector.reset (sampleRate);

    incomingMidi.ensureSize (maxMidiBufferBytes);
    scratchBuffer.setSize (1, juce::jmax (bufferSize, 4096));

    limiter.prepare ({ sampleRate, (juce::uint32) scratchBuffer.getNumSamples(), (juce::uint32) numOutputChannels });
    limiter.reset();
}

void AudioEngine::audioDeviceStopped()
{
    synth.allNotesOff();
}

} // namespace pianolume
