#include "MainComponent.h"
#include "Platform/WindowsTitleBar.h"
#include "UI/Theme.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace pianolume
{

namespace
{
    constexpr int defaultKeyCount = 37;
    constexpr int defaultScreenKeyCount = 88;
    constexpr double defaultLookAheadSeconds = 2.5;
    constexpr double seekStepSeconds = 5.0;
    constexpr float screenKeyAreaProportion = 0.19f;
    constexpr int partsPanelMargin = 16;

    /** 再生モード。ヘッダーのセグメントの ID としても使う。 */
    enum PlayMode
    {
        listen = 1,     // 手のパートも自動演奏する
        playAlong,      // 手のパートは自分で弾く。曲は止まらない
        waitForInput    // 手のパートは自分で弾く。正しく押すまで曲が止まる
    };

    std::vector<HeaderBar::Mode> getPlayModes()
    {
        return { { PlayMode::listen, "Listen" }, { PlayMode::playAlong, "Play Along" }, { PlayMode::waitForInput, "Wait" } };
    }

    std::vector<int> getSpeedPercents()
    {
        return { 25, 50, 60, 70, 75, 80, 90, 100, 110, 125, 150, 200 };
    }

    namespace SettingKeys
    {
        constexpr auto audioDeviceState = "audioDeviceState";
        constexpr auto midiInputIdentifier = "midiInputIdentifier";
        constexpr auto inputKeys = "inputKeys";
        constexpr auto inputLowestNote = "inputLowestNote";
        constexpr auto screenKeys = "screenKeys";
        constexpr auto legacyKeyboardKeys = "keyboardKeys";
        constexpr auto synthEnabled = "synthEnabled";
        constexpr auto masterVolumeDb = "masterVolumeDb";
        constexpr auto soundFontPath = "soundFontPath";
        constexpr auto presetBank = "presetBank";
        constexpr auto presetProgram = "presetProgram";
        constexpr auto lookAheadSeconds = "lookAheadSeconds";
        constexpr auto playMode = "playMode";
        constexpr auto legacyAutoPlayHands = "autoPlayHands";
        constexpr auto songPath = "songPath";
        constexpr auto projectionGrid = "projectionGrid";
        constexpr auto logVisible = "logVisible";
        constexpr auto calibration = "calibration";
        constexpr auto projectorDisplay = "projectorDisplay";
        constexpr auto noteNamesOnNotes = "noteNamesOnNotes";
        constexpr auto legacyNoteNamesOnKeys = "noteNamesOnKeys";
        constexpr auto keyNames = "keyNames";
    }

    bool isMidiFile (const juce::File& file)
    {
        return file.hasFileExtension ("mid;midi");
    }

    bool isSoundFontFile (const juce::File& file)
    {
        return file.hasFileExtension ("sf2;sf3");
    }

    void applyDarkTitleBarTo (juce::Component* window)
    {
        if (window != nullptr)
            if (auto* peer = window->getPeer())
                applyDarkTitleBar (peer->getNativeHandle());
    }

    const juce::String dot = juce::String::fromUTF8 (" \xc2\xb7 ");

    /** 投影先の一覧で、普通のウィンドウとして開く項目の ID。ディスプレイは 2 から。 */
    constexpr int windowedProjectionId = 1;
    constexpr int firstDisplayId = 2;

    /** 合わせ込み画面のウィンドウ。閉じるボタンで onClose を呼ぶ。 */
    class CalibrationWindow : public juce::DocumentWindow
    {
    public:
        CalibrationWindow (std::unique_ptr<CalibrationEditor> editor, std::function<void()> onCloseToUse)
            : DocumentWindow ("Projection Calibration", juce::Colour (0xff11141b), DocumentWindow::closeButton),
              onClose (std::move (onCloseToUse))
        {
            setUsingNativeTitleBar (true);
            setContentOwned (editor.release(), true);
            setResizable (true, false);
            setResizeLimits (900, 600, 4000, 3000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
            applyDarkTitleBarTo (this);
        }

        void closeButtonPressed() override
        {
            if (onClose != nullptr)
                onClose();
        }

    private:
        std::function<void()> onClose;
    };
}

MainComponent::MainComponent()
    : header (getPlayModes(), getSpeedPercents())
{
    juce::PropertiesFile::Options options;
    options.applicationName = "Pianolume";
    options.folderName = "Pianolume";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    appProperties.setStorageParameters (options);

    setUpHeader();
    setUpTransport();
    setUpSettingsPanel();

    addAndMakeVisible (notesView);
    notesView.setKeyAreaProportion (screenKeyAreaProportion);

    // パート一覧は落下エリアの子にして、OpenGL の描画の上に重ねて表示する
    notesView.addChildComponent (partsPanel);
    partsPanel.onRoleChanged = [this] (int partIndex, PartRole role) { getPlayer().setPartRole (partIndex, role); };
    partsPanel.onClose = [this] { setPartsVisible (false); };

    addChildComponent (logPanel);
    logPanel.onHide = [this] { setLogVisible (false); };

    restoreSettings();
    refreshDisplays();
    refreshMidiDevices();

    // MIDIデバイスの抜き差しを検知して一覧を更新する
    midiDeviceListConnection = juce::MidiDeviceListConnection::make ([this] { refreshMidiDevices(); });

    audioEngine.getDeviceManager().addChangeListener (this);
    logAudioDevice();

    setWantsKeyboardFocus (true);
    setSize (1440, 900);
    startTimerHz (30);

    // 開発用: 起動時に、投影をプレビューのウィンドウで開いて合わせ込み画面を出す
    if (juce::JUCEApplication::getCommandLineParameterArray().contains ("--calibrate-preview"))
    {
        juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<MainComponent> (this)]
        {
            if (safeThis == nullptr)
                return;

            safeThis->settingsPanel.displayBox.setSelectedId (windowedProjectionId, juce::dontSendNotification);
            safeThis->openCalibration();
        });
    }
}

