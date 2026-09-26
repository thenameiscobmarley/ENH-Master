#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include "DisplayBridge.h"

/*  The system effect's saved settings: one ParamSnapshot in a small file, so the effect sounds the way
    the user left it after a reboot, without the app running.

        Windows: %ProgramData%\ENH Master\system-effect-settings.bin
                 (the installer creates the folder: SYSTEM / Administrators / LOCAL SERVICE full,
                  Users modify - the app writes it as the signed-in user, audiodg reads it)

    Format: magic 'ENHS', format version, bridge version, payload size, FNV-1a of the payload, payload.
    Read by the effect OUTSIDE the audio thread (LockForProcess), treated as hostile like the shared
    block (every value is sanitised on use). JUCE-free. */

namespace enh::shared
{
    struct SnapshotFileHeader
    {
        std::uint32_t magic = 0x53484e45u;   // 'ENHS'
        std::uint32_t formatVersion = 1;
        std::uint32_t bridgeVersion = kBridgeVersion;
        std::uint32_t payloadSize = (std::uint32_t) sizeof (ParamSnapshot);
        std::uint32_t payloadHash = 0;
    };

    inline bool encodeSnapshotFile (const ParamSnapshot& s, unsigned char* out, std::size_t outSize) noexcept
    {
        if (outSize < sizeof (SnapshotFileHeader) + sizeof (ParamSnapshot))
            return false;
        SnapshotFileHeader h;
        h.payloadHash = fnv1a (kFnvSeed, &s, sizeof (s));
        std::memcpy (out, &h, sizeof (h));
        std::memcpy (out + sizeof (h), &s, sizeof (s));
        return true;
    }

    inline bool decodeSnapshotFile (const unsigned char* data, std::size_t size, ParamSnapshot& out) noexcept
    {
        if (size != sizeof (SnapshotFileHeader) + sizeof (ParamSnapshot))
            return false;
        SnapshotFileHeader h, expected;
        std::memcpy (&h, data, sizeof (h));
        if (h.magic != expected.magic || h.formatVersion != expected.formatVersion
             || h.bridgeVersion != expected.bridgeVersion || h.payloadSize != expected.payloadSize)
            return false;
        ParamSnapshot s;
        std::memcpy (&s, data + sizeof (h), sizeof (s));
        if (fnv1a (kFnvSeed, &s, sizeof (s)) != h.payloadHash || s.count > (std::uint32_t) kMaxParams)
            return false;
        out = s;
        return true;
    }

    inline constexpr std::size_t kSnapshotFileSize = sizeof (SnapshotFileHeader) + sizeof (ParamSnapshot);

   #if defined (_WIN32)
    inline bool loadSnapshotFile (const wchar_t* path, ParamSnapshot& out) noexcept
    {
        std::FILE* f = nullptr;
        if (_wfopen_s (&f, path, L"rb") != 0 || f == nullptr)
            return false;
   #else
    inline bool loadSnapshotFile (const char* path, ParamSnapshot& out) noexcept
    {
        std::FILE* f = std::fopen (path, "rb");
        if (f == nullptr)
            return false;
   #endif
        unsigned char buffer[kSnapshotFileSize + 1];
        const std::size_t n = std::fread (buffer, 1, sizeof (buffer), f);
        std::fclose (f);
        return decodeSnapshotFile (buffer, n, out);
    }
}
