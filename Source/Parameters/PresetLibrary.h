#pragma once

#include <memory>
#include "FactoryPresets.h"

/*  The rack's presets, read from a local JSON file so they can be edited and re-tuned (by hand or by
    an AI) without rebuilding the plugin.

    Where: $ENH_MASTER_PRESETS if set, else the user application-data folder:
        Linux    ~/.config/ENH Master/presets.json
        Windows  %APPDATA%\ENH Master\presets.json
        macOS    ~/Library/ENH Master/presets.json

    The plugin writes the factory presets there the first time it runs. After that the file is the
    source: edits are picked up the next time the host asks for its programs or a PRESET button is
    pressed (checked at most twice a second). A file that fails to parse is ignored (the last good
    list, or the factory list, stays in use) and left as it is. Delete the file to get the factory
    presets back.

    Format: { "presets": [ { "name", "purpose", "values": { "<parameter id>": <value>, ... } } ],
              "parameters": { "<id>": { range, default, units, ... } } }   - the "parameters" block
    is a reference for whoever edits the file; it is rewritten with the factory file and ignored on
    reading. Values are in each parameter's own units (as printed on the panels), clamped to its
    range; a parameter a preset leaves out goes to its default.
*/
namespace pad::presets
{
    /** The preset file this build reads (see above). */
    juce::File presetFile();

    /** The current preset list: the file if it exists and parses, else the factory presets.
        seedIfMissing: write the factory presets to the file when there is none (the plugin does;
        the tests do not). Safe to call from any thread. */
    std::shared_ptr<const std::vector<Preset>> library (bool seedIfMissing = true);

    /** Where the current list came from, for logs and the tests. */
    juce::String librarySource();

    /** The file's JSON for a list of presets (with the parameter reference). */
    juce::String toJson (const std::vector<Preset>&);

    /** Parses a preset file's text; empty on failure, with `error` set. */
    std::vector<Preset> fromJson (const juce::String& text, juce::String& error);
}
