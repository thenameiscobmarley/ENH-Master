#include "DisplayBridge.h"

#include <chrono>
#include <thread>

#if defined (_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <sddl.h>
 #pragma comment (lib, "advapi32.lib")   // ConvertStringSecurityDescriptorToSecurityDescriptorW
#else
 #include <fcntl.h>
 #include <sys/mman.h>
 #include <sys/stat.h>
 #include <time.h>
 #include <unistd.h>
 #include <cerrno>
#endif

/*  NAMES, SECURITY, LIFETIME

    Name: "ENHMaster-v<kBridgeVersion>-<key>". The version is in the name, so an app and a system effect
    from different releases never share a block with a different layout (they simply don't see each
    other; the app says so). The header's version/size are still checked, against damage.

    Linux: shm_open ("/ENHMaster-v1-<key>", O_RDWR | O_CREAT | O_EXCL, 0600): only this user can open
    it. The name is NOT unlinked when the last user closes it: unlinking while another process is about
    to open it would leave the two sides on two different blocks. It is 70 KB in /dev/shm (tmpfs) and
    goes away at reboot; DisplayBridge::unlinkName removes it by hand (the tests do).

    Windows: CreateFileMappingW (INVALID_HANDLE_VALUE, ...) - pagefile-backed, zero-filled, gone when the
    last handle closes.
      - Scope::session -> "Local\...": the default DACL of the creating user is right (same user only).
      - Scope::machine -> "Global\...": the system effect runs inside audiodg.exe as LOCAL SERVICE in
        session 0; the app runs in the user's session. "Local\" names are per session, so the two would
        never meet: it has to be "Global\". Creating a Global\ object needs SeCreateGlobalPrivilege,
        which services (LOCAL SERVICE included) and administrators have and standard users do not - so
        the system effect creates the block, the app only opens it (OpenFileMappingW, which needs no
        privilege, only the DACL's permission). The DACL is explicit, because audiodg's default DACL
        would give the user nothing:
            D:P                      protected (no inherited ACEs)
            (A;;GA;;;SY)             SYSTEM         full
            (A;;GA;;;LS)             LOCAL SERVICE  full (audiodg)
            (A;;GA;;;BA)             Administrators full
            (A;;GRGW;;;IU)           INTERACTIVE    read + write (the signed-in user's app)
        No label is set, so the object gets the default Medium mandatory label: normal apps may open
        it, sandboxed low-integrity processes may not.
      Squatting: only a service or an administrator can create a Global\ name, so a normal user can't
      pre-create a fake block for audiodg. Any interactive user CAN write parameters into it - which is
      the point (that is the app) - so the effect treats every value as hostile: finite-checked and
      clamped to the parameter's range (RackParamTable.h), method choices clamped to the list. The
      worst a writer can do is set the knobs; EAR GUARD and the output limiter can't be switched off.

    KEY POLICY
      "system"            the Windows system effect (one endpoint per machine) and the standalone app's
                          display of it. Scope::machine.
      "link-<name>"       plugin instances the user has linked by giving them the same LINK name (a
                          user-visible instance name stored in the session). Scope::session. Instances
                          with no name never link, so two unrelated instances can never drive each other.
      A host process id is NOT used: split-process hosts run the audio instance and the editor instance
      in different processes, so a pid would never match. */

namespace enh::shared
{
    namespace
    {
        constexpr std::size_t blockSize = sizeof (BridgeLayout);

        void initialiseHeader (BridgeLayout* b) noexcept
        {
            // The OS gave us zeros; the payload's seq = 0 means "never written"
            b->header.version.store (kBridgeVersion, std::memory_order_relaxed);
            b->header.totalSize.store ((std::uint32_t) blockSize, std::memory_order_relaxed);
            b->header.magic.store (kBridgeMagic, std::memory_order_release);
        }

        /** An existing block: wait a little for its creator, then check what it is. */
        DisplayBridge::OpenResult checkHeader (const BridgeLayout* b) noexcept
        {
            using R = DisplayBridge::OpenResult;
            for (int i = 0; i < 200; ++i)
            {
                const auto magic = b->header.magic.load (std::memory_order_acquire);
                if (magic == kBridgeMagic)
                {
                    if (b->header.version.load (std::memory_order_relaxed) != kBridgeVersion)
                        return R::versionMismatch;
                    if (b->header.totalSize.load (std::memory_order_relaxed) != (std::uint32_t) blockSize)
                        return R::sizeMismatch;
                    return R::ok;
                }
                if (magic != 0)
                    return R::badData;
                std::this_thread::sleep_for (std::chrono::milliseconds (1));
            }
            return R::notReady;
        }
    }

