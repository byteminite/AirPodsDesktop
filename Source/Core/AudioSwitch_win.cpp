//
// AirPodsDesktop - AirPods Desktop User Experience Enhancement Program.
// Copyright (C) 2021-2022 SpriteOvO
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "AudioSwitch_win.h"

#include <atomic>
#include <functional>
#include <optional>

#include <QMetaObject>
#include <QTimer>

#include "../Logger.h"
#include "AudioSwitchPolicy.h"
#include "OS/WindowsAudio.h"

#include <mmreg.h>

namespace Core::AudioSwitch {
namespace {

using namespace OS::Windows::Audio;
using namespace std::chrono_literals;

// `PKEY_AudioEndpoint_FormFactor`, spelled out so no translation unit has to own the GUID.
constexpr PROPERTYKEY kFormFactorKey{
    {0x1da5d803, 0xd492, 0x4edd, {0x8c, 0x23, 0xe0, 0xc0, 0xff, 0xee, 0x7f, 0x0e}}, 0};
constexpr ULONG kFormFactorHeadphones = 3; // EndpointFormFactor::Headphones (stereo, A2DP)
constexpr ULONG kFormFactorHeadset = 5;    // EndpointFormFactor::Headset (hands-free, HFP)

// The endpoint shows up a second or two after Bluetooth reports "connected"; poll for it a while,
// besides reacting to endpoint state notifications.
constexpr auto kReadyPollInterval = 500ms;
constexpr int kReadyPollAttempts = 20;

// `IPolicyConfig` (Windows 7 and later). Undocumented, but unchanged for years and the interface
// every audio switcher uses; there is no public API to change the default endpoint.
struct DECLSPEC_UUID("f8679f50-850a-41cf-9c72-430f290290c8") IPolicyConfig : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(PCWSTR, WAVEFORMATEX **) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(PCWSTR, INT, WAVEFORMATEX **) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(PCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(PCWSTR, WAVEFORMATEX *, WAVEFORMATEX *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(PCWSTR, INT, PINT64, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(PCWSTR, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(PCWSTR, void *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(PCWSTR, void *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PCWSTR, const PROPERTYKEY &, PROPVARIANT *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(PCWSTR, const PROPERTYKEY &, PROPVARIANT *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR deviceId, ERole role) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(PCWSTR, INT) = 0;
};
class DECLSPEC_UUID("870af99c-171d-4f9e-af0d-e63df40c2bc9") CPolicyConfigClient;

std::optional<ULONG> GetFormFactor(IMMDevice &endpoint)
{
    OS::Windows::Com::UniquePtr<IPropertyStore> store;
    if (FAILED(endpoint.OpenPropertyStore(STGM_READ, store.ReleaseAndAddressOf()))) {
        return std::nullopt;
    }
    PROPVARIANT value;
    PropVariantInit(&value);
    std::optional<ULONG> result;
    if (SUCCEEDED(store->GetValue(kFormFactorKey, &value)) && value.vt == VT_UI4) {
        result = value.ulVal;
    }
    PropVariantClear(&value);
    return result;
}

QString EndpointId(IMMDevice &endpoint)
{
    LPWSTR id = nullptr;
    if (FAILED(endpoint.GetId(&id)) || id == nullptr) {
        return {};
    }
    QString result = QString::fromWCharArray(id);
    CoTaskMemFree(id);
    return result;
}

} // namespace

namespace Details {

class Worker;

// Forwards endpoint notifications (they arrive on arbitrary system threads) to the worker thread.
class NotificationClient final : public IMMNotificationClient
{
public:
    explicit NotificationClient(std::function<void(std::function<void()>)> post)
        : _post{std::move(post)}
    {
    }

    std::function<void()> onStateChanged;
    std::function<void(QString)> onDefaultChanged;

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return ++_refs;
    }
    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG refs = --_refs;
        if (refs == 0) {
            delete this;
        }
        return refs;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **object) override
    {
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IMMNotificationClient)) {
            *object = static_cast<IMMNotificationClient *>(this);
            AddRef();
            return S_OK;
        }
        *object = nullptr;
        return E_NOINTERFACE;
    }

    // The handlers are set once before registering and never changed, and each posted task holds
    // its own copy, so nothing posted refers back to this object after it is released.
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override
    {
        _post(onStateChanged);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override
    {
        _post(onStateChanged);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR id) override
    {
        if (flow == eRender && role == eMultimedia) {
            QString newDefault = id == nullptr ? QString{} : QString::fromWCharArray(id);
            _post([handler = onDefaultChanged, newDefault] { handler(newDefault); });
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override
    {
        return S_OK;
    }

private:
    std::atomic<ULONG> _refs{1};
    std::function<void(std::function<void()>)> _post;
};

class Worker final : public QObject
{
public:
    std::function<void()> onSwitched;

    void Initialize()
    {
        const HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        _comInitialized = SUCCEEDED(result);
        _enumerator = CreateDeviceEnumerator();
        if (_enumerator) {
            _client = new NotificationClient{[this](std::function<void()> task) {
                QMetaObject::invokeMethod(this, std::move(task), Qt::QueuedConnection);
            }};
            _client->onStateChanged = [this] { OnEndpointsChanged(); };
            _client->onDefaultChanged = [this](QString id) { OnDefaultChanged(id); };
            if (FAILED(_enumerator->RegisterEndpointNotificationCallback(_client))) {
                LOG(Warn, "AudioSwitch: RegisterEndpointNotificationCallback failed.");
                _client->Release();
                _client = nullptr;
            }
        }
        _readyTimer.setInterval(kReadyPollInterval);
        connect(&_readyTimer, &QTimer::timeout, this, [this] {
            if (++_attempts >= kReadyPollAttempts) {
                LOG(Info, "AudioSwitch: The AirPods' stereo output didn't become ready; giving up.");
                _readyTimer.stop();
            }
            TryAirPodsReady();
        });
    }

    void Shutdown()
    {
        _readyTimer.stop();
        if (_client != nullptr) {
            _enumerator->UnregisterEndpointNotificationCallback(_client);
            _client->Release();
            _client = nullptr;
        }
        _enumerator = static_cast<IMMDeviceEnumerator *>(nullptr);
        if (_comInitialized) {
            CoUninitialize();
        }
    }

    void SetEnabled(bool enabled)
    {
        _enabled = enabled;
    }

    void SetContainerId(const QString &containerId)
    {
        _containerId = CanonicalContainerId(containerId);
    }

    void SetConnected(bool connected)
    {
        if (connected == _connected) {
            return;
        }
        _connected = connected;
        if (connected) {
            _policy.OnConnected();
            _attempts = 0;
            _readyTimer.start();
            TryAirPodsReady();
            return;
        }

        _readyTimer.stop();
        const auto previous = _policy.Previous();
        const bool previousActive = previous.has_value() && IsActive(*previous);
        if (const auto restore = _policy.OnDisconnected(CurrentDefault(), previousActive)) {
            LOG(Info, "AudioSwitch: AirPods disconnected; restoring the previous output {}.",
                MaskDeviceId(*restore));
            if (SetDefault(*restore)) {
                onSwitched();
            }
        }
    }

private:
    bool _comInitialized{false};
    OS::Windows::Com::UniquePtr<IMMDeviceEnumerator> _enumerator;
    NotificationClient *_client{nullptr};
    Policy _policy;
    bool _enabled{true};
    bool _connected{false};
    QString _containerId;
    QString _airPodsId;
    QTimer _readyTimer{this};
    int _attempts{0};

    void OnEndpointsChanged()
    {
        if (_readyTimer.isActive()) {
            TryAirPodsReady();
        }
    }

    void OnDefaultChanged(const QString &newDefaultId)
    {
        _policy.OnDefaultChanged(newDefaultId, !_airPodsId.isEmpty() && IsActive(_airPodsId));
    }

    void TryAirPodsReady()
    {
        if (!_connected || _containerId.isEmpty()) {
            return;
        }
        const auto airPods = FindAirPodsEndpoint();
        if (!airPods.has_value()) {
            return;
        }
        _readyTimer.stop();
        _airPodsId = *airPods;

        const auto current = CurrentDefault();
        if (const auto target = _policy.OnAirPodsReady(*airPods, current, _enabled)) {
            LOG(Info, "AudioSwitch: Switching the default output to the AirPods {} (was {}).",
                MaskDeviceId(*target), MaskDeviceId(current));
            if (SetDefault(*target)) {
                onSwitched();
            }
        }
        else {
            LOG(Info, "AudioSwitch: AirPods ready; no switch needed (enabled: {}, default: {}).",
                _enabled, current == *airPods ? "AirPods" : "other");
        }
    }

    // The bound AirPods' active stereo render endpoint, never the hands-free one.
    std::optional<QString> FindAirPodsEndpoint()
    {
        if (!_enumerator) {
            return std::nullopt;
        }
        OS::Windows::Com::UniquePtr<IMMDeviceCollection> collection;
        if (FAILED(_enumerator->EnumAudioEndpoints(
                eRender, DEVICE_STATE_ACTIVE, collection.ReleaseAndAddressOf())))
        {
            return std::nullopt;
        }
        UINT count = 0;
        collection->GetCount(&count);

        std::optional<QString> fallback;
        for (UINT i = 0; i < count; ++i) {
            OS::Windows::Com::UniquePtr<IMMDevice> endpoint;
            if (FAILED(collection->Item(i, endpoint.ReleaseAndAddressOf()))) {
                continue;
            }
            if (GetEndpointContainerId(*endpoint.operator->()) != _containerId) {
                continue;
            }
            const auto formFactor = GetFormFactor(*endpoint.operator->());
            if (formFactor == kFormFactorHeadphones) {
                return EndpointId(*endpoint.operator->());
            }
            if (formFactor != kFormFactorHeadset && !fallback.has_value()) {
                fallback = EndpointId(*endpoint.operator->());
            }
        }
        return fallback;
    }

    QString CurrentDefault()
    {
        if (!_enumerator) {
            return {};
        }
        OS::Windows::Com::UniquePtr<IMMDevice> endpoint;
        if (FAILED(_enumerator->GetDefaultAudioEndpoint(
                eRender, eMultimedia, endpoint.ReleaseAndAddressOf())))
        {
            return {};
        }
        return EndpointId(*endpoint.operator->());
    }

    bool IsActive(const QString &id)
    {
        if (!_enumerator || id.isEmpty()) {
            return false;
        }
        OS::Windows::Com::UniquePtr<IMMDevice> endpoint;
        if (FAILED(_enumerator->GetDevice(
                reinterpret_cast<LPCWSTR>(id.utf16()), endpoint.ReleaseAndAddressOf())))
        {
            return false;
        }
        DWORD state = 0;
        return SUCCEEDED(endpoint->GetState(&state)) && state == DEVICE_STATE_ACTIVE;
    }

    // Console and Multimedia only: the Communications default (calls) stays the user's.
    static bool SetDefault(const QString &id)
    {
        IPolicyConfig *config = nullptr;
        HRESULT result = CoCreateInstance(
            __uuidof(CPolicyConfigClient), nullptr, CLSCTX_ALL, __uuidof(IPolicyConfig),
            reinterpret_cast<void **>(&config));
        if (FAILED(result) || config == nullptr) {
            LOG(Warn, "AudioSwitch: Create PolicyConfigClient failed. HRESULT: {:#x}", result);
            return false;
        }
        const auto wide = reinterpret_cast<PCWSTR>(id.utf16());
        bool ok = true;
        for (const auto role : {eConsole, eMultimedia}) {
            result = config->SetDefaultEndpoint(wide, role);
            if (FAILED(result)) {
                LOG(Warn, "AudioSwitch: SetDefaultEndpoint failed. HRESULT: {:#x}", result);
                ok = false;
            }
        }
        config->Release();
        return ok;
    }
};

} // namespace Details

Switcher::Switcher(QObject *parent) : QObject{parent}, _worker{new Details::Worker}
{
    _thread.setObjectName("AudioSwitch");
    _worker->moveToThread(&_thread);
    _worker->onSwitched = [this] {
        QMetaObject::invokeMethod(
            this, [this] { emit DefaultOutputSwitched(); }, Qt::QueuedConnection);
    };
    connect(&_thread, &QThread::finished, _worker, &QObject::deleteLater);
    _thread.start();
    QMetaObject::invokeMethod(_worker, [worker = _worker] { worker->Initialize(); });
}

Switcher::~Switcher()
{
    QMetaObject::invokeMethod(
        _worker, [worker = _worker] { worker->Shutdown(); }, Qt::BlockingQueuedConnection);
    _thread.quit();
    _thread.wait();
}

void Switcher::SetEnabled(bool enabled)
{
    QMetaObject::invokeMethod(_worker, [worker = _worker, enabled] { worker->SetEnabled(enabled); });
}

void Switcher::SetBoundContainerId(const QString &containerId)
{
    QMetaObject::invokeMethod(
        _worker, [worker = _worker, containerId] { worker->SetContainerId(containerId); });
}

void Switcher::SetDeviceConnected(bool connected)
{
    QMetaObject::invokeMethod(
        _worker, [worker = _worker, connected] { worker->SetConnected(connected); });
}

} // namespace Core::AudioSwitch
