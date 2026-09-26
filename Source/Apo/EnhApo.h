#pragma once

/*  ENH Master as a Windows system effect (an "APO", audio processing object), like Equalizer APO but
    running ENH Master's own engine. NOT the recommended way to use ENH Master on Windows - that is the
    standalone app with its router (VB-Audio Cable). See Vault/Reference/System effect (APO).md.

    !! UNVERIFIED: written against the documented Windows SDK headers (audioenginebaseapo.h,
    !! audiomediatype.h) on a machine without the Windows SDK. It has never been compiled or run. It
    !! must be built on Windows (cmake -DENH_BUILD_APO=ON) and tested on real PCs - several endpoints,
    !! drivers with and without their own effects, 44.1/48/96 kHz, stereo and 5.1/7.1 - before anyone
    !! is offered it.

    WHICH KIND OF APO: an EFX (endpoint effect, Windows 8.1+). Windows has three slots per playback
    endpoint:
        SFX  stream effect   one instance PER STREAM (per app), before the streams are mixed
        MFX  mode effect     after mixing, once per processing mode (default, movie, communications ...)
        EFX  endpoint effect after everything is mixed, once for the endpoint - every app, every mode
    ENH Master is a mastering chain with loudness, EAR GUARD and a limiter: it has to hear the final mix,
    once. As an SFX there would be one full engine per app (CPU x apps, no shared loudness, the limiter
    unable to protect the sum); as an MFX, streams in other modes (communications, some games' raw or
    "movie" streams) would bypass it or get a second instance. EFX is also applied to RAW-mode streams on
    most drivers. Exclusive-mode streams (some games, ASIO/WASAPI-exclusive players) bypass every APO -
    nothing can be done about that from an APO.

    The COM object is a thin shell: the processing is ApoCore (platform-independent, tested on Linux).

    Interfaces: IAudioProcessingObject, IAudioProcessingObjectRT, IAudioProcessingObjectConfiguration,
    IAudioSystemEffects, IAudioSystemEffects2 (the audio engine requires IAudioSystemEffects2 for
    SFX/MFX/EFX on Windows 8.1+). Implemented directly, without CBaseAudioProcessingObject: that helper
    class ships as source/lib with the WDK samples, not as a public SDK library, and we need very little
    of it. */

#if defined (_WIN32)

#ifndef NOMINMAX
 #define NOMINMAX
#endif
#include <windows.h>
#include <unknwn.h>
#include <mmreg.h>
#include <audiomediatype.h>
#include <audioenginebaseapo.h>
#include <atomic>
#include "ApoCore.h"

namespace enh::apo
{
    class EnhApo final : public IAudioProcessingObject,
                         public IAudioProcessingObjectRT,
                         public IAudioProcessingObjectConfiguration,
                         public IAudioSystemEffects2
    {
    public:
        EnhApo();
        ~EnhApo();

        // IUnknown
        STDMETHODIMP QueryInterface (REFIID riid, void** ppv) override;
        STDMETHODIMP_(ULONG) AddRef() override;
        STDMETHODIMP_(ULONG) Release() override;

        // IAudioProcessingObject
        STDMETHODIMP Reset() override;
        STDMETHODIMP GetLatency (HNSTIME* pTime) override;
        STDMETHODIMP GetRegistrationProperties (APO_REG_PROPERTIES** ppRegProps) override;
        STDMETHODIMP Initialize (UINT32 cbDataSize, BYTE* pbyData) override;
        STDMETHODIMP IsInputFormatSupported (IAudioMediaType* pOppositeFormat, IAudioMediaType* pRequestedInputFormat,
                                             IAudioMediaType** ppSupportedInputFormat) override;
        STDMETHODIMP IsOutputFormatSupported (IAudioMediaType* pOppositeFormat, IAudioMediaType* pRequestedOutputFormat,
                                              IAudioMediaType** ppSupportedOutputFormat) override;
        STDMETHODIMP GetInputChannelCount (UINT32* pu32ChannelCount) override;

        // IAudioProcessingObjectRT (real-time: no allocation, locks, COM, logging, throwing)
        STDMETHODIMP_(void) APOProcess (UINT32 u32NumInputConnections, APO_CONNECTION_PROPERTY** ppInputConnections,
                                        UINT32 u32NumOutputConnections, APO_CONNECTION_PROPERTY** ppOutputConnections) override;
        STDMETHODIMP_(UINT32) CalcInputFrames (UINT32 u32OutputFrameCount) override;
        STDMETHODIMP_(UINT32) CalcOutputFrames (UINT32 u32InputFrameCount) override;

        // IAudioProcessingObjectConfiguration
        STDMETHODIMP LockForProcess (UINT32 u32NumInputConnections, APO_CONNECTION_DESCRIPTOR** ppInputConnections,
                                     UINT32 u32NumOutputConnections, APO_CONNECTION_DESCRIPTOR** ppOutputConnections) override;
        STDMETHODIMP UnlockForProcess() override;

        // IAudioSystemEffects2
        STDMETHODIMP GetEffectsList (LPGUID* ppEffectsIds, UINT* pcEffects, HANDLE Event) override;

        /** The registration record (DllRegisterServer and GetRegistrationProperties). */
        static void fillRegistration (APO_REG_PROPERTIES& props) noexcept;

    private:
        HRESULT checkFormat (IAudioMediaType* opposite, IAudioMediaType* requested, IAudioMediaType** supported) noexcept;
        void loadSavedSettings() noexcept;

        std::atomic<ULONG> refs { 1 };
        ApoCore core;
        bool initialized = false, locked = false, discoveryOnly = false;
        UINT32 channels = 2;
        float rate = 48000.0f;
    };
}

#endif