    bool DisplayBridge::isValidKey (const std::string& key) noexcept
    {
        if (key.empty() || key.size() > 40)
            return false;
        for (char c : key)
            if (! ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_'))
                return false;
        return true;
    }

    std::string DisplayBridge::objectName (const std::string& key, Scope scope)
    {
        const std::string base = "ENHMaster-v" + std::to_string (kBridgeVersion) + "-" + key;
       #if defined (_WIN32)
        return (scope == Scope::machine ? "Global\\" : "Local\\") + base;
       #else
        (void) scope;
        return "/" + base;
       #endif
    }

    void DisplayBridge::pushScope (int which, const float* samples, int n) noexcept
    {
        if (block == nullptr || which < 0 || which >= kNumScopes || samples == nullptr || n <= 0)
            return;
        auto& ring = block->scopes[which];
        std::uint32_t w = ring.writeIndex.load (std::memory_order_relaxed);
        if (n > kScopeSize)
        {
            samples += n - kScopeSize;   // only the newest fit
            n = kScopeSize;
        }
        for (int i = 0; i < n; ++i, ++w)
        {
            std::uint32_t bits;
            std::memcpy (&bits, samples + i, 4);
            ring.samples[w & (kScopeSize - 1)].store (bits, std::memory_order_relaxed);
        }
        ring.writeIndex.store (w, std::memory_order_release);
    }

    void DisplayBridge::beat (Role self, std::uint32_t nowMsValue) noexcept
    {
        if (block == nullptr)
            return;
        auto& hb = self == Role::audio ? block->audioBeat : block->uiBeat;
        hb.lastMs.store (nowMsValue, std::memory_order_relaxed);
        hb.count.fetch_add (1u, std::memory_order_release);
    }

    bool DisplayBridge::isAlive (Role who, std::uint32_t timeoutMs, std::uint32_t nowMsValue) const noexcept
    {
        if (block == nullptr)
            return false;
        const auto& hb = who == Role::audio ? block->audioBeat : block->uiBeat;
        if (hb.count.load (std::memory_order_acquire) == 0)
            return false;   // never beat
        const std::uint32_t age = nowMsValue - hb.lastMs.load (std::memory_order_relaxed);   // wrap-safe
        return age <= timeoutMs || age > 0x80000000u;   // "in the future" (a beat racing this read) counts as alive
    }

    std::uint32_t DisplayBridge::beatCount (Role who) const noexcept
    {
        if (block == nullptr)
            return 0;
        return (who == Role::audio ? block->audioBeat : block->uiBeat).count.load (std::memory_order_acquire);
    }

    //==============================================================================================
   #if defined (_WIN32)

    std::uint32_t DisplayBridge::nowMs() noexcept
    {
        return (std::uint32_t) GetTickCount64();
    }

    DisplayBridge::OpenResult DisplayBridge::open (const std::string& key, bool create, Scope scope)
    {
        close();
        if (! isValidKey (key))
            return OpenResult::badName;

        const std::string narrow = objectName (key, scope);
        const std::wstring name (narrow.begin(), narrow.end());   // ASCII only (isValidKey)

        HANDLE h = nullptr;
        bool madeIt = false;

        if (create)
        {
            SECURITY_ATTRIBUTES sa { sizeof (SECURITY_ATTRIBUTES), nullptr, FALSE };
            PSECURITY_DESCRIPTOR sd = nullptr;
            if (scope == Scope::machine)
            {
                if (! ConvertStringSecurityDescriptorToSecurityDescriptorW (L"D:P(A;;GA;;;SY)(A;;GA;;;LS)(A;;GA;;;BA)(A;;GRGW;;;IU)",
                                                                            SDDL_REVISION_1, &sd, nullptr))
                    return OpenResult::systemError;
                sa.lpSecurityDescriptor = sd;
            }

            h = CreateFileMappingW (INVALID_HANDLE_VALUE, scope == Scope::machine ? &sa : nullptr, PAGE_READWRITE,
                                    0, (DWORD) blockSize, name.c_str());
            const DWORD err = GetLastError();
            if (sd != nullptr)
                LocalFree (sd);

            if (h == nullptr)
            {
                // A standard user can't create a Global\ object: fall back to opening an existing one
                if (err != ERROR_ACCESS_DENIED)
                    return OpenResult::systemError;
            }
            else
            {
                madeIt = err != ERROR_ALREADY_EXISTS;
            }
        }

        if (h == nullptr)
        {
            h = OpenFileMappingW (FILE_MAP_READ | FILE_MAP_WRITE, FALSE, name.c_str());
            if (h == nullptr)
            {
                const DWORD err = GetLastError();
                return err == ERROR_FILE_NOT_FOUND ? OpenResult::notFound
                     : err == ERROR_ACCESS_DENIED  ? OpenResult::accessDenied
                                                   : OpenResult::systemError;
            }
        }

        void* view = MapViewOfFile (h, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
        if (view == nullptr)
        {
            CloseHandle (h);
            return OpenResult::systemError;
        }

        // An existing mapping may be smaller than ours (a damaged or foreign object): check before touching it
        MEMORY_BASIC_INFORMATION info {};
        if (VirtualQuery (view, &info, sizeof (info)) == 0 || info.RegionSize < blockSize)
        {
            UnmapViewOfFile (view);
            CloseHandle (h);
            return OpenResult::sizeMismatch;
        }

        auto* b = static_cast<BridgeLayout*> (view);
        if (madeIt)
        {
            initialiseHeader (b);
        }
        else if (const auto r = checkHeader (b); r != OpenResult::ok)
        {
            UnmapViewOfFile (view);
            CloseHandle (h);
            return r;
        }

        block = b;
        mappingHandle = h;
        created = madeIt;
        return OpenResult::ok;
    }

    void DisplayBridge::close() noexcept
    {
        if (block != nullptr)
            UnmapViewOfFile (block);
        if (mappingHandle != nullptr)
            CloseHandle (static_cast<HANDLE> (mappingHandle));
        block = nullptr;
        mappingHandle = nullptr;
        created = false;
    }

    void DisplayBridge::unlinkName (const std::string&) noexcept {}

    //==============================================================================================
   #else

    std::uint32_t DisplayBridge::nowMs() noexcept
    {
        timespec ts {};
        clock_gettime (CLOCK_MONOTONIC, &ts);
        return (std::uint32_t) ((std::uint64_t) ts.tv_sec * 1000u + (std::uint64_t) ts.tv_nsec / 1000000u);
    }

    DisplayBridge::OpenResult DisplayBridge::open (const std::string& key, bool create, Scope scope)
    {
        close();
        if (! isValidKey (key))
            return OpenResult::badName;

        const auto name = objectName (key, scope);
        bool madeIt = false;
        int f = -1;

        if (create)
        {
            f = shm_open (name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
            if (f >= 0)
            {
                madeIt = true;
                fchmod (f, 0600);   // whatever the umask
                if (ftruncate (f, (off_t) blockSize) != 0)
                {
                    ::close (f);
                    shm_unlink (name.c_str());
                    return OpenResult::systemError;
                }
            }
            else if (errno != EEXIST)
            {
                return errno == EACCES ? OpenResult::accessDenied : OpenResult::systemError;
            }
        }

        if (f < 0)
        {
            f = shm_open (name.c_str(), O_RDWR, 0);
            if (f < 0)
                return errno == ENOENT ? OpenResult::notFound
                     : errno == EACCES ? OpenResult::accessDenied
                                       : OpenResult::systemError;

            // Its creator may not have sized it yet: wait a little
            struct stat st {};
            int tries = 0;
            while (fstat (f, &st) == 0 && st.st_size == 0 && tries++ < 200)
                std::this_thread::sleep_for (std::chrono::milliseconds (1));
            if (fstat (f, &st) != 0)
            {
                ::close (f);
                return OpenResult::systemError;
            }
            if (st.st_size == 0)
            {
                ::close (f);
                return OpenResult::notReady;
            }
            if ((std::size_t) st.st_size != blockSize)
            {
                ::close (f);
                return OpenResult::sizeMismatch;
            }
        }

        void* view = mmap (nullptr, blockSize, PROT_READ | PROT_WRITE, MAP_SHARED, f, 0);
        if (view == MAP_FAILED)
        {
            ::close (f);
            if (madeIt)
                shm_unlink (name.c_str());
            return OpenResult::systemError;
        }

        auto* b = static_cast<BridgeLayout*> (view);
        if (madeIt)
        {
            initialiseHeader (b);
        }
        else if (const auto r = checkHeader (b); r != OpenResult::ok)
        {
            munmap (view, blockSize);
            ::close (f);
            return r;
        }

        block = b;
        fd = f;
        created = madeIt;
        return OpenResult::ok;
    }

    void DisplayBridge::close() noexcept
    {
        if (block != nullptr)
            munmap (block, blockSize);
        if (fd >= 0)
            ::close (fd);
        block = nullptr;
        fd = -1;
        created = false;
    }

    void DisplayBridge::unlinkName (const std::string& key) noexcept
    {
        if (isValidKey (key))
            shm_unlink (objectName (key, Scope::session).c_str());
    }

   #endif
}
