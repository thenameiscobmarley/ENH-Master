#include "PresetLibrary.h"
#include <algorithm>

#include <mutex>

namespace pad::presets
{
    namespace
    {
        std::mutex lock;
        std::shared_ptr<const std::vector<Preset>> cached;
        juce::Time loadedModTime;
        juce::File loadedFrom;
        double lastCheckMs = -1.0e9;
        juce::String source = "built-in factory presets";

        double tidy (float v) { return std::round ((double) v * 10000.0) / 10000.0; }

        /** Whether a preset file's "parameters" reference lists exactly this build's parameters. */
        bool referenceIsCurrent (const juce::String& text)
        {
            juce::var root;
            if (juce::JSON::parse (text, root).failed())
                return true;   // not ours to rewrite
            const auto* reference = root.getProperty ("parameters", {}).getDynamicObject();
            if (reference == nullptr)
                return false;
            int expected = 0;
            for (auto& s : params::allSpecs())
                if (s.automatable)
                {
                    ++expected;
                    if (! reference->hasProperty (s.id))
                        return false;
                }
            return reference->getProperties().size() == expected;
        }
    }

    juce::File presetFile()
    {
        const auto env = juce::SystemStats::getEnvironmentVariable ("ENH_MASTER_PRESETS", {});
        if (env.isNotEmpty())
            return juce::File (env);
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("ENH Master").getChildFile ("presets.json");
    }

    juce::String toJson (const std::vector<Preset>& list)
    {
        auto* root = new juce::DynamicObject();
        root->setProperty ("format", "ENH Master presets");
        root->setProperty ("version", 1);

        juce::Array<juce::var> about;
        for (auto* line : { "Every preset is a complete state of the rack: values are in each parameter's own units (as printed on the panels), clamped to its range.",
                            "A parameter a preset leaves out goes to its default. Order here = order in the host and on the PRESET buttons.",
                            "The plugin re-reads this file when it changes (next PRESET press or host program list). Delete it to get the factory presets back.",
                            "The 'parameters' block is a reference (ranges, defaults, what 0/1 or the choice numbers mean); it is ignored when the file is read.",
                            "Units bottom to top: ADAPTIVE ENHANCER, UPWARD LEVELER (lumen*), SPECTRAL LIMITER (spectral*), ADAPTIVE COMPRESSOR (tide*), TONE & SPACE (silk* = TONE, halo* = SPACE, heaven* = LOUDNESS / AUTO)." })
            about.add (juce::String (line));
        root->setProperty ("about", about);

        juce::Array<juce::var> presets;
        for (auto& p : list)
        {
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("name", p.name);
            obj->setProperty ("purpose", p.purpose);
            auto* values = new juce::DynamicObject();
            for (auto& [pid, v] : p.values)
                values->setProperty (pid, tidy (v));
            obj->setProperty ("values", juce::var (values));
            presets.add (juce::var (obj));
        }
        root->setProperty ("presets", presets);

        auto* reference = new juce::DynamicObject();
        for (auto& s : params::allSpecs())
        {
            if (! s.automatable)
                continue;
            auto* r = new juce::DynamicObject();
            r->setProperty ("name", s.name);
            r->setProperty ("min", tidy (s.minValue));
            r->setProperty ("max", tidy (s.maxValue));
            r->setProperty ("default", tidy (s.defaultValue));
            if (s.unit.trim().isNotEmpty())
                r->setProperty ("unit", s.unit.trim());
            if (s.kind == params::Kind::toggle)
                r->setProperty ("values", s.texts.size() == 2 ? "0 = " + s.texts[0] + ", 1 = " + s.texts[1] : juce::String ("0 = off, 1 = on"));
            else if (s.kind == params::Kind::choice)
            {
                juce::StringArray choices;
                for (int i = 0; i < s.texts.size(); ++i)
                    choices.add (juce::String (i) + " = " + s.texts[i]);
                r->setProperty ("values", choices.joinIntoString (", "));
            }
            reference->setProperty (s.id, juce::var (r));
        }
        root->setProperty ("parameters", juce::var (reference));

        return juce::JSON::toString (juce::var (root), false, 6);
    }

