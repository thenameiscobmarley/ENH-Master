/*  ENH Master's Windows system effect: the COM object and the DLL around ApoCore.
    See EnhApo.h. UNVERIFIED - never compiled (no Windows SDK where it was written); build and test on
    Windows before shipping. Off by default: cmake -DENH_BUILD_APO=ON (cmake/EnhApo.cmake). */

#if defined (_WIN32)

#include "EnhApo.h"
#include "EnhApoGuids.h"
#include "../Shared/ParamSnapshotFile.h"

#include <objbase.h>
#include <olectl.h>   // SELFREG_E_CLASS
#include <new>
#include <string>

#pragma comment (lib, "ole32.lib")
#pragma comment (lib, "advapi32.lib")

namespace
{
    HMODULE thisModule = nullptr;
    std::atomic<LONG> liveObjects { 0 }, serverLocks { 0 };

    // KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, spelled out (avoids INITGUID / ksmedia.h linkage questions)
    constexpr GUID subtypeFloat = { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

    constexpr HNSTIME estimatedLatency = 120000;   // 12 ms in 100 ns units, until the real rate is known

    struct Shape
    {
        UINT32 channels = 0;
        float rate = 0.0f;
    };

    /** 32-bit float, uncompressed, 1..32 channels, 8 kHz .. 384 kHz. */
    bool readShape (IAudioMediaType* type, Shape& shape) noexcept
    {
        if (type == nullptr)
            return false;
        BOOL compressed = TRUE;
        if (FAILED (type->IsCompressedFormat (&compressed)) || compressed)
            return false;
        UNCOMPRESSEDAUDIOFORMAT f {};
        if (FAILED (type->GetUncompressedAudioFormat (&f)))
            return false;
        if (! IsEqualGUID (f.guidFormatType, subtypeFloat) || f.dwBytesPerSampleContainer != 4 || f.dwValidBitsPerSample != 32)
            return false;
        if (f.dwSamplesPerFrame < 1 || f.dwSamplesPerFrame > 32 || ! (f.fFramesPerSecond >= 8000.0f && f.fFramesPerSecond <= 384000.0f))
            return false;
        shape.channels = f.dwSamplesPerFrame;
        shape.rate = f.fFramesPerSecond;
        return true;
    }
}

namespace enh::apo
{
    EnhApo::EnhApo()  { ++liveObjects; }
    EnhApo::~EnhApo() { core.closeBridge(); --liveObjects; }

    //==============================================================================================
    // IUnknown
    STDMETHODIMP EnhApo::QueryInterface (REFIID riid, void** ppv)
    {
        if (ppv == nullptr)
            return E_POINTER;
        *ppv = nullptr;
        if (riid == __uuidof (IUnknown) || riid == __uuidof (IAudioProcessingObject))
            *ppv = static_cast<IAudioProcessingObject*> (this);
        else if (riid == __uuidof (IAudioProcessingObjectRT))
            *ppv = static_cast<IAudioProcessingObjectRT*> (this);
        else if (riid == __uuidof (IAudioProcessingObjectConfiguration))
            *ppv = static_cast<IAudioProcessingObjectConfiguration*> (this);
        else if (riid == __uuidof (IAudioSystemEffects))
            *ppv = static_cast<IAudioSystemEffects*> (static_cast<IAudioSystemEffects2*> (this));
        else if (riid == __uuidof (IAudioSystemEffects2))
            *ppv = static_cast<IAudioSystemEffects2*> (this);
        else
            return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }

    STDMETHODIMP_(ULONG) EnhApo::AddRef()  { return ++refs; }

    STDMETHODIMP_(ULONG) EnhApo::Release()
    {
        const ULONG left = --refs;
        if (left == 0)
            delete this;
        return left;
    }

    //==============================================================================================
    // IAudioProcessingObject
    STDMETHODIMP EnhApo::Reset()
    {
        return S_OK;   // the engine starts clean at every LockForProcess; nothing is kept between streams
    }

    STDMETHODIMP EnhApo::GetLatency (HNSTIME* pTime)
    {
        if (pTime == nullptr)
            return E_POINTER;
        const int samples = core.getLatencySamples();
        *pTime = (locked && samples > 0 && rate > 0.0f) ? (HNSTIME) ((double) samples * 1.0e7 / (double) rate + 0.5)
                                                          : estimatedLatency;
        return S_OK;
    }

