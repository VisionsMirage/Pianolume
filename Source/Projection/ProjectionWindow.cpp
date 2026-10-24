#include "ProjectionWindow.h"
#include "Platform/WindowsTitleBar.h"

namespace pianolume
{

namespace
{
    constexpr float handleHitRadius = 24.0f;
}

ProjectionWindow::ProjectionWindow (const KeyState& keyState, const SongPlayer& player, CalibrationModel& model,
                                    const KeyboardLayout& layout, juce::Rectangle<int> area, bool isWindowed)
    : calibrationModel (model),
      view (keyState, player, model)
{
    setOpaque (true);
    setWantsKeyboardFocus (true);
    setName ("Pianolume Projection");

    view.setLayout (layout);
    addAndMakeVisible (view);

    // プレビューは普通のウィンドウ、本番はタイトルバーなしでディスプレイ全体を覆う
    const auto windowStyle = isWindowed ? juce::ComponentPeer::windowHasTitleBar | juce::ComponentPeer::windowIsResizable
                                        | juce::ComponentPeer::windowHasCloseButton | juce::ComponentPeer::windowHasMinimiseButton
                                        | juce::ComponentPeer::windowHasMaximiseButton | juce::ComponentPeer::windowAppearsOnTaskbar
                                  : 0;

    addToDesktop (windowStyle);
    setBounds (area);
    setVisible (true);

    if (isWindowed)
        if (auto* peer = getPeer())
            applyDarkTitleBar (peer->getNativeHandle());

    toFront (true);
    grabKeyboardFocus();
}

ProjectionWindow::~ProjectionWindow()
{
    removeFromDesktop();
}

void ProjectionWindow::setLayout (const KeyboardLayout& layout)
{
    view.setLayout (layout);
}

void ProjectionWindow::setLookAheadSeconds (double seconds)
{
    view.setLookAheadSeconds (seconds);
}

void ProjectionWindow::setShowGrid (bool shouldShow)
{
    view.setShowGrid (shouldShow);
}

void ProjectionWindow::setNoteNames (bool onNotes, NotesRenderer::KeyNames onKeys)
{
    view.setNoteNames (onNotes, onKeys);
}

void ProjectionWindow::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void ProjectionWindow::resized()
{
    view.setBounds (getLocalBounds());

    if (onResized != nullptr)
        onResized();
}

bool ProjectionWindow::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        userTriedToCloseWindow();
        return true;
    }

    // 合わせ込み中は矢印キーで選択中の角を 1px（Shift で 10px）動かし、Tab で次の角を選ぶ
    if (calibrationModel.isCalibrating())
    {
        const auto step = key.getModifiers().isShiftDown() ? 10.0f : 1.0f;
        const auto dx = step / (float) juce::jmax (1, getWidth());
        const auto dy = step / (float) juce::jmax (1, getHeight());

        if (key.isKeyCode (juce::KeyPress::leftKey))  { calibrationModel.nudgeSelectedHandle ({ -dx, 0.0f }); return true; }
        if (key.isKeyCode (juce::KeyPress::rightKey)) { calibrationModel.nudgeSelectedHandle ({ dx, 0.0f });  return true; }
        if (key.isKeyCode (juce::KeyPress::upKey))    { calibrationModel.nudgeSelectedHandle ({ 0.0f, -dy }); return true; }
        if (key.isKeyCode (juce::KeyPress::downKey))  { calibrationModel.nudgeSelectedHandle ({ 0.0f, dy });  return true; }
        if (key.isKeyCode (juce::KeyPress::tabKey))   { calibrationModel.selectNextHandle(); return true; }
    }

    return onKeyPressed != nullptr && onKeyPressed (key);
}

void ProjectionWindow::userTriedToCloseWindow()
{
    if (onCloseRequested != nullptr)
        onCloseRequested();
}

void ProjectionWindow::mouseDown (const juce::MouseEvent& event)
{
    draggingHandle = CalibrationModel::Handle::none;

    if (! calibrationModel.isCalibrating())
        return;

    auto closestDistance = handleHitRadius;

    for (const auto handle : calibrationModel.getEditableHandles())
    {
        const auto position = calibrationModel.getHandlePosition (handle);
        const juce::Point<float> onScreen (position.x * (float) getWidth(), position.y * (float) getHeight());
        const auto distance = onScreen.getDistanceFrom (event.position);

        if (distance < closestDistance)
        {
            closestDistance = distance;
            draggingHandle = handle;
        }
    }

    if (draggingHandle != CalibrationModel::Handle::none)
        calibrationModel.setSelectedHandle (draggingHandle);
}

void ProjectionWindow::mouseDrag (const juce::MouseEvent& event)
{
    if (draggingHandle != CalibrationModel::Handle::none)
        calibrationModel.moveHandle (draggingHandle, toNormalised (event.position));
}

juce::Point<float> ProjectionWindow::toNormalised (juce::Point<float> position) const
{
    return { position.x / (float) juce::jmax (1, getWidth()), position.y / (float) juce::jmax (1, getHeight()) };
}

} // namespace pianolume