    std::vector<Preset> fromJson (const juce::String& text, juce::String& error)
    {
        juce::var root;
        const auto result = juce::JSON::parse (text, root);
        if (result.failed())
        {
            error = "not valid JSON: " + result.getErrorMessage();
            return {};
        }
        const auto* presets = root.getProperty ("presets", {}).getArray();
        if (presets == nullptr || presets->isEmpty())
        {
            error = "no \"presets\" list";
            return {};
        }

        std::vector<Preset> list;
        for (auto& entry : *presets)
        {
            Preset p;
            p.name = entry.getProperty ("name", {}).toString().trim();
            p.purpose = entry.getProperty ("purpose", {}).toString();
            if (p.name.isEmpty())
            {
                error = "a preset without a name";
                return {};
            }
            if (auto* values = entry.getProperty ("values", {}).getDynamicObject())
            {
                for (auto& prop : values->getProperties())
                {
                    const auto pid = prop.name.toString();
                    const auto* spec = params::findSpec (pid);
                    if (spec == nullptr || ! spec->automatable || ! (prop.value.isDouble() || prop.value.isInt() || prop.value.isInt64() || prop.value.isBool()))
                        continue;   // unknown ids are skipped, not fatal: old files keep working
                    p.values.emplace_back (pid, std::clamp ((float) (double) prop.value, spec->minValue, spec->maxValue));
                }
            }
            list.push_back (std::move (p));
        }
        return list;
    }

    std::shared_ptr<const std::vector<Preset>> library (bool seedIfMissing)
    {
        const std::lock_guard<std::mutex> guard (lock);
        const auto file = presetFile();
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (cached != nullptr && file == loadedFrom && now - lastCheckMs < 500.0)
            return cached;
        lastCheckMs = now;
        if (file != loadedFrom)
        {
            cached = nullptr;   // a different file (ENH_MASTER_PRESETS changed): start over
            loadedFrom = file;
        }
        if (! file.existsAsFile() && seedIfMissing)
        {
            file.getParentDirectory().createDirectory();
            file.replaceWithText (toJson (factory()));
        }

        if (file.existsAsFile())
        {
            auto modTime = file.getLastModificationTime();
            if (cached == nullptr || modTime != loadedModTime)
            {
                juce::String error;
                const auto text = file.loadFileAsString();
                auto list = fromJson (text, error);
                if (! list.empty())
                {
                    // Keep the file's parameter reference current: a newer build may have parameters the
                    // file's reference block does not list yet (the presets themselves are left as they are)
                    if (seedIfMissing && ! referenceIsCurrent (text))
                    {
                        // ... and a newer build's new factory presets join the list (the file's own
                        // presets, tuned or not, are left exactly as they are)
                        for (const auto& f : factory())
                            if (std::none_of (list.begin(), list.end(), [&] (const Preset& p) { return p.name == f.name; }))
                                list.push_back (f);
                        file.replaceWithText (toJson (list));
                        modTime = file.getLastModificationTime();
                    }
                    cached = std::make_shared<const std::vector<Preset>> (std::move (list));
                    source = file.getFullPathName();
                }
                else
                {
                    DBG ("ENH Master: ignoring " + file.getFullPathName() + " (" + error + ")");
                    if (cached == nullptr)
                    {
                        cached = std::make_shared<const std::vector<Preset>> (factory());
                        source = "built-in factory presets (" + file.getFullPathName() + ": " + error + ")";
                    }
                }
                loadedModTime = modTime;
            }
            return cached;
        }

        if (cached == nullptr)
        {
            cached = std::make_shared<const std::vector<Preset>> (factory());
            source = "built-in factory presets (no file at " + file.getFullPathName() + ")";
        }
        return cached;
    }

    juce::String librarySource()
    {
        const std::lock_guard<std::mutex> guard (lock);
        return source;
    }
}