    void EnhApo::fillRegistration (APO_REG_PROPERTIES& props) noexcept
    {
        ZeroMemory (&props, sizeof (props));
        props.clsid = ENH_APO_CLSID;
        props.Flags = (APO_FLAG) (APO_FLAG_INPLACE | APO_FLAG_SAMPLESPERFRAME_MUST_MATCH
                                  | APO_FLAG_FRAMESPERSECOND_MUST_MATCH | APO_FLAG_BITSPERSAMPLE_MUST_MATCH);
        wcsncpy_s (props.szFriendlyName, ENH_APO_FRIENDLY_NAME, _TRUNCATE);
        wcsncpy_s (props.szCopyrightInfo, ENH_APO_COPYRIGHT, _TRUNCATE);
        props.u32MajorVersion = ENH_APO_MAJOR_VERSION;
        props.u32MinorVersion = ENH_APO_MINOR_VERSION;
        props.u32MinInputConnections = 1;
        props.u32MaxInputConnections = 1;
        props.u32MinOutputConnections = 1;
        props.u32MaxOutputConnections = 1;
        props.u32MaxInstances = 0xffffffff;
        props.u32NumAPOInterfaces = 1;
        props.iidAPOInterfaceList[0] = __uuidof (IAudioProcessingObject);
    }

    STDMETHODIMP EnhApo::GetRegistrationProperties (APO_REG_PROPERTIES** ppRegProps)
    {
        if (ppRegProps == nullptr)
            return E_POINTER;
        auto* props = static_cast<APO_REG_PROPERTIES*> (CoTaskMemAlloc (sizeof (APO_REG_PROPERTIES)));
        if (props == nullptr)
            return E_OUTOFMEMORY;
        fillRegistration (*props);
        *ppRegProps = props;
        return S_OK;
    }

    STDMETHODIMP EnhApo::Initialize (UINT32 cbDataSize, BYTE* pbyData)
    {
        if (initialized)
            return APOERR_ALREADY_INITIALIZED;
        if (pbyData == nullptr && cbDataSize != 0)
            return E_POINTER;

        // Windows 8.1+ passes APOInitSystemEffects2 (Windows 11: APOInitSystemEffects3, which starts the
        // same way). Discovery-only instances are made just to ask questions: they must not start anything.
        if (pbyData != nullptr && cbDataSize >= sizeof (APOInitSystemEffects2))
        {
            const auto* init = reinterpret_cast<const APOInitSystemEffects2*> (pbyData);
            discoveryOnly = init->InitializeForDiscoveryOnly != FALSE;
        }
        initialized = true;
        return S_OK;
    }

    HRESULT EnhApo::checkFormat (IAudioMediaType* opposite, IAudioMediaType* requested, IAudioMediaType** supported) noexcept
    {
        if (requested == nullptr || supported == nullptr)
            return E_POINTER;
        *supported = nullptr;

        Shape want, other;
        if (! readShape (requested, want))
            return APOERR_FORMAT_NOT_SUPPORTED;
        // In and out must be the same (APO_FLAG_*_MUST_MATCH): same channels, same rate
        if (opposite != nullptr && (! readShape (opposite, other) || other.channels != want.channels || other.rate != want.rate))
            return APOERR_FORMAT_NOT_SUPPORTED;

        requested->AddRef();
        *supported = requested;
        return S_OK;
    }

    STDMETHODIMP EnhApo::IsInputFormatSupported (IAudioMediaType* pOppositeFormat, IAudioMediaType* pRequestedInputFormat,
                                                 IAudioMediaType** ppSupportedInputFormat)
    {
        return checkFormat (pOppositeFormat, pRequestedInputFormat, ppSupportedInputFormat);
    }

    STDMETHODIMP EnhApo::IsOutputFormatSupported (IAudioMediaType* pOppositeFormat, IAudioMediaType* pRequestedOutputFormat,
                                                  IAudioMediaType** ppSupportedOutputFormat)
    {
        return checkFormat (pOppositeFormat, pRequestedOutputFormat, ppSupportedOutputFormat);
    }

