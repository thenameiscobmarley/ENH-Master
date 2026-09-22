#include "SystemAudioDevice.h"

#if ! JUCE_WINDOWS

namespace pad
{
    std::unique_ptr<juce::AudioIODeviceType> createSystemAudioDeviceType() { return {}; }
}

#else

#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>

#include <atomic>
#include <vector>

namespace pad
{
    using juce::ComSmartPtr;

    namespace
    {
        /** COM, for as long as this object is alive, in the apartment this thread wants. */
        struct ComScope
        {
            ComScope() : hr (CoInitializeEx (nullptr, COINIT_MULTITHREADED)) {}
            ~ComScope() { if (SUCCEEDED (hr)) CoUninitialize(); }
            HRESULT hr;
            JUCE_DECLARE_NON_COPYABLE (ComScope)
        };

        constexpr double captureBufferSeconds = 0.2;   // what WASAPI holds for us between polls
        constexpr double renderBufferSeconds  = 0.04;

        /** One playback endpoint of the machine. */
        struct Endpoint
        {
            juce::String name, id;
        };

        juce::String nameOf (IMMDevice* device)
        {
            ComSmartPtr<IPropertyStore> props;
            if (FAILED (device->OpenPropertyStore (STGM_READ, props.resetAndGetPointerAddress())) || props == nullptr)
                return {};

            PROPVARIANT value;
            PropVariantInit (&value);
            juce::String name;
            if (SUCCEEDED (props->GetValue (PKEY_Device_FriendlyName, &value)) && value.vt == VT_LPWSTR && value.pwszVal != nullptr)
                name = juce::String (value.pwszVal);
            PropVariantClear (&value);
            return name;
        }

        ComSmartPtr<IMMDeviceEnumerator> makeEnumerator()
        {
            ComSmartPtr<IMMDeviceEnumerator> e;
            if (FAILED (CoCreateInstance (__uuidof (MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                          __uuidof (IMMDeviceEnumerator), (void**) e.resetAndGetPointerAddress())))
                return {};
            return e;
        }

        /** Every playback endpoint, in the order Windows lists them, with the default one first. */
        std::vector<Endpoint> listPlaybackEndpoints()
        {
            std::vector<Endpoint> found;
            auto enumerator = makeEnumerator();
            if (enumerator == nullptr)
                return found;

            juce::String defaultId;
            {
                ComSmartPtr<IMMDevice> def;
                if (SUCCEEDED (enumerator->GetDefaultAudioEndpoint (eRender, eConsole, def.resetAndGetPointerAddress())) && def != nullptr)
                {
                    LPWSTR id = nullptr;
                    if (SUCCEEDED (def->GetId (&id)) && id != nullptr)
                    {
                        defaultId = juce::String (id);
                        CoTaskMemFree (id);
                    }
                }
            }

            ComSmartPtr<IMMDeviceCollection> collection;
            if (FAILED (enumerator->EnumAudioEndpoints (eRender, DEVICE_STATE_ACTIVE, collection.resetAndGetPointerAddress()))
                 || collection == nullptr)
                return found;

            UINT count = 0;
            collection->GetCount (&count);

            for (UINT i = 0; i < count; ++i)
            {
                ComSmartPtr<IMMDevice> device;
                if (FAILED (collection->Item (i, device.resetAndGetPointerAddress())) || device == nullptr)
                    continue;

                LPWSTR id = nullptr;
                if (FAILED (device->GetId (&id)) || id == nullptr)
                    continue;

                Endpoint e { nameOf (device), juce::String (id) };
                CoTaskMemFree (id);

                if (e.name.isEmpty())
                    e.name = "Playback device " + juce::String ((int) i + 1);

                if (e.id == defaultId)
                    found.insert (found.begin(), std::move (e));
                else
                    found.push_back (std::move (e));
            }

            return found;
        }

        /** Reads one frame's channels out of whatever format the endpoint mixes in. Shared mode gives
            32-bit float almost always, but 16- and 32-bit PCM are handled rather than assumed away. */
        struct FormatReader
        {
            enum Kind { float32, pcm16, pcm24, pcm32, unsupported };

            Kind kind = unsupported;
            int channels = 2, bytesPerFrame = 8;

            explicit FormatReader (const WAVEFORMATEX* f)
            {
                if (f == nullptr)
                    return;

                channels = (int) f->nChannels;
                bytesPerFrame = (int) f->nBlockAlign;

                auto tag = f->wFormatTag;
                int bits = (int) f->wBitsPerSample;

                if (tag == WAVE_FORMAT_EXTENSIBLE)
                {
                    auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*> (f);
                    tag = (ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) ? WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM;
                }

                if (tag == WAVE_FORMAT_IEEE_FLOAT && bits == 32)  kind = float32;
                else if (tag == WAVE_FORMAT_PCM && bits == 16)    kind = pcm16;
                else if (tag == WAVE_FORMAT_PCM && bits == 24)    kind = pcm24;
                else if (tag == WAVE_FORMAT_PCM && bits == 32)    kind = pcm32;
            }

            bool isValid() const noexcept { return kind != unsupported && channels > 0; }

            float sample (const juce::uint8* frame, int channel) const noexcept
            {
                const auto ch = juce::jmin (channel, channels - 1);

                switch (kind)
                {
                    case float32: return reinterpret_cast<const float*> (frame)[ch];
                    case pcm16:   return (float) reinterpret_cast<const juce::int16*> (frame)[ch] / 32768.0f;
                    case pcm32:   return (float) reinterpret_cast<const juce::int32*> (frame)[ch] / 2147483648.0f;
                    case pcm24:
                    {
                        auto* p = frame + ch * 3;
                        const auto v = (juce::int32) ((juce::uint32) p[0] << 8 | (juce::uint32) p[1] << 16 | (juce::uint32) p[2] << 24);
                        return (float) v / 2147483648.0f;
                    }
                    case unsupported:
                    default:      return 0.0f;
                }
            }

            void write (juce::uint8* frame, int channel, float value) const noexcept
            {
                const auto ch = juce::jmin (channel, channels - 1);
                const auto clipped = juce::jlimit (-1.0f, 1.0f, value);

                switch (kind)
                {
                    case float32: reinterpret_cast<float*> (frame)[ch] = clipped; break;
                    case pcm16:   reinterpret_cast<juce::int16*> (frame)[ch] = (juce::int16) juce::roundToInt (clipped * 32767.0f); break;
                    case pcm32:   reinterpret_cast<juce::int32*> (frame)[ch] = (juce::int32) (clipped * 2147483647.0); break;
                    case pcm24:
                    {
                        auto* p = frame + ch * 3;
                        const auto v = (juce::int32) (clipped * 8388607.0f);
                        p[0] = (juce::uint8) (v & 0xff); p[1] = (juce::uint8) ((v >> 8) & 0xff); p[2] = (juce::uint8) ((v >> 16) & 0xff);
                        break;
                    }
                    case unsupported:
                    default: break;
                }
            }
        };
    }

