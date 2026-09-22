/*  The standalone app: the rack, with a router above it.

    JUCE's standalone holder still runs the plugin, its audio device and its saved state; what is ours
    is the window around it:
      - the routing bar (RouterBar.h), which moves the computer's audio - the whole system, or chosen
        apps - into the rack and back out again,
      - on Windows, the "Windows system audio (loopback)" device type (SystemAudioDevice.h), which is
        how the rack hears what a playback device is playing,
      - the watchdog: the same executable started with --enh-restore-watch <pid> <journal>, which puts
        the audio routing back if this copy dies with the rack in.

    JUCE builds this instead of its own when JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1 (set on the
    Standalone target in CMakeLists.txt).
*/

// Ours first: JUCE's plugin-client headers below #define Component, which breaks juce::Component after them.
#include "AudioRouting.h"
#include "RouterBar.h"
#include "SystemAudioDevice.h"

#include <juce_audio_plugin_client/detail/juce_CheckSettingMacros.h>
#include <juce_audio_plugin_client/detail/juce_IncludeSystemHeaders.h>
#include <juce_audio_plugin_client/detail/juce_IncludeModuleHeaders.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace juce
{
    /** The router bar on top, the plugin's own editor below. The editor decides the width; the window
        is resizable and hands every change on to it. */
    class EnhRouterContent final : public Component,
                                   private ComponentListener
    {
    public:
        explicit EnhRouterContent (StandalonePluginHolder& h)
            : holder (h),
              bar (h.deviceManager,
                   [&h] (bool muted)
                   {
                       h.muteInput = muted;                 // now, for the audio thread
                       h.getMuteInputValue() = muted;       // and for the holder's saved settings
                   },
                   h.settings.get(),
                   [&h] { h.showAudioSettingsDialog(); })
        {
            addAndMakeVisible (bar);

            if (auto* e = h.processor->createEditorAndMakeActive())
            {
                editor.reset (e);
                addAndMakeVisible (*editor);
                editor->addComponentListener (this);
            }

            const auto editorSize = editor != nullptr ? editor->getBounds() : Rectangle<int> (1000, 740);
            setSize (editorSize.getWidth(), editorSize.getHeight() + pad::RouterBar::preferredHeight);
        }

        ~EnhRouterContent() override
        {
            bar.removeRack();

            if (editor != nullptr)
            {
                editor->removeComponentListener (this);
                holder.processor->editorBeingDeleted (editor.get());
                editor = nullptr;
            }
        }

        pad::RouterBar& getBar() { return bar; }

        void resized() override
        {
            auto r = getLocalBounds();
            bar.setBounds (r.removeFromTop (pad::RouterBar::preferredHeight));

            if (editor != nullptr)
            {
                const ScopedValueSetter<bool> s (layingOut, true);
                editor->setBounds (r);
            }
        }

    private:
        void componentMovedOrResized (Component&, bool, bool wasResized) override
        {
            // The editor asked for a new size itself: grow the window to fit.
            if (wasResized && ! layingOut && editor != nullptr)
                setSize (editor->getWidth(), editor->getHeight() + pad::RouterBar::preferredHeight);
        }

        StandalonePluginHolder& holder;
        pad::RouterBar bar;
        std::unique_ptr<AudioProcessorEditor> editor;
        bool layingOut = false;
    };

    //==============================================================================
    class EnhRouterWindow final : public DocumentWindow
    {
    public:
        EnhRouterWindow (const String& title, PropertySet* settings)
            : DocumentWindow (title, Colour (0xff080607), DocumentWindow::allButtons)
        {
            holder = std::make_unique<StandalonePluginHolder> (settings, false);

            if (auto type = pad::createSystemAudioDeviceType())
            {
                holder->deviceManager.addAudioDeviceType (std::move (type));

                // The holder set the audio up in its constructor, before that type existed, so a
                // saved choice of it would not have been found. Now it is there, ask again.
                holder->reloadAudioDeviceState (true, {}, nullptr);
            }

            // Nothing reaches the rack until the router has put it in a safe place.
            holder->muteInput = true;
            holder->getMuteInputValue() = true;

            setUsingNativeTitleBar (true);
            setContentOwned (new EnhRouterContent (*holder), true);
            setResizable (true, false);
            setResizeLimits (560, 320 + pad::RouterBar::preferredHeight, 2560, 1600 + pad::RouterBar::preferredHeight);

            const auto x = settings != nullptr ? settings->getIntValue ("windowX", -100) : -100;
            const auto y = settings != nullptr ? settings->getIntValue ("windowY", -100) : -100;

            if (x != -100 && y != -100)
                setTopLeftPosition (x, y);
            else
                centreWithSize (getWidth(), getHeight());
        }

        ~EnhRouterWindow() override
        {
            if (auto* props = holder->settings.get())
            {
                props->setValue ("windowX", getX());
                props->setValue ("windowY", getY());
            }

            clearContentComponent();   // the router puts the audio back here
            holder->stopPlaying();
            holder = nullptr;
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

        StandalonePluginHolder* getHolder() const { return holder.get(); }

    private:
        std::unique_ptr<StandalonePluginHolder> holder;
        TooltipWindow tooltips { nullptr, 600 };
    };

    //==============================================================================
    class EnhStandaloneApp final : public JUCEApplication
    {
    public:
        EnhStandaloneApp()
        {
            PropertiesFile::Options options;

            options.applicationName     = CharPointer_UTF8 (JucePlugin_Name);
            options.filenameSuffix      = ".settings";
            options.osxLibrarySubFolder = "Application Support";
           #if JUCE_LINUX || JUCE_BSD
            options.folderName          = "~/.config";
           #else
            options.folderName          = "";
           #endif

            appProperties.setStorageParameters (options);
        }

        const String getApplicationName() override           { return CharPointer_UTF8 (JucePlugin_Name); }
        const String getApplicationVersion() override        { return JucePlugin_VersionString; }
        bool moreThanOneInstanceAllowed() override           { return true; }
        void anotherInstanceStarted (const String&) override {}

        void initialise (const String& commandLine) override
        {
            // The watchdog: no window, no audio, just wait and (maybe) put the routing back.
            auto args = StringArray::fromTokens (commandLine, true);
            if (auto i = args.indexOf (pad::routing::watchdogArgument); i >= 0)
            {
                const auto pid = args[i + 1].getIntValue();
                const auto journal = File (args[i + 2].unquoted());

               #if JUCE_LINUX || JUCE_BSD
                setsid();   // out of the terminal's process group: a Ctrl+C meant for the app must not stop us too
               #endif

                Thread::launch ([pid, journal]
                {
                    pad::routing::runWatchdog (pid, journal);
                    MessageManager::callAsync ([] { JUCEApplicationBase::quit(); });
                });
                return;
            }

            if (Desktop::getInstance().getDisplays().displays.isEmpty())
            {
                jassertfalse;   // no displays: nothing to show
                return;
            }

            mainWindow = std::make_unique<EnhRouterWindow> (getApplicationName(), appProperties.getUserSettings());
            mainWindow->setVisible (true);
        }

        void shutdown() override
        {
            mainWindow = nullptr;
            appProperties.saveIfNeeded();
        }

        void systemRequestedQuit() override
        {
            if (mainWindow != nullptr)
                if (auto* holder = mainWindow->getHolder())
                    holder->savePluginState();

            if (ModalComponentManager::getInstance()->cancelAllModalComponents())
            {
                Timer::callAfterDelay (100, []
                {
                    if (auto app = JUCEApplicationBase::getInstance())
                        app->systemRequestedQuit();
                });
            }
            else
            {
                quit();
            }
        }

    private:
        ApplicationProperties appProperties;
        std::unique_ptr<EnhRouterWindow> mainWindow;
    };
}

juce::JUCEApplicationBase* juce_CreateApplication();
juce::JUCEApplicationBase* juce_CreateApplication() { return new juce::EnhStandaloneApp(); }
