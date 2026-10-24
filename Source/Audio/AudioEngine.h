#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_dsp/juce_dsp.h>
#include "ScheduledMidiEvent.h"
#include "SoundFontSynth.h"
#include "Song/SongPlayer.h"

namespace pianolume
{

/** オーディオデバイスを管理し、キーボードの演奏と曲の再生を内蔵音源で発音する。 */
class AudioEngine : private juce::AudioIODeviceCallback
{
public:
    AudioEngine();
    ~AudioEngine() override;

    /** 保存しておいたデバイス設定で初期化する（null なら既定のデバイスを使う）。 */
    void initialise (const juce::XmlElement* savedState);
    std::unique_ptr<juce::XmlElement> createStateXml() const;

    juce::AudioDeviceManager& getDeviceManager() noexcept      { return deviceManager; }
    SoundFontSynth& getSynth() noexcept                        { return synth; }
    SongPlayer& getSongPlayer() noexcept                       { return songPlayer; }

    /** MIDI入力スレッドからメッセージを渡す先。 */
    juce::MidiInputCallback& getMidiInputCallback() noexcept   { return midiCollector; }

    void setSynthEnabled (bool shouldBeEnabled);
    bool isSynthEnabled() const noexcept                       { return synthEnabled.load(); }

    /** マスター音量（dB）。出力はリミッターを通すので、上げすぎても音割れしない。 */
    void setMasterVolumeDb (float newVolumeDb);
    float getMasterVolumeDb() const noexcept                   { return masterVolumeDb; }

    static constexpr float minVolumeDb = -20.0f;
    static constexpr float maxVolumeDb = 12.0f;

private:
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    juce::AudioDeviceManager deviceManager;
    juce::MidiMessageCollector midiCollector;
    SoundFontSynth synth;
    SongPlayer songPlayer;
    juce::dsp::Limiter<float> limiter;
    float masterVolumeDb = 0.0f;

    juce::MidiBuffer incomingMidi;
    std::vector<ScheduledMidiEvent> scheduledEvents;
    juce::AudioBuffer<float> scratchBuffer;
    std::atomic<bool> synthEnabled { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};

} // namespace pianolume
