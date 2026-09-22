#include "AudioRouting.h"

#if JUCE_WINDOWS

#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>
#include <inspectable.h>
#include <roapi.h>
#include <winstring.h>

#pragma comment (lib, "runtimeobject.lib")   // RoGetActivationFactory, WindowsCreateString

#include <algorithm>
#include <utility>

/*  Windows.

    - The default playback device: IPolicyConfig, the interface the Sound control panel itself uses.
      Not in the SDK, but its layout has not changed since Windows 7 (every "switch audio device"
      tool relies on it).
    - Which device one program plays to: IAudioPolicyConfigFactory, behind Settings > System > Sound >
      Volume mixer (Windows 10 1803 and later). Also not in the SDK; it has had two interface ids (the
      one from 21H2 on, and the one before), and we try both. This is the part most likely to break
      on some future Windows, so when it fails the router says so and whole-system mode still works.
    - Who is playing: the audio sessions on every active playback device.

    Everything here runs on the message thread (and in the watchdog), never on the audio thread.
*/
namespace pad::routing
{
    namespace
    {
        template <typename T>
        class Com
        {
        public:
            Com() = default;
            ~Com() { reset(); }
            Com (const Com&) = delete;
            Com& operator= (const Com&) = delete;

            T* get() const noexcept        { return p; }
            T* operator->() const noexcept { return p; }
            explicit operator bool() const noexcept { return p != nullptr; }
            T** put() { reset(); return &p; }
            void** putVoid() { return reinterpret_cast<void**> (put()); }
            void reset() { if (auto* q = std::exchange (p, nullptr)) q->Release(); }

        private:
            T* p = nullptr;
        };

        struct ComScope
        {
            ComScope() : hr (CoInitializeEx (nullptr, COINIT_MULTITHREADED)) {}   // fails harmlessly on an STA thread
            ~ComScope() { if (SUCCEEDED (hr)) CoUninitialize(); }
            HRESULT hr;
        };

        //==============================================================================
        // IPolicyConfig (Windows 7 and later). Only SetDefaultEndpoint is called; the rest are here to
        // put it at the right place in the vtable.
        static const GUID clsidPolicyConfigClient { 0x870af99c, 0x171d, 0x4f9e, { 0xaf, 0x0d, 0xe6, 0x3d, 0xf4, 0x0c, 0x2b, 0xc9 } };
        static const GUID iidPolicyConfig         { 0xf8679f50, 0x850a, 0x41cf, { 0x9c, 0x72, 0x43, 0x0f, 0x29, 0x02, 0x90, 0xc8 } };

