#pragma once

#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "DesignedLayout.h"

/*  CUSTOM: the look of the design loaded into the slot - its name, its plate's colour and its print (the
    designed-unit Print format; the texts live in `strings`, whose addresses never move). Replaced whole
    when another design is loaded (message thread); readers take a shared_ptr and keep it while they use
    the Print's pointers (the render thread too). */
namespace pad::layout::custom
{
    struct Look
    {
        std::string name = "CUSTOM", model = "EMPTY", sub = "PASTE A DESIGN CODE IN ITS GLASS PANEL";
        float plate[3] { 0.045f, 0.047f, 0.052f };
        std::vector<designed::Print> print;
        std::deque<std::string> strings;
        const char* keep (std::string s) { strings.push_back (std::move (s)); return strings.back().c_str(); }
    };

    inline std::mutex mutex;
    inline std::shared_ptr<const Look> current = std::make_shared<const Look>();
    inline std::shared_ptr<const Look> get() { std::lock_guard<std::mutex> l (mutex); return current; }
    inline void set (std::shared_ptr<const Look> look) { std::lock_guard<std::mutex> l (mutex); current = std::move (look); }

    /** The slot's controls' names (their ControlDef labels point here, permanently). */
    inline std::array<std::array<char, 24>, 20> labels {};
}
