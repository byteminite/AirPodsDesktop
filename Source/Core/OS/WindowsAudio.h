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

#pragma once

#if !defined APD_OS_WIN
    #error "This file shouldn't be compiled."
#endif

// Audio endpoint helpers shared by quick connect and the automatic output switch (F008): COM
// apartments, MMDevice enumeration, and the container ids that tie a Bluetooth device to its
// Windows sound devices.

#include <optional>

#include <QString>

// The project's Windows header first: it brings in <Windows.h>, which the SDK headers below need.
#include "Windows.h"

#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>

namespace Core::OS::Windows::Audio {

class ScopedComApartment final
{
public:
    ScopedComApartment()
    {
        const HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        _uninitialize = SUCCEEDED(result);
        if (FAILED(result) && result != RPC_E_CHANGED_MODE) {
            LOG(Warn, "CoInitializeEx failed. HRESULT: {:#x}", result);
        }
    }

    ~ScopedComApartment()
    {
        if (_uninitialize) {
            CoUninitialize();
        }
    }

private:
    bool _uninitialize{};
};

inline QString GuidToString(const GUID &guid)
{
    wchar_t buffer[39]{};
    if (StringFromGUID2(guid, buffer, 39) == 0) {
        return {};
    }
    return QString::fromWCharArray(buffer);
}

inline GUID ToGuid(const winrt::guid &guid)
{
    return {
        guid.Data1,
        guid.Data2,
        guid.Data3,
        {
            guid.Data4[0],
            guid.Data4[1],
            guid.Data4[2],
            guid.Data4[3],
            guid.Data4[4],
            guid.Data4[5],
            guid.Data4[6],
            guid.Data4[7],
        },
    };
}

inline QString NormalizeContainerId(QString id)
{
    id = id.trimmed();
    if (id.startsWith('{') && id.endsWith('}')) {
        id = id.mid(1, id.size() - 2);
    }
    return id.toLower();
}

inline QString CanonicalContainerId(const QString &id)
{
    const auto normalized = NormalizeContainerId(id);
    if (normalized.isEmpty()) {
        return {};
    }
    return "{" + normalized.toUpper() + "}";
}

// Container ids and MMDevice endpoint ids are stable identifiers for the user's hardware, the same
// class of data the settings layer redacts for `device_address`. Log just enough of one to
// correlate lines within a single session.
inline QString MaskDeviceId(const QString &id)
{
    const auto trimmed = id.trimmed();
    if (trimmed.isEmpty()) {
        return "<empty>";
    }
    return "***" + trimmed.right(4);
}


inline QString ContainerIdFromProperty(const PROPVARIANT &value)
{
    if (value.vt == VT_CLSID && value.puuid != nullptr) {
        return GuidToString(*value.puuid);
    }
    if (value.vt == VT_LPWSTR && value.pwszVal != nullptr) {
        return QString::fromWCharArray(value.pwszVal);
    }
    if (value.vt == VT_BSTR && value.bstrVal != nullptr) {
        return QString::fromWCharArray(value.bstrVal);
    }
    return {};
}

inline OS::Windows::Com::UniquePtr<IMMDeviceEnumerator> CreateDeviceEnumerator()
{
    OS::Windows::Com::UniquePtr<IMMDeviceEnumerator> enumerator;
    const HRESULT result = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, enumerator.GetIID(),
        reinterpret_cast<void **>(enumerator.ReleaseAndAddressOf()));
    if (FAILED(result)) {
        LOG(Warn, "Create MMDeviceEnumerator failed. HRESULT: {:#x}", result);
        return {};
    }
    return enumerator;
}

inline std::optional<QString> GetEndpointContainerId(IMMDevice &endpoint)
{
    OS::Windows::Com::UniquePtr<IPropertyStore> propertyStore;
    HRESULT result = endpoint.OpenPropertyStore(STGM_READ, propertyStore.ReleaseAndAddressOf());
    if (FAILED(result)) {
        LOG(Warn, "Open endpoint property store failed. HRESULT: {:#x}", result);
        return std::nullopt;
    }

    PROPVARIANT containerValue;
    PropVariantInit(&containerValue);
    result = propertyStore->GetValue(PKEY_Device_ContainerId, &containerValue);
    if (FAILED(result)) {
        LOG(Warn, "Read endpoint container id failed. HRESULT: {:#x}", result);
        PropVariantClear(&containerValue);
        return std::nullopt;
    }

    const auto containerId = CanonicalContainerId(ContainerIdFromProperty(containerValue));
    PropVariantClear(&containerValue);
    if (containerId.isEmpty()) {
        return std::nullopt;
    }
    return containerId;
}

} // namespace Core::OS::Windows::Audio
