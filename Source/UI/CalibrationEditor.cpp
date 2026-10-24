#include "CalibrationEditor.h"
#include "Projection/Homography.h"
#include "Theme.h"

namespace pianolume
{

namespace
{
    constexpr int sidePanelWidth = 300;
    constexpr int padding = 20;
    constexpr int rowHeight = 30;
    constexpr int rowGap = 8;
    constexpr int sectionGap = 18;
    constexpr int footerHeight = 64;

    const juce::Colour fallPlaneColour { 0xff38bdf8 };
    const juce::Colour keyPlaneColour { 0xfffbbf24 };

    using Handle = CalibrationModel::Handle;

    constexpr float minZoom = 0.5f;
    constexpr float maxZoom = 32.0f;
    constexpr float wheelZoomStep = 1.25f;
    constexpr float buttonZoomStep = 1.5f;

    // Shift を押しながらドラッグすると、マウスの動きの 1/5 だけ四隅を動かす
    constexpr float fineDragFactor = 0.2f;
}

//==============================================================================
/** プロジェクタ画面の縮小表示。四隅をドラッグし、矢印キーで微調整する。 */
class CalibrationEditor::Canvas : public juce::Component,
                                  public juce::SettableTooltipClient
{
public:
    explicit Canvas (CalibrationModel& modelToUse)
        : model (modelToUse)
    {
        setWantsKeyboardFocus (true);
        setTooltip ("Drag the numbered corners (Shift: fine). Mouse wheel zooms, drag the background to pan. "
                    "Arrow keys move the selected corner (Shift: 10 px), Tab selects the next one.");

        for (auto* button : { &fitScreenButton, &zoomOutButton, &zoomInButton })
        {
            addAndMakeVisible (button);
            button->setWantsKeyboardFocus (false);
        }

        fitScreenButton.setTooltip ("Show the whole projector screen (0)");
        zoomOutButton.setTooltip ("Zoom out (-)");
        zoomInButton.setTooltip ("Zoom in (+)");

        fitScreenButton.onClick = [this] { resetView(); };
        zoomOutButton.onClick = [this] { zoomAround (getLocalBounds().toFloat().getCentre(), 1.0f / buttonZoomStep); };
        zoomInButton.onClick = [this] { zoomAround (getLocalBounds().toFloat().getCentre(), buttonZoomStep); };
    }

    void resized() override
    {
        auto bar = getLocalBounds().reduced (10).removeFromTop (26);
        zoomInButton.setBounds (bar.removeFromRight (30));
        bar.removeFromRight (4);
        zoomOutButton.setBounds (bar.removeFromRight (30));
        bar.removeFromRight (8);
        fitScreenButton.setBounds (bar.removeFromRight (106));
        zoomLabelArea = bar.removeFromRight (64);
    }

    void setProjectorSize (juce::Point<int> size)
    {
        projectorSize = { juce::jmax (1, size.x), juce::jmax (1, size.y) };
        repaint();
    }

    void setLayout (const KeyboardLayout& newLayout)
    {
        layout = newLayout;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace Theme::Colours;

        g.setColour (stage);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 12.0f);

        const auto screen = getScreenArea();
        g.setColour (juce::Colours::black);
        g.fillRect (screen);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawRect (screen, 1.0f);

        g.setColour (textFaint);
        g.setFont (Theme::uiFont (11.0f, 600));
        g.drawText ("PROJECTOR " + juce::String (projectorSize.x) + juce::String::fromUTF8 (" \xc3\x97 ") + juce::String (projectorSize.y),
                    screen.withHeight (18.0f).translated (0.0f, -20.0f), juce::Justification::centredLeft, false);

        const auto calibration = model.getCalibration();
        paintPlane (g, calibration.fallCorners, fallPlaneColour, "FALL AREA", false);
        paintPlane (g, calibration.keyCorners, keyPlaneColour, "KEYS", true);

        const auto selected = model.getSelectedHandle();

        for (const auto handle : model.getEditableHandles())
        {
            const auto centre = toCanvas (model.getHandlePosition (handle));
            const auto colour = CalibrationModel::isFallHandle (handle) ? fallPlaneColour : keyPlaneColour;
            const auto isSelected = handle == selected;
            const auto circle = juce::Rectangle<float> (22.0f, 22.0f).withCentre (centre);

            g.setColour (isSelected ? colour : panel);
            g.fillEllipse (circle);
            g.setColour (colour);
            g.drawEllipse (circle, 2.0f);

            if (isSelected && hasKeyboardFocus (false))
            {
                g.setColour (colour.withAlpha (0.35f));
                g.drawEllipse (circle.expanded (5.0f), 2.0f);
            }

            g.setColour (isSelected ? onPrimary : text);
            g.setFont (Theme::uiFont (11.0f, 700));
            g.drawText (CalibrationModel::getHandleLabel (handle), circle, juce::Justification::centred, false);
        }

        g.setColour (textMuted);
        g.setFont (Theme::monoFont (11.0f));
        g.drawText (juce::String (juce::roundToInt (zoom * 100.0f)) + "%", zoomLabelArea.withTrimmedRight (8),
                    juce::Justification::centredRight, false);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        grabKeyboardFocus();
        dragging = Handle::none;
        isPanning = false;
        lastDragPosition = event.position;

        // 中ボタン・右ボタンは、角の上でも表示の移動にする
        if (event.mods.isMiddleButtonDown() || event.mods.isRightButtonDown())
        {
            isPanning = true;
            return;
        }

        auto closest = 18.0f;

        for (const auto handle : model.getEditableHandles())
        {
            const auto distance = toCanvas (model.getHandlePosition (handle)).getDistanceFrom (event.position);

            if (distance < closest)
            {
                closest = distance;
                dragging = handle;
            }
        }

        if (dragging != Handle::none)
            model.setSelectedHandle (dragging);
        else
            isPanning = true;
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        const auto delta = event.position - lastDragPosition;
        lastDragPosition = event.position;

        if (isPanning)
        {
            pan += delta;
            repaint();
            return;
        }

        if (dragging == Handle::none)
            return;

        // 動いた分だけ足していくので、途中で Shift を押しても角が飛ばない
        const auto factor = event.mods.isShiftDown() ? fineDragFactor : 1.0f;
        const auto screen = getScreenArea();
        const juce::Point<float> step { delta.x * factor / screen.getWidth(), delta.y * factor / screen.getHeight() };
        model.moveHandle (dragging, model.getHandlePosition (dragging) + step);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        dragging = Handle::none;
        isPanning = false;
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if (dragging == Handle::none && ! event.mods.isPopupMenu())
            resetView();
    }

    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        const auto amount = wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX;

