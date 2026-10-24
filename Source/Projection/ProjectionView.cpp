#include "ProjectionView.h"
#include "Homography.h"
#include "UI/Theme.h"

namespace pianolume
{

using namespace juce::gl;

namespace
{
    // 各面を描くバッファの大きさ。変形して投影するので、プロジェクタの解像度に近い精細さにしておく
    constexpr int fallBufferWidth = 2048;
    constexpr int fallBufferHeight = 1024;
    constexpr int keyBufferWidth = 2048;
    constexpr int keyBufferHeight = 512;

    constexpr float handleRadius = 13.0f;

    const juce::Colour fallPlaneColour { 0xff38bdf8 };
    const juce::Colour keyPlaneColour { 0xfffbbf24 };

    const char* const vertexShader = R"(
        attribute vec2 position;

        void main()
        {
            gl_Position = vec4 (position, 0.0, 1.0);
        }
    )";

    // 画面上の各ピクセルについて、逆変換で面の上の位置を求めて色を取る
    const char* const fragmentShader = R"(
        uniform sampler2D planeTexture;
        uniform mat3 inverseHomography;
        uniform vec2 viewportSize;
        uniform float brightness;
        uniform float gamma;

        void main()
        {
            vec2 screenPosition = vec2 (gl_FragCoord.x / viewportSize.x, 1.0 - gl_FragCoord.y / viewportSize.y);
            vec3 mapped = inverseHomography * vec3 (screenPosition, 1.0);

            if (mapped.z <= 0.0)
                discard;

            vec2 uv = mapped.xy / mapped.z;

            if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
                discard;

            vec4 colour = texture2D (planeTexture, vec2 (uv.x, 1.0 - uv.y));
            vec3 adjusted = pow (max (colour.rgb, vec3 (0.0)), vec3 (gamma)) * brightness;
            gl_FragColor = vec4 (adjusted, 1.0);
        }
    )";
}

ProjectionView::ProjectionView (const KeyState& keyState, const SongPlayer& player, const CalibrationModel& model)
    : renderer (keyState, player),
      calibrationModel (model)
{
    // 描画スレッドより先に、メッセージスレッドでフォントを読み込んでおく
    Theme::getTypeface (400);

    settings.style = NotesRenderer::Style::projection;
    settings.showGrid = false;

    setOpaque (true);
    setInterceptsMouseClicks (false, false);

    openGLContext.setRenderer (this);
    openGLContext.setComponentPaintingEnabled (false);
    openGLContext.setContinuousRepainting (true);
    openGLContext.attachTo (*this);
}

ProjectionView::~ProjectionView()
{
    openGLContext.detach();
}

void ProjectionView::setLayout (const KeyboardLayout& newLayout)
{
    const juce::SpinLock::ScopedLockType lock (settingsLock);
    settings.layout = newLayout;
}

void ProjectionView::setLookAheadSeconds (double seconds)
{
    const juce::SpinLock::ScopedLockType lock (settingsLock);
    settings.lookAheadSeconds = juce::jlimit (0.5, 10.0, seconds);
}

void ProjectionView::setShowGrid (bool shouldShow)
{
    const juce::SpinLock::ScopedLockType lock (settingsLock);
    settings.showGrid = shouldShow;
}

void ProjectionView::setNoteNames (bool onNotes, NotesRenderer::KeyNames onKeys)
{
    const juce::SpinLock::ScopedLockType lock (settingsLock);
    settings.showNoteNames = onNotes;
    settings.keyNames = onKeys;
}

void ProjectionView::resized()
{
    viewWidth = getWidth();
    viewHeight = getHeight();
}