MainComponent::~MainComponent()
{
    stopTimer();
    audioEngine.getDeviceManager().removeChangeListener (this);
    saveSettings();
    calibrationEditor = nullptr;
    calibrationWindow.reset();
    closeProjection();

    if (settingsDialog != nullptr)
        delete settingsDialog.getComponent();
}

void MainComponent::setUpHeader()
{
    addAndMakeVisible (header);

    header.onOpen = [this] { chooseSong(); };
    header.onModeChanged = [this] (int mode) { setPlayMode (mode); };
    header.onSpeedChanged = [this] (int percent) { setSpeedPercent (percent); };
    header.onPartsClicked = [this] { setPartsVisible (! partsVisible); };
    header.onLogClicked = [this] { setLogVisible (! logVisible); };
    header.onProjectionClicked = [this] { toggleProjection(); };
    header.onSettingsClicked = [this] { showSettings(); };

    header.setSpeedPercent (speedPercent);
}

void MainComponent::setUpTransport()
{
    addAndMakeVisible (transport);

    transport.onReturnToStart = [this] { getPlayer().stop(); updateTransport(); };
    transport.onPlayPause = [this] { getPlayer().togglePlayPause(); updateTransport(); };
    transport.onSeek = [this] (double seconds) { getPlayer().seek (seconds); };
    transport.onSetLoopStart = [this] { setLoopPoint (true); };
    transport.onSetLoopEnd = [this] { setLoopPoint (false); };
    transport.onToggleLoop = [this] { toggleLoop(); };
}

void MainComponent::setUpSettingsPanel()
{
    settingsPanel.midiBox.onChange = [this] { midiDeviceChanged(); };

    for (auto count : KeyboardLayout::getPresetKeyCounts())
    {
        settingsPanel.keyboardBox.addItem (juce::String (count) + " keys", count);
        settingsPanel.screenKeysBox.addItem (juce::String (count) + " keys", count);
    }

    settingsPanel.keyboardBox.onChange = [this] { inputKeyboardChanged(); };
    settingsPanel.lowestNoteBox.onChange = [this] { inputLowestNoteChanged(); };
    settingsPanel.detectButton.onClick = [this] { startDetectingLowestNote(); };
    settingsPanel.screenKeysBox.onChange = [this] { screenKeysChanged(); };
    settingsPanel.lookAheadSlider.onValueChange = [this] { lookAheadChanged(); };

    settingsPanel.calibrateButton.onClick = [this] { openCalibration(); };

    settingsPanel.projectionGridToggle.onClick = [this]
    {
        if (projectionWindow != nullptr)
            projectionWindow->setShowGrid (settingsPanel.projectionGridToggle.getToggleState());
    };

    settingsPanel.noteNamesToggle.onClick = [this] { noteNamesChanged(); };
    settingsPanel.keyNamesBox.onChange = [this] { noteNamesChanged(); };

    settingsPanel.synthToggle.onClick = [this] { audioEngine.setSynthEnabled (settingsPanel.synthToggle.getToggleState()); };

    settingsPanel.volumeSlider.setRange (AudioEngine::minVolumeDb, AudioEngine::maxVolumeDb, 0.5);
    settingsPanel.volumeSlider.setDoubleClickReturnValue (true, 0.0);
    settingsPanel.volumeSlider.onValueChange = [this]
    {
        audioEngine.setMasterVolumeDb ((float) settingsPanel.volumeSlider.getValue());
    };

    settingsPanel.audioSettingsButton.onClick = [this] { showAudioSettings(); };
    settingsPanel.loadSoundFontButton.onClick = [this] { chooseSoundFont(); };
    settingsPanel.presetBox.onChange = [this] { presetChanged(); };

    settingsPanel.doneButton.onClick = [this]
    {
        if (settingsDialog != nullptr)
            settingsDialog->exitModalState (0);
    };
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (Theme::Colours::background);
}

void MainComponent::resized()
{
    auto area = getLocalBounds();

    header.setBounds (area.removeFromTop (HeaderBar::preferredHeight));
    transport.setBounds (area.removeFromTop (TransportBar::preferredHeight));

    if (logVisible)
        logPanel.setBounds (area.removeFromBottom (LogPanel::preferredHeight));

    notesView.setBounds (area);
    layoutPartsPanel();
}

