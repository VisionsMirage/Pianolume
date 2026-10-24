#include "SongPlayer.h"

#include <algorithm>
#include <limits>

namespace pianolume
{

namespace
{
    constexpr int allNotesOffController = 123;
    constexpr int sustainController = 64;

    // この時間内に始まるノートは同じ和音として、まとめて押されるのを待つ
    constexpr double chordToleranceSeconds = 0.05;

    // ノートの開始よりこの時間だけ早く押しても正解とする
    constexpr double earlyPressToleranceSeconds = 0.15;

    constexpr double neverPressed = -std::numeric_limits<double>::infinity();
    constexpr double noLimit = std::numeric_limits<double>::infinity();
}

SongPlayer::SongPlayer()
{
    for (auto& role : roles)
        role.store (PartRole::accompaniment);

    lastPressPositions.fill (neverPressed);
}

void SongPlayer::setSong (std::shared_ptr<const Song> newSong)
{
    playing = false;
    seekRequested = false;
    loopEnabled = false;
    loopStart = 0.0;
    loopEnd = 0.0;

    if (newSong != nullptr)
        for (size_t i = 0; i < newSong->parts.size() && i < (size_t) maxParts; ++i)
            roles[i].store (newSong->parts[i].defaultRole);

    {
        const juce::SpinLock::ScopedLockType lock (viewLock);
        songForView = newSong;
    }

    {
        const juce::SpinLock::ScopedLockType lock (songLock);
        song.swap (newSong);
        nextEventIndex = 0;
        nextWaitNoteIndex = 0;
        position = 0.0;
    }

    notesOffRequested = true;
    seek (0.0);

    // 古い曲データ（newSong に入れ替わった）はここで、ロックの外で解放される
}

std::shared_ptr<const Song> SongPlayer::getSongForView() const
{
    const juce::SpinLock::ScopedLockType lock (viewLock);
    return songForView;
}

void SongPlayer::play()
{
    const auto current = getSongForView();

    if (current == nullptr)
        return;

    // 最後まで再生し終えていたら先頭から
    if (getPosition() >= current->duration)
        seek (0.0);

    playing = true;
}

void SongPlayer::pause()
{
    playing = false;
}

void SongPlayer::stop()
{
    playing = false;
    seek (0.0);
}

void SongPlayer::togglePlayPause()
{
    if (isPlaying())
        pause();
    else
        play();
}

void SongPlayer::seek (double seconds)
{
    seekTarget = juce::jmax (0.0, seconds);
    seekRequested = true;
}

void SongPlayer::setRate (double newRate)
{
    rate = juce::jlimit (0.1, 4.0, newRate);
}

void SongPlayer::setPartRole (int partIndex, PartRole role)
{
    if (juce::isPositiveAndBelow (partIndex, maxParts))
    {
        roles[(size_t) partIndex].store (role);
        notesOffRequested = true;
    }
}

PartRole SongPlayer::getPartRole (int partIndex) const noexcept
{
    return juce::isPositiveAndBelow (partIndex, maxParts) ? roles[(size_t) partIndex].load()
                                                          : PartRole::accompaniment;
}

void SongPlayer::setAutoPlayHands (bool shouldAutoPlay)
{
    autoPlayHands = shouldAutoPlay;
    notesOffRequested = true;
}

void SongPlayer::setWaitForInput (bool shouldWait)
{
    waitForInput = shouldWait;
}

void SongPlayer::setPlayableRange (int lowestNote, int highestNote)
{
    playableLowest = lowestNote;
    playableHighest = highestNote;
    notesOffRequested = true;
}

void SongPlayer::setLoopRange (double startSeconds, double endSeconds)
{
    loopStart = juce::jmin (startSeconds, endSeconds);
    loopEnd = juce::jmax (startSeconds, endSeconds);
}

void SongPlayer::setLoopEnabled (bool shouldLoop)
{
    loopEnabled = shouldLoop;
}

double SongPlayer::getPosition() const noexcept
{
    // シーク要求がまだオーディオスレッドで処理されていなければ、その位置を返す
    if (seekRequested.load())
        return seekTarget.load();

    double basePosition = 0.0;
    double limit = 0.0;
    double baseTimeMs = 0.0;

    for (;;)
    {
        const auto before = snapshotSequence.load();

        if ((before & 1) != 0)
            continue;

        basePosition = snapshotPosition.load();
        limit = snapshotLimit.load();
        baseTimeMs = snapshotTimeMs.load();

        if (snapshotSequence.load() == before)
            break;
    }

    if (! playing.load())
        return basePosition;

    // 待機やループで止まる位置を越えて表示が先走らないようにする
    const auto elapsed = (juce::Time::getMillisecondCounterHiRes() - baseTimeMs) * 0.001;
    return juce::jmin (limit, basePosition + juce::jmax (0.0, elapsed) * rate.load());
}

void SongPlayer::prepare (double sampleRate)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
}

