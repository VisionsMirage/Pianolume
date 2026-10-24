#include "NotesRenderer.h"
#include "PartColours.h"
#include "UI/Theme.h"

#include <algorithm>

namespace pianolume
{

namespace
{
    // ノートが判定ラインに届く少し前から鍵盤を光らせ始める
    constexpr double preGlowSeconds = 0.25;

    // 待機中の表示で同じ和音とみなす範囲
    constexpr double waitingChordSeconds = 0.07;

    constexpr float noteCornerSize = 6.0f;
    constexpr float noteInset = 2.0f;
    constexpr float whiteKeyFrontEdge = 6.0f;
    constexpr float blackKeyFrontEdge = 5.0f;
    constexpr float keyGlowHeight = 48.0f;

    // 表示用の再生位置が、本当の位置からこれ以上ずれたら（シークやループなど）すぐに合わせる
    constexpr double positionSnapSeconds = 0.08;

    // 1 フレームごとに、ずれの何割を戻すか
    constexpr double positionCorrection = 0.08;

    // これより小さくなる音名は読めないので描かない
    constexpr float minimumLabelHeight = 7.0f;

    // 投影する音名の輪郭の太さ（文字の高さに対する割合）
    constexpr float boldLabelStroke = 0.09f;

    namespace Colours
    {
        const juce::Colour projectionBackground { 0xff000000 };
        const juce::Colour octaveLine           { 0x0dffffff };
        const juce::Colour measureLine          { 0x29ffffff };
        const juce::Colour beatLine             { 0x0bffffff };
        const juce::Colour unplayableLane       { 0x73000000 };
        const juce::Colour laneStripe           { 0x06ffffff };
        const juce::Colour projectionOutline    { 0xff3a3a3a };

        const juce::Colour whiteKeyTop          { 0xfff4f4f1 };
        const juce::Colour whiteKeyMiddle       { 0xffffffff };
        const juce::Colour whiteKeyBottom       { 0xffe9e9e5 };
        const juce::Colour whiteKeyEdge         { 0xffd5d5d0 };
        const juce::Colour greyKeyTop           { 0xff8e9198 };
        const juce::Colour greyKeyMiddle        { 0xffa3a6ad };
        const juce::Colour greyKeyBottom        { 0xff93969d };
        const juce::Colour greyKeyEdge          { 0xff7c7f86 };

        const juce::Colour blackKeyTop          { 0xff1a1b1e };
        const juce::Colour blackKeyMiddle       { 0xff2c2e33 };
        const juce::Colour blackKeyBottom       { 0xff111214 };
        const juce::Colour greyBlackTop         { 0xff3a3c42 };
        const juce::Colour greyBlackMiddle      { 0xff4a4d54 };
        const juce::Colour greyBlackBottom      { 0xff33353a };

        const juce::Colour whiteKeyLabel        { 0xff8b8f98 };
        const juce::Colour greyKeyLabel         { 0xff5f626a };
        const juce::Colour blackKeyLabel        { 0x99ffffff };
        const juce::Colour projectionLabel      { 0xff6a6a6a };
    }

    juce::Colour scaled (juce::Colour colour, float factor)
    {
        return juce::Colour::fromFloatRGBA (colour.getFloatRed() * factor, colour.getFloatGreen() * factor,
                                            colour.getFloatBlue() * factor, colour.getFloatAlpha());
    }

    /** 上・中・下の 3 色で縦方向に塗る。 */
    void fillVertical (juce::Graphics& g, const juce::Path& shape, juce::Rectangle<float> bounds,
                       juce::Colour top, juce::Colour middle, float middlePosition, juce::Colour bottom)
    {
        juce::ColourGradient gradient (top, 0.0f, bounds.getY(), bottom, 0.0f, bounds.getBottom(), false);
        gradient.addColour (middlePosition, middle);
        g.setGradientFill (gradient);
        g.fillPath (shape);
    }