    STDMETHODIMP EnhApo::GetInputChannelCount (UINT32* pu32ChannelCount)
    {
        if (pu32ChannelCount == nullptr)
            return E_POINTER;
        *pu32ChannelCount = channels;
        return S_OK;
    }

    //==============================================================================================
    // IAudioProcessingObjectConfiguration (not real-time: allocate here)
    STDMETHODIMP EnhApo::LockForProcess (UINT32 u32NumInputConnections, APO_CONNECTION_DESCRIPTOR** ppInputConnections,
                                         UINT32 u32NumOutputConnections, APO_CONNECTION_DESCRIPTOR** ppOutputConnections)
    {
        if (! initialized)
            return APOERR_NOT_INITIALIZED;
        if (locked)
            return APOERR_ALREADY_LOCKED;
        if (u32NumInputConnections != 1 || u32NumOutputConnections != 1 || ppInputConnections == nullptr
             || ppOutputConnections == nullptr || ppInputConnections[0] == nullptr || ppOutputConnections[0] == nullptr)
            return APOERR_INVALID_CONNECTION_FORMAT;

        Shape in, out;
        if (! readShape (ppInputConnections[0]->pFormat, in) || ! readShape (ppOutputConnections[0]->pFormat, out)
             || in.channels != out.channels || in.rate != out.rate)
            return APOERR_INVALID_CONNECTION_FORMAT;

        channels = in.channels;
        rate = in.rate;
        const UINT32 maxFrames = ppInputConnections[0]->u32MaxFrameCount;

        try
        {
            // If the engine can't be prepared, ApoCore stays in pass-through: the user keeps their sound
            core.prepare ((double) rate, (int) channels, (int) (maxFrames > 0 ? maxFrames : 480));
            if (! discoveryOnly)
            {
                loadSavedSettings();
                core.openBridge (ENH_APO_BRIDGE_KEY, enh::shared::DisplayBridge::Scope::machine);
            }
        }
        catch (...) {}

        locked = true;
        return S_OK;
    }

    STDMETHODIMP EnhApo::UnlockForProcess()
    {
        if (! locked)
            return APOERR_NOT_LOCKED;
        core.release();
        locked = false;
        return S_OK;
    }

    void EnhApo::loadSavedSettings() noexcept
    {
        wchar_t base[MAX_PATH] {};
        const DWORD n = GetEnvironmentVariableW (L"ProgramData", base, MAX_PATH);
        std::wstring path = (n > 0 && n < MAX_PATH) ? std::wstring (base) : std::wstring (L"C:\\ProgramData");
        path += L"\\ENH Master\\system-effect-settings.bin";
        enh::shared::ParamSnapshot saved;
        if (enh::shared::loadSnapshotFile (path.c_str(), saved))
            core.useSettings (saved);
    }

    //==============================================================================================
    // IAudioProcessingObjectRT
    STDMETHODIMP_(void) EnhApo::APOProcess (UINT32 u32NumInputConnections, APO_CONNECTION_PROPERTY** ppInputConnections,
                                            UINT32 u32NumOutputConnections, APO_CONNECTION_PROPERTY** ppOutputConnections)
    {
        if (u32NumInputConnections < 1 || u32NumOutputConnections < 1 || ppInputConnections == nullptr
             || ppOutputConnections == nullptr || ppInputConnections[0] == nullptr || ppOutputConnections[0] == nullptr)
            return;

        APO_CONNECTION_PROPERTY* in = ppInputConnections[0];
        APO_CONNECTION_PROPERTY* out = ppOutputConnections[0];
        const UINT32 frames = in->u32ValidFrameCount;

        if (in->u32BufferFlags == BUFFER_INVALID)
        {
            out->u32ValidFrameCount = frames;
            out->u32BufferFlags = BUFFER_INVALID;
            return;
        }

        const bool inputSilent = in->u32BufferFlags == BUFFER_SILENT;
        const auto* src = reinterpret_cast<const float*> (in->pBuffer);
        auto* dst = reinterpret_cast<float*> (out->pBuffer);

        // ApoCore::process is noexcept and passes the input through unchanged when it isn't prepared
        const bool outputSilent = core.process (src, dst, frames, inputSilent);

        out->u32ValidFrameCount = frames;
        out->u32BufferFlags = outputSilent ? BUFFER_SILENT : BUFFER_VALID;
    }

