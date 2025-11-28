/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2025 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#pragma once

#include "Module.h"
#include <interfaces/Ids.h>
#include <interfaces/IFirmwareDownload.h>

#include <com/com.h>
#include <core/core.h>

namespace WPEFramework
{
    namespace Plugin
    {
        class FirmwareDownloadImplementation : public Exchange::IFirmwareDownload
        {
            public:
                // We do not allow this plugin to be copied !!
                FirmwareDownloadImplementation();
                ~FirmwareDownloadImplementation() override;

                FirmwareDownloadImplementation(const FirmwareDownloadImplementation&) = delete;
                FirmwareDownloadImplementation& operator=(const FirmwareDownloadImplementation&) = delete;

                BEGIN_INTERFACE_MAP(FirmwareDownloadImplementation)
                INTERFACE_ENTRY(Exchange::IFirmwareDownload)
                END_INTERFACE_MAP

            public:
                enum Event
                {
                    FIRMWAREDOWNLOAD_EVT_ONFIRMWAREAVAILABLE
                };

            class EXTERNAL Job : public Core::IDispatch {
            protected:
                Job(FirmwareDownloadImplementation* firmwareDownloadImplementation, Event event, JsonObject &params)
                    : _firmwareDownloadImplementation(firmwareDownloadImplementation)
                    , _event(event)
                    , _params(params) {
                    if (_firmwareDownloadImplementation != nullptr) {
                        _firmwareDownloadImplementation->AddRef();
                    }
                }

            public:
                Job() = delete;
                Job(const Job&) = delete;
                Job& operator=(const Job&) = delete;
                ~Job() {
                    if (_firmwareDownloadImplementation != nullptr) {
                        _firmwareDownloadImplementation->Release();
                    }
                }

            public:
                static Core::ProxyType<Core::IDispatch> Create(FirmwareDownloadImplementation* firmwareDownloadImplementation, Event event, JsonObject params ) {
#ifndef USE_THUNDER_R4
                    return (Core::proxy_cast<Core::IDispatch>(Core::ProxyType<Job>::Create(firmwareDownloadImplementation, event, params)));
#else
                    return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<Job>::Create(firmwareDownloadImplementation, event, params)));
#endif
                }

                virtual void Dispatch() {
                    _firmwareDownloadImplementation->Dispatch(_event, _params);
                }

            private:
                FirmwareDownloadImplementation *_firmwareDownloadImplementation;
                const Event _event;
                JsonObject _params;
        };
        public:
            virtual Core::hresult Register(Exchange::IFirmwareDownload::INotification *notification ) override ;
            virtual Core::hresult Unregister(Exchange::IFirmwareDownload::INotification *notification ) override;

            Core::hresult GetDownloadedFirmwareInfo(string& currentFWVersion, string& downloadedFWVersion, string& downloadedFWLocation, bool& isRebootDeferred) override;
            Core::hresult GetFirmwareDownloadPercent( FirmwareDownloadPercent& firmwareDownloadPercent) override;
            Core::hresult SearchFirmware() override;
            Core::hresult GetDownloadState( FirmwareDownloadState& downloadState) override;
            Core::hresult GetDownloadFailureReason( DownloadFailureReason& downloadFailureReason) override;

        private:
            mutable Core::CriticalSection _adminLock;
            PluginHost::IShell* _service;
            std::list<Exchange::IFirmwareDownload::INotification*> _firmwareDownloadNotification;
            static FirmwareDownloadImplementation* _instance;

            void dispatchEvent(Event, const JsonObject &params);
            void Dispatch(Event event, const JsonObject params);
        public:
            void OnFirmwareAvailable (int searchStatus, string serverResponse, bool firmwareAvailable, string firmwareVersion, bool rebootImmediately);

            friend class Job;
        };
    } // namespace Plugin
} // namespace WPEFramework