void MainComponent::layoutPartsPanel()
{
    // 鍵盤にかからないよう、落下エリアの中に収める
    const auto fallHeight = juce::roundToInt ((float) notesView.getHeight() * (1.0f - screenKeyAreaProportion)) - 8;
    const auto maxHeight = juce::jmax (120, fallHeight - partsPanelMargin * 2);
    const auto width = juce::jmin (PartsPanel::preferredWidth, notesView.getWidth() - partsPanelMargin * 2);

    partsPanel.setBounds (partsPanelMargin, partsPanelMargin, width, juce::jmin (partsPanel.getPreferredHeight(), maxHeight));
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    const auto character = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());

    if (key == juce::KeyPress::spaceKey)
    {
        getPlayer().togglePlayPause();
        updateTransport();
        return true;
    }

    if (key == juce::KeyPress::homeKey)
    {
        getPlayer().stop();
        updateTransport();
        return true;
    }

    if (character == 'a' || character == 'b')
    {
        setLoopPoint (character == 'a');
        return true;
    }

    if (character == 'l')
    {
        toggleLoop();
        return true;
    }

    if (key == juce::KeyPress::leftKey || key == juce::KeyPress::rightKey)
    {
        const auto step = key == juce::KeyPress::leftKey ? -seekStepSeconds : seekStepSeconds;
        getPlayer().seek (getPlayer().getPosition() + step);
        return true;
    }

    return false;
}

bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& path : files)
        if (const juce::File file (path); isMidiFile (file) || isSoundFontFile (file))
            return true;

    return false;
}

void MainComponent::filesDropped (const juce::StringArray& files, int, int)
{
    for (const auto& path : files)
    {
        const juce::File file (path);

        if (isMidiFile (file))
            loadSong (file);
        else if (isSoundFontFile (file))
            loadSoundFont (file);
    }
}

void MainComponent::timerCallback()
{
    for (const auto& message : midiInput.popPendingMessages())
    {
        appendLog (describe (message));

        if (isDetectingLowestNote && message.isNoteOn())
            finishDetectingLowestNote (message.getNoteNumber());
        else if (message.isNoteOn())
            followOctaveShift (message.getNoteNumber());
        else if (message.isProgramChange())
            selectProgramFromKeyboard (message.getProgramChangeNumber());
    }

    updateTransport();
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    logAudioDevice();
}

void MainComponent::updateTransport()
{
    auto& player = getPlayer();
    const auto song = player.getSongForView();

    transport.setPlaying (player.isPlaying());

    if (! transport.isScrubbing())
        transport.setPosition (player.getPosition(), song != nullptr ? song->duration : 0.0, player.isWaitingForInput());
}

void MainComponent::updateNowPlaying()
{
    const auto song = getPlayer().getSongForView();

    if (song == nullptr)
    {
        header.setNowPlaying ({}, {});
        return;
    }

    int partCount = 0;

    for (const auto& part : song->parts)
        if (part.noteCount > 0)
            ++partCount;

    header.setNowPlaying (song->title,
                          TransportBar::formatTime (song->duration) + dot + juce::String (partCount) + " parts" + dot
                              + "Keyboard " + juce::String (inputLayout.getNumKeys()) + " keys (" + describeRange (inputLayout) + ")");
}

void MainComponent::setPlayMode (int mode)
{
    playMode = juce::jlimit ((int) PlayMode::listen, (int) PlayMode::waitForInput, mode);
    header.setMode (playMode);
    getPlayer().setAutoPlayHands (playMode == PlayMode::listen);
    getPlayer().setWaitForInput (playMode == PlayMode::waitForInput);
}

void MainComponent::setSpeedPercent (int percent)
{
    speedPercent = percent;
    header.setSpeedPercent (percent);
    getPlayer().setRate (percent / 100.0);
}

void MainComponent::setLoopPoint (bool isStart)
{
    const auto position = getPlayer().getPosition();

    if (isStart)
        loopStartSeconds = position;
    else
        loopEndSeconds = position;

    if (loopStartSeconds >= 0.0 && loopEndSeconds >= 0.0)
    {
        if (loopEndSeconds < loopStartSeconds)
            std::swap (loopStartSeconds, loopEndSeconds);

        // 始点と終点の両方を決めたらループを有効にする
        if (! juce::approximatelyEqual (loopStartSeconds, loopEndSeconds))
            loopEnabled = true;
    }

    updateLoop();
}

void MainComponent::toggleLoop()
{
    loopEnabled = ! loopEnabled;
    updateLoop();
}

void MainComponent::clearLoop()
{
    loopStartSeconds = -1.0;
    loopEndSeconds = -1.0;
    loopEnabled = false;
    updateLoop();
}

void MainComponent::updateLoop()
{
    const auto hasRange = loopStartSeconds >= 0.0 && loopEndSeconds > loopStartSeconds;
    loopEnabled = loopEnabled && hasRange;

    auto& player = getPlayer();
    player.setLoopRange (juce::jmax (0.0, loopStartSeconds), juce::jmax (0.0, loopEndSeconds));
    player.setLoopEnabled (loopEnabled);

    transport.setLoop (loopStartSeconds, loopEndSeconds, loopEnabled);

    // ループを有効にしたとき区間の外にいたら、始点へ移動する
    if (loopEnabled)
    {
        const auto position = player.getPosition();

        if (position < loopStartSeconds || position >= loopEndSeconds)
            player.seek (loopStartSeconds);
    }
}

void MainComponent::setPartsVisible (bool shouldBeVisible)
{
    partsVisible = shouldBeVisible;
    header.setPartsVisible (partsVisible);
    layoutPartsPanel();
    partsPanel.setVisible (partsVisible);
}

void MainComponent::setLogVisible (bool shouldBeVisible)
{
    logVisible = shouldBeVisible;
    header.setLogVisible (logVisible);
    logPanel.setVisible (logVisible);
    resized();
}

