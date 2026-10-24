#include "Song.h"

#include <algorithm>
#include <map>

namespace pianolume
{

namespace
{
    constexpr int drumChannel = 10;
    constexpr double minimumNoteLength = 0.05;
    constexpr double unmatchedNoteLength = 0.5;

    bool isPlaybackEvent (const juce::MidiMessage& message)
    {
        return message.isNoteOnOrOff() || message.isController() || message.isProgramChange()
            || message.isPitchWheel() || message.isChannelPressure() || message.isAftertouch();
    }

    constexpr double defaultSecondsPerQuarter = 0.5;
    constexpr size_t maxGridLines = 200000;

    /** テンポ変更を反映して、ティックを秒に変換する。 */
    class TempoMap
    {
    public:
        TempoMap (const juce::MidiFile& midiFile, int ticksPerQuarterToUse)
            : ticksPerQuarter ((double) ticksPerQuarterToUse)
        {
            juce::MidiMessageSequence tempoEvents;
            midiFile.findAllTempoEvents (tempoEvents);

            for (const auto* holder : tempoEvents)
                changes.push_back ({ holder->message.getTimeStamp(), holder->message.getTempoSecondsPerQuarterNote() });
        }

        double toSeconds (double tick) const
        {
            double seconds = 0.0;
            double lastTick = 0.0;
            double secondsPerQuarter = defaultSecondsPerQuarter;

            for (const auto& change : changes)
            {
                if (change.tick >= tick)
                    break;

                seconds += (change.tick - lastTick) / ticksPerQuarter * secondsPerQuarter;
                lastTick = change.tick;
                secondsPerQuarter = change.secondsPerQuarter;
            }

            return seconds + (tick - lastTick) / ticksPerQuarter * secondsPerQuarter;
        }

    private:
        struct Change
        {
            double tick;
            double secondsPerQuarter;
        };

        double ticksPerQuarter;
        std::vector<Change> changes;
    };

    /** 拍子記号から小節線・拍線を作る。ティック単位の情報が必要なので、秒に変換する前に呼ぶ。 */
    std::vector<GridLine> createGridLines (const juce::MidiFile& midiFile)
    {
        const auto ticksPerQuarter = midiFile.getTimeFormat();

        // SMPTE 形式（負の値）のファイルには拍の情報が無い
        if (ticksPerQuarter <= 0)
            return {};

        const TempoMap tempoMap (midiFile, ticksPerQuarter);

        juce::MidiMessageSequence timeSignatures;
        midiFile.findAllTimeSigEvents (timeSignatures);

        const auto lastTick = midiFile.getLastTimestamp();
        std::vector<GridLine> lines;
        int numerator = 4;
        int denominator = 4;
        int signatureIndex = 0;
        int measureNumber = 1;
        double tick = 0.0;

        while (tick <= lastTick && lines.size() < maxGridLines)
        {
            // 小節の頭までに現れた拍子記号を適用する（小節の途中の変更は次の小節から）
            while (signatureIndex < timeSignatures.getNumEvents()
                   && timeSignatures.getEventTime (signatureIndex) <= tick + 0.5)
            {
                timeSignatures.getEventPointer (signatureIndex)->message.getTimeSignatureInfo (numerator, denominator);
                ++signatureIndex;
            }

            const auto beatTicks = ticksPerQuarter * 4.0 / juce::jmax (1, denominator);

            for (int beat = 0; beat < juce::jmax (1, numerator); ++beat)
                lines.push_back ({ tempoMap.toSeconds (tick + beat * beatTicks), beat == 0, measureNumber });

            tick += beatTicks * juce::jmax (1, numerator);
            ++measureNumber;
        }

        return lines;
    }

    juce::String findTrackName (const juce::MidiMessageSequence& sequence)
    {
        for (const auto* holder : sequence)
            if (holder->message.isTrackNameEvent())
                return holder->message.getTextFromTextMetaEvent().trim();

        return {};
    }