    STDMETHODIMP_(UINT32) EnhApo::CalcInputFrames (UINT32 u32OutputFrameCount)  { return u32OutputFrameCount; }
    STDMETHODIMP_(UINT32) EnhApo::CalcOutputFrames (UINT32 u32InputFrameCount)  { return u32InputFrameCount; }

    //==============================================================================================
    // IAudioSystemEffects2: the effects the Sound control panel may list. We list none (ENH Master is one
    // effect with its own UI, not an equaliser / bass boost the OS can toggle).
    STDMETHODIMP EnhApo::GetEffectsList (LPGUID* ppEffectsIds, UINT* pcEffects, HANDLE)
    {
        if (ppEffectsIds == nullptr || pcEffects == nullptr)
            return E_POINTER;
        *ppEffectsIds = nullptr;
        *pcEffects = 0;
        return S_OK;
    }
}

//==================================================================================================
// Class factory and the DLL's exports (EnhApo.def)

namespace
{
    class Factory final : public IClassFactory
    {
    public:
        STDMETHODIMP QueryInterface (REFIID riid, void** ppv) override
        {
            if (ppv == nullptr)
                return E_POINTER;
            *ppv = nullptr;
            if (riid == __uuidof (IUnknown) || riid == __uuidof (IClassFactory))
            {
                *ppv = static_cast<IClassFactory*> (this);
                return S_OK;   // static object: no reference counting
            }
            return E_NOINTERFACE;
        }
        STDMETHODIMP_(ULONG) AddRef() override  { return 2; }
        STDMETHODIMP_(ULONG) Release() override { return 1; }

        STDMETHODIMP CreateInstance (IUnknown* outer, REFIID riid, void** ppv) override
        {
            if (ppv == nullptr)
                return E_POINTER;
            *ppv = nullptr;
            if (outer != nullptr)
                return CLASS_E_NOAGGREGATION;
            auto* apo = new (std::nothrow) enh::apo::EnhApo();
            if (apo == nullptr)
                return E_OUTOFMEMORY;
            const HRESULT hr = apo->QueryInterface (riid, ppv);
            apo->Release();   // the QI's reference is the caller's (or the object goes if QI failed)
            return hr;
        }

        STDMETHODIMP LockServer (BOOL lock) override
        {
            if (lock) ++serverLocks; else --serverLocks;
            return S_OK;
        }
    };

    Factory factory;

    // Registry, the way RegisterAPO () writes it, under HKLM\SOFTWARE\Classes (= HKCR, machine-wide).
    // RegisterAPO itself is declared in audioenginebaseapo.h but its import library is not in every
    // SDK; define ENH_APO_USE_REGISTERAPO to call it instead (it writes the same AudioEngine key).
    constexpr const wchar_t* clsidKey = L"SOFTWARE\\Classes\\CLSID\\" ENH_APO_CLSID_STRING;
    constexpr const wchar_t* apoKey   = L"SOFTWARE\\Classes\\AudioEngine\\AudioProcessingObjects\\" ENH_APO_CLSID_STRING;

    LSTATUS setString (HKEY key, const wchar_t* name, const std::wstring& value)
    {
        return RegSetValueExW (key, name, 0, REG_SZ, reinterpret_cast<const BYTE*> (value.c_str()),
                               (DWORD) ((value.size() + 1) * sizeof (wchar_t)));
    }

    LSTATUS setDword (HKEY key, const wchar_t* name, DWORD value)
    {
        return RegSetValueExW (key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*> (&value), sizeof (value));
    }

    std::wstring guidString (const GUID& g)
    {
        wchar_t text[64] {};
        StringFromGUID2 (g, text, 64);
        return text;
    }