    juce::Path bottomRoundedRect (juce::Rectangle<float> bounds, float radius)
    {
        juce::Path path;
        path.addRoundedRectangle (bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(),
                                  radius, radius, false, false, true, true);
        return path;
    }

    juce::String getNoteLetter (int note)
    {
        return juce::MidiMessage::getMidiNoteName (note, true, false, 4);
    }

    /** 音名を area の中央下寄せで描く。幅に収まるよう文字を小さくし、小さすぎるときは描かない。
        bold を指定すると、文字の輪郭を太らせて描く（投影では細い線が見えにくいため）。 */
    void drawNoteLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float maxHeight,
                        bool bold = false)
    {
        auto height = juce::jmin (maxHeight, area.getHeight());
        const auto widthAtHeight = juce::GlyphArrangement::getStringWidth (Theme::uiFont (height, 700), text);

        if (widthAtHeight > area.getWidth() && widthAtHeight > 0.0f)
            height *= area.getWidth() / widthAtHeight;

        if (height < minimumLabelHeight)
            return;

        const auto font = Theme::uiFont (height, 700);
        const auto box = area.withTop (area.getBottom() - height);

        if (! bold)
        {
            g.setFont (font);
            g.drawText (text, box, juce::Justification::centred, false);
            return;
        }

        juce::GlyphArrangement glyphs;
        glyphs.addFittedText (font, text, box.getX(), box.getY(), box.getWidth(), box.getHeight(),
                              juce::Justification::centred, 1, 1.0f);

        juce::Path path;
        glyphs.createPath (path);
        g.fillPath (path);
        g.strokePath (path, juce::PathStrokeType (height * boldLabelStroke, juce::PathStrokeType::curved));
    }

    /** 押鍵とガイドから、鍵を光らせる色を決める。光らせないなら透明。 */
    juce::Colour getKeyLightColour (bool isDown, const juce::Colour& guideColour, float guideLevel, bool isPlaying)
    {
        if (isDown)
            return guideLevel > 0.0f ? guideColour : (isPlaying ? PartColours::mistake : PartColours::freePlay);

        return guideLevel > 0.0f ? guideColour : juce::Colours::transparentBlack;
    }
}

bool NotesRenderer::Settings::showsKeyName (int note) const noexcept
{
    switch (keyNames)
    {
        case KeyNames::whiteKeys: return ! KeyboardLayout::isBlackKey (note);
        case KeyNames::blackKeys: return KeyboardLayout::isBlackKey (note);
        case KeyNames::cOnly:     return note % 12 == 0;
        case KeyNames::none:      break;
    }

    return false;
}

NotesRenderer::NotesRenderer (const KeyState& keyStateToUse, const SongPlayer& playerToUse)
    : keyState (keyStateToUse),
      player (playerToUse)
{
}

NotesRenderer::Frame NotesRenderer::beginFrame()
{
    Frame frame;
    frame.song = player.getSongForView();
    frame.isPlaying = player.isPlaying();
    frame.isWaiting = player.isWaitingForInput();
    frame.now = getSmoothedPosition (frame.isPlaying, frame.isWaiting);
    return frame;
}

double NotesRenderer::getSmoothedPosition (bool isPlaying, bool isWaiting)
{
    // 再生位置はオーディオの処理単位ごとに更新されるので、そのまま使うと数ミリ秒ずつ揺れてノートがカクつく。
    // 前のフレームからの経過時間で進め、本当の位置とのずれは少しずつ戻す
    const auto reported = player.getPosition();
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();
    const auto elapsed = (nowMs - lastFrameTimeMs) * 0.001;
    lastFrameTimeMs = nowMs;

    if (! isPlaying || isWaiting || ! hasDisplayedPosition || elapsed <= 0.0 || elapsed > 0.25)
    {
        displayedPosition = reported;
        hasDisplayedPosition = true;
        return displayedPosition;
    }

    const auto predicted = displayedPosition + elapsed * player.getRate();
    const auto error = reported - predicted;

    displayedPosition = std::abs (error) > positionSnapSeconds ? reported : predicted + error * positionCorrection;
    return displayedPosition;
}