void MainComponent::restoreSettings()
{
    auto* settings = appProperties.getUserSettings();

    const auto audioState = settings->getXmlValue (SettingKeys::audioDeviceState);
    audioEngine.initialise (audioState.get());

    audioEngine.setMasterVolumeDb ((float) settings->getDoubleValue (SettingKeys::masterVolumeDb, 0.0));
    settingsPanel.volumeSlider.setValue (audioEngine.getMasterVolumeDb(), juce::dontSendNotification);

    const auto enabled = settings->getBoolValue (SettingKeys::synthEnabled, true);
    audioEngine.setSynthEnabled (enabled);
    settingsPanel.synthToggle.setToggleState (enabled, juce::dontSendNotification);

    const juce::File soundFont (settings->getValue (SettingKeys::soundFontPath));

    if (soundFont.existsAsFile())
    {
        loadSoundFont (soundFont);

        if (settings->containsKey (SettingKeys::presetBank))
        {
            audioEngine.getSynth().selectPreset (settings->getIntValue (SettingKeys::presetBank),
                                                 settings->getIntValue (SettingKeys::presetProgram));
            refreshPresets();
        }
    }
    else
    {
        refreshPresets();
    }

    preferredMidiIdentifier = settings->getValue (SettingKeys::midiInputIdentifier);

    const auto presetCounts = KeyboardLayout::getPresetKeyCounts();

    auto inputKeys = settings->getIntValue (SettingKeys::inputKeys,
                                            settings->getIntValue (SettingKeys::legacyKeyboardKeys, defaultKeyCount));

    if (! presetCounts.contains (inputKeys))
        inputKeys = defaultKeyCount;

    inputLayout = { inputKeys, settings->getIntValue (SettingKeys::inputLowestNote,
                                                      KeyboardLayout::fromPreset (inputKeys).getLowestNote()) };

    auto screenKeys = settings->getIntValue (SettingKeys::screenKeys, defaultScreenKeyCount);

    if (! presetCounts.contains (screenKeys))
        screenKeys = defaultScreenKeyCount;

    screenLayout = KeyboardLayout::fromPreset (screenKeys);

    settingsPanel.keyboardBox.setSelectedId (inputLayout.getNumKeys(), juce::dontSendNotification);
    settingsPanel.screenKeysBox.setSelectedId (screenLayout.getNumKeys(), juce::dontSendNotification);
    refreshLowestNoteBox();
    applyLayouts();

    settingsPanel.lookAheadSlider.setValue (settings->getDoubleValue (SettingKeys::lookAheadSeconds, defaultLookAheadSeconds),
                                            juce::dontSendNotification);
    lookAheadChanged();

    settingsPanel.projectionGridToggle.setToggleState (settings->getBoolValue (SettingKeys::projectionGrid, false),
                                                       juce::dontSendNotification);
    settingsPanel.noteNamesToggle.setToggleState (settings->getBoolValue (SettingKeys::noteNamesOnNotes, false),
                                                  juce::dontSendNotification);
    // 以前の ON / OFF の設定は、ON なら白鍵のみに読み替える
    const auto legacyKeyNames = settings->getBoolValue (SettingKeys::legacyNoteNamesOnKeys, false) ? NotesRenderer::KeyNames::whiteKeys
                                                                                                   : NotesRenderer::KeyNames::none;
    settingsPanel.keyNamesBox.setSelectedId (settings->getIntValue (SettingKeys::keyNames, (int) legacyKeyNames) + 1,
                                             juce::dontSendNotification);
    noteNamesChanged();

    const auto legacyMode = settings->getBoolValue (SettingKeys::legacyAutoPlayHands, true) ? PlayMode::listen
                                                                                            : PlayMode::playAlong;
    setPlayMode (settings->getIntValue (SettingKeys::playMode, legacyMode));

    setLogVisible (settings->getBoolValue (SettingKeys::logVisible, false));

    calibrationModel.setCalibration (ProjectionCalibration::fromVar (juce::JSON::parse (settings->getValue (SettingKeys::calibration))));
    refreshDisplays();
    settingsPanel.displayBox.setSelectedId (settings->getIntValue (SettingKeys::projectorDisplay,
                                                                   settingsPanel.displayBox.getSelectedId()),
                                            juce::dontSendNotification);

    const juce::File song (settings->getValue (SettingKeys::songPath));

    if (song.existsAsFile())
        loadSong (song);
}

void MainComponent::saveSettings()
{
    auto* settings = appProperties.getUserSettings();
    auto& synth = audioEngine.getSynth();

    if (const auto audioState = audioEngine.createStateXml())
        settings->setValue (SettingKeys::audioDeviceState, audioState.get());

    if (midiInput.getCurrentDeviceIdentifier().isNotEmpty())
        settings->setValue (SettingKeys::midiInputIdentifier, midiInput.getCurrentDeviceIdentifier());

    settings->setValue (SettingKeys::inputKeys, inputLayout.getNumKeys());
    settings->setValue (SettingKeys::inputLowestNote, inputLayout.getLowestNote());
    settings->setValue (SettingKeys::screenKeys, screenLayout.getNumKeys());
    settings->removeValue (SettingKeys::legacyKeyboardKeys);
    settings->setValue (SettingKeys::synthEnabled, audioEngine.isSynthEnabled());
    settings->setValue (SettingKeys::masterVolumeDb, audioEngine.getMasterVolumeDb());
    settings->removeValue ("synthGain"); // 旧形式（倍率）の設定
    settings->setValue (SettingKeys::soundFontPath, synth.getSoundFontFile().getFullPathName());
    settings->setValue (SettingKeys::lookAheadSeconds, settingsPanel.lookAheadSlider.getValue());
    settings->setValue (SettingKeys::projectionGrid, settingsPanel.projectionGridToggle.getToggleState());
    settings->setValue (SettingKeys::noteNamesOnNotes, settingsPanel.noteNamesToggle.getToggleState());
    settings->setValue (SettingKeys::keyNames, (int) getKeyNames());
    settings->removeValue (SettingKeys::legacyNoteNamesOnKeys);
    settings->setValue (SettingKeys::playMode, playMode);
    settings->removeValue (SettingKeys::legacyAutoPlayHands);
    settings->setValue (SettingKeys::logVisible, logVisible);
    settings->setValue (SettingKeys::calibration, juce::JSON::toString (calibrationModel.getCalibration().toVar(), true));
    settings->setValue (SettingKeys::projectorDisplay, settingsPanel.displayBox.getSelectedId());

    if (const auto song = getPlayer().getSongForView())
        settings->setValue (SettingKeys::songPath, song->file.getFullPathName());

    const auto presetIndex = synth.getSelectedPresetIndex();

    if (juce::isPositiveAndBelow (presetIndex, synth.getPresets().size()))
    {
        const auto& preset = synth.getPresets().getReference (presetIndex);
        settings->setValue (SettingKeys::presetBank, preset.bank);
        settings->setValue (SettingKeys::presetProgram, preset.program);
    }

    appProperties.saveIfNeeded();
}

