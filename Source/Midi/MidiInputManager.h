#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include "Keyboard/KeyState.h"

namespace pianolume
{

/** MIDI入力デバイスを開き、受信したメッセージを KeyState に反映して転送先（音源）へ渡す。 */
class MidiInputManager : private juce::MidiInputCallback
{
public:
    MidiInputManager (KeyState& keyState, juce::MidiInputCallback& forwardTarget);
    ~MidiInputManager() override;

    static juce::Array<juce::MidiDeviceInfo> getAvailableDevices();

    bool openDevice (const juce::String& identifier);
    void closeDevice();
    juce::String getCurrentDeviceIdentifier() const { return currentIdentifier; }

    /** 受信したメッセージのうち、まだ取り出していないものを返す。メッセージスレッドから呼ぶ。 */
    juce::Array<juce::MidiMessage> popPendingMessages();

private:
    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override;

    KeyState& keyState;
    juce::MidiInputCallback& forwardTarget;
    std::unique_ptr<juce::MidiInput> input;
    juce::String currentIdentifier;

    juce::CriticalSection pendingLock;
    juce::Array<juce::MidiMessage> pendingMessages;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiInputManager)
};

} // namespace pianolume
