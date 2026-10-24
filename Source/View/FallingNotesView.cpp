#include "FallingNotesView.h"
#include "UI/Theme.h"

namespace pianolume
{

namespace
{
    constexpr float feltHeight = 8.0f;
}

FallingNotesView::FallingNotesView (const KeyState& keyState, const SongPlayer& player)
    : renderer (keyState, player)
{
    // 描画スレッドより先に、メッセージスレッドでフォントを読み込んでおく
    Theme::getTypeface (400);

    setOpaque (false);

    openGLContext.setRenderer (this);
    openGLContext.setComponentPaintingEnabled (true);
    openGLContext.setContinuousRepainting (true);
    openGLContext.attachTo (*this);
}

FallingNotesView::~FallingNotesView()
{
    openGLContext.detach();
}

void FallingNotesView::setLayout (const KeyboardLayout& newLayout)
{
    const juce::SpinLock::ScopedLockType lock (settingsLock);
    settings.layout = newLayout;
}

void FallingNotesView::setPlayableRange (int lowestNote, int highestNote)
{
    const juce::SpinLock::ScopedLockType lock (settingsLock);
    settings.playableLowest = lowestNote;
    settings.playableHighest = highestNote;
}

void FallingNotesView::setLookAheadSeconds (double seconds)
{
    const juce::SpinLock::ScopedLockType lock (settingsLock);
    settings.lookAheadSeconds = juce::jlimit (0.5, 10.0, seconds);
}

void FallingNotesView::setNoteNames (bool onNotes, NotesRenderer::KeyNames onKeys)
{
    const juce::SpinLock::ScopedLockType lock (settingsLock);
    settings.showNoteNames = onNotes;
    settings.keyNames = onKeys;
}

void FallingNotesView::resized()
{
    // 描画スレッドからコンポーネントの大きさを直接読まないよう、ここで保存しておく
    viewWidth = getWidth();
    viewHeight = getHeight();
}

void FallingNotesView::renderOpenGL()
{
    const auto scale = (float) openGLContext.getRenderingScale();
    const auto width = viewWidth.load();
    const auto height = viewHeight.load();

    juce::OpenGLHelpers::clear (Theme::Colours::stage);

    if (width <= 0 || height <= 0)
        return;

    const auto graphicsContext = juce::createOpenGLGraphicsContext (openGLContext,
                                                                    juce::roundToInt (scale * (float) width),
                                                                    juce::roundToInt (scale * (float) height));

    if (graphicsContext == nullptr)
        return;

    NotesRenderer::Settings currentSettings;

    {
        const juce::SpinLock::ScopedLockType lock (settingsLock);
        currentSettings = settings;
    }

    juce::Graphics g (*graphicsContext);
    g.addTransform (juce::AffineTransform::scale (scale));

    juce::Rectangle<float> area ((float) width, (float) height);
    g.setColour (Theme::Colours::stage);
    g.fillRect (area);

    auto fallArea = area;
    const auto keyArea = fallArea.removeFromBottom (area.getHeight() * keyAreaProportion.load());
    const auto feltArea = fallArea.removeFromBottom (feltHeight);

    auto frame = renderer.beginFrame();
    renderer.paintFallArea (g, fallArea, currentSettings, frame);

    // 鍵盤の奥のフェルト
    g.setColour (Theme::Colours::felt);
    g.fillRect (feltArea);
    g.setColour (Theme::Colours::feltEdge);
    g.fillRect (feltArea.withHeight (1.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, 0.0f, feltArea.getBottom() - 3.0f,
                                             juce::Colours::black.withAlpha (0.5f), 0.0f, feltArea.getBottom(), false));
    g.fillRect (feltArea.withTop (feltArea.getBottom() - 3.0f));

    renderer.paintKeys (g, keyArea, currentSettings, frame);
}

} // namespace pianolume