void SongPlayer::handleLiveNoteOn (int note) noexcept
{
    if (juce::isPositiveAndBelow (note, 128))
        lastPressPositions[(size_t) note] = position;
}

void SongPlayer::processBlock (int numSamples, std::vector<ScheduledMidiEvent>& output)
{
    const juce::SpinLock::ScopedTryLockType lock (songLock);

    if (! lock.isLocked())
        return;

    if (notesOffRequested.exchange (false))
        emitAllNotesOff (output);

    if (song == nullptr)
    {
        publishPosition (0.0, 0.0);
        return;
    }

    if (seekRequested.exchange (false))
        jumpTo (juce::jlimit (0.0, song->duration, seekTarget.load()), output);

    if (! playing.load())
    {
        if (wasPlaying)
            emitAllNotesOff (output);

        wasPlaying = false;
        waiting = false;
        publishPosition (position, position);
        return;
    }

    wasPlaying = true;

    const auto currentRate = rate.load();
    auto blockEnd = position + numSamples / currentSampleRate * currentRate;

    // ループの終点でブロックを打ち切る
    const auto currentLoopEnd = loopEnd.load();
    const auto loopActive = loopEnabled.load() && currentLoopEnd > loopStart.load() && position < currentLoopEnd;
    auto reachedLoopEnd = false;

    if (loopActive && blockEnd >= currentLoopEnd)
    {
        blockEnd = currentLoopEnd;
        reachedLoopEnd = true;
    }

    // 待機モードでは、まだ押されていない和音の開始位置で止める
    auto isWaiting = false;
    double waitTime = 0.0;

    if (waitForInput.load() && findUnsatisfiedChord (position, blockEnd, waitTime))
    {
        blockEnd = waitTime;
        isWaiting = true;
        reachedLoopEnd = false;
    }

    waiting = isWaiting;
    publishPosition (position, isWaiting || reachedLoopEnd ? blockEnd : noLimit);

    const auto shouldAutoPlayHands = autoPlayHands.load();
    const auto lowestPlayable = playableLowest.load();
    const auto highestPlayable = playableHighest.load();
    const auto& events = song->events;

    while (nextEventIndex < events.size() && events[nextEventIndex].time < blockEnd)
    {
        const auto& event = events[nextEventIndex++];
        const auto role = getPartRole (event.part);

        if (role == PartRole::mute)
            continue;

        const auto isHand = isHandRole (role);

        if (isHand && ! shouldAutoPlayHands && event.message.isNoteOnOrOff()
            && event.message.getNoteNumber() >= lowestPlayable && event.message.getNoteNumber() <= highestPlayable)
            continue;

        const auto offset = juce::jlimit (0, numSamples - 1,
                                          (int) ((event.time - position) / currentRate * currentSampleRate));

        // 手のパートは演奏と同じ音色で鳴らし、伴奏は曲のチャンネルと音色のまま鳴らす
        appendEvent (output, offset, event.message,
                     isHand ? SynthChannels::songHands : event.message.getChannel() - 1,
                     ! isHand);
    }

    position = blockEnd;

    if (reachedLoopEnd)
    {
        jumpTo (loopStart.load(), output);
    }
    else if (position >= song->duration)
    {
        position = song->duration;
        playing = false;
    }
}

void SongPlayer::jumpTo (double seconds, std::vector<ScheduledMidiEvent>& output)
{
    position = seconds;
    nextEventIndex = findFirstEventAtOrAfter (position);
    nextWaitNoteIndex = findFirstNoteAtOrAfter (position);
    lastPressPositions.fill (neverPressed);
    waiting = false;

    emitAllNotesOff (output);
    emitChasedControllers (output);
    publishPosition (position, position);
}

