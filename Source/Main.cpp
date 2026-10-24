#include <juce_gui_basics/juce_gui_basics.h>
#include "MainComponent.h"
#include "Platform/WindowsTitleBar.h"
#include "UI/PianolumeLookAndFeel.h"
#include "UI/Theme.h"

namespace pianolume
{

class PianolumeApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override           { return false; }

    void initialise (const juce::String&) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        tooltipWindow = std::make_unique<juce::TooltipWindow> (nullptr, 600);
        mainWindow = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        tooltipWindow = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name, Theme::Colours::background, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, true);
            setResizeLimits (1100, 680, 10000, 10000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);

            if (auto* peer = getPeer())
                applyDarkTitleBar (peer->getNativeHandle());
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    PianolumeLookAndFeel lookAndFeel;
    std::unique_ptr<juce::TooltipWindow> tooltipWindow;
    std::unique_ptr<MainWindow> mainWindow;
};

} // namespace pianolume

START_JUCE_APPLICATION (pianolume::PianolumeApplication)