juce::Range<float> NotesRenderer::getKeyX (const KeyboardLayout& layout, juce::Rectangle<float> area, int note)
{
    const auto keyWidth = area.getWidth() / (float) juce::jmax (1, layout.getNumWhiteKeys());
    const auto range = layout.getKeyRange (note);
    return { area.getX() + range.getStart() * keyWidth, area.getX() + range.getEnd() * keyWidth };
}

juce::Rectangle<float> NotesRenderer::getKeyBounds (const Settings& settings, juce::Rectangle<float> area, int note)
{
    const auto x = getKeyX (settings.layout, area, note);

    if (! KeyboardLayout::isBlackKey (note))
        return { x.getStart(), area.getY(), x.getLength(), area.getHeight() };

    const auto keyWidth = area.getWidth() / (float) juce::jmax (1, settings.layout.getNumWhiteKeys());
    const auto proportion = area.getWidth() > 0.0f
                              ? juce::jlimit (0.0f, 1.0f, ((x.getStart() + x.getEnd()) * 0.5f - area.getX()) / area.getWidth())
                              : 0.0f;
    const auto offsetX = juce::jmap (proportion, settings.blackKeyOffsetXLeft, settings.blackKeyOffsetXRight);

    return { x.getStart() + offsetX * keyWidth,
             area.getY() + settings.blackKeyOffsetY * area.getHeight(),
             x.getLength(),
             settings.blackKeyLength * area.getHeight() };
}

void NotesRenderer::paintFallArea (juce::Graphics& g, juce::Rectangle<float> area, const Settings& settings, Frame& frame) const
{
    const auto& layout = settings.layout;

    // オクターブ（C）ごとの縦線
    g.setColour (Colours::octaveLine);

    for (int note = layout.getLowestNote(); note <= layout.getHighestNote(); ++note)
        if (note % 12 == 0)
            g.fillRect (getKeyX (layout, area, note).getStart(), area.getY(), 1.0f, area.getHeight());

    if (frame.song != nullptr && settings.showGrid)
        paintGrid (g, *frame.song, area, settings, frame.now);

    paintUnplayableLanes (g, area, settings);

    if (frame.song != nullptr)
        paintNotes (g, area, settings, frame);

    paintHitLine (g, area, settings, frame);

    if (frame.song != nullptr && settings.style == Style::screen && frame.isWaiting)
        paintWaitingChip (g, area, settings, frame);
}

void NotesRenderer::paintGrid (juce::Graphics& g, const Song& song, juce::Rectangle<float> area,
                               const Settings& settings, double now) const
{
    const auto pixelsPerSecond = area.getHeight() / (float) settings.lookAheadSeconds;
    const auto& lines = song.gridLines;

    const auto first = std::lower_bound (lines.begin(), lines.end(), now,
                                         [] (const GridLine& line, double time) { return line.time < time; });

    g.setFont (Theme::monoFont (11.0f));

    for (auto it = first; it != lines.end() && it->time <= now + settings.lookAheadSeconds; ++it)
    {
        const auto y = area.getBottom() - (float) (it->time - now) * pixelsPerSecond;

        g.setColour (it->isMeasure ? Colours::measureLine : Colours::beatLine);
        g.fillRect (area.getX(), y, area.getWidth(), 1.0f);

        if (it->isMeasure && settings.style == Style::screen)
        {
            g.setColour (Theme::Colours::textFaint);
            g.drawText (juce::String (it->measureNumber),
                        juce::Rectangle<float> (area.getRight() - 72.0f, y - 18.0f, 60.0f, 14.0f),
                        juce::Justification::centredRight, false);
        }
    }
}