void MainComponent::chooseSong()
{
    const auto startLocation = lastSongFolder.isDirectory() ? lastSongFolder
                                                            : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    fileChooser = std::make_unique<juce::FileChooser> ("Open a MIDI file", startLocation, "*.mid;*.midi");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& chooser)
                              {
                                  const auto file = chooser.getResult();

                                  if (file != juce::File())
                                      loadSong (file);
                              });
}

void MainComponent::loadSong (const juce::File& file)
{
    juce::String error;
    auto song = Song::load (file, error);

    if (song == nullptr)
    {
        appendLog (error);
        return;
    }

    lastSongFolder = file.getParentDirectory();

    auto& player = getPlayer();
    player.setSong (song);

    partsPanel.setSong (song.get(), [&player] (int partIndex) { return player.getPartRole (partIndex); });
    layoutPartsPanel();
    clearLoop();
    updateNowPlaying();

    int outsideScreen = 0;
    int outsideInput = 0;
    int lowest = 127;
    int highest = 0;

    for (const auto& note : song->notes)
    {
        if (song->parts[(size_t) note.part].channel == 10)
            continue;

        lowest = juce::jmin (lowest, note.note);
        highest = juce::jmax (highest, note.note);

        if (! screenLayout.contains (note.note))
            ++outsideScreen;

        if (! inputLayout.contains (note.note))
            ++outsideInput;
    }

    appendLog ("Song loaded: " + song->title + " (" + TransportBar::formatTime (song->duration) + ", "
               + juce::String ((int) song->notes.size()) + " notes, range "
               + juce::MidiMessage::getMidiNoteName (lowest, true, true, 4) + " - "
               + juce::MidiMessage::getMidiNoteName (highest, true, true, 4) + ")");

    if (outsideInput > 0)
        appendLog ("  " + juce::String (outsideInput) + " notes are outside your keyboard (greyed out, played automatically)");

    if (outsideScreen > 0)
        appendLog ("  " + juce::String (outsideScreen) + " notes are outside the screen keys and are not shown");

    updateTransport();
}

void MainComponent::refreshMidiDevices()
{
    midiDevices = MidiInputManager::getAvailableDevices();

    auto& midiBox = settingsPanel.midiBox;
    const auto currentId = midiInput.getCurrentDeviceIdentifier();
    int selectedIndex = -1;
    int preferredIndex = -1;

    midiBox.clear (juce::dontSendNotification);

    for (int i = 0; i < midiDevices.size(); ++i)
    {
        midiBox.addItem (midiDevices[i].name, i + 1);

        if (midiDevices[i].identifier == currentId)
            selectedIndex = i;

        if (midiDevices[i].identifier == preferredMidiIdentifier)
            preferredIndex = i;
    }

    // 開いていたデバイスが無くなったら閉じる
    if (currentId.isNotEmpty() && selectedIndex < 0)
    {
        midiInput.closeDevice();
        audioEngine.getSynth().allNotesOff();
        appendLog ("Disconnected");
    }

    // 未選択なら前回のデバイス、無ければ先頭のデバイスを開く
    if (selectedIndex < 0 && ! midiDevices.isEmpty())
    {
        midiBox.setSelectedItemIndex (juce::jmax (0, preferredIndex), juce::sendNotificationSync);
        return;
    }

    midiBox.setSelectedItemIndex (selectedIndex, juce::dontSendNotification);
}

void MainComponent::refreshDisplays()
{
    auto& displayBox = settingsPanel.displayBox;
    const auto previousId = displayBox.getSelectedId();

    displayBox.clear (juce::dontSendNotification);

    // プロジェクタが無くても試せるよう、普通のウィンドウに出す項目を先頭に置く
    displayBox.addItem ("Window (preview)", windowedProjectionId);

    const auto& displays = juce::Desktop::getInstance().getDisplays().displays;
    int defaultId = windowedProjectionId;

    for (int i = 0; i < displays.size(); ++i)
    {
        const auto& display = displays.getReference (i);
        const auto area = display.logicalBounds.toNearestInt();

        displayBox.addItem ("Display " + juce::String (i + 1)
                                + " (" + juce::String (area.getWidth()) + juce::String::fromUTF8 (" \xc3\x97 ")
                                + juce::String (area.getHeight()) + ")" + (display.isMain ? " - Main" : ""),
                            firstDisplayId + i);

        // プロジェクタはメイン以外のディスプレイである可能性が高い
        if (! display.isMain)
            defaultId = firstDisplayId + i;
    }

    const auto previousStillExists = previousId == windowedProjectionId
                                  || juce::isPositiveAndBelow (previousId - firstDisplayId, displays.size());
    displayBox.setSelectedId (previousStillExists ? previousId : defaultId, juce::dontSendNotification);
}

