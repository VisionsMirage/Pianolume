#pragma once

#include <array>
#include <atomic>
#include "Audio/ScheduledMidiEvent.h"
#include "Song.h"

namespace pianolume
{

/** 曲の再生位置を管理し、オーディオブロックごとに発音すべきイベントを出す。

    再生位置はオーディオスレッドで進めるので、音と表示がずれない。
    操作系はメッセージスレッドから、processBlock() と handleLiveNoteOn() はオーディオスレッドから、
    getSongForView() / getPosition() / getPartRole() は描画スレッドからも呼べる。
*/
class SongPlayer
{
public:
    static constexpr int maxParts = 64;

    SongPlayer();

    void setSong (std::shared_ptr<const Song> newSong);
    std::shared_ptr<const Song> getSongForView() const;

    void play();
    void pause();
    void stop();
    void togglePlayPause();
    bool isPlaying() const noexcept                 { return playing.load(); }

    void seek (double seconds);

    void setRate (double newRate);
    double getRate() const noexcept                 { return rate.load(); }

    void setPartRole (int partIndex, PartRole role);
    PartRole getPartRole (int partIndex) const noexcept;

    /** 右手・左手のパートを自動演奏するか（オフにすると練習用に音を鳴らさない）。 */
    void setAutoPlayHands (bool shouldAutoPlay);
    bool getAutoPlayHands() const noexcept          { return autoPlayHands.load(); }

    /** 待機モード。手のパートの音を、入力鍵盤で正しく押すまで曲を止める。 */
    void setWaitForInput (bool shouldWait);
    bool getWaitForInput() const noexcept           { return waitForInput.load(); }
    bool isWaitingForInput() const noexcept         { return waiting.load(); }

    /** 入力鍵盤で弾ける音域。自動演奏しない設定でも、この範囲外の手のノートは弾けないので自動で鳴らし、待機もしない。 */
    void setPlayableRange (int lowestNote, int highestNote);

    /** A-B ループ。終点に達すると始点に戻る。 */
    void setLoopRange (double startSeconds, double endSeconds);
    void setLoopEnabled (bool shouldLoop);
    bool isLoopEnabled() const noexcept             { return loopEnabled.load(); }
    double getLoopStart() const noexcept            { return loopStart.load(); }
    double getLoopEnd() const noexcept              { return loopEnd.load(); }

    /** 現在の再生位置（秒）。最後のオーディオブロックからの経過時間で補間するので、表示が滑らかになる。 */
    double getPosition() const noexcept;

    void prepare (double sampleRate);

    /** キーボードで押された音を待機モードの判定に使う。processBlock() より先に呼ぶ。 */
    void handleLiveNoteOn (int note) noexcept;

    void processBlock (int numSamples, std::vector<ScheduledMidiEvent>& output);

private:
    void jumpTo (double seconds, std::vector<ScheduledMidiEvent>& output);
    bool findUnsatisfiedChord (double from, double to, double& waitTime);
    bool requiresInput (const SongNote& note) const noexcept;

    void publishPosition (double seconds, double limit) noexcept;
    size_t findFirstEventAtOrAfter (double seconds) const;
    size_t findFirstNoteAtOrAfter (double seconds) const;
    void emitAllNotesOff (std::vector<ScheduledMidiEvent>& output) const;
    void emitChasedControllers (std::vector<ScheduledMidiEvent>& output) const;

    // オーディオスレッドが使う曲データ。差し替え時のみロックし、オーディオ側は try-lock で待たない
    juce::SpinLock songLock;
    std::shared_ptr<const Song> song;
    size_t nextEventIndex = 0;
    size_t nextWaitNoteIndex = 0;
    double position = 0.0;
    bool wasPlaying = false;
    double currentSampleRate = 44100.0;
    std::array<double, 128> lastPressPositions {};

    // 描画スレッド用。オーディオスレッドのロックと競合しないよう別に持つ
    mutable juce::SpinLock viewLock;
    std::shared_ptr<const Song> songForView;

    std::atomic<bool> playing { false };
    std::atomic<bool> seekRequested { false };
    std::atomic<double> seekTarget { 0.0 };
    std::atomic<bool> notesOffRequested { false };
    std::atomic<double> rate { 1.0 };
    std::atomic<bool> autoPlayHands { true };
    std::atomic<bool> waitForInput { false };
    std::atomic<bool> waiting { false };
    std::atomic<int> playableLowest { 0 };
    std::atomic<int> playableHighest { 127 };
    std::atomic<bool> loopEnabled { false };
    std::atomic<double> loopStart { 0.0 };
    std::atomic<double> loopEnd { 0.0 };
    std::array<std::atomic<PartRole>, maxParts> roles;

    // 再生位置のスナップショット（シーケンスロックで一貫した組を読む）
    std::atomic<std::uint64_t> snapshotSequence { 0 };
    std::atomic<double> snapshotPosition { 0.0 };
    std::atomic<double> snapshotLimit { 0.0 };
    std::atomic<double> snapshotTimeMs { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongPlayer)
};

} // namespace pianolume