void NotesRenderer::paintUnplayableLanes (juce::Graphics& g, juce::Rectangle<float> area, const Settings& settings) const
{
    const auto& layout = settings.layout;

    // 弾ける範囲の左端・右端の位置（範囲が画面外にはみ出していれば画面の端）
    const auto left = settings.playableLowest > layout.getLowestNote()
                        ? getKeyX (layout, area, settings.playableLowest).getStart() : area.getX();
    const auto right = settings.playableHighest < layout.getHighestNote()
                        ? getKeyX (layout, area, settings.playableHighest).getEnd() : area.getRight();

    const auto paintLane = [&] (juce::Rectangle<float> lane)
    {
        if (lane.isEmpty())
            return;

        g.setColour (Colours::unplayableLane);
        g.fillRect (lane);

        // 斜線で「弾けない範囲」であることを示す
        juce::Graphics::ScopedSaveState saveState (g);
        g.reduceClipRegion (lane.toNearestInt());
        g.setColour (Colours::laneStripe);

        for (auto x = lane.getX() - lane.getHeight(); x < lane.getRight(); x += 16.0f)
            g.drawLine (x, lane.getBottom(), x + lane.getHeight(), lane.getY(), 5.6f);
    };

    paintLane (area.withRight (juce::jlimit (area.getX(), area.getRight(), left)));
    paintLane (area.withLeft (juce::jlimit (area.getX(), area.getRight(), right)));
}

void NotesRenderer::paintNotes (juce::Graphics& g, juce::Rectangle<float> area, const Settings& settings, Frame& frame) const
{
    const auto& song = *frame.song;
    const auto& layout = settings.layout;
    const auto now = frame.now;
    const auto lookAhead = settings.lookAheadSeconds;
    const auto hitY = area.getBottom();
    const auto pixelsPerSecond = area.getHeight() / (float) lookAhead;

    const auto timeToY = [&] (double time)
    {
        return hitY - (float) (time - now) * pixelsPerSecond;
    };

    const auto whiteKeyWidth = area.getWidth() / (float) juce::jmax (1, layout.getNumWhiteKeys());
    const auto labelHeight = juce::jmin (whiteKeyWidth * 0.5f, area.getHeight() * 0.045f);

    // 長いノートは画面に入る前から鳴っているので、最長ノート分さかのぼって探す
    const auto& notes = song.notes;
    const auto first = std::lower_bound (notes.begin(), notes.end(), now - song.maxNoteLength,
                                         [] (const SongNote& note, double time) { return note.start < time; });

    // 白鍵のノートを先に描き、黒鍵のノートを上に重ねる
    for (const bool drawBlack : { false, true })
    {
        for (auto it = first; it != notes.end(); ++it)
        {
            const auto& note = *it;

            if (note.start > now + lookAhead)
                break;

            if (note.end <= now || ! layout.contains (note.note))
                continue;

            const auto isBlack = KeyboardLayout::isBlackKey (note.note);

            if (isBlack != drawBlack)
                continue;

            const auto role = player.getPartRole (note.part);

            if (role == PartRole::mute)
                continue;

            if (isHandRole (role) && note.start - now < preGlowSeconds)
            {
                const auto level = note.start <= now ? 1.0f : 1.0f - (float) ((note.start - now) / preGlowSeconds);
                auto& guide = frame.guides[(size_t) note.note];

                if (level > guide.level)
                    guide = { level, role };
            }

            const auto top = juce::jmax (area.getY(), timeToY (note.end));
            const auto bottom = juce::jmin (hitY, timeToY (note.start));

            if (bottom <= top)
                continue;

            const auto x = getKeyX (layout, area, note.note);
            const juce::Rectangle<float> bar (x.getStart() + noteInset, top, x.getLength() - noteInset * 2.0f, bottom - top);

            const auto base = PartColours::forRole (role);
            const auto bottomColour = isBlack ? scaled (base, 0.72f) : base;
            auto opacity = role == PartRole::accompaniment ? 0.35f : 1.0f;

            if (! settings.isPlayable (note.note))
                opacity *= 0.45f;

            const auto radius = juce::jmin (noteCornerSize, bar.getWidth() * 0.5f, bar.getHeight() * 0.5f);

            // ほのかな光
            if (role != PartRole::accompaniment)
            {
                for (const auto [spread, alpha] : { std::pair { 7.0f, 0.06f }, std::pair { 3.5f, 0.10f } })
                {
                    g.setColour (base.withAlpha (alpha * opacity));
                    g.fillRoundedRectangle (bar.expanded (spread), radius + spread);
                }
            }

            g.setGradientFill (juce::ColourGradient (base.withAlpha (0.95f * opacity), 0.0f, bar.getY(),
                                                     bottomColour.withMultipliedAlpha (opacity), 0.0f, bar.getBottom(), false));
            g.fillRoundedRectangle (bar, radius);

            g.setColour (juce::Colours::white.withAlpha (0.35f * opacity));
            g.fillRect (bar.reduced (radius, 0.0f).withHeight (1.0f).translated (0.0f, 1.0f));

            g.setColour (base.withAlpha (0.9f * opacity));
            g.drawRoundedRectangle (bar.reduced (0.5f), radius, 1.0f);

            if (settings.showNoteNames)
            {
                // 明るいバーには暗い文字、薄いバーには白い文字
                const auto isBright = opacity > 0.5f && base.getPerceivedBrightness() > 0.45f;
                g.setColour (isBright ? juce::Colours::black.withAlpha (0.75f) : juce::Colours::white.withAlpha (0.85f));
                drawNoteLabel (g, getNoteLetter (note.note), bar.reduced (1.0f, 3.0f), labelHeight,
                               settings.style == Style::projection);
            }
        }
    }
}

