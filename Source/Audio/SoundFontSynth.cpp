#include "SoundFontSynth.h"

#include <fluidsynth.h>
#include "ScheduledMidiEvent.h"

namespace pianolume
{

namespace
{
    constexpr double defaultSampleRate = 44100.0;
}

SoundFontSynth::SoundFontSynth()
{
    createSynth (defaultSampleRate);
}

SoundFontSynth::~SoundFontSynth()
{
    destroySynth();
}

void SoundFontSynth::prepare (double sampleRate)
{
    // FluidSynth のサンプルレートは生成時にしか指定できないため、変わったら作り直して音色を読み込み直す
    if (synth != nullptr && juce::approximatelyEqual (sampleRate, currentSampleRate))
        return;

    destroySynth();
    createSynth (sampleRate);

    if (soundFontFile.existsAsFile())
        loadIntoSynth();
}

bool SoundFontSynth::loadSoundFont (const juce::File& file)
{
    soundFontFile = file;
    currentBank = 0;
    currentProgram = 0;

    if (! loadIntoSynth())
    {
        soundFontFile = juce::File();
        return false;
    }

    if (! presets.isEmpty())
        selectPreset (presets.getReference (0).bank, presets.getReference (0).program);

    return true;
}

void SoundFontSynth::selectPreset (int bank, int program)
{
    currentBank = bank;
    currentProgram = program;
    applyPreset();
}

int SoundFontSynth::getSelectedPresetIndex() const
{
    for (int i = 0; i < presets.size(); ++i)
        if (presets.getReference (i).bank == currentBank && presets.getReference (i).program == currentProgram)
            return i;

    return -1;
}

void SoundFontSynth::setGain (float newGain)
{
    gain = juce::jlimit (0.0f, 10.0f, newGain); // FluidSynth の synth.gain の範囲

    if (synth != nullptr)
        fluid_synth_set_gain (synth, gain);
}

void SoundFontSynth::allNotesOff()
{
    if (synth != nullptr)
        fluid_synth_all_notes_off (synth, -1);
}

void SoundFontSynth::handleMidiMessage (const juce::MidiMessage& message, int channel, bool allowProgramChange)
{
    if (synth == nullptr)
        return;

    if (message.isNoteOn())
        fluid_synth_noteon (synth, channel, message.getNoteNumber(), message.getVelocity());
    else if (message.isNoteOff())
        fluid_synth_noteoff (synth, channel, message.getNoteNumber());
    else if (message.isController())
        fluid_synth_cc (synth, channel, message.getControllerNumber(), message.getControllerValue());
    else if (message.isPitchWheel())
        fluid_synth_pitch_bend (synth, channel, message.getPitchWheelValue());
    else if (message.isChannelPressure())
        fluid_synth_channel_pressure (synth, channel, message.getChannelPressureValue());
    else if (message.isAftertouch())
        fluid_synth_key_pressure (synth, channel, message.getNoteNumber(), message.getAfterTouchValue());
    else if (message.isProgramChange() && allowProgramChange)
        fluid_synth_program_change (synth, channel, message.getProgramChangeNumber());

    // 演奏用チャンネルのプログラムチェンジはここでは処理しない。MainComponent が受けて音色の選択（設定画面の Preset）を切り替える
}

void SoundFontSynth::render (float* left, float* right, int numSamples)
{
    if (synth == nullptr)
    {
        juce::FloatVectorOperations::clear (left, numSamples);
        juce::FloatVectorOperations::clear (right, numSamples);
        return;
    }

    fluid_synth_write_float (synth, numSamples, left, 0, 1, right, 0, 1);
}

void SoundFontSynth::createSynth (double sampleRate)
{
    settings = new_fluid_settings();
    fluid_settings_setnum (settings, "synth.sample-rate", sampleRate);
    fluid_settings_setint (settings, "synth.midi-channels", SynthChannels::total);

    synth = new_fluid_synth (settings);
    currentSampleRate = sampleRate;

    fluid_synth_set_gain (synth, gain);
    soundFontId = -1;
}

void SoundFontSynth::destroySynth()
{
    if (synth != nullptr)
        delete_fluid_synth (synth);

    if (settings != nullptr)
        delete_fluid_settings (settings);

    synth = nullptr;
    settings = nullptr;
    soundFontId = -1;
}

bool SoundFontSynth::loadIntoSynth()
{
    presets.clearQuick();

    if (synth == nullptr)
        return false;

    if (soundFontId >= 0)
    {
        fluid_synth_sfunload (synth, soundFontId, 1);
        soundFontId = -1;
    }

    const auto id = fluid_synth_sfload (synth, soundFontFile.getFullPathName().toRawUTF8(), 1);

    if (id == FLUID_FAILED)
        return false;

    soundFontId = id;

    if (auto* sfont = fluid_synth_get_sfont_by_id (synth, soundFontId))
    {
        fluid_sfont_iteration_start (sfont);

        while (auto* preset = fluid_sfont_iteration_next (sfont))
            presets.add ({ fluid_preset_get_banknum (preset),
                           fluid_preset_get_num (preset),
                           juce::String::fromUTF8 (fluid_preset_get_name (preset)) });
    }

    std::sort (presets.begin(), presets.end(), [] (const Preset& a, const Preset& b)
    {
        return a.bank != b.bank ? a.bank < b.bank : a.program < b.program;
    });

    applyPreset();
    return true;
}

void SoundFontSynth::applyPreset()
{
    if (synth == nullptr || soundFontId < 0)
        return;

    // 選択した音色は演奏用チャンネルにだけ割り当てる。曲のチャンネル（0〜15）は曲の音色を使う
    for (const auto channel : { SynthChannels::live, SynthChannels::songHands })
    {
        fluid_synth_set_channel_type (synth, channel, CHANNEL_TYPE_MELODIC);
        fluid_synth_program_select (synth, channel, soundFontId, currentBank, currentProgram);
    }
}

} // namespace pianolume
