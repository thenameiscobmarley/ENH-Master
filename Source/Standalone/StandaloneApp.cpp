/*  The standalone app.

    This is JUCE's own standalone app (juce_audio_plugin_client_Standalone.cpp) with one addition:
    the "Windows system audio (loopback)" device type is added to the device manager, so the app can
    take what the computer is playing as its input instead of a microphone. See SystemAudioDevice.h.

    JUCE builds this instead of its own when JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1 (set on the
    Standalone target in CMakeLists.txt).
*/

#include <juce_audio_plugin_client/detail/juce_CheckSettingMacros.h>
#include <juce_audio_plugin_client/detail/juce_IncludeSystemHeaders.h>
#include <juce_audio_plugin_client/detail/juce_IncludeModuleHeaders.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#include "SystemAudioDevice.h"

namespace juce
{
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

        void initialise (const String&) override
        {
            if (Desktop::getInstance().getDisplays().displays.isEmpty())
            {
                jassertfalse;   // no displays: nothing to show
                return;
            }

            mainWindow = std::make_unique<StandaloneFilterWindow> (getApplicationName(),
                                                                   LookAndFeel::getDefaultLookAndFeel()
                                                                       .findColour (ResizableWindow::backgroundColourId),
                                                                   appProperties.getUserSettings(),
                                                                   false);

            if (auto* holder = mainWindow->pluginHolder.get())
            {
                if (auto type = pad::createSystemAudioDeviceType())
                {
                    holder->deviceManager.addAudioDeviceType (std::move (type));

                    // The holder set the audio up in its constructor, before that type existed, so a
                    // saved choice of it would not have been found. Now it is there, ask again.
                    holder->reloadAudioDeviceState (true, {}, nullptr);
                }
            }

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
                mainWindow->pluginHolder->savePluginState();

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
        std::unique_ptr<StandaloneFilterWindow> mainWindow;
    };
}

juce::JUCEApplicationBase* juce_CreateApplication();
juce::JUCEApplicationBase* juce_CreateApplication() { return new juce::EnhStandaloneApp(); }