void NotesRenderer::paintHitLine (juce::Graphics& g, juce::Rectangle<float> area, const Settings& settings, const Frame& frame) const
{
    const auto& layout = settings.layout;
    const auto hitY = area.getBottom();

    // 光っている鍵から判定ラインの上へ、光が立ちのぼるように見せる
    for (int note = layout.getLowestNote(); note <= layout.getHighestNote(); ++note)
    {
        const auto& guide = frame.guides[(size_t) note];
        const auto isDown = keyState.isDown (note);
        const auto light = getKeyLightColour (isDown, PartColours::forRole (guide.role), guide.level, frame.isPlaying);

        if (light.isTransparent())
            continue;

        const auto level = isDown ? 1.0f : guide.level;
        const auto x = getKeyX (layout, area, note);

        g.setGradientFill (juce::ColourGradient (light.withAlpha (0.45f * level), 0.0f, hitY,
                                                 light.withAlpha (0.0f), 0.0f, hitY - keyGlowHeight, false));
        g.fillRect (juce::Rectangle<float> (x.getStart(), hitY - keyGlowHeight, x.getLength(), keyGlowHeight));
    }

    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.22f), 0.0f, hitY,
                                             juce::Colours::white.withAlpha (0.0f), 0.0f, hitY - 14.0f, false));
    g.fillRect (area.withTop (hitY - 14.0f));

    g.setColour (juce::Colours::white.withAlpha (0.75f));
    g.fillRect (area.withTop (hitY - 2.0f));
}

