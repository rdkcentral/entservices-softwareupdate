/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2024 RDK Management
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
#include <interfaces/IFirmwareUpdate.h>
#include <interfaces/IConfiguration.h>
#include "tracing/Logging.h"
#include <vector>
#include "sysMgr.h"
#include <com/com.h>
#include <core/core.h>
#include <plugins/plugins.h>
#include "FirmwareUpdateHelper.h"

std::thread flashThread;

namespace WPEFramework {
namespace Plugin {
    class FirmwareUpdateImplementation : public Exchange::IFirmwareUpdate , public Exchange::IConfiguration{

    public:
        static const std::map<string, string> firmwareupdateDefaultMap;
    public:
        // We do not allow this plugin to be copied !!
        FirmwareUpdateImplementation();
        ~FirmwareUpdateImplementation() override;

        static FirmwareUpdateImplementation* instance(FirmwareUpdateImplementation *FirmwareUpdateImpl = nullptr);

        // We do not allow this plugin to be copied !!
        FirmwareUpdateImplementation(const FirmwareUpdateImplementation&) = delete;
        FirmwareUpdateImplementation& operator=(const FirmwareUpdateImplementation&) = delete;

        BEGIN_INTERFACE_MAP(FirmwareUpdateImplementation)
        INTERFACE_ENTRY(Exchange::IFirmwareUpdate)
        INTERFACE_ENTRY(Exchange::IConfiguration)
        END_INTERFACE_MAP

    public:
        enum Event {
            ON_UPDATE_STATE_CHANGE,
            ON_FLASHING_STATE_CHANGE
        };

        class EXTERNAL Job : public Core::IDispatch {
        protected:
             Job(FirmwareUpdateImplementation *firmwareUpdateImplementation, Event event, JsonValue &params)
                : _firmwareUpdateImplementation(firmwareUpdateImplementation)
                , _event(event)
                , _params(params) {
                if (_firmwareUpdateImplementation != nullptr) {
                    _firmwareUpdateImplementation->AddRef();
                }
            }

       public:
            Job() = delete;
            Job(const Job&) = delete;
            Job& operator=(const Job&) = delete;
            ~Job() {
                if (_firmwareUpdateImplementation != nullptr) {
                    _firmwareUpdateImplementation->Release();
                }
            }

       public:
            static Core::ProxyType<Core::IDispatch> Create(FirmwareUpdateImplementation *firmwareUpdateImplementation, Event event, JsonValue params) {
#ifndef USE_THUNDER_R4
                return (Core::proxy_cast<Core::IDispatch>(Core::ProxyType<Job>::Create(firmwareUpdateImplementation, event, params)));
#else
                return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<Job>::Create(firmwareUpdateImplementation, event, params)));
#endif
            }

            virtual void Dispatch() {
                _firmwareUpdateImplementation->Dispatch(_event, _params);
            }
        private:
            FirmwareUpdateImplementation *_firmwareUpdateImplementation;
            const Event _event;
            const JsonObject _params;
        };

    public:
        Core::hresult Register(Exchange::IFirmwareUpdate::INotification *notification ) ;
        Core::hresult Unregister(Exchange::IFirmwareUpdate::INotification *notification ) ;
        Core::hresult UpdateFirmware(const string& firmwareFilepath , const string& firmwareType , Result &result ) override ;
        Core::hresult GetUpdateState(GetUpdateStateResult& getUpdateStateResult )  override;
        Core::hresult SetFirmwareRebootDelay (const uint32_t delaySeconds, bool& success) override;
        Core::hresult SetAutoReboot(const bool enable, Result& result) override;
        void startProgressTimer() ;
        int flashImage(const char *server_url, const char *upgrade_file, const char *reboot_flag, const char *proto, int upgrade_type, const char *maint ,const char *initiated_type ,const char * codebig) ;
        void flashImageThread(std::string firmwareFilepath,std::string firmwareType) ;
        int postFlash(const char *maint, const char *upgrade_file, int upgrade_type, const char *reboot_flag ,const char *initiated_type);
        //void updateSecurityStage ();
        void dispatchAndUpdateEvent (string state ,string substate);

        // IConfiguration interface
        uint32_t Configure(PluginHost::IShell* shell);

    private:
        mutable Core::CriticalSection _adminLock;
        std::list<Exchange::IFirmwareUpdate::INotification*> _FirmwareUpdateNotification;
        PluginHost::IShell* mShell;
        
        void dispatchEvent(Event, const JsonObject &params);
        void Dispatch(Event event, const JsonObject params);
        
        void InitializeIARM();
        void DeinitializeIARM();

        friend class Job;
    };
} // namespace Plugin
} // namespace WPEFramework