    //==============================================================================
    /** Loopback capture of one playback endpoint, straight into the callback, straight out to
        another playback endpoint.

        The two endpoints are driven by different clocks, so they drift apart (a few samples a
        minute). Everything captured goes into a ring, and the reader walks it at a rate that is
        nudged to keep the ring at its target depth: no dropped or repeated blocks, just a fraction
        of a cent of pitch that follows the drift. */
    class SystemAudioDevice final : public juce::AudioIODevice,
                                    private juce::Thread
    {
    public:
        SystemAudioDevice (const juce::String& typeName, Endpoint captureEndpoint, Endpoint renderEndpoint)
            : AudioIODevice (renderEndpoint.name + " <- " + captureEndpoint.name, typeName),
              Thread ("ENH system audio"),
              capture (std::move (captureEndpoint)),
              render (std::move (renderEndpoint))
        {
        }

        ~SystemAudioDevice() override
        {
            close();
        }

        juce::StringArray getOutputChannelNames() override { return { "Left", "Right" }; }
        juce::StringArray getInputChannelNames() override  { return { "Left", "Right" }; }

        juce::Array<double> getAvailableSampleRates() override
        {
            // Shared mode runs at the endpoint's own mix rate; anything else is resampled by Windows.
            if (discoveredRate <= 0.0)
                discoveredRate = readMixRate (render.id);

            return { discoveredRate > 0.0 ? discoveredRate : 48000.0 };
        }

        juce::Array<int> getAvailableBufferSizes() override { return { 128, 192, 256, 384, 480, 512, 768, 1024 }; }
        int getDefaultBufferSize() override                 { return 256; }

        juce::String open (const juce::BigInteger&, const juce::BigInteger&, double sampleRate, int bufferSizeSamples) override
        {
            close();

            if (capture.id == render.id)
                return "Capturing and playing to the same device would feed back. Pick a different playback device "
                       "for Windows (an unused HDMI or monitor output will do), capture that one, and play to your headset.";

            lastError = {};
            requestedRate = sampleRate > 0.0 ? sampleRate : 48000.0;
            blockSize = bufferSizeSamples > 0 ? bufferSizeSamples : getDefaultBufferSize();
            opened = false;

            startThread (Priority::highest);

            if (! ready.wait (5000) || ! opened)
            {
                signalThreadShouldExit();
                stopThread (2000);

                if (lastError.isEmpty())
                    lastError = "The system audio device did not start.";

                return lastError;
            }

            return {};
        }

        void close() override
        {
            stop();
            signalThreadShouldExit();
            stopThread (3000);
            opened = false;
        }

        bool isOpen() override                        { return opened; }
        bool isPlaying() override                     { return running.load(); }
        juce::String getLastError() override          { return lastError; }
        int getCurrentBufferSizeSamples() override    { return blockSize; }
        double getCurrentSampleRate() override        { return actualRate > 0.0 ? actualRate : requestedRate; }
        int getCurrentBitDepth() override             { return 32; }
        int getOutputLatencyInSamples() override      { return outputLatency; }
        int getInputLatencyInSamples() override       { return inputLatency; }

        juce::BigInteger getActiveOutputChannels() const override { juce::BigInteger b; b.setRange (0, 2, true); return b; }
        juce::BigInteger getActiveInputChannels() const override  { juce::BigInteger b; b.setRange (0, 2, true); return b; }

        void start (juce::AudioIODeviceCallback* newCallback) override
        {
            if (! opened || newCallback == nullptr)
                return;

            newCallback->audioDeviceAboutToStart (this);

            {
                const juce::ScopedLock sl (callbackLock);
                callback = newCallback;
            }

            running = true;
        }

        void stop() override
        {
            juce::AudioIODeviceCallback* old = nullptr;

            {
                const juce::ScopedLock sl (callbackLock);
                old = callback;
                callback = nullptr;
            }

            running = false;

            if (old != nullptr)
                old->audioDeviceStopped();
        }

    private:
        //==============================================================================
        static double readMixRate (const juce::String& endpointId)
        {
            ComScope com;
            auto enumerator = makeEnumerator();
            if (enumerator == nullptr)
                return 0.0;

            ComSmartPtr<IMMDevice> device;
            if (FAILED (enumerator->GetDevice (endpointId.toWideCharPointer(), device.resetAndGetPointerAddress())) || device == nullptr)
                return 0.0;

            ComSmartPtr<IAudioClient> client;
            if (FAILED (device->Activate (__uuidof (IAudioClient), CLSCTX_ALL, nullptr, (void**) client.resetAndGetPointerAddress()))
                 || client == nullptr)
                return 0.0;

            WAVEFORMATEX* format = nullptr;
            if (FAILED (client->GetMixFormat (&format)) || format == nullptr)
                return 0.0;

            const auto rate = (double) format->nSamplesPerSec;
            CoTaskMemFree (format);
            return rate;
        }

        /** Everything COM happens on this thread, so the apartment it was created in is the apartment
            it is used and released in. */
        void run() override
        {
            ComScope com;

            if (! setUp())
            {
                ready.signal();
                tearDown();
                return;
            }

            opened = true;
            ready.signal();

            captureClient.start();
            renderClient.start();

            int missedWakeups = 0;

            while (! threadShouldExit())
            {
                // The render endpoint sets the pace: it wakes us whenever it wants another period.
                const auto woke = WaitForSingleObject (renderEvent, 200) == WAIT_OBJECT_0;

                if (! woke)
                {
                    if (++missedWakeups > 10)
                    {
                        lastError = "The playback device stopped asking for audio.";
                        break;
                    }

                    continue;
                }

                missedWakeups = 0;
                drainCapture();
                feedRender();
            }

            captureClient.stop();
            renderClient.stop();
            tearDown();
            opened = false;
        }

        //==============================================================================
        struct Client
        {
            ComSmartPtr<IAudioClient> client;
            FormatReader format { nullptr };
            UINT32 bufferFrames = 0;
            double rate = 0.0;

            void start() const { if (client != nullptr) client->Start(); }
            void stop()  const { if (client != nullptr) client->Stop(); }
            void reset()       { client = nullptr; }
        };

        bool fail (const juce::String& what)
        {
            lastError = what;
            return false;
        }

        bool setUp()
        {
            auto enumerator = makeEnumerator();
            if (enumerator == nullptr)
                return fail ("Windows audio is not available.");

            if (! openClient (enumerator, capture.id, true, captureClient))
                return false;

            if (! openClient (enumerator, render.id, false, renderClient))
                return false;

            if (! captureClient.format.isValid() || ! renderClient.format.isValid())
                return fail ("This device mixes in a format ENH Master does not read (not 16, 24 or 32 bit).");

            actualRate = renderClient.rate;

            renderEvent = CreateEvent (nullptr, FALSE, FALSE, nullptr);
            if (renderEvent == nullptr)
                return fail ("Could not create the playback event.");

            if (FAILED (renderClient.client->SetEventHandle (renderEvent)))
                return fail ("The playback device would not take an event handle.");

            if (FAILED (captureClient.client->GetService (__uuidof (IAudioCaptureClient), (void**) captureService.resetAndGetPointerAddress()))
                 || captureService == nullptr)
                return fail ("Could not start capturing what this device is playing.");

            if (FAILED (renderClient.client->GetService (__uuidof (IAudioRenderClient), (void**) renderService.resetAndGetPointerAddress()))
                 || renderService == nullptr)
                return fail ("Could not open the playback device.");

            // Latency the host should know about: what sits in the two endpoint buffers, plus our ring's target.
            inputLatency = (int) captureClient.bufferFrames;
            outputLatency = (int) renderClient.bufferFrames + targetFill();

            ringSize = juce::nextPowerOfTwo ((int) (actualRate * 0.5));
            ring[0].calloc ((size_t) ringSize);
            ring[1].calloc ((size_t) ringSize);
            writePos = 0;
            readPos = 0.0;
            filled = 0;

            outputFifo[0].calloc ((size_t) ringSize);
            outputFifo[1].calloc ((size_t) ringSize);
            outputCount = 0;
            outputRead = 0;

            blockIn[0].calloc ((size_t) blockSize);
            blockIn[1].calloc ((size_t) blockSize);
            blockOut[0].calloc ((size_t) blockSize);
            blockOut[1].calloc ((size_t) blockSize);

            nominalRatio = captureClient.rate / juce::jmax (1.0, renderClient.rate);
            return true;
        }

        bool openClient (const ComSmartPtr<IMMDeviceEnumerator>& enumerator, const juce::String& id, bool loopback, Client& out)
        {
            ComSmartPtr<IMMDevice> device;
            if (FAILED (enumerator->GetDevice (id.toWideCharPointer(), device.resetAndGetPointerAddress())) || device == nullptr)
                return fail (loopback ? "That playback device is no longer there to capture."
                                      : "That playback device is no longer there.");

            if (FAILED (device->Activate (__uuidof (IAudioClient), CLSCTX_ALL, nullptr, (void**) out.client.resetAndGetPointerAddress()))
                 || out.client == nullptr)
                return fail ("Windows would not let ENH Master open that device.");

            WAVEFORMATEX* format = nullptr;
            if (FAILED (out.client->GetMixFormat (&format)) || format == nullptr)
                return fail ("Could not read the device's format.");

            out.format = FormatReader (format);
            out.rate = (double) format->nSamplesPerSec;

            const auto seconds = loopback ? captureBufferSeconds : renderBufferSeconds;
            const REFERENCE_TIME duration = (REFERENCE_TIME) (seconds * 1.0e7);
            const DWORD flags = loopback ? AUDCLNT_STREAMFLAGS_LOOPBACK : AUDCLNT_STREAMFLAGS_EVENTCALLBACK;

            const auto hr = out.client->Initialize (AUDCLNT_SHAREMODE_SHARED, flags, duration, 0, format, nullptr);
            CoTaskMemFree (format);

            if (FAILED (hr))
                return fail (loopback ? "Windows would not capture that device (another program may have it exclusively)."
                                      : "Windows would not open that device for playback (another program may have it exclusively).");

            if (FAILED (out.client->GetBufferSize (&out.bufferFrames)))
                return fail ("Could not read the device's buffer size.");

            return true;
        }

        void tearDown()
        {
            captureService = nullptr;
            renderService = nullptr;
            captureClient.reset();
            renderClient.reset();

            if (renderEvent != nullptr)
            {
                CloseHandle (renderEvent);
                renderEvent = nullptr;
            }
        }

        //==============================================================================
        int targetFill() const noexcept { return juce::jmax (blockSize * 2, (int) (actualRate * 0.02)); }

        /** Everything the captured endpoint has played since the last wake-up goes into the ring. */
        void drainCapture()
        {
            for (;;)
            {
                UINT32 packet = 0;
                if (FAILED (captureService->GetNextPacketSize (&packet)) || packet == 0)
                    return;

                BYTE* data = nullptr;
                UINT32 frames = 0;
                DWORD flags = 0;

                if (FAILED (captureService->GetBuffer (&data, &frames, &flags, nullptr, nullptr)))
                    return;

                const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0 || data == nullptr;
                const auto& fmt = captureClient.format;

                for (UINT32 i = 0; i < frames; ++i)
                {
                    const auto slot = (size_t) (writePos & (ringSize - 1));

                    if (silent)
                    {
                        ring[0][slot] = 0.0f;
                        ring[1][slot] = 0.0f;
                    }
                    else
                    {
                        auto* frame = reinterpret_cast<const juce::uint8*> (data) + (size_t) i * (size_t) fmt.bytesPerFrame;
                        ring[0][slot] = fmt.sample (frame, 0);
                        ring[1][slot] = fmt.sample (frame, 1);
                    }

                    ++writePos;
                }

                captureService->ReleaseBuffer (frames);

                // Nothing is reading fast enough (the playback device has stalled): keep the newest.
                const auto available = (double) writePos - readPos;
                if (available > (double) ringSize * 0.9)
                    readPos = (double) writePos - targetFill();

                filled = (int) juce::jmax (0.0, (double) writePos - readPos);
            }
        }

        /** Reads the ring at whatever rate keeps it at its target depth, processes whole blocks, and
            hands the endpoint as much as it asked for. */
        void feedRender()
        {
            UINT32 padding = 0;
            if (FAILED (renderClient.client->GetCurrentPadding (&padding)))
                return;

            auto wanted = (int) renderClient.bufferFrames - (int) padding;
            if (wanted <= 0)
                return;

            while (outputAvailable() < wanted && canProcessBlock())
                processBlock();

            const auto toWrite = juce::jmin (wanted, juce::jmax (outputAvailable(), 0));
            const auto frames = (UINT32) juce::jmax (toWrite, 0);

            if (frames == 0)
                return;

            BYTE* data = nullptr;
            if (FAILED (renderService->GetBuffer (frames, &data)) || data == nullptr)
                return;

            const auto& fmt = renderClient.format;

            for (UINT32 i = 0; i < frames; ++i)
            {
                auto* frame = reinterpret_cast<juce::uint8*> (data) + (size_t) i * (size_t) fmt.bytesPerFrame;

                for (int ch = 0; ch < fmt.channels; ++ch)
                    fmt.write (frame, ch, outputFifo[juce::jmin (ch, 1)][(size_t) (outputRead + i) & (size_t) (ringSize - 1)]);
            }

            outputRead += (int) frames;
            outputCount -= (int) frames;
            renderService->ReleaseBuffer (frames, 0);
        }

        int outputAvailable() const noexcept { return outputCount; }

        bool canProcessBlock() const noexcept
        {
            // Enough captured audio to read a block through the drift-following reader, and room to put it.
            const auto needed = (double) blockSize * nominalRatio * 1.02 + 4.0;
            return (double) writePos - readPos >= needed && outputCount + blockSize <= ringSize;
        }

        void processBlock()
        {
            // Follow the drift: read a little faster when the ring is filling, a little slower when it
            // is emptying. A tenth of a percent is inaudible and far more than the clocks ever differ by.
            const auto target = (double) targetFill();
            const auto fill = (double) writePos - readPos;
            const auto correction = juce::jlimit (-0.001, 0.001, (fill - target) / juce::jmax (1.0, target) * 0.02);
            const auto step = nominalRatio * (1.0 + correction);

            for (int i = 0; i < blockSize; ++i)
            {
                const auto pos = readPos + (double) i * step;
                const auto index = (juce::int64) pos;
                const auto frac = (float) (pos - (double) index);
                const auto a = (size_t) (index & (ringSize - 1));
                const auto b = (size_t) ((index + 1) & (ringSize - 1));

                blockIn[0][(size_t) i] = ring[0][a] + (ring[0][b] - ring[0][a]) * frac;
                blockIn[1][(size_t) i] = ring[1][a] + (ring[1][b] - ring[1][a]) * frac;
            }

            readPos += (double) blockSize * step;

            const float* inputs[2] { blockIn[0].get(),  blockIn[1].get() };
            float* outputs[2]      { blockOut[0].get(), blockOut[1].get() };

            {
                const juce::ScopedLock sl (callbackLock);

                if (callback != nullptr)
                    callback->audioDeviceIOCallbackWithContext (inputs, 2, outputs, 2, blockSize, {});
                else
                    for (int ch = 0; ch < 2; ++ch)
                        juce::FloatVectorOperations::clear (outputs[ch], blockSize);
            }

            const auto writeAt = outputRead + outputCount;

            for (int i = 0; i < blockSize; ++i)
            {
                const auto slot = (size_t) ((writeAt + i) & (ringSize - 1));
                outputFifo[0][slot] = outputs[0][i];
                outputFifo[1][slot] = outputs[1][i];
            }

            outputCount += blockSize;
        }

        //==============================================================================
        Endpoint capture, render;

        Client captureClient, renderClient;
        ComSmartPtr<IAudioCaptureClient> captureService;
        ComSmartPtr<IAudioRenderClient> renderService;
        HANDLE renderEvent = nullptr;

        juce::HeapBlock<float> ring[2], outputFifo[2], blockIn[2], blockOut[2];
        int ringSize = 0;
        juce::int64 writePos = 0;
        double readPos = 0.0;
        int filled = 0;
        int outputCount = 0;
        juce::int64 outputRead = 0;
        double nominalRatio = 1.0;

        juce::CriticalSection callbackLock;
        juce::AudioIODeviceCallback* callback = nullptr;
        juce::WaitableEvent ready;
        std::atomic<bool> running { false };
        bool opened = false;

        double requestedRate = 48000.0, actualRate = 0.0, discoveredRate = 0.0;
        int blockSize = 256, inputLatency = 0, outputLatency = 0;
        juce::String lastError;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SystemAudioDevice)
    };

