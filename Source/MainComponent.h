#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_data_structures/juce_data_structures.h>
#include "Audio/AudioEngine.h"
#include "Keyboard/KeyboardLayout.h"
#include "Keyboard/KeyState.h"
#include "Midi/MidiInputManager.h"
#include "Projection/CalibrationModel.h"
#include "Projection/ProjectionWindow.h"
#include "UI/CalibrationEditor.h"
#include "UI/HeaderBar.h"
#include "UI/LogPanel.h"
#include "UI/PartsPanel.h"
#include "UI/SettingsPanel.h"
#include "UI/TransportBar.h"
#include "View/FallingNotesView.h"

namespace pianolume
{

/** 操作画面。上部に再生操作、中央に落下バー、必要に応じてパート一覧（重ねて表示）とログを出す。 */
class MainComponent : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      private juce::Timer,
                      private juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    void setUpHeader();
    void setUpTransport();
    void setUpSettingsPanel();
    void restoreSettings();
    void saveSettings();

    void chooseSong();
    void loadSong (const juce::File& file);
    void updateTransport();
    void updateNowPlaying();
    void setPlayMode (int mode);
    void setSpeedPercent (int percent);

    void setLoopPoint (bool isStart);
    void toggleLoop();
    void clearLoop();
    void updateLoop();

    void setPartsVisible (bool shouldBeVisible);
    void setLogVisible (bool shouldBeVisible);
    void layoutPartsPanel();

    void refreshMidiDevices();
    void refreshDisplays();
    void midiDeviceChanged();
    void inputKeyboardChanged();
    void inputLowestNoteChanged();
    void screenKeysChanged();
    void applyLayouts();
    void refreshLowestNoteBox();
    void startDetectingLowestNote();
    void finishDetectingLowestNote (int note);
    void followOctaveShift (int note);
    void lookAheadChanged();
    void noteNamesChanged();
    NotesRenderer::KeyNames getKeyNames() const;
    void toggleProjection();
    void openProjection();
    void closeProjection();
    void openCalibration();
    void closeCalibration();
    juce::File getProfilesFolder();
    void showSettings();

    void chooseSoundFont();
    void loadSoundFont (const juce::File& file);
    void refreshPresets();
    void selectProgramFromKeyboard (int program);
    void presetChanged();
    void showAudioSettings();
    void logAudioDevice();

    void appendLog (const juce::String& text);
    static juce::String describe (const juce::MidiMessage& message);
    static juce::String describeRange (const KeyboardLayout& layout);

    SongPlayer& getPlayer() noexcept { return audioEngine.getSongPlayer(); }

    juce::ApplicationProperties appProperties;
    AudioEngine audioEngine;
    KeyState keyState;
    MidiInputManager midiInput { keyState, audioEngine.getMidiInputCallback() };
    KeyboardLayout inputLayout;   // 実際のキーボードで弾ける範囲（投影もこの範囲）
    KeyboardLayout screenLayout;  // 操作画面に描く範囲
    bool isDetectingLowestNote = false;
    juce::String preferredMidiIdentifier;
    juce::File lastSongFolder;
    int playMode = 1;
    int speedPercent = 100;
    double loopStartSeconds = -1.0;
    double loopEndSeconds = -1.0;
    bool loopEnabled = false;
    bool partsVisible = false;
    bool logVisible = false;

    HeaderBar header;
    TransportBar transport;
    FallingNotesView notesView { keyState, audioEngine.getSongPlayer() };
    PartsPanel partsPanel;
    LogPanel logPanel;

    SettingsPanel settingsPanel;
    juce::Component::SafePointer<juce::DialogWindow> settingsDialog;

    juce::Array<juce::MidiDeviceInfo> midiDevices;
    juce::MidiDeviceListConnection midiDeviceListConnection;

    std::unique_ptr<juce::FileChooser> fileChooser;
    CalibrationModel calibrationModel;
    std::unique_ptr<ProjectionWindow> projectionWindow;
    std::unique_ptr<juce::DocumentWindow> calibrationWindow;
    CalibrationEditor* calibrationEditor = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace pianolume