    /** 右手・左手の初期割り当て。ドラム以外の最初の2パートを、音域の高い方を右手とする。 */
    void assignDefaultRoles (std::vector<SongPart>& parts)
    {
        std::vector<SongPart*> candidates;

        for (auto& part : parts)
            if (part.channel != drumChannel && part.noteCount > 0)
                candidates.push_back (&part);

        if (candidates.empty())
            return;

        if (candidates.size() == 1)
        {
            candidates[0]->defaultRole = PartRole::rightHand;
            return;
        }

        const auto averagePitch = [] (const SongPart* part) { return part->lowestNote + part->highestNote; };
        const auto rightIsFirst = averagePitch (candidates[0]) >= averagePitch (candidates[1]);

        candidates[rightIsFirst ? 0 : 1]->defaultRole = PartRole::rightHand;
        candidates[rightIsFirst ? 1 : 0]->defaultRole = PartRole::leftHand;
    }
}

std::shared_ptr<Song> Song::load (const juce::File& file, juce::String& errorMessage)
{
    juce::FileInputStream stream (file);

    if (! stream.openedOk())
    {
        errorMessage = "Cannot open file: " + file.getFullPathName();
        return nullptr;
    }

    juce::MidiFile midiFile;

    if (! midiFile.readFrom (stream, true))
    {
        errorMessage = "Not a valid MIDI file: " + file.getFileName();
        return nullptr;
    }

    auto song = std::make_shared<Song>();
    song->file = file;
    song->title = file.getFileNameWithoutExtension();
    song->gridLines = createGridLines (midiFile);

    // テンポ変更を反映して、タイムスタンプをティックから秒に変換する
    midiFile.convertTimestampTicksToSeconds();

    std::map<std::pair<int, int>, int> partIndices;
    std::map<std::pair<int, int>, int> firstPrograms;

    for (int trackIndex = 0; trackIndex < midiFile.getNumTracks(); ++trackIndex)
    {
        juce::MidiMessageSequence sequence (*midiFile.getTrack (trackIndex));
        sequence.updateMatchedPairs();

        const auto trackName = findTrackName (sequence);

        const auto getPartIndex = [&] (int channel)
        {
            const auto key = std::make_pair (trackIndex, channel);

            if (const auto found = partIndices.find (key); found != partIndices.end())
                return found->second;

            SongPart part;
            part.track = trackIndex;
            part.channel = channel;
            part.name = trackName.isNotEmpty() ? trackName : "Track " + juce::String (trackIndex + 1);

            if (channel == drumChannel)
                part.instrument = "Drums";
            else if (const auto program = firstPrograms.find (key); program != firstPrograms.end())
                part.instrument = juce::MidiMessage::getGMInstrumentName (program->second);

            const auto index = (int) song->parts.size();
            song->parts.push_back (part);
            partIndices[key] = index;
            return index;
        };

        for (const auto* holder : sequence)
        {
            const auto& message = holder->message;

            if (! isPlaybackEvent (message))
                continue;

            const auto channel = message.getChannel();
            const auto time = message.getTimeStamp();

            if (message.isProgramChange())
                firstPrograms.emplace (std::make_pair (trackIndex, channel), message.getProgramChangeNumber());

            const auto partIndex = getPartIndex (channel);
            song->events.push_back ({ time, message, partIndex });

            if (message.isNoteOn())
            {
                auto end = holder->noteOffObject != nullptr ? holder->noteOffObject->message.getTimeStamp()
                                                            : time + unmatchedNoteLength;
                end = juce::jmax (end, time + minimumNoteLength);

                song->notes.push_back ({ time, end, message.getNoteNumber(), message.getVelocity(), partIndex });

                auto& part = song->parts[(size_t) partIndex];
                ++part.noteCount;
                part.lowestNote = juce::jmin (part.lowestNote, message.getNoteNumber());
                part.highestNote = juce::jmax (part.highestNote, message.getNoteNumber());

                song->duration = juce::jmax (song->duration, end);
                song->maxNoteLength = juce::jmax (song->maxNoteLength, end - time);
            }
        }
    }

    if (song->notes.empty())
    {
        errorMessage = "No notes in MIDI file: " + file.getFileName();
        return nullptr;
    }

    std::stable_sort (song->notes.begin(), song->notes.end(),
                      [] (const SongNote& a, const SongNote& b) { return a.start < b.start; });

    std::stable_sort (song->events.begin(), song->events.end(), [] (const SongEvent& a, const SongEvent& b)
    {
        if (a.time != b.time)
            return a.time < b.time;

        // 同じ時刻に同じ音を弾き直すとき、新しい音を消さないようにノートオフを先にする
        return a.message.isNoteOff() && ! b.message.isNoteOff();
    });

    assignDefaultRoles (song->parts);
    return song;
}

} // namespace pianolume