        // Windows ではホイール 1 目盛りで deltaY がおよそ 0.47 になり、約 1.2 倍になる
        if (amount != 0.0f)
            zoomAround (event.position, std::pow (wheelZoomStep, amount * (wheel.isReversed ? -2.0f : 2.0f)));
    }

    void mouseMagnify (const juce::MouseEvent& event, float scaleFactor) override
    {
        zoomAround (event.position, scaleFactor);
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        const auto step = key.getModifiers().isShiftDown() ? 10.0f : 1.0f;
        const auto dx = step / (float) projectorSize.x;
        const auto dy = step / (float) projectorSize.y;

        if (key.isKeyCode (juce::KeyPress::leftKey))  { model.nudgeSelectedHandle ({ -dx, 0.0f }); return true; }
        if (key.isKeyCode (juce::KeyPress::rightKey)) { model.nudgeSelectedHandle ({ dx, 0.0f });  return true; }
        if (key.isKeyCode (juce::KeyPress::upKey))    { model.nudgeSelectedHandle ({ 0.0f, -dy }); return true; }
        if (key.isKeyCode (juce::KeyPress::downKey))  { model.nudgeSelectedHandle ({ 0.0f, dy });  return true; }
        if (key.isKeyCode (juce::KeyPress::tabKey))   { model.selectNextHandle(); return true; }

        const auto character = key.getTextCharacter();
        const auto centre = getLocalBounds().toFloat().getCentre();

        if (character == '+' || character == '=' || character == ';') { zoomAround (centre, buttonZoomStep); return true; }
        if (character == '-')                                          { zoomAround (centre, 1.0f / buttonZoomStep); return true; }
        if (character == '0')                                          { resetView(); return true; }

        return false;
    }

    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override   { repaint(); }