void MainComponent::midiDeviceChanged()
{
    const auto index = settingsPanel.midiBox.getSelectedItemIndex();

    if (! juce::isPositiveAndBelow (index, midiDevices.size()))
        return;

    const auto& device = midiDevices.getReference (index);

    if (device.identifier == midiInput.getCurrentDeviceIdentifier())
        return;

    audioEngine.getSynth().allNotesOff();

    if (midiInput.openDevice (device.identifier))
    {
        preferredMidiIdentifier = device.identifier;
        appendLog ("Opened: " + device.name);
    }
    else
    {
        appendLog ("Failed to open: " + device.name);
    }
}

void MainComponent::inputKeyboardChanged()
{
    // 鍵数を変えたら、最低音はその鍵数の一般的な値に戻す
    inputLayout = KeyboardLayout::fromPreset (settingsPanel.keyboardBox.getSelectedId());
    refreshLowestNoteBox();
    applyLayouts();
}

void MainComponent::inputLowestNoteChanged()
{
    inputLayout = { inputLayout.getNumKeys(), settingsPanel.lowestNoteBox.getSelectedId() - 1 };
    applyLayouts();
}

void MainComponent::screenKeysChanged()
{
    screenLayout = KeyboardLayout::fromPreset (settingsPanel.screenKeysBox.getSelectedId());
    applyLayouts();
}

void MainComponent::applyLayouts()
{
    const auto lowest = inputLayout.getLowestNote();
    const auto highest = inputLayout.getHighestNote();

    notesView.setLayout (screenLayout);
    notesView.setPlayableRange (lowest, highest);
    getPlayer().setPlayableRange (lowest, highest);

    // 投影は実際のキーボードに重ねるので、入力鍵盤の範囲だけを描く
    if (projectionWindow != nullptr)
        projectionWindow->setLayout (inputLayout);

    if (calibrationEditor != nullptr)
        calibrationEditor->setLayout (inputLayout);

    updateNowPlaying();

    appendLog ("Keyboard: " + juce::String (inputLayout.getNumKeys()) + " keys (" + describeRange (inputLayout)
               + "), screen: " + juce::String (screenLayout.getNumKeys()) + " keys");
}

void MainComponent::refreshLowestNoteBox()
{
    auto& box = settingsPanel.lowestNoteBox;
    box.clear (juce::dontSendNotification);

    for (int note = 0; note + inputLayout.getNumKeys() <= 128; ++note)
        box.addItem ("Lowest: " + juce::MidiMessage::getMidiNoteName (note, true, true, 4), note + 1);

    box.setSelectedId (inputLayout.getLowestNote() + 1, juce::dontSendNotification);
}

void MainComponent::startDetectingLowestNote()
{
    isDetectingLowestNote = true;
    settingsPanel.detectButton.setButtonText ("Press key...");
    appendLog ("Press the leftmost key of your keyboard");
}

void MainComponent::finishDetectingLowestNote (int note)
{
    isDetectingLowestNote = false;
    settingsPanel.detectButton.setButtonText ("Detect");

    inputLayout = { inputLayout.getNumKeys(), note };
    refreshLowestNoteBox();
    applyLayouts();
}

void MainComponent::lookAheadChanged()
{
    const auto seconds = settingsPanel.lookAheadSlider.getValue();
    notesView.setLookAheadSeconds (seconds);

    if (projectionWindow != nullptr)
        projectionWindow->setLookAheadSeconds (seconds);
}

void MainComponent::noteNamesChanged()
{
    const auto onNotes = settingsPanel.noteNamesToggle.getToggleState();
    const auto onKeys = getKeyNames();
    notesView.setNoteNames (onNotes, onKeys);

    if (projectionWindow != nullptr)
        projectionWindow->setNoteNames (onNotes, onKeys);
}

NotesRenderer::KeyNames MainComponent::getKeyNames() const
{
    const auto id = settingsPanel.keyNamesBox.getSelectedId();
    return id >= 1 && id <= 4 ? (NotesRenderer::KeyNames) (id - 1) : NotesRenderer::KeyNames::none;
}

void MainComponent::toggleProjection()
{
    if (projectionWindow != nullptr)
        closeProjection();
    else
        openProjection();
}