    //==============================================================================
    class SystemAudioDeviceType final : public juce::AudioIODeviceType
    {
    public:
        SystemAudioDeviceType() : AudioIODeviceType ("Windows system audio (loopback)") {}

        void scanForDevices() override
        {
            ComScope com;
            endpoints = listPlaybackEndpoints();
            hasScanned = true;
            callDeviceChangeListeners();
        }

        juce::StringArray getDeviceNames (bool wantInputNames) const override
        {
            juce::StringArray names;

            for (auto& e : endpoints)
                names.add (wantInputNames ? e.name + " (what it is playing)" : e.name);

            return names;
        }

        int getDefaultDeviceIndex (bool /*forInput*/) const override { return 0; }

        int getIndexOfDevice (juce::AudioIODevice* device, bool asInput) const override
        {
            if (auto* d = dynamic_cast<SystemAudioDevice*> (device))
                return getDeviceNames (asInput).indexOf (asInput ? inputNameOf (d) : outputNameOf (d));

            return -1;
        }

        bool hasSeparateInputsAndOutputs() const override { return true; }

        juce::AudioIODevice* createDevice (const juce::String& outputDeviceName, const juce::String& inputDeviceName) override
        {
            if (! hasScanned)
                scanForDevices();

            const auto inputs = getDeviceNames (true);
            const auto outputs = getDeviceNames (false);

            const auto captureIndex = inputs.indexOf (inputDeviceName);
            const auto renderIndex = outputs.indexOf (outputDeviceName);

            if (captureIndex < 0 || renderIndex < 0)
                return nullptr;

            return new SystemAudioDevice (getTypeName(),
                                          endpoints[(size_t) captureIndex],
                                          endpoints[(size_t) renderIndex]);
        }

    private:
        static juce::String inputNameOf (SystemAudioDevice* d)  { return d->getName().fromFirstOccurrenceOf (" <- ", false, false) + " (what it is playing)"; }
        static juce::String outputNameOf (SystemAudioDevice* d) { return d->getName().upToFirstOccurrenceOf (" <- ", false, false); }

        std::vector<Endpoint> endpoints;
        bool hasScanned = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SystemAudioDeviceType)
    };

    std::unique_ptr<juce::AudioIODeviceType> createSystemAudioDeviceType()
    {
        return std::make_unique<SystemAudioDeviceType>();
    }
}

#endif