private:
    void resetView()
    {
        zoom = 1.0f;
        pan = {};
        repaint();
    }

    /** position（キャンバス上の位置）の下にある点を動かさずに拡大・縮小する。 */
    void zoomAround (juce::Point<float> position, float factor)
    {
        const auto newZoom = juce::jlimit (minZoom, maxZoom, zoom * factor);

        if (newZoom == zoom)
            return;

        const auto anchor = toNormalised (position);
        zoom = newZoom;
        pan += position - toCanvas (anchor);
        repaint();
    }

    /** 拡大していないときの、プロジェクタ画面の表示位置。 */
    juce::Rectangle<float> getFittedScreenArea() const
    {
        const auto available = getLocalBounds().toFloat().reduced (36.0f);
        const auto aspect = (float) projectorSize.x / (float) projectorSize.y;
        auto width = available.getWidth();
        auto height = width / aspect;

        if (height > available.getHeight())
        {
            height = available.getHeight();
            width = height * aspect;
        }

        return juce::Rectangle<float> (width, height).withCentre (available.getCentre());
    }

    juce::Rectangle<float> getScreenArea() const
    {
        const auto fitted = getFittedScreenArea();
        return fitted.withSizeKeepingCentre (fitted.getWidth() * zoom, fitted.getHeight() * zoom) + pan;
    }

    juce::Point<float> toCanvas (juce::Point<float> normalised) const
    {
        const auto screen = getScreenArea();
        return { screen.getX() + normalised.x * screen.getWidth(), screen.getY() + normalised.y * screen.getHeight() };
    }

    juce::Point<float> toNormalised (juce::Point<float> position) const
    {
        const auto screen = getScreenArea();
        return { (position.x - screen.getX()) / screen.getWidth(), (position.y - screen.getY()) / screen.getHeight() };
    }

    void paintPlane (juce::Graphics& g, const ProjectionCalibration::Corners& corners, juce::Colour colour,
                     const juce::String& title, bool showKeys) const
    {
        juce::Path outline;
        outline.startNewSubPath (toCanvas (corners[0]));

        for (size_t i = 1; i < corners.size(); ++i)
            outline.lineTo (toCanvas (corners[i]));

        outline.closeSubPath();

        g.setColour (colour.withAlpha (0.10f));
        g.fillPath (outline);

        // 面の中の位置（0〜1）を、四隅に合わせた変形で画面上に写して、鍵の位置の線を描く
        const auto homography = Homography::squareToQuad (corners);
        const auto mapLine = [&] (float x) { return juce::Line<float> (toCanvas (homography.map ({ x, 0.0f })),
                                                                       toCanvas (homography.map ({ x, 1.0f }))); };
        const auto numWhiteKeys = (float) juce::jmax (1, layout.getNumWhiteKeys());

        for (int note = layout.getLowestNote(); note <= layout.getHighestNote(); ++note)
        {
            const auto isOctave = note % 12 == 0;

            if (KeyboardLayout::isBlackKey (note) || (! showKeys && ! isOctave))
                continue;

            g.setColour (colour.withAlpha (isOctave ? 0.55f : 0.2f));
            g.drawLine (mapLine (layout.getKeyRange (note).getStart() / numWhiteKeys), isOctave ? 1.5f : 1.0f);
        }

        g.setColour (colour);
        g.strokePath (outline, juce::PathStrokeType (2.0f));

        g.setColour (colour.withAlpha (0.8f));
        g.setFont (Theme::uiFont (12.0f, 700));
        g.drawText (title, juce::Rectangle<float> (120.0f, 18.0f).withCentre (toCanvas (homography.map ({ 0.5f, 0.5f }))),
                    juce::Justification::centred, false);
    }

    CalibrationModel& model;
    KeyboardLayout layout;
    juce::Point<int> projectorSize { 1920, 1080 };
    Handle dragging = Handle::none;

    float zoom = 1.0f;
    juce::Point<float> pan;
    bool isPanning = false;
    juce::Point<float> lastDragPosition;

    juce::TextButton fitScreenButton { "Whole Screen" };
    juce::TextButton zoomOutButton { "-" }, zoomInButton { "+" };
    juce::Rectangle<int> zoomLabelArea;
};