        struct IPolicyConfig : public IUnknown
        {
            virtual HRESULT STDMETHODCALLTYPE GetMixFormat (PCWSTR, WAVEFORMATEX**) = 0;
            virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat (PCWSTR, INT, WAVEFORMATEX**) = 0;
            virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat (PCWSTR) = 0;
            virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat (PCWSTR, WAVEFORMATEX*, WAVEFORMATEX*) = 0;
            virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod (PCWSTR, INT, PINT64, PINT64) = 0;
            virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod (PCWSTR, PINT64) = 0;
            virtual HRESULT STDMETHODCALLTYPE GetShareMode (PCWSTR, void*) = 0;
            virtual HRESULT STDMETHODCALLTYPE SetShareMode (PCWSTR, void*) = 0;
            virtual HRESULT STDMETHODCALLTYPE GetPropertyValue (PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
            virtual HRESULT STDMETHODCALLTYPE SetPropertyValue (PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
            virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint (PCWSTR deviceId, ERole role) = 0;
            virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility (PCWSTR, INT) = 0;
        };

        //==============================================================================
        // IAudioPolicyConfigFactory (Windows 10 1803 and later). An IInspectable (six slots), nineteen
        // methods we never call, then the three that matter.
        static const GUID iidPolicyFactory21H2      { 0xab3d4648, 0xe242, 0x459f, { 0xb0, 0x2f, 0x54, 0x1c, 0x70, 0x30, 0x63, 0x24 } };
        static const GUID iidPolicyFactoryDownlevel { 0x2a59116d, 0x6c4f, 0x45e0, { 0xa7, 0x4f, 0x70, 0x7e, 0x3f, 0xef, 0x92, 0x58 } };

        struct IAudioPolicyConfigFactory : public IInspectable
        {
            virtual HRESULT STDMETHODCALLTYPE unused00() = 0;  virtual HRESULT STDMETHODCALLTYPE unused01() = 0;
            virtual HRESULT STDMETHODCALLTYPE unused02() = 0;  virtual HRESULT STDMETHODCALLTYPE unused03() = 0;
            virtual HRESULT STDMETHODCALLTYPE unused04() = 0;  virtual HRESULT STDMETHODCALLTYPE unused05() = 0;
            virtual HRESULT STDMETHODCALLTYPE unused06() = 0;  virtual HRESULT STDMETHODCALLTYPE unused07() = 0;
            virtual HRESULT STDMETHODCALLTYPE unused08() = 0;  virtual HRESULT STDMETHODCALLTYPE unused09() = 0;
            virtual HRESULT STDMETHODCALLTYPE unused10() = 0;  virtual HRESULT STDMETHODCALLTYPE unused11() = 0;
            virtual HRESULT STDMETHODCALLTYPE unused12() = 0;  virtual HRESULT STDMETHODCALLTYPE unused13() = 0;
            virtual HRESULT STDMETHODCALLTYPE unused14() = 0;  virtual HRESULT STDMETHODCALLTYPE unused15() = 0;
            virtual HRESULT STDMETHODCALLTYPE unused16() = 0;  virtual HRESULT STDMETHODCALLTYPE unused17() = 0;
            virtual HRESULT STDMETHODCALLTYPE unused18() = 0;

            virtual HRESULT STDMETHODCALLTYPE SetPersistedDefaultAudioEndpoint (UINT processId, EDataFlow, ERole, HSTRING deviceId) = 0;
            virtual HRESULT STDMETHODCALLTYPE GetPersistedDefaultAudioEndpoint (UINT processId, EDataFlow, ERole, HSTRING* deviceId) = 0;
            virtual HRESULT STDMETHODCALLTYPE ClearAllPersistedApplicationDefaultEndpoints() = 0;
        };

        // The per-app setting names devices in a longer form than the MMDevice id.
        constexpr const wchar_t* packedPrefix = L"\\\\?\\SWD#MMDEVAPI#";
        constexpr const wchar_t* renderInterfaceSuffix = L"#{e6327cad-dcec-4949-ae8a-991e976a79d2}";

        juce::String packDeviceId (const juce::String& id)   { return juce::String (packedPrefix) + id + juce::String (renderInterfaceSuffix); }

        juce::String unpackDeviceId (juce::String packed)
        {
            if (packed.startsWith (juce::String (packedPrefix)))
                packed = packed.substring (juce::String (packedPrefix).length());

            return packed.upToFirstOccurrenceOf ("#", false, false);
        }

        struct HString
        {
            HSTRING h = nullptr;
            explicit HString (const juce::String& s)
            {
                if (s.isNotEmpty())
                    WindowsCreateString (s.toWideCharPointer(), (UINT32) wcslen (s.toWideCharPointer()), &h);
            }
            ~HString() { if (h != nullptr) WindowsDeleteString (h); }
        };

        juce::String processExeName (DWORD pid)
        {
            juce::String name;
            if (auto h = OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid))
            {
                wchar_t path[MAX_PATH * 2] {};
                DWORD size = MAX_PATH * 2;
                if (QueryFullProcessImageNameW (h, 0, path, &size))
                    name = juce::File (juce::String (path)).getFileName();
                CloseHandle (h);
            }
            return name;
        }

        juce::String deviceName (IMMDevice* device)
        {
            Com<IPropertyStore> props;
            if (FAILED (device->OpenPropertyStore (STGM_READ, props.put())) || ! props)
                return {};

            PROPVARIANT v;
            PropVariantInit (&v);
            juce::String name;
            if (SUCCEEDED (props->GetValue (PKEY_Device_FriendlyName, &v)) && v.vt == VT_LPWSTR && v.pwszVal != nullptr)
                name = juce::String (v.pwszVal);
            PropVariantClear (&v);
            return name;
        }

