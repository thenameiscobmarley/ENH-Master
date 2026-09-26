#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include "MeterSnapshot.h"

/*  DisplayBridge: one small named shared-memory block between the process that runs the sound (the
    system effect in audiodg.exe, or a plugin instance that is getting audio) and the process that
    shows the rack (the standalone app, or a plugin instance whose host sends it no audio).

        UI side    ── parameters (every knob, method and knob modifier) ──►  audio side
        UI side    ◄── meters (all of EngineMeters) + 4 scope rings ───────  audio side
        both       ── a heartbeat each, so each side knows the other is there

    JUCE-free, C++17, Linux (POSIX shm_open + mmap) and Windows (CreateFileMappingW + MapViewOfFile).

    Real-time rules (the audio side calls these from the audio thread):
      - publishMeters, pushScope, beat: wait-free. A writer never waits for anything: if another writer
        is in the middle of the same block (two audio instances on one key), this publish is skipped.
      - readParams: bounded. At most `maxAttempts` tries (the audio side passes 2: one retry), then it
        reports `busy` and the caller keeps the values it had.
      - No allocation, no locks, no system calls (the clock read is a vDSO / shared-page read).

    Seqlock (single block, many readers, writers serialised by a CAS on the sequence):
      writer: CAS seq even s -> s+1 (skip the write if odd or the CAS fails); release fence; relaxed
              stores of every 32-bit word; store seq = s+2 (release)
      reader: s1 = seq (acquire); odd -> retry; relaxed loads of every word; acquire fence;
              s2 = seq (relaxed); valid when s1 == s2
    The payload is stored as std::atomic<uint32_t> words, so torn reads are never a data race, just a
    rejected copy. Lock-free 32-bit atomics are address-free, so they work across processes.

    Everything read from the block is untrusted (another process wrote it): the users of the parameter
    block clamp every value (Shared/RackParamTable.h), and the version/size checks happen at open.

    Naming, security and key policy: see DisplayBridge.cpp (top) and Vault/Reference/System effect (APO).md. */

namespace enh::shared
{
    //==============================================================================================
    inline constexpr std::uint32_t kBridgeMagic   = 0x42484e45u;   // 'ENHB' (little-endian)
    inline constexpr std::uint32_t kBridgeVersion = 1;             // bump on ANY layout change (it is in the object name too)

    inline constexpr int kMaxParams = 192;           // parameters by index, in ParameterSpecs order
    inline constexpr int kMaxModifierKnobs = 64;     // enh::dsp::knobFields (45 today)
    inline constexpr int kModifierKinds = 3;         // SMO, CRV, LIM (methods::numModifierKinds)
    inline constexpr int kNumScopes = 4;             // input, output, balancer input, balancer output
    inline constexpr int kScopeSize = 4096;          // = ScopeFifo::size (checked in BridgeScopes.h)

    enum ScopeIndex { scopeInput = 0, scopeOutput, scopeBalancerIn, scopeBalancerOut };

    /** 32-bit FNV-1a, for the parameter layout hash (both sides hash the same thing the same way). */
    inline std::uint32_t fnv1a (std::uint32_t h, const void* data, std::size_t size) noexcept
    {
        auto* p = static_cast<const unsigned char*> (data);
        for (std::size_t i = 0; i < size; ++i)
            h = (h ^ p[i]) * 16777619u;
        return h;
    }
    inline constexpr std::uint32_t kFnvSeed = 2166136261u;

    /** One parameter's contribution to the layout hash: its ID (with the terminating zero) and range. */
    inline std::uint32_t hashParam (std::uint32_t h, const char* id, std::size_t idLength, float minValue, float maxValue) noexcept
    {
        const char zero = 0;
        h = fnv1a (h, id, idLength);
        h = fnv1a (h, &zero, 1);
        h = fnv1a (h, &minValue, sizeof (float));
        return fnv1a (h, &maxValue, sizeof (float));
    }

    //==============================================================================================
    /** Written by the UI side. Values are each parameter's plain value (its own units), by index in
        pad::params::allSpecs() order; `layoutHash` says which order that is, and a reader with a
        different layout ignores the block. */
    struct ParamSnapshot
    {
        std::uint32_t layoutHash = 0;
        std::uint32_t count = 0;              // parameters in values[]
        std::uint32_t loudnessResets = 0;     // bumped on every RESET press: the audio side resets when it changes
        std::uint32_t flags = 0;              // reserved (0)
        std::array<float, kMaxParams> values {};
        std::array<float, kMaxModifierKnobs * kModifierKinds> modifiers {};   // [knob * kModifierKinds + kind]
    };

    /** What the audio side is running at, for the display (DELAY readout, status line). */
    struct AudioStatus
    {
        float sampleRate = 0.0f;
        std::int32_t latencySamples = 0;
        std::int32_t channels = 0;
        std::int32_t maxFrames = 0;
        std::uint32_t flags = 0;              // AudioStatusFlags
        std::uint32_t processCalls = 0;
    };
    enum AudioStatusFlags : std::uint32_t
    {
        statusRunning   = 1u << 0,   // the engine is processing
        statusFailsafe  = 1u << 1,   // something failed: audio passes through unchanged
        statusIdle      = 1u << 2,   // input silent for a while: processing paused
        statusSystemApo = 1u << 3,   // the writer is the Windows system effect
        statusUsingUi   = 1u << 4,   // the parameters come from a UI (not the defaults / saved file)
    };

