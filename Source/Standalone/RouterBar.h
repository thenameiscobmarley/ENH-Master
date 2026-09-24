#pragma once

#include <functional>

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "AudioRouting.h"

namespace pad
{
    /** The strip above the rack in the standalone app: what goes into the rack, where the rack's
        input is, where you listen, and the INSERT / REMOVE button that reroutes and puts it all back.

            SOURCE          RACK INPUT                 LISTEN ON
            [Whole system]  [ENH Master rack input]    [Headphones]        [ INSERT RACK ]  [...]
            Rack out. Your audio plays as it did.

        INSERT, in order: open the rack's audio (capture the rack input, play to the listening device),
        write down how things are (the journal), then move the audio - the default device, or the
        chosen apps - onto the rack input, then let the rack hear it. REMOVE runs that backwards.
        Nothing is rerouted unless the rack's audio came up first, so a failure never leaves you
        without sound. */
    class RouterBar final : public juce::Component,
                            private juce::Timer
    {
    public:
        RouterBar (juce::AudioDeviceManager& deviceManager,
                   std::function<void (bool)> setInputMuted,
                   juce::PropertySet* settings,
                   std::function<void()> showAudioSettings,
                   juce::AudioProcessor* rack = nullptr);
        ~RouterBar() override;

        static constexpr int preferredHeight = 66;
        static constexpr int guideHeight = 76;    // the getting-started strip under the controls, while it is open

        /** How tall the bar is now (taller while the getting-started strip is open). */
        int currentHeight() const noexcept { return preferredHeight + (guideOpen ? guideHeight : 0); }
        std::function<void()> onHeightChanged;   // the window lays itself out again

        /** "Start with the computer": an autostart entry that opens ENH Master in the tray with the
            rack in (Linux: ~/.config/autostart, Windows: the Run key). */
        static bool isStartingWithComputer();
        static bool setStartingWithComputer (bool shouldStart);
        static constexpr const char* backgroundArgument = "--background";

        /** Puts everything back. Called before the app quits (the destructor does it too). */
        void removeRack();

        /** For the automatic insert at start-up and the dev hook. */
        void insertRack();

        bool isInserted() const { return router.isInserted(); }

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        class Look;
        class RefreshingCombo;

        void timerCallback() override;
        void refreshLists();
        void refreshAppsButton();
        void showAppsMenu();
        void showMoreMenu();
        void showGuide();                  // the getting-started steps (the ? button; once on the first run)
        juce::String nextStepHint() const; // what to do next while the rack is out
        void updateControls();
        void setStatus (const juce::String& text, bool isProblem = false);
        void saveChoices();

        routing::Plan currentPlan() const;
        juce::String nameOfEndpoint (const juce::String& id) const;
        bool openRackAudio (const juce::String& rackInputId, const juce::String& listenId, juce::String& errorOut);

        juce::AudioDeviceManager& devices;
        std::function<void (bool)> setMuted;   // must take effect at once, not on the next message
        juce::PropertySet* props;
        std::function<void()> openAudioSettings;

        std::unique_ptr<routing::Backend> backend;
        routing::Router router;
        std::vector<std::unique_ptr<juce::ChildProcess>> watchdogs;

        std::vector<routing::Endpoint> endpoints;
        juce::StringArray rackInputIds, listenIds;   // parallel to the combo items
        juce::StringArray chosenApps;
        juce::String testListen;                     // PAD_ROUTER_TEST_LISTEN
        juce::String rackInputInUse;                 // while inserted

        std::unique_ptr<Look> look;
        std::unique_ptr<RefreshingCombo> sourceBox, rackInputBox, listenBox, levelBox;
        juce::RangedAudioParameter* targetParam = nullptr;   // the rack's LOUDNESS TARGET
        juce::RangedAudioParameter* compareParam = nullptr;  // the rack's COMPARE (level-matched A/B)
        float inLevel = 0.0f, outLevel = 0.0f;
        juce::AudioDeviceManager::LevelMeter::Ptr inMeter, outMeter;   // held: the device only measures while someone holds them
        juce::TextButton insertButton, appsButton, moreButton, compareButton, helpButton;
        juce::TextButton gotItButton;
        bool guideOpen = false;
        void closeGuide();
        juce::String status;
        bool statusIsProblem = false;
        float lamp = 0.0f;
        int tick = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RouterBar)
    };
}
