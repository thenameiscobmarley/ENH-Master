#include "RouterBar.h"

#if JUCE_WINDOWS
 #include <juce_core/juce_core.h>
#endif

namespace pad
{
    namespace
    {
        const juce::Colour ink        { 0xfff2efe9 };
        const juce::Colour faintInk   { 0x99f2efe9 };
        const juce::Colour hairline   { 0x24ffffff };
        const juce::Colour fieldFill  { 0xff17151a };
        const juce::Colour barTop     { 0xff151317 };
        const juce::Colour barBottom  { 0xff0c0b0d };
        const juce::Colour amber      { 0xffe0a84a };
        const juce::Colour problem    { 0xffe38b6f };

        constexpr const char* loopbackTypeName = "Windows system audio (loopback)";
        constexpr const char* loopbackInputSuffix = " (what it is playing)";

        juce::Font uiFont (float height, bool bold = false)
        {
            auto o = juce::FontOptions().withHeight (height);
            return juce::Font (bold ? o.withStyle ("Bold") : o);
        }

        bool looksLikeVirtualCable (const juce::String& name)
        {
            for (auto* hint : { "cable", "virtual", "voicemeeter", "vb-audio", "loopback" })
                if (name.containsIgnoreCase (hint))
                    return true;
            return false;
        }
    }