void ProjectionView::newOpenGLContextCreated()
{
    auto shader = std::make_unique<juce::OpenGLShaderProgram> (openGLContext);

    if (shader->addVertexShader (juce::OpenGLHelpers::translateVertexShaderToV3 (vertexShader))
        && shader->addFragmentShader (juce::OpenGLHelpers::translateFragmentShaderToV3 (fragmentShader))
        && shader->link())
    {
        warpShader = std::move (shader);
    }
    else
    {
        juce::Logger::writeToLog ("Projection shader error: " + shader->getLastError());
    }

    // 画面全体を覆う 2 枚の三角形
    const GLfloat vertices[] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
    glGenBuffers (1, &vertexBuffer);
    glBindBuffer (GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData (GL_ARRAY_BUFFER, sizeof (vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
}

void ProjectionView::openGLContextClosing()
{
    warpShader.reset();
    fallBuffer.release();
    keyBuffer.release();

    if (vertexBuffer != 0)
        glDeleteBuffers (1, &vertexBuffer);

    vertexBuffer = 0;
}

void ProjectionView::renderOpenGL()
{
    const auto scale = (float) openGLContext.getRenderingScale();
    const auto width = viewWidth.load();
    const auto height = viewHeight.load();
    const auto pixelWidth = juce::roundToInt (scale * (float) width);
    const auto pixelHeight = juce::roundToInt (scale * (float) height);

    if (width <= 0 || height <= 0 || warpShader == nullptr)
    {
        juce::OpenGLHelpers::clear (juce::Colours::black);
        return;
    }

    const auto calibration = calibrationModel.getCalibration();
    const auto isCalibrating = calibrationModel.isCalibrating();

    NotesRenderer::Settings planeSettings;

    {
        const juce::SpinLock::ScopedLockType lock (settingsLock);
        planeSettings = settings;
    }

    planeSettings.blackKeyOffsetXLeft = calibration.blackKeyOffsetXLeft;
    planeSettings.blackKeyOffsetXRight = calibration.blackKeyOffsetXRight;
    planeSettings.blackKeyOffsetY = calibration.blackKeyOffsetY;
    planeSettings.blackKeyLength = calibration.blackKeyLength;

    auto frame = renderer.beginFrame();

    renderToBuffer (fallBuffer, fallBufferWidth, fallBufferHeight, [&] (juce::Graphics& g, juce::Rectangle<float> area)
    {
        renderer.paintFallArea (g, area, planeSettings, frame);

        if (isCalibrating)
            paintFallTestPattern (g, area, planeSettings);
    });

    renderToBuffer (keyBuffer, keyBufferWidth, keyBufferHeight, [&] (juce::Graphics& g, juce::Rectangle<float> area)
    {
        renderer.paintKeys (g, area, planeSettings, frame);

        if (isCalibrating)
            paintKeyTestPattern (g, area, planeSettings);
    });

    // 画面へ、各面を四隅に合わせて変形して描く
    glBindFramebuffer (GL_FRAMEBUFFER, openGLContext.getFrameBufferID());
    glViewport (0, 0, pixelWidth, pixelHeight);
    glDisable (GL_SCISSOR_TEST);
    glDisable (GL_BLEND);
    juce::OpenGLHelpers::clear (juce::Colours::black);

    drawWarped (fallBuffer, calibration.fallCorners, calibration, pixelWidth, pixelHeight);
    drawWarped (keyBuffer, calibration.keyCorners, calibration, pixelWidth, pixelHeight);

    if (isCalibrating)
    {
        if (const auto graphicsContext = juce::createOpenGLGraphicsContext (openGLContext, pixelWidth, pixelHeight))
        {
            juce::Graphics g (*graphicsContext);
            g.addTransform (juce::AffineTransform::scale (scale));
            paintHandles (g, { (float) width, (float) height }, calibration);
        }
    }
}

void ProjectionView::renderToBuffer (juce::OpenGLFrameBuffer& buffer, int width, int height,
                                     const std::function<void (juce::Graphics&, juce::Rectangle<float>)>& paint)
{
    if (! buffer.isValid() || buffer.getWidth() != width || buffer.getHeight() != height)
        buffer.initialise (openGLContext, width, height);

    if (const auto graphicsContext = juce::createOpenGLGraphicsContext (openGLContext, buffer))
    {
        juce::Graphics g (*graphicsContext);
        g.fillAll (juce::Colours::black);
        paint (g, { (float) width, (float) height });
    }
}

void ProjectionView::drawWarped (juce::OpenGLFrameBuffer& buffer, const ProjectionCalibration::Corners& corners,
                                 const ProjectionCalibration& calibration, int viewportWidth, int viewportHeight)
{
    const auto inverse = Homography::squareToQuad (corners).inverted().toColumnMajor();

    warpShader->use();
    warpShader->setUniformMat3 ("inverseHomography", inverse.data(), 1, GL_FALSE);
    warpShader->setUniform ("viewportSize", (GLfloat) viewportWidth, (GLfloat) viewportHeight);
    warpShader->setUniform ("brightness", (GLfloat) calibration.brightness);
    warpShader->setUniform ("gamma", (GLfloat) calibration.gamma);
    warpShader->setUniform ("planeTexture", (GLint) 0);

    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, buffer.getTextureID());

    const auto position = (GLuint) glGetAttribLocation (warpShader->getProgramID(), "position");
    glBindBuffer (GL_ARRAY_BUFFER, vertexBuffer);
    glVertexAttribPointer (position, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray (position);

    glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray (position);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glBindTexture (GL_TEXTURE_2D, 0);
    glUseProgram (0);
}

void ProjectionView::paintFallTestPattern (juce::Graphics& g, juce::Rectangle<float> area,
                                           const NotesRenderer::Settings& planeSettings) const
{
    const auto& layout = planeSettings.layout;

    // 横方向のグリッドと、オクターブ（C）ごとの縦線で、面のゆがみが分かるようにする
    g.setColour (fallPlaneColour.withAlpha (0.3f));

    for (int i = 1; i < 8; ++i)
        g.fillRect (area.getX(), area.getY() + area.getHeight() * (float) i / 8.0f, area.getWidth(), 2.0f);

    for (int note = layout.getLowestNote(); note <= layout.getHighestNote(); ++note)
        if (note % 12 == 0)
            g.fillRect (NotesRenderer::getKeyX (layout, area, note).getStart() - 1.0f, area.getY(), 3.0f, area.getHeight());

    g.setColour (fallPlaneColour);
    g.drawRect (area, 6.0f);

    g.setFont (Theme::uiFont (72.0f, 700));
    g.setColour (fallPlaneColour.withAlpha (0.6f));
    g.drawText ("FALL AREA", area, juce::Justification::centred, false);
}

void ProjectionView::paintKeyTestPattern (juce::Graphics& g, juce::Rectangle<float> area,
                                          const NotesRenderer::Settings& planeSettings) const
{
    const auto& layout = planeSettings.layout;

    // 全鍵の輪郭。黒鍵は補正を反映した位置に描く
    for (const bool drawBlack : { false, true })
    {
        for (int note = layout.getLowestNote(); note <= layout.getHighestNote(); ++note)
        {
            if (KeyboardLayout::isBlackKey (note) != drawBlack)
                continue;

            const auto bounds = NotesRenderer::getKeyBounds (planeSettings, area, note).reduced (2.0f);

            if (drawBlack)
            {
                g.setColour (juce::Colours::black.withAlpha (0.6f));
                g.fillRect (bounds);
            }

            g.setColour (drawBlack ? keyPlaneColour : juce::Colours::white.withAlpha (0.85f));
            g.drawRect (bounds, 4.0f);

            if (note % 12 == 0)
            {
                g.setFont (Theme::uiFont (40.0f, 700));
                g.setColour (keyPlaneColour);
                g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 4),
                            bounds.withTop (bounds.getBottom() - 70.0f), juce::Justification::centred, false);
            }
        }
    }

    g.setColour (keyPlaneColour);
    g.drawRect (area, 6.0f);
}

void ProjectionView::paintHandles (juce::Graphics& g, juce::Rectangle<float> screen, const ProjectionCalibration& calibration) const
{
    const auto toScreen = [&] (juce::Point<float> normalised)
    {
        return juce::Point<float> (screen.getX() + normalised.x * screen.getWidth(), screen.getY() + normalised.y * screen.getHeight());
    };

    const auto drawQuad = [&] (const ProjectionCalibration::Corners& corners, juce::Colour colour)
    {
        juce::Path path;
        path.startNewSubPath (toScreen (corners[0]));

        for (size_t i = 1; i < corners.size(); ++i)
            path.lineTo (toScreen (corners[i]));

        path.closeSubPath();
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (2.0f));
    };

    drawQuad (calibration.fallCorners, fallPlaneColour);
    drawQuad (calibration.keyCorners, keyPlaneColour);

    const auto selected = calibrationModel.getSelectedHandle();

    for (const auto handle : calibrationModel.getEditableHandles())
    {
        auto copy = calibration;
        const auto& corners = CalibrationModel::isFallHandle (handle) ? copy.fallCorners : copy.keyCorners;
        const auto centre = toScreen (corners[(size_t) handle % 4]);
        const auto colour = CalibrationModel::isFallHandle (handle) ? fallPlaneColour : keyPlaneColour;
        const auto isSelected = handle == selected;
        const auto circle = juce::Rectangle<float> (handleRadius * 2.0f, handleRadius * 2.0f).withCentre (centre);

        g.setColour (isSelected ? colour : juce::Colours::black.withAlpha (0.8f));
        g.fillEllipse (circle);
        g.setColour (colour);
        g.drawEllipse (circle, 2.0f);

        g.setColour (isSelected ? juce::Colours::black : juce::Colours::white);
        g.setFont (Theme::uiFont (13.0f, 700));
        g.drawText (CalibrationModel::getHandleLabel (handle), circle, juce::Justification::centred, false);
    }
}

} // namespace pianolume