void MainComponent::openProjection()
{
    if (projectionWindow != nullptr)
        return;

    refreshDisplays();

    const auto& displays = juce::Desktop::getInstance().getDisplays().displays;
    const auto selectedId = settingsPanel.displayBox.getSelectedId();
    const auto isWindowed = selectedId == windowedProjectionId;
    juce::Rectangle<int> area;

    if (isWindowed)
    {
        const auto mainArea = displays.isEmpty() ? juce::Rectangle<int> (0, 0, 1600, 900)
                                                 : displays.getReference (0).userBounds.toNearestInt();
        area = juce::Rectangle<int> (1280, 720).withCentre (mainArea.getCentre());
    }
    else if (juce::isPositiveAndBelow (selectedId - firstDisplayId, displays.size()))
    {
        area = displays.getReference (selectedId - firstDisplayId).logicalBounds.toNearestInt();
    }
    else
    {
        return;
    }

    projectionWindow = std::make_unique<ProjectionWindow> (keyState, getPlayer(), calibrationModel, inputLayout, area, isWindowed);
    projectionWindow->setLookAheadSeconds (settingsPanel.lookAheadSlider.getValue());
    projectionWindow->setShowGrid (settingsPanel.projectionGridToggle.getToggleState());
    projectionWindow->setNoteNames (settingsPanel.noteNamesToggle.getToggleState(), getKeyNames());

    // ウィンドウ自身のキーイベント処理中に削除しないよう、非同期で閉じる
    projectionWindow->onCloseRequested = [safeThis = juce::Component::SafePointer<MainComponent> (this)]
    {
        juce::MessageManager::callAsync ([safeThis]
        {
            if (safeThis != nullptr)
                safeThis->closeProjection();
        });
    };

    projectionWindow->onKeyPressed = [this] (const juce::KeyPress& key) { return keyPressed (key); };

    projectionWindow->onResized = [this]
    {
        if (calibrationEditor != nullptr && projectionWindow != nullptr)
            calibrationEditor->setProjectorSize ({ projectionWindow->getWidth(), projectionWindow->getHeight() });
    };

    header.setProjectionActive (true);
}

void MainComponent::closeProjection()
{
    if (calibrationWindow != nullptr)
        closeCalibration();

    projectionWindow.reset();
    header.setProjectionActive (false);
}

juce::File MainComponent::getProfilesFolder()
{
    return appProperties.getUserSettings()->getFile().getSiblingFile ("Profiles");
}

void MainComponent::openCalibration()
{
    if (settingsDialog != nullptr)
        settingsDialog->exitModalState (0);

    if (calibrationWindow != nullptr)
    {
        calibrationWindow->toFront (true);
        return;
    }

    openProjection();

    if (projectionWindow == nullptr)
        return;

    auto editor = std::make_unique<CalibrationEditor> (calibrationModel, getProfilesFolder());
    editor->setProjectorSize ({ projectionWindow->getWidth(), projectionWindow->getHeight() });
    editor->setLayout (inputLayout);
    editor->onDone = [this] { closeCalibration(); };
    calibrationEditor = editor.get();

    calibrationModel.setCalibrating (true);

    // ウィンドウ自身のボタン処理中に削除しないよう、非同期で閉じる
    calibrationWindow = std::make_unique<CalibrationWindow> (std::move (editor), [safeThis = juce::Component::SafePointer<MainComponent> (this)]
    {
        juce::MessageManager::callAsync ([safeThis]
        {
            if (safeThis != nullptr)
                safeThis->closeCalibration();
        });
    });
}

void MainComponent::closeCalibration()
{
    calibrationModel.setCalibrating (false);
    calibrationEditor = nullptr;
    calibrationWindow.reset();
    saveSettings();
}

void MainComponent::showSettings()
{
    if (settingsDialog != nullptr)
    {
        settingsDialog->toFront (true);
        return;
    }

    refreshDisplays();

    juce::DialogWindow::LaunchOptions options;
    options.content.setNonOwned (&settingsPanel);
    options.dialogTitle = "Settings";
    options.dialogBackgroundColour = juce::Colour (0xff11141b);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    settingsDialog = options.launchAsync();
    applyDarkTitleBarTo (settingsDialog.getComponent());
}

void MainComponent::chooseSoundFont()
{
    const auto current = audioEngine.getSynth().getSoundFontFile();
    const auto startLocation = current.existsAsFile() ? current.getParentDirectory()
                                                      : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    fileChooser = std::make_unique<juce::FileChooser> ("Select a SoundFont", startLocation, "*.sf2;*.sf3");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& chooser)
                              {
                                  const auto file = chooser.getResult();

                                  if (file != juce::File())
                                      loadSoundFont (file);
                              });
}

void MainComponent::loadSoundFont (const juce::File& file)
{
    juce::MouseCursor::showWaitCursor();
    const auto loaded = audioEngine.getSynth().loadSoundFont (file);
    juce::MouseCursor::hideWaitCursor();

    if (loaded)
        appendLog ("SoundFont loaded: " + file.getFileName()
                   + " (" + juce::String (audioEngine.getSynth().getPresets().size()) + " presets)");
    else
        appendLog ("Failed to load SoundFont: " + file.getFullPathName());

    // 音色を読み込み直すと曲のチャンネルの音色が初期化されるので、現在位置から設定し直す
    getPlayer().seek (getPlayer().getPosition());

    refreshPresets();
}

void MainComponent::followOctaveShift (int note)
{
    // キーボードのオクターブボタンは MIDI で知らせてこないので、鍵盤の範囲の外の音が来たらずらしたとみなす。
    // 範囲が重なる音だけを弾いているあいだは区別できない
    const auto lowest = inputLayout.getLowestNote();
    const auto highest = inputLayout.getHighestNote();

    if (note >= lowest && note <= highest)
        return;

    const auto octaves = note < lowest ? -((lowest - note + 11) / 12) : (note - highest + 11) / 12;
    const auto newLowest = juce::jlimit (0, 128 - inputLayout.getNumKeys(), lowest + octaves * 12);

    if (newLowest == lowest)
        return;

    inputLayout = { inputLayout.getNumKeys(), newLowest };
    refreshLowestNoteBox();
    applyLayouts();
    appendLog ("Octave shift " + juce::String (octaves > 0 ? "+" : "") + juce::String (octaves) + " detected");
}

