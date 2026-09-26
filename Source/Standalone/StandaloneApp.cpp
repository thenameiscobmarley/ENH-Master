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

// Order matters twice over. JUCE's modules first: the plugin-client system headers below bring in
// <windows.h>, whose `small` macro breaks juce_gui_extra if it is parsed after them. Then ours: the
// plugin-client module header #defines Component, which breaks juce::Component after it.
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_processors/juce_audio_processors.h>

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
                   [&h] { h.showAudioSettingsDialog(); },
                   h.processor.get())
        {
            addAndMakeVisible (bar);
            // The bar grows while its getting-started strip is open: the rack gives it the room
            bar.onHeightChanged = [this] { resized(); };

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
            bar.setBounds (r.removeFromTop (bar.currentHeight()));

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
    /** The rack's tray icon, while the window is closed with the rack in: click to bring the window
        back, right-click for a menu. */
    class EnhTrayIcon final : public SystemTrayIconComponent
    {
    public:
        std::function<void()> onShow;
        std::function<void (PopupMenu&)> fillMenu;

        EnhTrayIcon()
        {
            const auto icon = drawIcon();
            setIconImage (icon, icon);
            setIconTooltip ("ENH Master");
        }

        void mouseDown (const MouseEvent& e) override
        {
            if (e.mods.isPopupMenu() && fillMenu)
            {
                PopupMenu m;
                fillMenu (m);
                m.showMenuAsync (PopupMenu::Options());
            }
            else if (onShow)
            {
                onShow();
            }
        }

    private:
        /** Three rack units in a dark case, the middle one lit: drawn here, so no image file is needed. */
        static Image drawIcon()
        {
            Image img (Image::ARGB, 64, 64, true);
            Graphics g (img);
            g.setColour (Colour (0xff17130f));
            g.fillRoundedRectangle (4.0f, 4.0f, 56.0f, 56.0f, 8.0f);
            for (int u = 0; u < 3; ++u)
            {
                const auto y = 13.0f + (float) u * 14.0f;
                g.setColour (u == 1 ? Colour (0xffe0a84a) : Colour (0xff8a8279));
                g.fillRect (12.0f, y, 40.0f, 9.0f);
            }
            return img;
        }
    };

    /** One router at a time: starting ENH Master again shows the window that is already running (which
        may be hidden in the tray) instead of a second copy moving the same audio about. The watchdog
        is a second copy on purpose and never gets here. */
    class EnhInstanceLink final : public InterprocessConnection
    {
    public:
        explicit EnhInstanceLink (bool callbacksOnMessageThread = true)
            : InterprocessConnection (callbacksOnMessageThread) {}

        std::function<void()> onShowRequested;
        bool isServer = false;
        WaitableEvent answered;   // the probe's: the running copy said it heard

        static String pipeName() { return "ENH-Master-router-" + SystemStats::getLogonName(); }

        void connectionMade() override {}

        void connectionLost() override
        {
            // A pipe serves one caller: open it again for the next one
            if (isServer)
                MessageManager::callAsync ([this] { createPipe (pipeName(), -1, false); });
        }

        void messageReceived (const MemoryBlock& m) override
        {
            if (m.toString() == "show")
            {
                sendMessage (MemoryBlock ("ok", 2));
                if (onShowRequested)
                    onShowRequested();
            }
            else if (m.toString() == "ok")
            {
                answered.signal();
            }
        }
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

            // Nobody chose a rate yet: 48 kHz and a 256-sample buffer (the ALSA default device lists 8 kHz
            // first and JUCE would open it there - poor sound and a long delay)
            if (settings == nullptr || ! settings->containsKey ("audioSetup"))
                if (auto* device = holder->deviceManager.getCurrentAudioDevice())
                {
                    auto setup = holder->deviceManager.getAudioDeviceSetup();
                    const auto rates = device->getAvailableSampleRates();
                    const auto sizes = device->getAvailableBufferSizes();
                    bool change = false;
                    if (setup.sampleRate < 44100.0 && (rates.contains (48000.0) || rates.contains (44100.0)))
                    {
                        setup.sampleRate = rates.contains (48000.0) ? 48000.0 : 44100.0;
                        change = true;
                    }
                    if (setup.bufferSize > 256 && sizes.contains (256))
                    {
                        setup.bufferSize = 256;
                        change = true;
                    }
                    if (change)
                        holder->deviceManager.setAudioDeviceSetup (setup, true);
                }

            // Nothing reaches the rack until the router has put it in a safe place.
            holder->muteInput = true;
            holder->getMuteInputValue() = true;

            setUsingNativeTitleBar (true);
            content = new EnhRouterContent (*holder);
            setContentOwned (content, true);
            setResizable (true, false);
            setResizeLimits (560, 320 + pad::RouterBar::preferredHeight, 2560, 1600 + pad::RouterBar::preferredHeight);

            const auto x = settings != nullptr ? settings->getIntValue ("windowX", -100) : -100;
            const auto y = settings != nullptr ? settings->getIntValue ("windowY", -100) : -100;

            if (x != -100 && y != -100)
                setTopLeftPosition (x, y);
            else
                centreWithSize (getWidth(), getHeight());

            // Dev-only: PAD_UI_TEST_CLOSE=<seconds> presses the close button then
            if (const auto closeAt = SystemStats::getEnvironmentVariable ("PAD_UI_TEST_CLOSE", {}).getDoubleValue(); closeAt > 0.0)
                Timer::callAfterDelay ((int) (closeAt * 1000.0), [safe = Component::SafePointer<EnhRouterWindow> (this)]
                {
                    if (safe != nullptr)
                        safe->closeButtonPressed();
                });
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

        /** Closing with the rack in keeps it in: the window goes (and with it every frame the rack was
            drawing) and a tray icon stays. Closing with the rack out quits. */
        void closeButtonPressed() override
        {
            auto* props = holder->settings.get();
            const bool toTray = props == nullptr || props->getBoolValue ("router.closeToTray", true);

            if (toTray && content != nullptr && content->getBar().isInserted())
                hideToTray();
            else
                JUCEApplication::getInstance()->systemRequestedQuit();
        }

        void hideToTray()
        {
            if (tray == nullptr)
            {
                tray = std::make_unique<EnhTrayIcon>();
                tray->onShow = [this] { bringBack(); };
                tray->fillMenu = [this] (PopupMenu& m)
                {
                    const bool in = content != nullptr && content->getBar().isInserted();
                    m.addItem ("Show ENH Master", [this] { bringBack(); });
                    m.addItem ("Remove the rack", in, false, [this] { if (content != nullptr) content->getBar().removeRack(); updateTray(); });
                    m.addSeparator();
                    m.addItem ("Quit (puts your audio back)", [] { JUCEApplication::getInstance()->systemRequestedQuit(); });
                };
            }

            updateTray();
            setVisible (false);
        }

        void bringBack()
        {
            setVisible (true);
            setMinimised (false);
            toFront (true);
            tray = nullptr;
        }

        StandalonePluginHolder* getHolder() const { return holder.get(); }
        bool isRackIn() const { return content != nullptr && content->getBar().isInserted(); }

    private:
        void updateTray()
        {
            if (tray != nullptr)
                tray->setIconTooltip (content != nullptr && content->getBar().isInserted()
                                          ? "ENH Master: rack in. Click to show."
                                          : "ENH Master: rack out. Click to show.");
        }

        std::unique_ptr<StandalonePluginHolder> holder;
        EnhRouterContent* content = nullptr;   // owned by the window
        std::unique_ptr<EnhTrayIcon> tray;
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

            // Already running (perhaps hidden in the tray)? Show that one and leave. It has to answer:
            // a copy that crashed leaves its pipe behind, and that must not stop this one starting.
            {
                EnhInstanceLink probe (false);
                if (probe.connectToPipe (EnhInstanceLink::pipeName(), 1000)
                     && probe.sendMessage (MemoryBlock ("show", 4))
                     && probe.answered.wait (1500))
                {
                    probe.disconnect();
                    quit();
                    return;
                }
                probe.disconnect();
            }

            instanceLink = std::make_unique<EnhInstanceLink>();
            instanceLink->isServer = true;
            instanceLink->onShowRequested = [this] { if (mainWindow != nullptr) mainWindow->bringBack(); };
            instanceLink->createPipe (EnhInstanceLink::pipeName(), -1, false);

            mainWindow = std::make_unique<EnhRouterWindow> (getApplicationName(), appProperties.getUserSettings());

            // Started with the computer: straight to the tray once the rack is in; if it did not go in,
            // show the window so the reason can be read.
            if (args.contains (pad::RouterBar::backgroundArgument))
            {
                Timer::callAfterDelay (2500, [this]
                {
                    if (mainWindow != nullptr)
                    {
                        if (mainWindow->isRackIn())
                            mainWindow->hideToTray();
                        else
                            mainWindow->setVisible (true);
                    }
                });
            }
            else
            {
                mainWindow->setVisible (true);
            }
        }

        void shutdown() override
        {
            if (instanceLink != nullptr)
            {
                instanceLink->isServer = false;
                instanceLink->disconnect();
                instanceLink = nullptr;
            }
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
        std::unique_ptr<EnhInstanceLink> instanceLink;
        std::unique_ptr<EnhRouterWindow> mainWindow;
    };
}

juce::JUCEApplicationBase* juce_CreateApplication();
juce::JUCEApplicationBase* juce_CreateApplication() { return new juce::EnhStandaloneApp(); }