//==============================================================================
CalibrationEditor::CalibrationEditor (CalibrationModel& modelToUse, juce::File folder)
    : model (modelToUse),
      profilesFolder (std::move (folder)),
      canvas (std::make_unique<Canvas> (modelToUse))
{
    addAndMakeVisible (*canvas);

    for (auto* component : std::initializer_list<juce::Component*> {
             &linkToggle, &flipHorizontalButton, &flipVerticalButton, &rotateButton, &resetButton,
             &squareFallButton, &squareKeysButton,
             &blackOffsetLeftSlider, &blackOffsetRightSlider, &blackOffsetYSlider, &blackLengthSlider, &brightnessSlider, &gammaSlider,
             &profileBox, &saveProfileButton, &deleteProfileButton, &doneButton })
        addAndMakeVisible (component);

    linkToggle.onClick = [this] { model.setLinkPlanes (linkToggle.getToggleState()); };
    flipHorizontalButton.onClick = [this] { model.flipHorizontally(); };
    flipVerticalButton.onClick = [this] { model.flipVertically(); };
    rotateButton.onClick = [this] { model.rotate180(); };
    squareFallButton.onClick = [this] { model.squarePlane (true); };
    squareKeysButton.onClick = [this] { model.squarePlane (false); };
    resetButton.onClick = [this] { model.resetCorners(); };

    flipHorizontalButton.setTooltip ("Mirror left and right (e.g. rear projection)");
    flipVerticalButton.setTooltip ("Mirror top and bottom");
    rotateButton.setTooltip ("For a projector mounted upside down");
    squareFallButton.setTooltip ("Make 1-2-3-4 a level rectangle, keeping corner 1, the width to 2 and the height to 4");
    squareKeysButton.setTooltip ("Make the keys a level rectangle, keeping corner 4, the width to 3 and the height to 6");

    const auto setUpSlider = [] (juce::Slider& slider, double minimum, double maximum, double interval, double defaultValue)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
        slider.setRange (minimum, maximum, interval);
        slider.setDoubleClickReturnValue (true, defaultValue);
    };

    setUpSlider (blackOffsetLeftSlider, -1.0, 1.0, 0.005, 0.0);
    setUpSlider (blackOffsetRightSlider, -1.0, 1.0, 0.005, 0.0);
    setUpSlider (blackOffsetYSlider, -0.3, 0.3, 0.005, 0.0);
    setUpSlider (blackLengthSlider, 0.4, 0.9, 0.005, KeyboardLayout::blackKeyLength);
    setUpSlider (brightnessSlider, 0.2, 1.0, 0.01, 1.0);
    setUpSlider (gammaSlider, 0.5, 2.0, 0.01, 1.0);

    for (auto* slider : { &blackOffsetLeftSlider, &blackOffsetRightSlider, &blackOffsetYSlider, &blackLengthSlider })
        slider->onValueChange = [this] { applyBlackKeys(); };

    blackOffsetLeftSlider.setTooltip ("Move the black keys at the left end sideways (in white-key widths). Double-click to reset.");
    blackOffsetRightSlider.setTooltip ("Move the black keys at the right end sideways (in white-key widths). Double-click to reset.");

    // 矢印キーは四隅の微調整に使うので、スライダーやボタンにはフォーカスを渡さない
    for (auto* component : std::initializer_list<juce::Component*> {
             &linkToggle, &flipHorizontalButton, &flipVerticalButton, &rotateButton, &resetButton,
             &squareFallButton, &squareKeysButton, &blackOffsetLeftSlider, &blackOffsetRightSlider,
             &blackOffsetYSlider, &blackLengthSlider, &brightnessSlider, &gammaSlider,
             &saveProfileButton, &deleteProfileButton })
        component->setWantsKeyboardFocus (false);

    for (auto* slider : { &brightnessSlider, &gammaSlider })
        slider->onValueChange = [this] { applyOutput(); };

    profileBox.setTextWhenNothingSelected ("Load a profile...");
    profileBox.setTextWhenNoChoicesAvailable ("No saved profiles");
    profileBox.onChange = [this] { loadProfile (profileBox.getText()); };
    saveProfileButton.onClick = [this] { askProfileName(); };
    deleteProfileButton.onClick = [this] { deleteSelectedProfile(); };

    doneButton.setButtonText ("Done");
    doneButton.onClick = [this] { if (onDone != nullptr) onDone(); };

    model.addChangeListener (this);
    refreshFromModel();
    refreshProfiles();

    setSize (1100, 720);
}

