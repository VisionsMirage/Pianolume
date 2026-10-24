#include "MidiInputManager.h"

namespace pianolume
{

namespace
{
    // メッセージスレッドが詰まった場合でもメモリを使い続けないための上限
    constexpr int maxPendingMessages = 1024;
}

MidiInputManager::MidiInputManager (KeyState& keyStateToUse, juce::MidiInputCallback& forwardTargetToUse)
    : keyState (keyStateToUse),
      forwardTarget (forwardTargetToUse)
{
}

MidiInputManager::~MidiInputManager()
{
    closeDevice();
}

juce::Array<juce::MidiDeviceInfo> MidiInputManager::getAvailableDevices()
{
    return juce::MidiInput::getAvailableDevices();
}

bool MidiInputManager::openDevice (const juce::String& identifier)
{
    closeDevice();

    input = juce::MidiInput::openDevice (identifier, this);

    if (input == nullptr)
        return false;

    currentIdentifier = identifier;
    input->start();
    return true;
}

void MidiInputManager::closeDevice()
{
    if (input != nullptr)
    {
        input->stop();
        input.reset();
    }

    currentIdentifier.clear();
    keyState.clear();
}

juce::Array<juce::MidiMessage> MidiInputManager::popPendingMessages()
{
    juce::Array<juce::MidiMessage> messages;

    const juce::ScopedLock sl (pendingLock);
    messages.swapWith (pendingMessages);
    return messages;
}

void MidiInputManager::handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message)
{
    forwardTarget.handleIncomingMidiMessage (source, message);

    if (message.isNoteOn())
        keyState.setNoteOn (message.getNoteNumber(), message.getVelocity());
    else if (message.isNoteOff())
        keyState.setNoteOff (message.getNoteNumber());
    else if (message.isSustainPedalOn())
        keyState.setSustain (true);
    else if (message.isSustainPedalOff())
        keyState.setSustain (false);
    else if (message.isAllNotesOff() || message.isAllSoundOff())
        keyState.clear();

    if (message.isActiveSense() || message.isMidiClock())
        return;

    const juce::ScopedLock sl (pendingLock);

    if (pendingMessages.size() < maxPendingMessages)
        pendingMessages.add (message);
}

} // namespace pianolume