    struct MeterBlock
    {
        AudioStatus status {};
        MeterSnapshot meters {};
    };

    static_assert (std::is_trivially_copyable_v<ParamSnapshot> && sizeof (ParamSnapshot) % 4 == 0);
    static_assert (std::is_trivially_copyable_v<MeterBlock> && sizeof (MeterBlock) % 4 == 0);
    static_assert (std::atomic<std::uint32_t>::is_always_lock_free, "cross-process atomics must be lock-free");
    static_assert (sizeof (std::atomic<std::uint32_t>) == 4);

    //==============================================================================================
    /** A seqlocked block of T, stored as 32-bit atomic words. Lives inside the shared mapping. */
    template <typename T>
    struct SeqBlock
    {
        static constexpr std::size_t numWords = sizeof (T) / 4;

        std::atomic<std::uint32_t> seq;
        std::atomic<std::uint32_t> words[numWords];

        /** Wait-free. False when another writer holds the block right now (this write is skipped). */
        bool write (const T& value) noexcept
        {
            std::uint32_t s = seq.load (std::memory_order_relaxed);
            if ((s & 1u) != 0 || ! seq.compare_exchange_strong (s, s + 1u, std::memory_order_relaxed))
                return false;
            std::atomic_thread_fence (std::memory_order_release);

            const auto* bytes = reinterpret_cast<const unsigned char*> (&value);
            for (std::size_t i = 0; i < numWords; ++i)
            {
                std::uint32_t w;
                std::memcpy (&w, bytes + i * 4, 4);
                words[i].store (w, std::memory_order_relaxed);
            }
            seq.store (s + 2u, std::memory_order_release);
            return true;
        }

        enum class Read { fresh, unchanged, busy, empty };

        /** Bounded: at most maxAttempts copies. `lastSeq` is the version the caller holds (0 = none): the
            same version again returns `unchanged` without copying. `out` is only written on `fresh`. */
        Read read (T& out, std::uint32_t& lastSeq, int maxAttempts) noexcept
        {
            alignas (T) unsigned char scratch[sizeof (T)];
            for (int attempt = 0; attempt < maxAttempts; ++attempt)
            {
                const std::uint32_t s1 = seq.load (std::memory_order_acquire);
                if (s1 == 0)
                    return Read::empty;
                if ((s1 & 1u) != 0)
                    continue;
                if (s1 == lastSeq)
                    return Read::unchanged;

                for (std::size_t i = 0; i < numWords; ++i)
                {
                    const std::uint32_t w = words[i].load (std::memory_order_relaxed);
                    std::memcpy (scratch + i * 4, &w, 4);
                }
                std::atomic_thread_fence (std::memory_order_acquire);
                if (seq.load (std::memory_order_relaxed) == s1)
                {
                    std::memcpy (&out, scratch, sizeof (T));
                    lastSeq = s1;
                    return Read::fresh;
                }
            }
            return Read::busy;
        }
    };

    /** One analyser tap: a ring of mono samples, like enh::dsp::ScopeFifo. One writer. */
    struct ScopeRing
    {
        std::atomic<std::uint32_t> writeIndex;
        std::atomic<std::uint32_t> samples[kScopeSize];   // float bits
    };

    /** Each side's heartbeat: a counter and the time of the last beat (system-wide monotonic ms). */
    struct Heartbeat
    {
        std::atomic<std::uint32_t> count;
        std::atomic<std::uint32_t> lastMs;
        std::atomic<std::uint32_t> processId;
    };

    struct BridgeHeader
    {
        std::atomic<std::uint32_t> magic;     // written last by the creator (release)
        std::atomic<std::uint32_t> version;
        std::atomic<std::uint32_t> totalSize;
        std::atomic<std::uint32_t> reserved;
    };

    /** The whole shared block (about 70 KB). Zero-filled by the OS when created. */
    struct BridgeLayout
    {
        BridgeHeader header;
        Heartbeat audioBeat, uiBeat;
        SeqBlock<ParamSnapshot> params;
        SeqBlock<MeterBlock> meters;
        ScopeRing scopes[kNumScopes];
    };

    static_assert (std::is_standard_layout_v<BridgeLayout>);

    //==============================================================================================
    class DisplayBridge
    {
    public:
        enum class Role { audio, ui };

        /** Where the name lives. Linux has one per-machine namespace (and the object is 0600: this user
            only), so it only matters on Windows:
              session - "Local\"  : this logon session (plugin instances, same user). Anyone may create.
              machine - "Global\" : all sessions. Needed for the system effect, because audiodg.exe runs as
                                    LOCAL SERVICE in session 0 and the app in the user's session. Only
                                    services / administrators may CREATE there, so the system effect creates
                                    it (with a DACL that lets interactive users open it) and the app only
                                    opens it. */
        enum class Scope { session, machine };