    HRESULT registerServer()
    {
        wchar_t path[MAX_PATH] {};
        const DWORD n = GetModuleFileNameW (thisModule, path, MAX_PATH);
        if (n == 0 || n >= MAX_PATH)
            return E_FAIL;

        HKEY key = nullptr;
        if (RegCreateKeyExW (HKEY_LOCAL_MACHINE, clsidKey, 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS)
            return SELFREG_E_CLASS;
        setString (key, nullptr, ENH_APO_FRIENDLY_NAME);
        HKEY server = nullptr;
        LSTATUS st = RegCreateKeyExW (key, L"InprocServer32", 0, nullptr, 0, KEY_WRITE, nullptr, &server, nullptr);
        if (st == ERROR_SUCCESS)
        {
            st = setString (server, nullptr, path);
            if (st == ERROR_SUCCESS)
                st = setString (server, L"ThreadingModel", L"Both");
            RegCloseKey (server);
        }
        RegCloseKey (key);
        if (st != ERROR_SUCCESS)
            return SELFREG_E_CLASS;

        APO_REG_PROPERTIES props;
        enh::apo::EnhApo::fillRegistration (props);
       #if defined (ENH_APO_USE_REGISTERAPO)
        return RegisterAPO (&props);
       #else
        if (RegCreateKeyExW (HKEY_LOCAL_MACHINE, apoKey, 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS)
            return SELFREG_E_CLASS;
        st = setString (key, L"FriendlyName", props.szFriendlyName);
        if (st == ERROR_SUCCESS) st = setString (key, L"Copyright", props.szCopyrightInfo);
        if (st == ERROR_SUCCESS) st = setDword (key, L"MajorVersion", props.u32MajorVersion);
        if (st == ERROR_SUCCESS) st = setDword (key, L"MinorVersion", props.u32MinorVersion);
        if (st == ERROR_SUCCESS) st = setDword (key, L"Flags", (DWORD) props.Flags);
        if (st == ERROR_SUCCESS) st = setDword (key, L"MinInputConnections", props.u32MinInputConnections);
        if (st == ERROR_SUCCESS) st = setDword (key, L"MaxInputConnections", props.u32MaxInputConnections);
        if (st == ERROR_SUCCESS) st = setDword (key, L"MinOutputConnections", props.u32MinOutputConnections);
        if (st == ERROR_SUCCESS) st = setDword (key, L"MaxOutputConnections", props.u32MaxOutputConnections);
        if (st == ERROR_SUCCESS) st = setDword (key, L"MaxInstances", props.u32MaxInstances);
        if (st == ERROR_SUCCESS) st = setDword (key, L"NumAPOInterfaces", props.u32NumAPOInterfaces);
        if (st == ERROR_SUCCESS) st = setString (key, L"APOInterface0", guidString (props.iidAPOInterfaceList[0]));
        RegCloseKey (key);
        return st == ERROR_SUCCESS ? S_OK : SELFREG_E_CLASS;
       #endif
    }

    HRESULT unregisterServer()
    {
       #if defined (ENH_APO_USE_REGISTERAPO)
        UnregisterAPO (ENH_APO_CLSID);
       #else
        const LSTATUS a = RegDeleteTreeW (HKEY_LOCAL_MACHINE, apoKey);
       #endif
        const LSTATUS c = RegDeleteTreeW (HKEY_LOCAL_MACHINE, clsidKey);
       #if defined (ENH_APO_USE_REGISTERAPO)
        const LSTATUS a = ERROR_SUCCESS;
       #endif
        const auto ok = [] (LSTATUS s) { return s == ERROR_SUCCESS || s == ERROR_FILE_NOT_FOUND; };
        return ok (a) && ok (c) ? S_OK : SELFREG_E_CLASS;
    }
}

extern "C" BOOL WINAPI DllMain (HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        thisModule = instance;
        DisableThreadLibraryCalls (instance);
    }
    return TRUE;
}

STDAPI DllGetClassObject (REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    if (ppv == nullptr)
        return E_POINTER;
    *ppv = nullptr;
    if (! IsEqualCLSID (rclsid, ENH_APO_CLSID))
        return CLASS_E_CLASSNOTAVAILABLE;
    return factory.QueryInterface (riid, ppv);
}

STDAPI DllCanUnloadNow()
{
    return liveObjects.load() == 0 && serverLocks.load() == 0 ? S_OK : S_FALSE;
}

STDAPI DllRegisterServer()
{
    try { return registerServer(); } catch (...) { return E_FAIL; }
}

STDAPI DllUnregisterServer()
{
    try { return unregisterServer(); } catch (...) { return E_FAIL; }
}

#endif
