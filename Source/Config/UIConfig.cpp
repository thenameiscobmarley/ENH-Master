#include "UIConfig.h"

namespace pad
{
    juce::File UIConfig::getDefaultFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                 .getChildFile ("ENHMaster")
                 .getChildFile ("ui-config.json");
    }

    juce::var UIConfig::toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("schemaVersion", schemaVersion);
        obj->setProperty ("frameRate", frameRate);
        obj->setProperty ("idleFrameRate", idleFrameRate);
        obj->setProperty ("msaaSamples", msaaSamples);
        obj->setProperty ("anisotropy", anisotropy);
        obj->setProperty ("panelTextureWidth", panelTextureWidth);
        obj->setProperty ("parallaxAmount", parallaxAmount);
        obj->setProperty ("reduceMotion", reduceMotion);
        return juce::var (obj);
    }

    UIConfig UIConfig::fromVar (const juce::var& v)
    {
        UIConfig c;
        auto* obj = v.getDynamicObject();

        if (obj == nullptr)
        {
            c.warnings.add ("Config root is not an object; using defaults");
            return c;
        }

        const auto known = juce::StringArray { "schemaVersion", "frameRate", "idleFrameRate", "msaaSamples",
                                               "anisotropy", "panelTextureWidth", "parallaxAmount", "reduceMotion" };

        for (auto& prop : obj->getProperties())
            if (! known.contains (prop.name.toString()))
                c.warnings.add ("Unknown key ignored: " + prop.name.toString());

        if ((int) obj->getProperty ("schemaVersion") != schemaVersion)
            c.warnings.add ("schemaVersion mismatch; unrecognised values fall back to defaults");

        auto readInt = [&] (const char* key, int& target, int lo, int hi)
        {
            auto value = obj->getProperty (key);
            if (value.isVoid())
                return;

            if (! (value.isInt() || value.isInt64() || value.isDouble()))
            {
                c.warnings.add (juce::String (key) + " must be a number");
                return;
            }

            const int n = (int) value;
            target = juce::jlimit (lo, hi, n);
            if (target != n)
                c.warnings.add (juce::String (key) + " clamped to " + juce::String (target));
        };

        readInt ("frameRate", c.frameRate, 15, 144);
        readInt ("idleFrameRate", c.idleFrameRate, 5, 60);
        readInt ("msaaSamples", c.msaaSamples, 0, 4);
        readInt ("anisotropy", c.anisotropy, 1, 8);
        readInt ("panelTextureWidth", c.panelTextureWidth, 1024, 2048);

        if (c.msaaSamples == 1 || c.msaaSamples == 3)
            c.msaaSamples = 2;

        c.panelTextureWidth = c.panelTextureWidth >= 1536 ? 2048 : 1024;
        c.idleFrameRate = juce::jmin (c.idleFrameRate, c.frameRate);

        auto parallax = obj->getProperty ("parallaxAmount");
        if (! parallax.isVoid())
        {
            if (parallax.isDouble() || parallax.isInt())
                c.parallaxAmount = juce::jlimit (0.0f, 2.0f, (float) parallax);
            else
                c.warnings.add ("parallaxAmount must be a number");
        }

        auto reduce = obj->getProperty ("reduceMotion");
        if (! reduce.isVoid())
        {
            if (reduce.isBool())
                c.reduceMotion = (bool) reduce;
            else
                c.warnings.add ("reduceMotion must be true/false");
        }

        return c;
    }

    UIConfig UIConfig::loadOrCreate (const juce::File& file)
    {
        if (! file.existsAsFile())
        {
            UIConfig defaults;
            if (file.getParentDirectory().createDirectory().wasOk())
                file.replaceWithText (juce::JSON::toString (defaults.toVar()));
            return defaults;
        }

        juce::var parsed;
        const auto result = juce::JSON::parse (file.loadFileAsString(), parsed);

        if (result.failed())
        {
            UIConfig defaults;
            defaults.warnings.add ("JSON parse error: " + result.getErrorMessage());
            return defaults;
        }

        auto config = fromVar (parsed);

        for (auto& w : config.warnings)
        {
            juce::ignoreUnused (w);
            DBG ("[ui-config] " << w);
        }

        return config;
    }
}