CalibrationEditor::~CalibrationEditor()
{
    model.removeChangeListener (this);
}

void CalibrationEditor::setProjectorSize (juce::Point<int> size)
{
    canvas->setProjectorSize (size);
}

void CalibrationEditor::setLayout (const KeyboardLayout& layout)
{
    canvas->setLayout (layout);
}

void CalibrationEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshFromModel();
}

void CalibrationEditor::refreshFromModel()
{
    const auto calibration = model.getCalibration();

    linkToggle.setToggleState (calibration.linkPlanes, juce::dontSendNotification);
    blackOffsetLeftSlider.setValue (calibration.blackKeyOffsetXLeft, juce::dontSendNotification);
    blackOffsetRightSlider.setValue (calibration.blackKeyOffsetXRight, juce::dontSendNotification);
    blackOffsetYSlider.setValue (calibration.blackKeyOffsetY, juce::dontSendNotification);
    blackLengthSlider.setValue (calibration.blackKeyLength, juce::dontSendNotification);
    brightnessSlider.setValue (calibration.brightness, juce::dontSendNotification);
    gammaSlider.setValue (calibration.gamma, juce::dontSendNotification);

    canvas->repaint();
}

void CalibrationEditor::applyBlackKeys()
{
    model.setBlackKeys ((float) blackOffsetLeftSlider.getValue(), (float) blackOffsetRightSlider.getValue(),
                        (float) blackOffsetYSlider.getValue(),
                        (float) blackLengthSlider.getValue());
}

void CalibrationEditor::applyOutput()
{
    model.setOutput ((float) brightnessSlider.getValue(), (float) gammaSlider.getValue());
}

juce::File CalibrationEditor::getProfileFile (const juce::String& name) const
{
    return profilesFolder.getChildFile (juce::File::createLegalFileName (name.trim()) + ".json");
}

void CalibrationEditor::refreshProfiles()
{
    profileBox.clear (juce::dontSendNotification);

    auto files = profilesFolder.findChildFiles (juce::File::findFiles, false, "*.json");
    files.sort();

    for (int i = 0; i < files.size(); ++i)
        profileBox.addItem (files[i].getFileNameWithoutExtension(), i + 1);
}

void CalibrationEditor::loadProfile (const juce::String& name)
{
    if (name.isEmpty())
        return;

    const auto parsed = juce::JSON::parse (getProfileFile (name));

    if (parsed.isObject())
        model.setCalibration (ProjectionCalibration::fromVar (parsed));
}

void CalibrationEditor::askProfileName()
{
    nameDialog = std::make_unique<juce::AlertWindow> ("Save profile", "Name for this calibration:",
                                                      juce::MessageBoxIconType::NoIcon, this);
    nameDialog->addTextEditor ("name", profileBox.getText().isNotEmpty() ? profileBox.getText() : "My piano");
    nameDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    nameDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    nameDialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result)
    {
        const auto name = nameDialog->getTextEditorContents ("name").trim();
        nameDialog->setVisible (false);

        if (result == 1 && name.isNotEmpty())
            saveProfile (name);
    }), false);
}

void CalibrationEditor::saveProfile (const juce::String& name)
{
    profilesFolder.createDirectory();

    const auto file = getProfileFile (name);

    if (file.replaceWithText (juce::JSON::toString (model.getCalibration().toVar())))
    {
        refreshProfiles();

        for (int i = 0; i < profileBox.getNumItems(); ++i)
            if (profileBox.getItemText (i) == file.getFileNameWithoutExtension())
                profileBox.setSelectedItemIndex (i, juce::dontSendNotification);
    }
}