void NotesRenderer::paintWaitingChip (juce::Graphics& g, juce::Rectangle<float> area, const Settings& settings,
                                      const Frame& frame) const
{
    // 待っている和音の音名を集める
    const auto& notes = frame.song->notes;
    const auto now = frame.now;
    const auto first = std::lower_bound (notes.begin(), notes.end(), now - 0.001,
                                         [] (const SongNote& note, double time) { return note.start < time; });

    juce::Array<int> waitingNotes;

    for (auto it = first; it != notes.end() && it->start <= now + waitingChordSeconds; ++it)
        if (isHandRole (player.getPartRole (it->part)) && settings.isPlayable (it->note))
            waitingNotes.addIfNotAlreadyThere (it->note);

    if (waitingNotes.isEmpty())
        return;

    waitingNotes.sort();

    juce::StringArray names;

    for (const auto note : waitingNotes)
        names.add (juce::MidiMessage::getMidiNoteName (note, true, true, 4));

    const auto text = "Waiting for " + names.joinIntoString (juce::String::fromUTF8 (" \xc2\xb7 "));
    const auto font = Theme::uiFont (13.0f, 500);
    const auto width = juce::GlyphArrangement::getStringWidth (font, text) + 46.0f;
    const auto chip = juce::Rectangle<float> (width, 34.0f).withCentre ({ area.getCentreX(), area.getBottom() - 39.0f });

    g.setColour (juce::Colour (0xd9141006));
    g.fillRoundedRectangle (chip, 17.0f);
    g.setColour (Theme::Colours::loop.withAlpha (0.45f));
    g.drawRoundedRectangle (chip.reduced (0.5f), 17.0f, 1.0f);

    g.setColour (Theme::Colours::loop);
    g.fillEllipse (chip.getX() + 14.0f, chip.getCentreY() - 4.0f, 8.0f, 8.0f);

    g.setColour (Theme::Colours::loopText);
    g.setFont (font);
    g.drawText (text, chip.withTrimmedLeft (30.0f).withTrimmedRight (14.0f), juce::Justification::centredLeft, false);
}

