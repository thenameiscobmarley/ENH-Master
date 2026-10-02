#pragma once

#include <atomic>
#include <cstdint>

/*  A bit per rack unit, 128 of them (the rack outgrew 64 in 3.8.0.1): THE GEAR LOCKER's stored units, the
    SIMPLE view's hidden ones. Used as a plain integer was (| & ~ >> ==, and `(m >> u) & 1u`); it converts from
    a 64-bit value (its low half: sessions saved it so, and still do - the high half beside it). */
namespace enh::dsp
{
    struct UnitMask
    {
        std::uint64_t lo = 0, hi = 0;

        constexpr UnitMask() noexcept = default;
        constexpr UnitMask (std::uint64_t low) noexcept : lo (low) {}
        constexpr UnitMask (std::uint64_t low, std::uint64_t high) noexcept : lo (low), hi (high) {}

        static constexpr UnitMask bit (int u) noexcept
        {
            return u < 0 || u >= 128 ? UnitMask {} : u < 64 ? UnitMask { std::uint64_t { 1 } << u, 0 } : UnitMask { 0, std::uint64_t { 1 } << (u - 64) };
        }
        constexpr bool test (int u) const noexcept { return u >= 0 && u < 128 && ((u < 64 ? lo >> u : hi >> (u - 64)) & 1u) != 0u; }

        friend constexpr UnitMask operator| (UnitMask a, UnitMask b) noexcept { return { a.lo | b.lo, a.hi | b.hi }; }
        friend constexpr UnitMask operator& (UnitMask a, UnitMask b) noexcept { return { a.lo & b.lo, a.hi & b.hi }; }
        friend constexpr UnitMask operator^ (UnitMask a, UnitMask b) noexcept { return { a.lo ^ b.lo, a.hi ^ b.hi }; }
        constexpr UnitMask operator~() const noexcept { return { ~lo, ~hi }; }
        constexpr UnitMask& operator|= (UnitMask b) noexcept { lo |= b.lo; hi |= b.hi; return *this; }
        constexpr UnitMask& operator&= (UnitMask b) noexcept { lo &= b.lo; hi &= b.hi; return *this; }
        friend constexpr bool operator== (UnitMask a, UnitMask b) noexcept { return a.lo == b.lo && a.hi == b.hi; }
        friend constexpr bool operator!= (UnitMask a, UnitMask b) noexcept { return ! (a == b); }
        constexpr UnitMask operator>> (int n) const noexcept
        {
            if (n <= 0) return *this;
            if (n >= 128) return {};
            if (n >= 64) return { hi >> (n - 64), 0 };
            return { (lo >> n) | (hi << (64 - n)), hi >> n };
        }
        constexpr UnitMask operator<< (int n) const noexcept
        {
            if (n <= 0) return *this;
            if (n >= 128) return {};
            if (n >= 64) return { 0, lo << (n - 64) };
            return { lo << n, (hi << n) | (lo >> (64 - n)) };
        }
    };

    /** A UnitMask shared between threads: two relaxed words. A reader may catch one half of a change for one
        block or frame - harmless here (the UI and the engine look again on the next), and it stays lock-free
        everywhere (a 16-byte std::atomic needs libatomic on Linux). */
    class AtomicUnitMask
    {
    public:
        constexpr AtomicUnitMask (UnitMask m = {}) noexcept : lo (m.lo), hi (m.hi) {}
        UnitMask load (std::memory_order o = std::memory_order_seq_cst) const noexcept { return { lo.load (o), hi.load (o) }; }
        void store (UnitMask m, std::memory_order o = std::memory_order_seq_cst) noexcept { lo.store (m.lo, o); hi.store (m.hi, o); }
        AtomicUnitMask& operator= (UnitMask m) noexcept { store (m); return *this; }
        operator UnitMask() const noexcept { return load(); }
    private:
        std::atomic<std::uint64_t> lo, hi;
    };
}
