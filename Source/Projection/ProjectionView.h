#pragma once

#include <juce_opengl/juce_opengl.h>
#include "CalibrationModel.h"
#include "View/NotesRenderer.h"

namespace pianolume
{

/** プロジェクタへの出力。落下エリアと鍵盤を別々の面に描き、合わせ込みの四隅に合わせて変形して投影する。

    合わせ込み中は、面の枠・四隅の番号・鍵の輪郭を重ねて表示する。
*/
class ProjectionView : public juce::Component,
                       private juce::OpenGLRenderer
{
public:
    ProjectionView (const KeyState& keyState, const SongPlayer& player, const CalibrationModel& calibrationModel);
    ~ProjectionView() override;

    void setLayout (const KeyboardLayout& newLayout);
    void setLookAheadSeconds (double seconds);
    void setShowGrid (bool shouldShow);
    void setNoteNames (bool onNotes, NotesRenderer::KeyNames onKeys);

    void paint (juce::Graphics&) override {}
    void resized() override;

private:
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    void renderToBuffer (juce::OpenGLFrameBuffer& buffer, int width, int height,
                         const std::function<void (juce::Graphics&, juce::Rectangle<float>)>& paint);
    void drawWarped (juce::OpenGLFrameBuffer& buffer, const ProjectionCalibration::Corners& corners,
                     const ProjectionCalibration& calibration, int viewportWidth, int viewportHeight);

    void paintFallTestPattern (juce::Graphics& g, juce::Rectangle<float> area, const NotesRenderer::Settings& settings) const;
    void paintKeyTestPattern (juce::Graphics& g, juce::Rectangle<float> area, const NotesRenderer::Settings& settings) const;
    void paintHandles (juce::Graphics& g, juce::Rectangle<float> screen, const ProjectionCalibration& calibration) const;

    NotesRenderer renderer;
    const CalibrationModel& calibrationModel;

    juce::OpenGLContext openGLContext;
    std::unique_ptr<juce::OpenGLShaderProgram> warpShader;
    juce::OpenGLFrameBuffer fallBuffer, keyBuffer;
    GLuint vertexBuffer = 0;

    juce::SpinLock settingsLock;
    NotesRenderer::Settings settings;

    std::atomic<int> viewWidth { 0 };
    std::atomic<int> viewHeight { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectionView)
};

} // namespace pianolume