bool SongPlayer::findUnsatisfiedChord (double from, double to, double& waitTime)
{
    const auto& notes = song->notes;

    while (nextWaitNoteIndex < notes.size())
    {
        const auto& first = notes[nextWaitNoteIndex];

        if (first.start >= to)
            return false;

        if (first.start < from || ! requiresInput (first))
        {
            ++nextWaitNoteIndex;
            continue;
        }

        const auto chordStart = first.start;
        auto chordEnd = nextWaitNoteIndex;
        auto satisfied = true;

        for (; chordEnd < notes.size() && notes[chordEnd].start <= chordStart + chordToleranceSeconds; ++chordEnd)
        {
            const auto& note = notes[chordEnd];

            if (requiresInput (note) && lastPressPositions[(size_t) note.note] < chordStart - earlyPressToleranceSeconds)
                satisfied = false;
        }

        if (! satisfied)
        {
            waitTime = chordStart;
            return true;
        }

        // 同じ音の連打で、1 回の打鍵が次の和音にも数えられないようにする
        for (auto i = nextWaitNoteIndex; i < chordEnd; ++i)
            if (requiresInput (notes[i]))
                lastPressPositions[(size_t) notes[i].note] = neverPressed;

        nextWaitNoteIndex = chordEnd;
    }

    return false;
}

bool SongPlayer::requiresInput (const SongNote& note) const noexcept
{
    return isHandRole (getPartRole (note.part))
        && note.note >= playableLowest.load() && note.note <= playableHighest.load();
}

void SongPlayer::publishPosition (double seconds, double limit) noexcept
{
    snapshotSequence.fetch_add (1);
    snapshotPosition.store (seconds);
    snapshotLimit.store (limit);
    snapshotTimeMs.store (juce::Time::getMillisecondCounterHiRes());
    snapshotSequence.fetch_add (1);
}

size_t SongPlayer::findFirstEventAtOrAfter (double seconds) const
{
    const auto& events = song->events;
    const auto found = std::lower_bound (events.begin(), events.end(), seconds,
                                         [] (const SongEvent& event, double time) { return event.time < time; });
    return (size_t) std::distance (events.begin(), found);
}

size_t SongPlayer::findFirstNoteAtOrAfter (double seconds) const
{
    const auto& notes = song->notes;
    const auto found = std::lower_bound (notes.begin(), notes.end(), seconds,
                                         [] (const SongNote& note, double time) { return note.start < time; });
    return (size_t) std::distance (notes.begin(), found);
}

void SongPlayer::emitAllNotesOff (std::vector<ScheduledMidiEvent>& output) const
{
    const auto allNotesOff = juce::MidiMessage::controllerEvent (1, allNotesOffController, 0);
    const auto sustainOff = juce::MidiMessage::controllerEvent (1, sustainController, 0);

    const auto emit = [&] (int channel)
    {
        appendEvent (output, 0, sustainOff, channel, false);
        appendEvent (output, 0, allNotesOff, channel, false);
    };

    for (int channel = 0; channel < SynthChannels::numSongChannels; ++channel)
        emit (channel);

    emit (SynthChannels::songHands);
}

void SongPlayer::emitChasedControllers (std::vector<ScheduledMidiEvent>& output) const
{
    // シーク先より前の音色・コントローラの最終値を送り、途中から再生しても楽器や音量が正しくなるようにする
    std::array<int, SynthChannels::numSongChannels> programs;
    std::array<int, SynthChannels::numSongChannels> pitchBends;
    std::array<std::array<int, 128>, SynthChannels::numSongChannels> controllers;

    programs.fill (-1);
    pitchBends.fill (-1);

    for (auto& channelControllers : controllers)
        channelControllers.fill (-1);

    for (size_t i = 0; i < nextEventIndex; ++i)
    {
        const auto& event = song->events[i];

        if (isHandRole (getPartRole (event.part)))
            continue;

        const auto& message = event.message;
        const auto channel = (size_t) (message.getChannel() - 1);

        if (message.isProgramChange())
            programs[channel] = message.getProgramChangeNumber();
        else if (message.isPitchWheel())
            pitchBends[channel] = message.getPitchWheelValue();
        else if (message.isController() && message.getControllerNumber() < 120)
            controllers[channel][(size_t) message.getControllerNumber()] = message.getControllerValue();
    }

    for (int channel = 0; channel < SynthChannels::numSongChannels; ++channel)
    {
        const auto index = (size_t) channel;

        // バンクセレクト（CC0 / CC32）はプログラムチェンジより先に送る
        for (int controller = 0; controller < 120; ++controller)
            if (const auto value = controllers[index][(size_t) controller]; value >= 0)
                appendEvent (output, 0, juce::MidiMessage::controllerEvent (1, controller, value), channel, false);

        if (programs[index] >= 0)
            appendEvent (output, 0, juce::MidiMessage::programChange (1, programs[index]), channel, true);

        if (pitchBends[index] >= 0)
            appendEvent (output, 0, juce::MidiMessage::pitchWheel (1, pitchBends[index]), channel, false);
    }
}

} // namespace pianolume