        enum class OpenResult { ok, notFound, notReady, versionMismatch, sizeMismatch, badData, accessDenied, badName, systemError };

        DisplayBridge() = default;
        ~DisplayBridge() { close(); }
        DisplayBridge (const DisplayBridge&) = delete;
        DisplayBridge& operator= (const DisplayBridge&) = delete;

        /** Opens (and, with `create`, creates when missing) the block for `key`. Not real-time: call it
            from a normal thread (it may wait up to ~200 ms for a creator in another process to finish). */
        OpenResult open (const std::string& key, bool create, Scope scope = Scope::session);
        void close() noexcept;
        bool isOpen() const noexcept     { return block != nullptr; }
        bool createdIt() const noexcept  { return created; }

        /** Key -> the OS object name ("/ENHMaster-v1-<key>" or "Global\ENHMaster-v1-<key>"). Keys are
            1..40 characters of [A-Za-z0-9_-]; anything else is rejected (badName). */
        static bool isValidKey (const std::string& key) noexcept;
        static std::string objectName (const std::string& key, Scope scope);

        /** Linux only: removes the name (mappings stay valid until closed). The objects are ~70 KB and
            are otherwise kept until reboot, on purpose: see DisplayBridge.cpp. No-op on Windows. */
        static void unlinkName (const std::string& key) noexcept;

        //------------------------------------------------------------------------------------------
        // UI side
        bool publishParams (const ParamSnapshot& p) noexcept { return block != nullptr && block->params.write (p); }
        SeqBlock<MeterBlock>::Read readMeters (MeterBlock& out, std::uint32_t& lastSeq, int maxAttempts = 4) noexcept
        {
            return block != nullptr ? block->meters.read (out, lastSeq, maxAttempts) : SeqBlock<MeterBlock>::Read::empty;
        }
        /** Hands the samples written since `readIndex` (at most kScopeSize, the newest) to
            `sink (const float* samples, int n)` in runs of up to 256, advances readIndex; returns how
            many. A sample may be overwritten while it is read (a scope can live with that). */
        template <typename Sink>
        int readScope (int which, std::uint32_t& readIndex, Sink&& sink) const noexcept;

        // Audio side (real-time safe)
        bool publishMeters (const MeterBlock& m) noexcept { return block != nullptr && block->meters.write (m); }
        SeqBlock<ParamSnapshot>::Read readParams (ParamSnapshot& out, std::uint32_t& lastSeq, int maxAttempts = 2) noexcept
        {
            return block != nullptr ? block->params.read (out, lastSeq, maxAttempts) : SeqBlock<ParamSnapshot>::Read::empty;
        }
        void pushScope (int which, const float* samples, int n) noexcept;

        // Heartbeats (either side, real-time safe)
        void beat (Role self, std::uint32_t nowMsValue) noexcept;
        void beat (Role self) noexcept { beat (self, nowMs()); }
        /** The other side beat within `timeoutMs` of `nowMsValue` (wrap-safe). */
        bool isAlive (Role who, std::uint32_t timeoutMs, std::uint32_t nowMsValue) const noexcept;
        bool isAlive (Role who, std::uint32_t timeoutMs) const noexcept { return isAlive (who, timeoutMs, nowMs()); }
        std::uint32_t beatCount (Role who) const noexcept;

        /** System-wide monotonic milliseconds (CLOCK_MONOTONIC / GetTickCount64), low 32 bits. The same
            clock in every process on the machine, so beats can be compared across processes. */
        static std::uint32_t nowMs() noexcept;

        /** Tests only: overwrite the version in the shared header (to check that a mismatch is refused). */
        void overwriteVersionForTest (std::uint32_t v) noexcept { if (block != nullptr) block->header.version.store (v); }

    private:
        BridgeLayout* block = nullptr;
        bool created = false;
       #if defined (_WIN32)
        void* mappingHandle = nullptr;
       #else
        int fd = -1;
       #endif
    };

    //==============================================================================================
    template <typename Sink>
    int DisplayBridge::readScope (int which, std::uint32_t& readIndex, Sink&& sink) const noexcept
    {
        if (block == nullptr || which < 0 || which >= kNumScopes)
            return 0;
        const auto& ring = block->scopes[which];
        const std::uint32_t w = ring.writeIndex.load (std::memory_order_acquire);
        std::uint32_t available = w - readIndex;                  // wrap-safe
        if (available > (std::uint32_t) kScopeSize)
            readIndex = w - (std::uint32_t) kScopeSize, available = (std::uint32_t) kScopeSize;

        float run[256];
        int total = 0;
        while (available > 0)
        {
            const int n = (int) (available < 256u ? available : 256u);
            for (int i = 0; i < n; ++i)
            {
                const std::uint32_t bits = ring.samples[(readIndex + (std::uint32_t) i) & (kScopeSize - 1)].load (std::memory_order_relaxed);
                std::memcpy (&run[i], &bits, 4);
            }
            sink (static_cast<const float*> (run), n);
            readIndex += (std::uint32_t) n;
            available -= (std::uint32_t) n;
            total += n;
        }
        return total;
    }
}
