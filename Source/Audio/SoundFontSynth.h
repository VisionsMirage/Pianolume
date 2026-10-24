#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

struct _fluid_hashtable_t;
struct _fluid_synth_t;

namespace pianolume
{

/** FluidSynth による SoundFont（.sf2 / .sf3）音源。

    prepare() と loadSoundFont() などの設定系はメッセージスレッドから、
    handleMidiMessage() と render() はオーディオスレッドから呼ぶ。
*/
class SoundFontSynth
{
public:
    struct Preset
    {
        int bank = 0;
        int program = 0;
        juce::String name;
    };

    SoundFontSynth();
    ~SoundFontSynth();

    /** サンプルレートが変わったときにシンセを作り直す。オーディオ停止中に呼ぶ。 */
    void prepare (double sampleRate);

    bool loadSoundFont (const juce::File& file);
    juce::File getSoundFontFile() const                 { return soundFontFile; }
    const juce::Array<Preset>& getPresets() const       { return presets; }

    void selectPreset (int bank, int program);
    int getSelectedPresetIndex() const;

    void setGain (float newGain);
    float getGain() const noexcept                      { return gain; }

    void allNotesOff();

    /** synthChannel は FluidSynth のチャンネル番号（SynthChannels を参照）。 */
    void handleMidiMessage (const juce::MidiMessage& message, int synthChannel, bool allowProgramChange);
    void render (float* left, float* right, int numSamples);

private:
    void createSynth (double sampleRate);
    void destroySynth();
    bool loadIntoSynth();
    void applyPreset();

    _fluid_hashtable_t* settings = nullptr;
    _fluid_synth_t* synth = nullptr;
    double currentSampleRate = 0.0;

    juce::File soundFontFile;
    int soundFontId = -1;
    juce::Array<Preset> presets;
    int currentBank = 0;
    int currentProgram = 0;
    float gain = 0.5f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundFontSynth)
};

} // namespace pianolume