void MainComponent::selectProgramFromKeyboard (int program)
{
    auto& synth = audioEngine.getSynth();
    const auto& presets = synth.getPresets();

    if (presets.isEmpty())
        return;

    // 今のバンクに同じ番号の音色があればそれを、なければバンク 0、それもなければ同じ番号の最初の音色を選ぶ
    const auto current = synth.getSelectedPresetIndex();
    const auto currentBank = juce::isPositiveAndBelow (current, presets.size()) ? presets.getReference (current).bank : 0;
    auto found = -1;

    for (const auto bank : { currentBank, 0, -1 })
    {
        for (int i = 0; i < presets.size() && found < 0; ++i)
            if (presets.getReference (i).program == program && (bank < 0 || presets.getReference (i).bank == bank))
                found = i;

        if (found >= 0)
            break;
    }

    if (found < 0)
    {
        appendLog ("Program " + juce::String (program) + ": no preset with this number");
        return;
    }

    const auto& preset = presets.getReference (found);
    synth.selectPreset (preset.bank, preset.program);
    settingsPanel.presetBox.setSelectedItemIndex (found, juce::dontSendNotification);
    appendLog ("Program " + juce::String (program) + ": " + preset.name);
}

void MainComponent::refreshPresets()
{
    const auto& synth = audioEngine.getSynth();
    const auto& presets = synth.getPresets();
    auto& presetBox = settingsPanel.presetBox;

    presetBox.clear (juce::dontSendNotification);

    for (int i = 0; i < presets.size(); ++i)
    {
        const auto& preset = presets.getReference (i);
        presetBox.addItem (juce::String (preset.bank).paddedLeft ('0', 3) + ":"
                               + juce::String (preset.program).paddedLeft ('0', 3) + "  " + preset.name,
                           i + 1);
    }

    presetBox.setSelectedItemIndex (synth.getSelectedPresetIndex(), juce::dontSendNotification);

    const auto file = synth.getSoundFontFile();
    settingsPanel.soundFontName.setText (file.existsAsFile() ? file.getFileName() : "Not loaded - choose a .sf2 / .sf3 file",
                                         juce::dontSendNotification);
}

void MainComponent::presetChanged()
{
    auto& synth = audioEngine.getSynth();
    const auto index = settingsPanel.presetBox.getSelectedItemIndex();

    if (! juce::isPositiveAndBelow (index, synth.getPresets().size()))
        return;

    const auto& preset = synth.getPresets().getReference (index);
    synth.selectPreset (preset.bank, preset.program);
}

void MainComponent::showAudioSettings()
{
    auto selector = std::make_unique<juce::AudioDeviceSelectorComponent> (audioEngine.getDeviceManager(),
                                                                          0, 0,  // 入力チャンネル
                                                                          1, 2,  // 出力チャンネル
                                                                          false, false, true, false);
    selector->setSize (520, 420);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (selector.release());
    options.dialogTitle = "Audio Device";
    options.dialogBackgroundColour = juce::Colour (0xff11141b);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    applyDarkTitleBarTo (options.launchAsync());
}

void MainComponent::logAudioDevice()
{
    auto* device = audioEngine.getDeviceManager().getCurrentAudioDevice();

    if (device == nullptr)
    {
        appendLog ("Audio: no device (song playback needs an audio device)");
        return;
    }

    const auto sampleRate = device->getCurrentSampleRate();
    const auto bufferSize = device->getCurrentBufferSizeSamples();
    const auto latencyMs = sampleRate > 0.0 ? (bufferSize + device->getOutputLatencyInSamples()) * 1000.0 / sampleRate : 0.0;

    appendLog ("Audio: " + device->getTypeName() + " / " + device->getName()
               + "  " + juce::String (sampleRate, 0) + " Hz, " + juce::String (bufferSize) + " samples"
               + " (output latency approx. " + juce::String (latencyMs, 1) + " ms)");
}

void MainComponent::appendLog (const juce::String& text)
{
    logPanel.append (text);
}

juce::String MainComponent::describe (const juce::MidiMessage& message)
{
    const auto noteName = [&message]
    {
        return juce::MidiMessage::getMidiNoteName (message.getNoteNumber(), true, true, 4)
               + " (" + juce::String (message.getNoteNumber()) + ")";
    };

    if (message.isNoteOn())
        return "Note On   " + noteName() + "  vel " + juce::String (message.getVelocity());

    if (message.isNoteOff())
        return "Note Off  " + noteName();

    if (message.isSustainPedalOn())
        return "Sustain On";

    if (message.isSustainPedalOff())
        return "Sustain Off";

    if (message.isController())
        return "CC " + juce::String (message.getControllerNumber()) + " = " + juce::String (message.getControllerValue());

    if (message.isPitchWheel())
        return "Pitch Bend " + juce::String (message.getPitchWheelValue());

    return message.getDescription();
}

juce::String MainComponent::describeRange (const KeyboardLayout& layout)
{
    return juce::MidiMessage::getMidiNoteName (layout.getLowestNote(), true, true, 4) + juce::String::fromUTF8 ("\xe2\x80\x93")
         + juce::MidiMessage::getMidiNoteName (layout.getHighestNote(), true, true, 4);
}

} // namespace pianolume