void CalibrationEditor::deleteSelectedProfile()
{
    const auto name = profileBox.getText();

    if (name.isEmpty())
        return;

    getProfileFile (name).deleteFile();
    refreshProfiles();
}

void CalibrationEditor::visibilityChanged()
{
    // 開いたらすぐに矢印キーで四隅を動かせるようにする
    if (isVisible())
        juce::MessageManager::callAsync ([safeCanvas = juce::Component::SafePointer<juce::Component> (canvas.get())]
        {
            if (safeCanvas != nullptr && safeCanvas->isShowing())
                safeCanvas->grabKeyboardFocus();
        });
}

bool CalibrationEditor::keyPressed (const juce::KeyPress& key)
{
    // 一覧などにフォーカスがあるときも、矢印キーなどは四隅の操作に回す
    return canvas->keyPressed (key);
}

void CalibrationEditor::paint (juce::Graphics& g)
{
    using namespace Theme::Colours;

    g.fillAll (juce::Colour (0xff11141b));

    for (const auto& label : labels)
    {
        g.setColour (label.isSection ? textMuted : textSecondary);
        g.setFont (label.isSection ? Theme::uiFont (11.0f, 600) : Theme::uiFont (13.0f));
        g.drawText (label.text, label.bounds, juce::Justification::centredLeft, false);
    }

    g.setColour (divider);
    g.fillRect (0, getHeight() - footerHeight, getWidth(), 1);

    g.setColour (textMuted);
    g.setFont (Theme::uiFont (12.0f));
    g.drawText ("Drag corners 1-6 onto the real keys (Shift: fine). Wheel: zoom, drag background: pan. "
                "Arrows: nudge (Shift: 10 px), Tab: next corner.",
                getLocalBounds().removeFromBottom (footerHeight).reduced (padding, 0).withTrimmedRight (120),
                juce::Justification::centredLeft, true);
}

void CalibrationEditor::resized()
{
    labels.clear();

    auto area = getLocalBounds();
    const auto footer = area.removeFromBottom (footerHeight).reduced (padding, 14);
    doneButton.setBounds (footer.withLeft (footer.getRight() - 96));

    auto side = area.removeFromRight (sidePanelWidth).reduced (padding);
    canvas->setBounds (area.reduced (padding).withTrimmedRight (0));

    const auto section = [&] (const juce::String& title)
    {
        labels.push_back ({ title, side.removeFromTop (20), true });
        side.removeFromTop (6);
    };

    const auto row = [&]
    {
        auto bounds = side.removeFromTop (rowHeight);
        side.removeFromTop (rowGap);
        return bounds;
    };

    const auto labelledRow = [&] (const juce::String& label)
    {
        auto bounds = row();
        labels.push_back ({ label, bounds.removeFromLeft (84), false });
        return bounds;
    };

    const auto pair = [&] (juce::Component& left, juce::Component& right)
    {
        auto bounds = row();
        left.setBounds (bounds.removeFromLeft ((bounds.getWidth() - 8) / 2));
        bounds.removeFromLeft (8);
        right.setBounds (bounds);
    };

    section ("CORNERS");
    linkToggle.setBounds (row());
    pair (flipHorizontalButton, flipVerticalButton);
    pair (rotateButton, resetButton);
    pair (squareFallButton, squareKeysButton);

    side.removeFromTop (sectionGap - rowGap);
    section ("BLACK KEYS");
    blackOffsetLeftSlider.setBounds (labelledRow ("Left end"));
    blackOffsetRightSlider.setBounds (labelledRow ("Right end"));
    blackOffsetYSlider.setBounds (labelledRow ("Back/Front"));
    blackLengthSlider.setBounds (labelledRow ("Length"));

    side.removeFromTop (sectionGap - rowGap);
    section ("OUTPUT");
    brightnessSlider.setBounds (labelledRow ("Brightness"));
    gammaSlider.setBounds (labelledRow ("Gamma"));

    side.removeFromTop (sectionGap - rowGap);
    section ("PROFILES");
    profileBox.setBounds (row());
    pair (saveProfileButton, deleteProfileButton);
}

} // namespace pianolume
