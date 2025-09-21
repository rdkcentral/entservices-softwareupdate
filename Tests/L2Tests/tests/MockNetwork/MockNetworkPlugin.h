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
#include "../interfaces/IIarm.h"
#include "../interfaces/IProc.h"
#include "../interfaces/IRBus.h"
#include "../interfaces/IRfc.h"
#include "../interfaces/IPowerManagerHal.h"
#include "../interfaces/IMfr.h"
//#include "JSONRPC.h"

#include <mutex>
#include "PowerManagerAPI.h"
#include "mfr.h"

namespace WPEFramework
{
    namespace Plugin
    {
        class MockNetworkPlugin : public PluginHost::IPlugin,
                                        public Exchange::IIarm,
                                        public Exchange::IProc,
                                        public Exchange::IRBus,
                                        public Exchange::IRfc,
                                        public Exchange::IPowerManagerHal,
                                        public Exchange::IMfr,
                                        public PluginHost::JSONRPC
                                        {
        public:
            MockNetworkPlugin();
            ~MockNetworkPlugin() override;
            static MockNetworkPlugin *_instance;

            MockNetworkPlugin(const MockNetworkPlugin &) = delete;
            MockNetworkPlugin &operator=(const MockNetworkPlugin &) = delete;

            //all the supported interfaces are to be added here
            BEGIN_INTERFACE_MAP(MockNetworkPlugin)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            INTERFACE_ENTRY(Exchange::IIarm)
            INTERFACE_ENTRY(Exchange::IProc)
            INTERFACE_ENTRY(Exchange::IRBus)
            INTERFACE_ENTRY(Exchange::IRfc)
            INTERFACE_ENTRY(Exchange::IPowerManagerHal)
            INTERFACE_ENTRY(Exchange::IMfr)
            END_INTERFACE_MAP

            // Implement the basic IPlugin interface 
            const std::string Initialize(PluginHost::IShell *service) override;
            void Deinitialize(PluginHost::IShell *service) override;
            std::string Information() const override;

//
/* ********************************     IARM related mocks *********************************/
//
            // Declare the Iarm methods
            uint32_t IARM_Bus_IsConnected(const string& message, int& result );
            uint32_t IARM_Bus_Init(const string& message) override;
            uint32_t IARM_Bus_Connect() override;
            uint32_t IARM_Bus_RegisterEventHandler(const string& ownerName, int eventId) override;
            uint32_t IARM_Bus_RemoveEventHandler(const string& ownerName, int eventId) override;
            uint32_t IARM_Bus_Call(const string& ownerName, const string& methodName, uint8_t* arg, uint32_t argLen) override;
            uint32_t IARM_Bus_BroadcastEvent(const string& ownerName, int eventId, uint8_t *arg, uint32_t argLen) override;
            uint32_t IARM_Bus_RegisterCall(const string& methodName, const uint8_t* handler) override;
            uint32_t IARM_Bus_CallWithIPCTimeout(const string& ownerName, const string& methodName, uint8_t *arg, uint32_t argLen, int timeout) override;
            
            // Declare the methods to register for Iarm notifications
            uint32_t Register(Exchange::IIarm::INotification* notification ) override;
            uint32_t Unregister(Exchange::IIarm::INotification* notification ) override;
            uint32_t isConnectedToInternet(const JsonObject& parameters, JsonObject& response);

            // non-Iplugin methods
            static void sendNotificationIarm(const char* ownerName,IARM_EventId_t eventId,void* data,size_t len);

//
/* ********************************     Proc related mocks *********************************/
//
            uint32_t openproc(int flags , uint32_t &PT) override;
            uint32_t closeproc(const uint32_t PT ) override;
            uint32_t readproc(const uint32_t PT, int &tid , int &ppid , string &cmd ) override;
//
/* ********************************     RBus related mocks *********************************/
//
            uint32_t rbus_close(const std::string& handle) override;

//
/* ********************************     Rfc related mocks *********************************/
//
            uint32_t getRFCParameter(const string& pcCallerID /* @in */, const string& pcParameterName /* @in */, RFC_ParamData& pstParamData /* @in @out */) override;
            uint32_t setRFCParameter(const string& pcCallerID /* @in */, const string& pcParameterName /* @in */, const string& pcParameterValue /* @in */, uint32_t eDataType /* @in */) override;
            string& getRFCErrorString(uint32_t code /* @in */) override;

//
/* ********************************     PowerManager HAL related mocks *********************************/
//
            uint32_t PLAT_INIT() override;
            uint32_t PLAT_API_SetPowerState(uint32_t newState/* @in */) override;
            uint32_t PLAT_API_GetPowerState(uint32_t &curState/* @out */) const override;
            uint32_t PLAT_API_SetWakeupSrc(uint32_t srcType/* @in */, bool  enable/* @in */) override;
            uint32_t PLAT_API_GetWakeupSrc(uint32_t srcType/* @in */, bool  &enable/* @in */) override;
            uint32_t PLAT_Reset(uint32_t newState) override;
            uint32_t PLAT_TERM() override;

            uint32_t PLAT_DS_INIT() override;
            uint32_t PLAT_DS_SetDeepSleep(uint32_t deep_sleep_timeout/* @in */, bool& isGPIOWakeup/* @out */, bool networkStandby/* @in */) override;
            uint32_t PLAT_DS_DeepSleepWakeup() override;
            uint32_t PLAT_DS_GetLastWakeupReason(uint32_t &wakeupReason/* @out */) const override;
            uint32_t PLAT_DS_GetLastWakeupKeyCode(uint32_t &keyCode/* @out */) const override;
            uint32_t PLAT_DS_TERM() override;

//
/* ********************************     MFR related mocks *********************************/
//
            uint32_t mfrGetTempThresholds(int &high /* @out  */, int &critical /* @out  */) const override;
            uint32_t mfrSetTempThresholds(int tempHigh /* @in  */, int tempCritical /* @in  */) override;
            uint32_t mfrGetTemperature(uint32_t &curState /* @out  */, int &curTemperature /* @out  */, int &wifiTemperature /* @out  */) const override;

        private:
            //plugin common
            uint32_t _connectionId;
            PluginHost::IShell *_service;
            std::mutex _notificationMutex;

            //Iarm related
            static IarmBusImpl* _iarmImpl;
            static std::list<Exchange::IIarm::INotification*> _iarmNotificationCallbacks;

            //Proc related
            static readprocImpl* _procImpl;
            static RBusApiImpl* _rbusImpl;
            static RfcApiImpl* _rfcImpl;
            static PowerManagerImpl* _powerManagerHalMock;
            static mfrImpl* _mfrImpl;
        };
    }
}