        juce::String deviceId (IMMDevice* device)
        {
            LPWSTR id = nullptr;
            juce::String s;
            if (SUCCEEDED (device->GetId (&id)) && id != nullptr)
            {
                s = juce::String (id);
                CoTaskMemFree (id);
            }
            return s;
        }
    }

    //==============================================================================
    class WindowsBackend final : public Backend
    {
    public:
        std::vector<Endpoint> outputs() override
        {
            ComScope com;
            std::vector<Endpoint> found;
            const auto def = defaultOutput();

            forEachDevice ([&] (IMMDevice* d)
            {
                Endpoint e { deviceId (d), deviceName (d), false };
                e.isDefault = e.id == def;
                if (e.name.isEmpty())
                    e.name = "Playback device " + juce::String ((int) found.size() + 1);
                found.push_back (e);
            });

            return found;
        }

        juce::String defaultOutput() override
        {
            ComScope com;
            Com<IMMDeviceEnumerator> e;
            if (! makeEnumerator (e))
                return {};

            Com<IMMDevice> d;
            if (FAILED (e->GetDefaultAudioEndpoint (eRender, eMultimedia, d.put())) || ! d)
                return {};

            return deviceId (d.get());
        }

        bool setDefaultOutput (const juce::String& id) override
        {
            ComScope com;
            Com<IPolicyConfig> policy;
            if (FAILED (CoCreateInstance (clsidPolicyConfigClient, nullptr, CLSCTX_ALL, iidPolicyConfig, policy.putVoid())) || ! policy)
            {
                error = "Windows would not let ENH Master change the default device.";
                return false;
            }

            // All three roles, as the Sound control panel does: games, media and calls all follow.
            for (auto role : { eConsole, eMultimedia, eCommunications })
            {
                if (FAILED (policy->SetDefaultEndpoint (id.toWideCharPointer(), role)))
                {
                    error = "Windows refused to change the default device.";
                    return false;
                }
            }

            return true;
        }

        std::vector<App> apps() override
        {
            ComScope com;
            std::vector<App> found;
            const auto ourPid = GetCurrentProcessId();

            forEachDevice ([&] (IMMDevice* device)
            {
                const auto endpoint = deviceId (device);

                Com<IAudioSessionManager2> manager;
                if (FAILED (device->Activate (__uuidof (IAudioSessionManager2), CLSCTX_ALL, nullptr, manager.putVoid())) || ! manager)
                    return;

                Com<IAudioSessionEnumerator> sessions;
                if (FAILED (manager->GetSessionEnumerator (sessions.put())) || ! sessions)
                    return;

                int count = 0;
                sessions->GetCount (&count);

                for (int i = 0; i < count; ++i)
                {
                    Com<IAudioSessionControl> control;
                    if (FAILED (sessions->GetSession (i, control.put())) || ! control)
                        continue;

                    Com<IAudioSessionControl2> control2;
                    if (FAILED (control->QueryInterface (__uuidof (IAudioSessionControl2), control2.putVoid())) || ! control2)
                        continue;

                    if (control2->IsSystemSoundsSession() == S_OK)
                        continue;

                    AudioSessionState state = AudioSessionStateExpired;
                    control2->GetState (&state);
                    if (state == AudioSessionStateExpired)
                        continue;

                    DWORD pid = 0;
                    if (FAILED (control2->GetProcessId (&pid)) || pid == 0 || pid == ourPid)
                        continue;

                    const auto exe = processExeName (pid);
                    if (exe.isEmpty())
                        continue;

                    const auto key = exe.toLowerCase();
                    auto it = std::find_if (found.begin(), found.end(), [&] (const App& a) { return a.key == key; });

                    if (it == found.end())
                    {
                        App a;
                        a.key = key;
                        a.name = exe.upToLastOccurrenceOf (".", false, false);
                        a.endpointId = endpoint;
                        found.push_back (a);
                        it = found.end() - 1;
                    }

                    if (! it->handles.contains ((int) pid))
                        it->handles.add ((int) pid);

                    it->active = it->active || state == AudioSessionStateActive;
                }
            });

            for (auto& a : found)
                a.followsDefault = a.handles.isEmpty() || endpointOfHandle (a.handles.getFirst()).isEmpty();

            return found;
        }