void NotesRenderer::paintKeys (juce::Graphics& g, juce::Rectangle<float> area, const Settings& settings, const Frame& frame) const
{
    const auto& layout = settings.layout;
    const auto whiteKeyWidth = area.getWidth() / (float) juce::jmax (1, layout.getNumWhiteKeys());
    const auto labelHeight = juce::jmin (whiteKeyWidth * 0.5f, area.getHeight() * 0.16f);

    // 黒鍵が白鍵の上に重なるように、白鍵を先に描く
    for (const bool drawBlack : { false, true })
    {
        for (int note = layout.getLowestNote(); note <= layout.getHighestNote(); ++note)
        {
            if (KeyboardLayout::isBlackKey (note) != drawBlack)
                continue;

            const auto bounds = getKeyBounds (settings, area, note);
            const auto& guide = frame.guides[(size_t) note];
            const auto isDown = keyState.isDown (note);
            const auto light = getKeyLightColour (isDown, PartColours::forRole (guide.role), guide.level, frame.isPlaying);
            const auto level = isDown ? 1.0f : guide.level;
            const auto playable = settings.isPlayable (note);

            if (settings.style == Style::projection)
            {
                auto fill = Colours::projectionBackground;

                if (! light.isTransparent())
                    fill = fill.interpolatedWith (isDown ? light.brighter (0.4f) : light, level);

                g.setColour (fill);
                g.fillRect (bounds.reduced (1.0f));
                g.setColour (Colours::projectionOutline);
                g.drawRect (bounds.reduced (1.0f), 1.0f);

                if (settings.showsKeyName (note))
                {
                    // 実物の鍵に映すので、消えているときは控えめな明るさにする
                    g.setColour (light.isTransparent() ? Colours::projectionLabel : juce::Colours::black.withAlpha (0.8f));
                    drawNoteLabel (g, getNoteLetter (note), bounds.reduced (2.0f, 6.0f), labelHeight, true);
                }

                continue;
            }

            if (! drawBlack)
            {
                // 白鍵: 隙間 1px、下側の角を丸め、手前の縁と奥の影を付ける
                const auto keyBounds = bounds.withTrimmedRight (1.0f);
                const auto shape = bottomRoundedRect (keyBounds, 5.0f);

                auto top = playable ? Colours::whiteKeyTop : Colours::greyKeyTop;
                auto middle = playable ? Colours::whiteKeyMiddle : Colours::greyKeyMiddle;
                auto bottom = playable ? Colours::whiteKeyBottom : Colours::greyKeyBottom;
                auto edge = playable ? Colours::whiteKeyEdge : Colours::greyKeyEdge;

                if (! light.isTransparent())
                {
                    const auto litTop = isDown ? scaled (light, 0.85f) : juce::Colours::white.interpolatedWith (light, 0.55f);
                    const auto litBottom = isDown ? light : juce::Colours::white.interpolatedWith (light, 0.85f);
                    top = top.interpolatedWith (litTop, level);
                    middle = middle.interpolatedWith (litTop.interpolatedWith (litBottom, 0.55f), level);
                    bottom = bottom.interpolatedWith (litBottom, level);
                    edge = edge.interpolatedWith (scaled (light, 0.7f), level);
                }

                fillVertical (g, shape, keyBounds, top, middle, 0.55f, bottom);

                juce::Graphics::ScopedSaveState saveState (g);
                g.reduceClipRegion (shape);
                g.setColour (edge);
                g.fillRect (keyBounds.withTop (keyBounds.getBottom() - whiteKeyFrontEdge));

                g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), 0.0f, keyBounds.getY(),
                                                         juce::Colours::transparentBlack, 0.0f, keyBounds.getY() + 10.0f, false));
                g.fillRect (keyBounds.withHeight (10.0f));

                if (settings.showsKeyName (note))
                {
                    g.setColour (light.isTransparent() ? (playable ? Colours::whiteKeyLabel : Colours::greyKeyLabel)
                                                       : scaled (light, 0.4f).interpolatedWith (Colours::whiteKeyLabel, 1.0f - level));
                    drawNoteLabel (g, getNoteLetter (note), keyBounds.reduced (2.0f, 0.0f).withTrimmedBottom (whiteKeyFrontEdge + 4.0f),
                                   labelHeight);
                }
            }
            else
            {
                // 黒鍵: 光沢のある立体、下に影を落とす
                const auto shape = bottomRoundedRect (bounds, 3.0f);

                g.setColour (juce::Colours::black.withAlpha (0.45f));
                g.fillPath (bottomRoundedRect (bounds.translated (0.0f, 3.0f).expanded (1.0f, 0.0f), 4.0f));

                auto top = playable ? Colours::blackKeyTop : Colours::greyBlackTop;
                auto middle = playable ? Colours::blackKeyMiddle : Colours::greyBlackMiddle;
                auto bottom = playable ? Colours::blackKeyBottom : Colours::greyBlackBottom;

                if (! light.isTransparent())
                {
                    top = top.interpolatedWith (scaled (light, 0.45f), level);
                    middle = middle.interpolatedWith (scaled (light, isDown ? 0.95f : 0.8f), level);
                    bottom = bottom.interpolatedWith (scaled (light, 0.4f), level);
                }

                fillVertical (g, shape, bounds, top, middle, 0.75f, bottom);

                juce::Graphics::ScopedSaveState saveState (g);
                g.reduceClipRegion (shape);
                g.setColour (juce::Colours::black.withAlpha (0.55f));
                g.fillRect (bounds.withTop (bounds.getBottom() - blackKeyFrontEdge));
                g.setColour (juce::Colours::white.withAlpha (0.08f));
                g.fillRect (bounds.withWidth (1.0f));
                g.setColour (juce::Colours::black.withAlpha (0.6f));
                g.fillRect (bounds.withLeft (bounds.getRight() - 1.0f));

                if (settings.showsKeyName (note))
                {
                    g.setColour (Colours::blackKeyLabel);
                    drawNoteLabel (g, getNoteLetter (note), bounds.reduced (1.0f, 0.0f).withTrimmedBottom (blackKeyFrontEdge + 3.0f),
                                   labelHeight * 0.8f);
                }
            }
        }
    }
}

} // namespace pianolume