    //==============================================================================
    /** Square, dark, white text: the same quiet look as the rack's glass panels. */
    class RouterBar::Look final : public juce::LookAndFeel_V4
    {
    public:
        Look()
        {
            setColour (juce::ComboBox::textColourId, ink);
            setColour (juce::ComboBox::backgroundColourId, fieldFill);
            setColour (juce::ComboBox::outlineColourId, hairline);
            setColour (juce::ComboBox::arrowColourId, faintInk);
            setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xf0141216));
            setColour (juce::PopupMenu::textColourId, ink);
            setColour (juce::PopupMenu::headerTextColourId, faintInk);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0x22ffffff));
            setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
            setColour (juce::TextButton::textColourOffId, ink);
            setColour (juce::TextButton::textColourOnId, juce::Colour (0xff17130c));
        }

        void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
        {
            auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height);
            g.setColour (fieldFill.withMultipliedAlpha (box.isEnabled() ? 1.0f : 0.5f));
            g.fillRect (r);
            g.setColour (box.isMouseOver (true) && box.isEnabled() ? hairline.withAlpha (0.4f) : hairline);
            g.drawRect (r, 1.0f);

            // A small chevron.
            auto a = r.removeFromRight (18.0f).withSizeKeepingCentre (7.0f, 4.0f);
            juce::Path p;
            p.startNewSubPath (a.getX(), a.getY());
            p.lineTo (a.getCentreX(), a.getBottom());
            p.lineTo (a.getRight(), a.getY());
            g.setColour (faintInk.withMultipliedAlpha (box.isEnabled() ? 1.0f : 0.4f));
            g.strokePath (p, juce::PathStrokeType (1.2f));
        }

        juce::Font getComboBoxFont (juce::ComboBox&) override { return uiFont (12.5f); }

        void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
        {
            label.setBounds (6, 0, box.getWidth() - 24, box.getHeight());
            label.setFont (getComboBoxFont (box));
        }

        juce::Font getPopupMenuFont() override { return uiFont (13.0f); }

        void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
        {
            g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
            g.setColour (hairline);
            g.drawRect (0, 0, width, height);
        }

        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
        {
            auto r = b.getLocalBounds().toFloat();
            const bool on = b.getToggleState();

            if (on)
            {
                g.setGradientFill (juce::ColourGradient (amber.brighter (0.15f), 0, r.getY(), amber.darker (0.25f), 0, r.getBottom(), false));
                g.fillRect (r);
            }
            else
            {
                g.setColour (juce::Colours::white.withAlpha (down ? 0.16f : over ? 0.10f : 0.06f));
                g.fillRect (r);
            }

            g.setColour (on ? amber.darker (0.5f) : hairline.withAlpha (over ? 0.4f : 0.2f));
            g.drawRect (r, 1.0f);
        }

        juce::Font getTextButtonFont (juce::TextButton& b, int) override { return uiFont (b.getComponentID() == "insert" ? 12.5f : 12.0f, b.getComponentID() == "insert"); }
    };

    /** A combo box that re-reads the devices just before it opens, so the list is never stale. */
    class RouterBar::RefreshingCombo final : public juce::ComboBox
    {
    public:
        std::function<void()> beforePopup;

        void showPopup() override
        {
            if (beforePopup)
                beforePopup();
            ComboBox::showPopup();
        }
    };

    //==============================================================================
    RouterBar::RouterBar (juce::AudioDeviceManager& deviceManager, std::function<void (bool)> setInputMuted,
                          juce::PropertySet* settings, std::function<void()> showAudioSettings,
                          juce::AudioProcessor* rack)
        : devices (deviceManager),
          setMuted (std::move (setInputMuted)),
          props (settings),
          openAudioSettings (std::move (showAudioSettings)),
          backend (routing::createBackend()),
          router (*backend, routing::defaultJournalFile()),
          look (std::make_unique<Look>()),
          sourceBox (std::make_unique<RefreshingCombo>()),
          rackInputBox (std::make_unique<RefreshingCombo>()),
          listenBox (std::make_unique<RefreshingCombo>()),
          levelBox (std::make_unique<RefreshingCombo>())
    {
        setLookAndFeel (look.get());
        setOpaque (true);

        sourceBox->addItem ("Whole system", 1);
        sourceBox->addItem ("Chosen apps", 2);
        sourceBox->setTooltip ("Whole system: everything that plays to the default device goes through the rack.\n"
                               "Chosen apps: only the apps you tick (the game, Discord...) go through it.");

        rackInputBox->setTooltip ("The device the rack listens to. Your audio is moved onto it, captured, processed, "
                                  "and played to LISTEN ON. Nobody should be listening to this device directly.");
        listenBox->setTooltip ("Where the processed audio plays: your headset or speakers.");

        for (auto* c : { sourceBox.get(), rackInputBox.get(), listenBox.get() })
        {
            addAndMakeVisible (*c);
            c->beforePopup = [this] { if (! router.isInserted()) refreshLists(); };
            c->onChange = [this] { saveChoices(); updateControls(); };
        }

        // LEVEL: the rack's LOUDNESS TARGET (OUTPUT MONITOR's panel), here too because it is what a
        // router wants most: every app and game at one loudness
        if (rack != nullptr)
            for (auto* p : rack->getParameters())
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p); ranged != nullptr && ranged->getParameterID() == "outputTarget")
                    targetParam = ranged;

        levelBox->addItem ("As it is", 1);
        levelBox->addItem ("-23 LUFS (quiet)", 2);
        levelBox->addItem ("-18 LUFS (even)", 3);
        levelBox->addItem ("-14 LUFS (loud)", 4);
        levelBox->setTooltip ("LOUDNESS TARGET: bring everything that goes through the rack to one loudness, so games, music "
                              "and calls come out equally loud. Moves slowly and holds through explosions and pauses.");
        levelBox->onChange = [this]
        {
            if (targetParam != nullptr)
                targetParam->setValueNotifyingHost (targetParam->convertTo0to1 ((float) (levelBox->getSelectedId() - 1)));
        };
        levelBox->setVisible (targetParam != nullptr);
        addChildComponent (*levelBox);

        insertButton.setComponentID ("insert");
        insertButton.setClickingTogglesState (false);
        insertButton.onClick = [this] { router.isInserted() ? removeRack() : insertRack(); };
        addAndMakeVisible (insertButton);

        appsButton.onClick = [this] { showAppsMenu(); };
        appsButton.setTooltip ("Pick the apps that go through the rack.");
        addAndMakeVisible (appsButton);

        moreButton.setButtonText (juce::String::fromUTF8 ("\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2"));
        moreButton.onClick = [this] { showMoreMenu(); };
        addAndMakeVisible (moreButton);

        if (props != nullptr)
        {
            sourceBox->setSelectedId (props->getIntValue ("router.source", 1), juce::dontSendNotification);
            chosenApps.addTokens (props->getValue ("router.apps"), "\n", {});
            chosenApps.removeEmptyStrings();
        }
        else
        {
            sourceBox->setSelectedId (1, juce::dontSendNotification);
        }

        // Dev hooks, so the app can be tested without touching the saved choices:
        // PAD_ROUTER_TEST_SOURCE=system|apps, PAD_ROUTER_TEST_APPS=key,key, PAD_ROUTER_TEST_LISTEN=<device id>
        // (with PAD_ROUTER_TEST_INSERT=1 to insert at start).
        const auto testSource = juce::SystemStats::getEnvironmentVariable ("PAD_ROUTER_TEST_SOURCE", {});
        if (testSource.isNotEmpty())
            sourceBox->setSelectedId (testSource == "apps" ? 2 : 1, juce::dontSendNotification);
        if (auto testApps = juce::SystemStats::getEnvironmentVariable ("PAD_ROUTER_TEST_APPS", {}); testApps.isNotEmpty())
            chosenApps = juce::StringArray::fromTokens (testApps, ",", {});
        testListen = juce::SystemStats::getEnvironmentVariable ("PAD_ROUTER_TEST_LISTEN", {});

        // A session that crashed with the rack in: put the computer's sound back first.
        const auto recovered = router.recoverLeftovers();

        refreshLists();
        updateControls();
        setStatus (recovered.isNotEmpty() ? recovered : "Rack out. Your audio plays as it did before.");

        const bool autoInsert = (props != nullptr && props->getBoolValue ("router.autoInsert", false))
                             || juce::SystemStats::getEnvironmentVariable ("PAD_ROUTER_TEST_INSERT", {}) == "1";
        if (autoInsert)
            juce::Timer::callAfterDelay (600, [safe = juce::Component::SafePointer<RouterBar> (this)]
            {
                if (safe != nullptr && ! safe->isInserted())
                    safe->insertRack();
            });

        inMeter = devices.getInputLevelGetter();
        outMeter = devices.getOutputLevelGetter();
        startTimerHz (20);
    }

    RouterBar::~RouterBar()
    {
        stopTimer();
        removeRack();
        setLookAndFeel (nullptr);
    }

    //==============================================================================
    routing::Plan RouterBar::currentPlan() const
    {
        routing::Plan plan;
        plan.source = sourceBox->getSelectedId() == 2 ? routing::Plan::Source::chosenApps : routing::Plan::Source::wholeSystem;
        plan.rackInputId = rackInputIds[rackInputBox->getSelectedItemIndex()];
        plan.listenId = listenIds[listenBox->getSelectedItemIndex()];
        plan.appKeys = chosenApps;
        return plan;
    }

    juce::String RouterBar::nameOfEndpoint (const juce::String& id) const
    {
        for (auto& e : endpoints)
            if (e.id == id)
                return e.name;
        return id;
    }

    void RouterBar::refreshLists()
    {
        endpoints = backend->outputs();

        const auto savedRack   = props != nullptr ? props->getValue ("router.rackInput") : juce::String();
        const auto savedListen = props != nullptr ? props->getValue ("router.listen") : juce::String();
        const auto wasRack   = rackInputIds[rackInputBox->getSelectedItemIndex()];
        const auto wasListen = listenIds[listenBox->getSelectedItemIndex()];

        rackInputBox->clear (juce::dontSendNotification);
        listenBox->clear (juce::dontSendNotification);
        rackInputIds.clear();
        listenIds.clear();

        if (backend->canCreateRackInput())
        {
            rackInputBox->addItem ("ENH Master rack input (made for you)", 1);
            rackInputIds.add ({});
        }

        juce::String defaultId;

        for (auto& e : endpoints)
        {
            if (e.isDefault)
                defaultId = e.id;

            const bool isOurs = e.id == "enh_rack_input";

            if (! isOurs)
            {
                rackInputBox->addItem (e.name, rackInputIds.size() + 1);
                rackInputIds.add (e.id);

                listenBox->addItem (e.name + (e.isDefault ? "  (default now)" : ""), listenIds.size() + 1);
                listenIds.add (e.id);
            }
        }

        auto pick = [] (juce::ComboBox& box, const juce::StringArray& ids, juce::StringArray preferred, int fallback)
        {
            for (auto& p : preferred)
            {
                if (auto i = ids.indexOf (p); p.isNotEmpty() && i >= 0)
                {
                    box.setSelectedItemIndex (i, juce::dontSendNotification);
                    return;
                }
            }

            if (ids.size() > 0)
                box.setSelectedItemIndex (juce::jlimit (0, ids.size() - 1, fallback), juce::dontSendNotification);
        };

        // Listening device: what was picked, else wherever the sound goes today.
        pick (*listenBox, listenIds, { testListen, wasListen, savedListen, defaultId }, 0);

        // Rack input: what was picked; else (Linux) our own; else a virtual cable; else anything that
        // is not the listening device.
        int rackFallback = 0;
        if (! backend->canCreateRackInput())
        {
            const auto listen = listenIds[listenBox->getSelectedItemIndex()];
            rackFallback = -1;

            for (int i = 0; i < rackInputIds.size() && rackFallback < 0; ++i)
                if (looksLikeVirtualCable (nameOfEndpoint (rackInputIds[i])))
                    rackFallback = i;

            for (int i = 0; i < rackInputIds.size() && rackFallback < 0; ++i)
                if (rackInputIds[i] != listen)
                    rackFallback = i;

            rackFallback = juce::jmax (0, rackFallback);
        }

        pick (*rackInputBox, rackInputIds, { wasRack, savedRack }, rackFallback);
        if (backend->canCreateRackInput() && wasRack.isEmpty() && savedRack.isEmpty())
            rackInputBox->setSelectedItemIndex (0, juce::dontSendNotification);

        refreshAppsButton();
    }

    void RouterBar::refreshAppsButton()
    {
        appsButton.setButtonText (chosenApps.isEmpty() ? "Apps..." : "Apps (" + juce::String (chosenApps.size()) + ")");
    }

    void RouterBar::saveChoices()
    {
        if (props == nullptr)
            return;

        props->setValue ("router.source", sourceBox->getSelectedId());
        props->setValue ("router.rackInput", rackInputIds[rackInputBox->getSelectedItemIndex()]);
        props->setValue ("router.listen", listenIds[listenBox->getSelectedItemIndex()]);
        props->setValue ("router.apps", chosenApps.joinIntoString ("\n"));
    }

    void RouterBar::updateControls()
    {
        const bool in = router.isInserted();

        for (auto* c : { sourceBox.get(), rackInputBox.get(), listenBox.get() })
            c->setEnabled (! in);

        appsButton.setVisible (sourceBox->getSelectedId() == 2);
        appsButton.setEnabled (! in);
        insertButton.setToggleState (in, juce::dontSendNotification);
        insertButton.setButtonText (in ? "RACK IN  -  REMOVE" : "INSERT RACK");
        insertButton.setTooltip (in ? "Put your audio back where it was, without the rack."
                                    : "Move the audio into the rack and play the result to LISTEN ON.");
        lamp = in ? 1.0f : 0.0f;
        resized();
        repaint();
    }

    void RouterBar::setStatus (const juce::String& text, bool isProblem)
    {
        status = text;
        statusIsProblem = isProblem;
        repaint();
    }

    //==============================================================================
    bool RouterBar::openRackAudio (const juce::String& rackInputId, const juce::String& listenId, juce::String& errorOut)
    {
        auto setup = devices.getAudioDeviceSetup();

       #if JUCE_WINDOWS
        devices.setCurrentAudioDeviceType (loopbackTypeName, true);

        if (auto* type = devices.getCurrentDeviceTypeObject(); type == nullptr || type->getTypeName() != loopbackTypeName)
        {
            errorOut = "The system-audio device type is missing from this build.";
            return false;
        }
        else
        {
            type->scanForDevices();
        }

        setup = devices.getAudioDeviceSetup();
        setup.inputDeviceName = nameOfEndpoint (rackInputId) + loopbackInputSuffix;
        setup.outputDeviceName = nameOfEndpoint (listenId);
       #else
        juce::ignoreUnused (rackInputId, listenId);

        // The ALSA default is PipeWire / PulseAudio: open it, then move our two streams where they belong.
        if (devices.getCurrentAudioDeviceType() != "ALSA")
            devices.setCurrentAudioDeviceType ("ALSA", true);

        auto* type = devices.getCurrentDeviceTypeObject();
        if (type == nullptr)
        {
            errorOut = "No ALSA audio on this system.";
            return false;
        }

        type->scanForDevices();
        setup = devices.getAudioDeviceSetup();
        setup.inputDeviceName = type->getDeviceNames (true)[juce::jmax (0, type->getDefaultDeviceIndex (true))];
        setup.outputDeviceName = type->getDeviceNames (false)[juce::jmax (0, type->getDefaultDeviceIndex (false))];
       #endif

        setup.useDefaultInputChannels = false;
        setup.useDefaultOutputChannels = false;
        setup.inputChannels.clear();
        setup.outputChannels.clear();
        setup.inputChannels.setRange (0, 2, true);
        setup.outputChannels.setRange (0, 2, true);

        errorOut = devices.setAudioDeviceSetup (setup, true);

        if (errorOut.isEmpty() && devices.getCurrentAudioDevice() == nullptr)
            errorOut = "The audio device did not open.";

        if (errorOut.isNotEmpty())
            return false;

       #if ! JUCE_WINDOWS
        if (! backend->connectOwnStreams (rackInputId, listenId))
        {
            errorOut = backend->getLastError();
            return false;
        }
       #endif

        return true;
    }

    void RouterBar::insertRack()
    {
        if (router.isInserted())
            return;

        refreshLists();
        auto plan = currentPlan();

        if (plan.source == routing::Plan::Source::chosenApps && plan.appKeys.isEmpty())
        {
            setStatus ("Pick at least one app first (Apps...).", true);
            showAppsMenu();
            return;
        }

        if (plan.listenId.isEmpty())
        {
            setStatus ("There is no playback device to listen on.", true);
            return;
        }

        juce::String token;
        auto rackId = plan.rackInputId;

        if (rackId.isEmpty() && backend->canCreateRackInput())
        {
            rackId = backend->createRackInput().id;
            token = backend->rackInputToken();

            if (rackId.isEmpty())
            {
                setStatus (backend->getLastError(), true);
                return;
            }
        }

        auto giveUp = [&] (const juce::String& why)
        {
            setMuted (true);
            if (token.isNotEmpty())
                backend->destroyRackInput (token);
            setStatus (why, true);
            updateControls();
        };

        if (rackId == plan.listenId)
            return giveUp ("RACK INPUT and LISTEN ON are the same device, which would feed back. "
                           "Pick a device nobody listens to as the rack input (a virtual cable, or an unused HDMI output).");

        // 1. The rack's own audio, before anything is moved: if this fails, nothing has changed.
        setMuted (true);
        juce::String audioError;
        if (! openRackAudio (rackId, plan.listenId, audioError))
            return giveUp ("Could not start the rack's audio: " + audioError);

        // 2. Write it down, then move the audio.
        if (! router.insert (plan, rackId, token))
            return giveUp (router.getLastError());

        // 3. Now it is safe to let the rack hear its input.
        rackInputInUse = rackId;
        setMuted (false);

        if (auto w = routing::startWatchdog (router.getJournalFile()))
            watchdogs.push_back (std::move (w));

        const auto what = plan.source == routing::Plan::Source::wholeSystem ? juce::String ("Everything on the default device")
                                                                            : chosenApps.joinIntoString (", ");
        auto message = "Rack in. " + what + " goes through the rack to " + nameOfEndpoint (plan.listenId) + ".";
        if (router.getLastError().isNotEmpty())
            message << " " << router.getLastError();

        setStatus (message);
        updateControls();
    }

    void RouterBar::removeRack()
    {
        if (! router.isInserted())
            return;

        // Deafen the rack first: once its input is gone the capture could fall back to a microphone.
        setMuted (true);

        const bool ok = router.remove();
        setStatus (ok ? "Rack out. Your audio is back where it was."
                      : "Rack out, but not everything went back: " + router.getLastError(), ! ok);
        updateControls();
    }

    //==============================================================================
    void RouterBar::showAppsMenu()
    {
        juce::PopupMenu m;
        m.setLookAndFeel (look.get());
        m.addSectionHeader ("Send these apps through the rack");

        auto apps = backend->apps();
        juce::StringArray keys;

        for (auto& a : apps)
        {
            keys.add (a.key);
            m.addItem (keys.size(), a.name + (a.active ? "" : "  (quiet)"), true, chosenApps.contains (a.key));
        }

        for (auto& k : chosenApps)
        {
            if (keys.contains (k))
                continue;
            keys.add (k);
            m.addItem (keys.size(), k + "  (not playing)", true, true);
        }

        if (keys.isEmpty())
            m.addItem (-1, "No app is playing. Start one and open this again.", false);

        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&appsButton),
                         [this, keys] (int chosen)
                         {
                             if (chosen <= 0)
                                 return;

                             const auto key = keys[chosen - 1];
                             if (chosenApps.contains (key))
                                 chosenApps.removeString (key);
                             else
                                 chosenApps.add (key);

                             saveChoices();
                             refreshAppsButton();
                             showAppsMenu();   // stay open for the next tick
                         });
    }

    void RouterBar::showMoreMenu()
    {
        juce::PopupMenu m;
        m.setLookAndFeel (look.get());

        const bool autoInsert = props != nullptr && props->getBoolValue ("router.autoInsert", false);
        m.addItem (1, "Insert the rack when ENH Master starts", props != nullptr, autoInsert);
        m.addItem (4, "Closing the window with the rack in keeps it running (tray icon)", props != nullptr,
                   props == nullptr || props->getBoolValue ("router.closeToTray", true));
        m.addItem (5, "Start with the computer (in the tray, rack in)", true, isStartingWithComputer());
        m.addItem (2, "Refresh devices and apps", ! router.isInserted());
        m.addSeparator();
        m.addItem (3, "Audio device settings (advanced)...", ! router.isInserted());

        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&moreButton),
                         [this, autoInsert] (int chosen)
                         {
                             if (chosen == 1 && props != nullptr) props->setValue ("router.autoInsert", ! autoInsert);
                             if (chosen == 5)
                             {
                                 const bool want = ! isStartingWithComputer();
                                 if (setStartingWithComputer (want))
                                 {
                                     if (want && props != nullptr)
                                         props->setValue ("router.autoInsert", true);   // starting in the tray is for the rack being in
                                     setStatus (want ? "ENH Master will start with the computer, in the tray, with the rack in."
                                                     : "ENH Master will no longer start with the computer.");
                                 }
                                 else
                                 {
                                     setStatus ("Could not change the start-up setting.", true);
                                 }
                             }
                             if (chosen == 4 && props != nullptr) props->setValue ("router.closeToTray", ! props->getBoolValue ("router.closeToTray", true));
                             if (chosen == 2) refreshLists();
                             if (chosen == 3 && openAudioSettings) openAudioSettings();
                         });
    }

    void RouterBar::timerCallback()
    {
        ++tick;

        // Meters: what reaches the rack, and what it plays (20 times a second, falling gently)
        const auto in = (float) inMeter->getCurrentLevel();
        const auto out = (float) outMeter->getCurrentLevel();
        const auto newIn = std::max (in, inLevel * 0.85f), newOut = std::max (out, outLevel * 0.85f);
        if (std::abs (newIn - inLevel) > 0.002f || std::abs (newOut - outLevel) > 0.002f)
        {
            inLevel = newIn;
            outLevel = newOut;
            repaint (getLocalBounds().removeFromBottom (22));
        }

        // LEVEL follows the rack's own setting (it can be changed in its panel too)
        if (targetParam != nullptr && tick % 5 == 0)
        {
            const int id = juce::roundToInt (targetParam->convertFrom0to1 (targetParam->getValue())) + 1;
            if (id != levelBox->getSelectedId())
                levelBox->setSelectedId (id, juce::dontSendNotification);
        }

        if (tick % 20 != 0)
            return;

        // Once a second. Our own capture must stay on the rack input. If it has slipped (the device
        // was reopened, a sound server restart), deafen the rack until it is back.
        if (router.isInserted())
        {
            const auto plan = currentPlan();

            // The listening device went away (a headset unplugged) or the audio stopped: the audio
            // would sit on the rack input with nobody hearing it - and on Linux our output would fall
            // back to the default device, which is the rack input: a loop. Take the rack out.
            const auto outputsNow = backend->outputs();
            const bool listenStillThere = std::any_of (outputsNow.begin(), outputsNow.end(),
                                                       [&] (const routing::Endpoint& e) { return e.id == plan.listenId; });
            auto* device = devices.getCurrentAudioDevice();

            if (! listenStillThere || device == nullptr || ! device->isPlaying())
            {
                const auto why = ! listenStillThere ? nameOfEndpoint (plan.listenId) + " went away"
                                                    : juce::String ("the audio device stopped");
                removeRack();
                setStatus ("Rack out: " + why + ". Your audio is back where it was.", true);
                return;
            }

            if (! backend->ownStreamsConnected (rackInputInUse, plan.listenId))
            {
                setMuted (true);

                if (backend->connectOwnStreams (rackInputInUse, plan.listenId))
                    setMuted (false);
                else
                    setStatus ("Rack in, but its audio lost its connection. Muted until it is back.", true);
            }

            // Apps that start (or open a new stream) while the rack is in follow the others into it.
            if (plan.source == routing::Plan::Source::chosenApps && tick % 40 == 0)
                if (auto n = router.followNewStreams(); n > 0)
                    setStatus ("Rack in. Moved " + juce::String (n) + " new stream" + (n == 1 ? "" : "s") + " into it.");
        }

        // Reap finished watchdogs (asking is what collects them).
        watchdogs.erase (std::remove_if (watchdogs.begin(), watchdogs.end(),
                                         [] (auto& w) { return ! w->isRunning(); }),
                         watchdogs.end());
    }

    //==============================================================================
    void RouterBar::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (barTop, 0, 0, barBottom, 0, r.getBottom(), false));
        g.fillRect (r);
        g.setColour (hairline);
        g.fillRect (r.removeFromBottom (1.0f));

        g.setFont (uiFont (9.5f, true));
        g.setColour (faintInk);

        auto caption = [&] (juce::Component& c, const juce::String& text)
        {
            if (c.isVisible())
                g.drawText (text, c.getX(), 6, c.getWidth(), 11, juce::Justification::centredLeft, false);
        };

        caption (*sourceBox, "SOURCE");
        caption (*rackInputBox, "RACK INPUT");
        caption (*listenBox, "LISTEN ON");
        caption (*levelBox, "LEVEL");

        // The lamp on the insert button's left: dark when out, warm when in.
        auto b = insertButton.getBounds().toFloat();
        auto lampArea = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ b.getX() - 9.0f, b.getCentreY() });
        g.setColour (lamp > 0.5f ? amber : juce::Colour (0xff2a2622));
        g.fillEllipse (lampArea);
        if (lamp > 0.5f)
        {
            g.setColour (amber.withAlpha (0.25f));
            g.fillEllipse (lampArea.expanded (3.0f));
        }

        // IN / OUT meters on the right of the status line (dB scale, -60 .. 0)
        auto meterRow = juce::Rectangle<int> (getWidth() - 12 - 200, getHeight() - 17, 200, 10);
        auto meter = [&] (juce::Rectangle<int> r, const char* label, float level)
        {
            g.setFont (uiFont (9.5f, true));
            g.setColour (faintInk);
            g.drawText (label, r.removeFromLeft (28), juce::Justification::centredLeft, false);
            const auto bar = r.toFloat().reduced (0.0f, 2.0f);
            g.setColour (fieldFill);
            g.fillRect (bar);
            const float db = juce::Decibels::gainToDecibels (level, -60.0f);
            const float fill = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
            g.setColour (db > -1.0f ? problem : amber.withAlpha (0.85f));
            g.fillRect (bar.withWidth (bar.getWidth() * fill));
        };
        meter (meterRow.removeFromLeft (96), "IN", inLevel);
        meterRow.removeFromLeft (8);
        meter (meterRow, "OUT", outLevel);

        g.setFont (uiFont (11.5f));
        g.setColour (statusIsProblem ? problem : faintInk);
        g.drawText (status, 12, getHeight() - 20, getWidth() - 24 - 212, 16, juce::Justification::centredLeft, true);
    }

    void RouterBar::resized()
    {
        auto r = getLocalBounds().reduced (12, 0);
        auto row = r.withTop (19).withHeight (26);

        moreButton.setBounds (row.removeFromRight (34));
        row.removeFromRight (8);
        insertButton.setBounds (row.removeFromRight (juce::jlimit (120, 170, getWidth() / 6)));
        row.removeFromRight (24);   // room for the lamp

        if (levelBox->isVisible())
        {
            levelBox->setBounds (row.removeFromRight (juce::jlimit (96, 140, getWidth() / 8)));
            row.removeFromRight (10);
        }

        sourceBox->setBounds (row.removeFromLeft (juce::jlimit (110, 140, row.getWidth() / 5)));
        row.removeFromLeft (6);

        if (appsButton.isVisible())
        {
            appsButton.setBounds (row.removeFromLeft (84));
            row.removeFromLeft (6);
        }

        const auto half = (row.getWidth() - 10) / 2;
        rackInputBox->setBounds (row.removeFromLeft (half));
        row.removeFromLeft (10);
        listenBox->setBounds (row);
    }

    //==============================================================================
    namespace
    {
        juce::String startCommand()
        {
            return "\"" + juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName() + "\" "
                   + RouterBar::backgroundArgument;
        }

       #if JUCE_WINDOWS
        const juce::String runKey = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\\ENH Master";
       #else
        juce::File autostartFile()
        {
            return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".config/autostart/enh-master.desktop");
        }
       #endif
    }

    bool RouterBar::isStartingWithComputer()
    {
       #if JUCE_WINDOWS
        return juce::WindowsRegistry::valueExists (runKey);
       #else
        return autostartFile().existsAsFile();
       #endif
    }

    bool RouterBar::setStartingWithComputer (bool shouldStart)
    {
       #if JUCE_WINDOWS
        if (shouldStart)
            return juce::WindowsRegistry::setValue (runKey, startCommand());
        juce::WindowsRegistry::deleteValue (runKey);
        return ! isStartingWithComputer();
       #else
        auto f = autostartFile();
        if (! shouldStart)
            return ! f.exists() || f.deleteFile();

        f.getParentDirectory().createDirectory();
        return f.replaceWithText ("[Desktop Entry]\nType=Application\nName=ENH Master\n"
                                  "Comment=The ENH Master rack, in the tray, with the rack in\n"
                                  "Exec=" + startCommand() + "\nX-GNOME-Autostart-enabled=true\nTerminal=false\n");
       #endif
    }
}