        bool moveApp (const App& app, int handle, const juce::String& endpointId) override
        {
            ComScope com;
            Com<IAudioPolicyConfigFactory> factory;
            if (! makeFactory (factory))
                return false;

            HString target (endpointId.isNotEmpty() ? packDeviceId (endpointId) : juce::String());
            bool ok = true;

            for (auto pid : app.handles)
            {
                if (handle >= 0 && pid != handle)
                    continue;

                for (auto role : { eConsole, eMultimedia })
                {
                    if (FAILED (factory->SetPersistedDefaultAudioEndpoint ((UINT) pid, eRender, role, target.h)))
                    {
                        error = "Windows would not move " + app.name + " (per-app devices need Windows 10 1803 or later). "
                                "Whole-system mode still works.";
                        ok = false;
                    }
                }
            }

            return ok;
        }

        juce::String endpointOfHandle (int pid) override
        {
            ComScope com;
            Com<IAudioPolicyConfigFactory> factory;
            if (! makeFactory (factory))
                return {};

            HSTRING h = nullptr;
            juce::String id;
            if (SUCCEEDED (factory->GetPersistedDefaultAudioEndpoint ((UINT) pid, eRender, eMultimedia, &h)) && h != nullptr)
            {
                UINT32 length = 0;
                if (auto* raw = WindowsGetStringRawBuffer (h, &length))
                    id = unpackDeviceId (juce::String (raw, (size_t) length));
                WindowsDeleteString (h);
            }

            return id;
        }

    private:
        static bool makeEnumerator (Com<IMMDeviceEnumerator>& e)
        {
            return SUCCEEDED (CoCreateInstance (__uuidof (MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                                __uuidof (IMMDeviceEnumerator), e.putVoid())) && e;
        }

        template <typename Fn>
        static void forEachDevice (Fn&& fn)
        {
            Com<IMMDeviceEnumerator> e;
            if (! makeEnumerator (e))
                return;

            Com<IMMDeviceCollection> devices;
            if (FAILED (e->EnumAudioEndpoints (eRender, DEVICE_STATE_ACTIVE, devices.put())) || ! devices)
                return;

            UINT count = 0;
            devices->GetCount (&count);

            for (UINT i = 0; i < count; ++i)
            {
                Com<IMMDevice> d;
                if (SUCCEEDED (devices->Item (i, d.put())) && d)
                    fn (d.get());
            }
        }

        bool makeFactory (Com<IAudioPolicyConfigFactory>& factory)
        {
            HString name (juce::String ("Windows.Media.Internal.AudioPolicyConfig"));

            for (auto& iid : { iidPolicyFactory21H2, iidPolicyFactoryDownlevel })
                if (SUCCEEDED (RoGetActivationFactory (name.h, iid, factory.putVoid())) && factory)
                    return true;

            error = "This version of Windows does not let ENH Master move single apps. Whole-system mode still works.";
            return false;
        }
    };

    std::unique_ptr<Backend> createBackend() { return std::make_unique<WindowsBackend>(); }
}

#endif

#if ! JUCE_WINDOWS && ! JUCE_LINUX && ! JUCE_BSD
namespace pad::routing
{
    class NoBackend final : public Backend
    {
    public:
        NoBackend() { error = "Audio routing is not available on this system."; }
        std::vector<Endpoint> outputs() override                      { return {}; }
        juce::String defaultOutput() override                         { return {}; }
        bool setDefaultOutput (const juce::String&) override          { return false; }
        std::vector<App> apps() override                              { return {}; }
        bool moveApp (const App&, int, const juce::String&) override  { return false; }
        juce::String endpointOfHandle (int) override                  { return {}; }
    };

    std::unique_ptr<Backend> createBackend() { return std::make_unique<NoBackend>(); }
}
#endif
