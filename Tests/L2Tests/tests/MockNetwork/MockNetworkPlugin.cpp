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

#include "MockNetworkPlugin.h"
#include "MockAccessor.h"
#include "Rfc.h"
#include "MockUtils/RfcUtils.h"
#define API_VERSION_NUMBER_MAJOR 2
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0

#define returnJson(rc) \
    { \
        if (Core::ERROR_NONE == rc)                 \
            response["success"] = true;             \
        else                                        \
            response["success"] = false;            \
                                                    \
        return rc;                                  \
    }


#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

namespace WPEFramework
{
    static Plugin::Metadata<Plugin::Network> metadata(
            // Version (Major, Minor, Patch)
            API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH,
            // Preconditions
            {},
            // Terminations
            {},
            // Controls
            {}
    );
    namespace Plugin
    {
        // Register the MockNetworkPlugin 
        SERVICE_REGISTRATION(Network, 1,0);
        Network *Network::_instance = nullptr;
        Network::Network()
            : PluginHost::JSONRPC(), _connectionId(0), _service(nullptr)
        {
            TEST_LOG("Inside Mock Network plugin constructor");
            Network::_instance = this;
            PluginHost::JSONRPC::Register("isConnectedToInternet", &Network::isConnectedToInternet, this);
        }

        Network::~Network()
        {
            TEST_LOG("Inside Mock Network plugin destructor");
        }

        IarmBusImpl* Network::_iarmImpl = nullptr;
        readprocImpl* Network::_procImpl = nullptr;
        RBusApiImpl* Network::_rbusImpl = nullptr;
        RfcApiImpl* Network::_rfcImpl = nullptr;
        PowerManagerImpl* Network::_powerManagerHalMock = nullptr;
        mfrImpl* Network::_mfrImpl = nullptr;

        std::list<Exchange::IIarm::INotification*> Network::_iarmNotificationCallbacks;

        const std::string Network::Initialize(PluginHost::IShell *service)
        {
            TEST_LOG("Inside Mock Network plugin Initialize");
            _service = service;
            _service->AddRef();
            return "";
        }

        void Network::Deinitialize(PluginHost::IShell *service)
        {
            TEST_LOG("Inside Mock plugin Deinitialize Entry _iarmNotificationCallbacks size = %ld", _iarmNotificationCallbacks.size());
            std::lock_guard<std::mutex> lock(_notificationMutex);
            _iarmNotificationCallbacks.clear();
            _service = nullptr;
            _iarmImpl = nullptr;
            _procImpl = nullptr;
            _rbusImpl = nullptr;
            _rfcImpl = nullptr;
            TEST_LOG("Inside Mock plugin Deinitialize Exit _iarmNotificationCallbacks size = %ld", _iarmNotificationCallbacks.size());
        }

        std::string Network::Information() const
        {
            TEST_LOG("Inside Mock plugin Information");
            return "NetworkPlugin Information";
        }

//
/* ********************************     IARM related mocks starts. *********************************/
//
        uint32_t Network::IARM_Bus_Init(const string& message)
        {
            TEST_LOG("Inside Mock plugin IARM_Bus_Init IarmBusImpl = %p", _iarmImpl);
            TEST_LOG("Calling IARM_Bus_Init");

            return (uint32_t)_iarmImpl-> IARM_Bus_Init("Thunder_Plugins");
        }

        uint32_t Network::IARM_Bus_Connect()
        {
            TEST_LOG("Inside Mock plugin IARM_Bus_Connect IarmBusImpl = %p", _iarmImpl);
            TEST_LOG("Calling IARM_Bus_Connect");

            return (uint32_t)_iarmImpl-> IARM_Bus_Connect();
        }

        uint32_t Network::IARM_Bus_RegisterEventHandler(const string& ownerName, int eventId)
        {
            TEST_LOG("Inside Mock plugin IARM_Bus_RegisterEventHandler IarmBusImpl = %p", _iarmImpl);
            TEST_LOG("Calling IARM_Bus_RegisterEventHandler");

            return (uint32_t)_iarmImpl-> IARM_Bus_RegisterEventHandler(ownerName.c_str(),eventId,reinterpret_cast<IARM_EventHandler_t>(&sendNotificationIarm));
        }

        uint32_t Network::IARM_Bus_RemoveEventHandler(const string& ownerName, int eventId)
        {
            TEST_LOG("Inside Mock plugin IARM_Bus_RemoveEventHandler IarmBusImpl = %p", _iarmImpl);
            TEST_LOG("Calling IARM_Bus_RemoveEventHandler");

            return (uint32_t)_iarmImpl-> IARM_Bus_RemoveEventHandler(ownerName.c_str(),eventId,reinterpret_cast<IARM_EventHandler_t>(&sendNotificationIarm));
        }

        uint32_t Network::IARM_Bus_Call(const string& ownerName, const string& methodName, uint8_t* arg, uint32_t argLen)
        {
            TEST_LOG("Inside Mock plugin IARM_Bus_Call IarmBusImpl = %p", _iarmImpl);
            TEST_LOG("Calling IARM_Bus_Call, argLen: %u",argLen);

            return (uint32_t)_iarmImpl-> IARM_Bus_Call(ownerName.c_str(), methodName.c_str(), reinterpret_cast<void*>(arg), argLen);
        }

        uint32_t Network::IARM_Bus_BroadcastEvent(const string& ownerName, int eventId, uint8_t *arg, uint32_t argLen)
        {
            TEST_LOG("Inside Mock plugin IARM_Bus_BroadcastEvent IarmBusImpl = %p", _iarmImpl);
            TEST_LOG("Calling IARM_Bus_BroadcastEvent");

            return (uint32_t)_iarmImpl-> IARM_Bus_BroadcastEvent(ownerName.c_str(), eventId, reinterpret_cast<void*>(arg), argLen);
        }

        uint32_t Network::IARM_Bus_RegisterCall(const string& methodName, const uint8_t* handler)
        {
            TEST_LOG("Inside Mock plugin IARM_Bus_RegisterCall IarmBusImpl = %p", _iarmImpl);
            TEST_LOG("Calling IARM_Bus_RegisterCall");

            return (uint32_t)_iarmImpl-> IARM_Bus_RegisterCall(methodName.c_str(), reinterpret_cast<IARM_BusCall_t>(handler));
        }

        uint32_t Network::IARM_Bus_CallWithIPCTimeout(const string& ownerName, const string& methodName, uint8_t *arg, uint32_t argLen, int timeout)
        {
            TEST_LOG("Inside Mock plugin IARM_Bus_CallWithIPCTimeout IarmBusImpl = %p", _iarmImpl);
            TEST_LOG("Calling IARM_Bus_CallWithIPCTimeout");

            return (uint32_t)_iarmImpl-> IARM_Bus_Call_with_IPCTimeout(ownerName.c_str(), methodName.c_str(), reinterpret_cast<void*>(arg), argLen, timeout);
        }

        uint32_t Network::IARM_Bus_IsConnected(const string& message, int& result)
        {
            TEST_LOG("Inside Mock plugin IARM_Bus_IsConnected IarmBusImpl = %p", _iarmImpl);
            int isRegistered = 0;
            IARM_Result_t returnValue;
            TEST_LOG("Calling IARM_Bus_IsConnected");
            returnValue = _iarmImpl-> IARM_Bus_IsConnected(message.c_str(),&isRegistered);
            TEST_LOG("IARM_Bus_IsConnected completed isRegistered: %d", isRegistered);
            result = isRegistered;
            return (uint32_t)returnValue;
        }

        void Network::sendNotificationIarm(const char* ownerName,IARM_EventId_t eventId,void* data,size_t len)
        {
            TEST_LOG("sendNotificationIarm called from gtest mock");
            for (const auto& notifyCb : _iarmNotificationCallbacks) 
            {
                std::string eventData(static_cast<char*>(data), len);
                uint32_t result = notifyCb->IARM_EventHandler(ownerName,eventId, eventData);
                TEST_LOG("sendNotificationIarm completed result: %d", result);
            }
        }

        uint32_t Network::Register(Exchange::IIarm::INotification *notification)
        {
            TEST_LOG("MockNetworkPlugin::Register Entry");
            if (nullptr == notification)
            {
                TEST_LOG("Error: Received a null notification pointer.");
                return Core::ERROR_GENERAL;
            }

            std::lock_guard<std::mutex> locker(_notificationMutex);

            if (std::find(_iarmNotificationCallbacks.begin(), _iarmNotificationCallbacks.end(), notification) == _iarmNotificationCallbacks.end())
            {
                _iarmNotificationCallbacks.push_back(notification);
                notification->AddRef();
            }
            else
            {
                TEST_LOG(" Already registered the notification");
            }

            return Core::ERROR_NONE;
            TEST_LOG("Network::Register Exit");
        }

        uint32_t Network::Unregister(IIarm::INotification *notification)
        {
            std::lock_guard<std::mutex> locker(_notificationMutex);

            auto itr = std::find(_iarmNotificationCallbacks.begin(), _iarmNotificationCallbacks.end(), notification);
            if (itr != _iarmNotificationCallbacks.end())
            {
                (*itr)->Release();
                _iarmNotificationCallbacks.erase(itr);
            }
            else
            {
                TEST_LOG("Error Item not found in the list");
            }
            return Core::ERROR_NONE;
        }

//
/* ********************************     IARM related mocks End. *********************************/
//

//
/* ********************************     Proc related mocks starts. *********************************/
//
        uint32_t Network::openproc(int flags , uint32_t &PT)
        {
            PROCTAB *pTab = NULL;
            if ((pTab = _procImpl->openproc(flags)) == NULL)
            {
                return Core::ERROR_GENERAL;
            }
            else
            {
                PT = (uint32_t)(uintptr_t)pTab->procfs;
            }
            return Core::ERROR_NONE;
        }

        uint32_t Network::closeproc(const uint32_t PT )
        {
            PROCTAB pTab = {};
            pTab.procfs = reinterpret_cast<DIR*>(PT);
            _procImpl->closeproc(&pTab);
            return Core::ERROR_NONE;
        }

        uint32_t Network::readproc(const uint32_t PT, int &tid , int &ppid , string &cmd )
        {
            PROCTAB pTab = {};
            proc_t p = {};
            pTab.procfs = reinterpret_cast<DIR*>(PT);
            if ((_procImpl->readproc(&pTab,&p)) == NULL)
            {
                return Core::ERROR_GENERAL;
            }
            else
            {
                cmd = string(p.cmd,16);
                tid = p.tid;
                ppid = p.ppid;
            }
            return Core::ERROR_NONE;
        }

//
/* ********************************     Proc related mocks End. *********************************/
//

//
/* ********************************     Rbus related mocks starts. *********************************/
//
        uint32_t Network::rbus_close(const std::string& handle)
        {
            TEST_LOG("Inside Mock plugin rbus_close, rbusImpl = %p", _rbusImpl);
            TEST_LOG("Calling rbus_close with handle: %s", handle.c_str());

            // Call the mocked implementation of rbus_close
            _rbusHandle* rbusHandle = nullptr;
            uint32_t result = static_cast<uint32_t>(_rbusImpl->rbus_close(rbusHandle));

            return result;
        }

//
/* ********************************     Rbus related mocks Ends. *********************************/
//

//
/* ********************************     Rfc related mocks starts. *********************************/
//
        uint32_t Network::getRFCParameter(const string& pcCallerID, const string& pcParameterName, RFC_ParamData& pstParamData)
        {
            TEST_LOG("Inside Mock plugin getRFCParameter pcCallerID = %s pcParameterName = %s", pcCallerID.c_str(), pcParameterName.c_str());

            char* callerId = new char[pcCallerID.length() + 1];
            strcpy(callerId, pcCallerID.c_str());

            char* parameterName = new char[pcParameterName.length() + 1];
            strcpy(parameterName, pcParameterName.c_str());

            RFC_ParamData_t paramData = {0};
            utilConvertParamDataToRfc(pstParamData, &paramData);

            TEST_LOG("Calling gmock getRFCParameter _rfcImpl = %p", _rfcImpl);
            assert (nullptr != _rfcImpl);
            _rfcImpl-> getRFCParameter(callerId, parameterName, &paramData);
            utilConvertParamDataFromRfc(&paramData, pstParamData);

            delete[] callerId;
            delete[] parameterName;

            TEST_LOG("Completed gmock getRFCParameter type = %d name = %s value = %s", paramData.type, paramData.name, paramData.value);
            return Core::ERROR_NONE;
        }

        uint32_t Network::setRFCParameter(const string& pcCallerID, const string& pcParameterName, const string& pcParameterValue, uint32_t eDataType)
        {
            TEST_LOG("Inside Mock plugin setRFCParameter _rfcImpl = %p pcParameterName = %s", _rfcImpl, pcParameterName);
            return Core::ERROR_NONE;
        }

        string rfcError;
        string& Network::getRFCErrorString(uint32_t code)
        {
            TEST_LOG("Inside Mock plugin getRFCErrorString _rfcImpl = %p", _rfcImpl);
            return rfcError;
        }
//
/* ********************************     Rfc related mocks Ends. *********************************/
//

//
/* ********************************     PowerManager HAL related mocks starts. *********************************/
//

        uint32_t Network::PLAT_INIT()
        {
            TEST_LOG("Entry");
            uint32_t result = static_cast<uint32_t>(_powerManagerHalMock->PLAT_INIT());

            return result;
        }

        uint32_t Network::PLAT_API_SetPowerState(uint32_t newState)
        {
            TEST_LOG("Entry");
            uint32_t result = static_cast<uint32_t>(_powerManagerHalMock->PLAT_API_SetPowerState(static_cast<PWRMgr_PowerState_t>(newState)));
            return result;
        }

        uint32_t Network::PLAT_API_GetPowerState(uint32_t& curState) const
        {
            PWRMgr_PowerState_t _curState;
            TEST_LOG("Entry");
            uint32_t result = static_cast<uint32_t>(_powerManagerHalMock->PLAT_API_GetPowerState(&_curState));
            curState = static_cast<uint32_t>(_curState);
            return result;
        }

        uint32_t Network::PLAT_API_SetWakeupSrc(uint32_t srcType, bool enable)
        {
            TEST_LOG("Entry");
            uint32_t result = static_cast<uint32_t>(_powerManagerHalMock->PLAT_API_SetWakeupSrc(static_cast<PWRMGR_WakeupSrcType_t>(srcType), enable));
            return result;
        }

        uint32_t Network::PLAT_API_GetWakeupSrc(uint32_t srcType, bool& enable)
        {
            TEST_LOG("Entry");
            uint32_t result = static_cast<uint32_t>(_powerManagerHalMock->PLAT_API_GetWakeupSrc(static_cast<PWRMGR_WakeupSrcType_t>(srcType), &enable));
            return result;
        }
        uint32_t Network::isConnectedToInternet(const JsonObject& parameters, JsonObject& response)
        {
           TEST_LOG("Entry - Inside isConnectedToInternet");
           uint32_t rc = Core::ERROR_NONE;
           response["connectedToInternet"] = "true";
           response["success"] = "true"; 
           return rc;           
        }

        uint32_t Network::PLAT_Reset(uint32_t newState)
        {
            TEST_LOG("Entry");
            uint32_t result = static_cast<uint32_t>(_powerManagerHalMock->PLAT_Reset(static_cast<PWRMgr_PowerState_t>(newState)));
            return result;
        }

        uint32_t Network::PLAT_TERM(void)
        {
            TEST_LOG("Entry");
            uint32_t result = static_cast<uint32_t>(_powerManagerHalMock->PLAT_TERM());
            return result;
        }

        uint32_t Network::PLAT_DS_INIT(void)
        {
            TEST_LOG("Entry");
            uint32_t result = static_cast<uint32_t>(_powerManagerHalMock->PLAT_DS_INIT());
            return result;
        }

        uint32_t Network::PLAT_DS_SetDeepSleep(uint32_t deep_sleep_timeout, bool& isGPIOWakeup, bool networkStandby)
        {
            TEST_LOG("Entry");
            uint32_t result = static_cast<uint32_t>(_powerManagerHalMock->PLAT_DS_SetDeepSleep(deep_sleep_timeout, &isGPIOWakeup, networkStandby));
            return result;
        }

        uint32_t Network::PLAT_DS_DeepSleepWakeup(void)
        {
            TEST_LOG("Entry");
            uint32_t result = static_cast<uint32_t>(_powerManagerHalMock->PLAT_DS_DeepSleepWakeup());
            return result;
        }

        uint32_t Network::PLAT_DS_GetLastWakeupReason(uint32_t &wakeupReason) const
        {
            DeepSleep_WakeupReason_t _wakeupReason;
            TEST_LOG("Entry");
            uint32_t result = static_cast<uint32_t>(_powerManagerHalMock->PLAT_DS_GetLastWakeupReason(&_wakeupReason));
            wakeupReason =static_cast<DeepSleep_WakeupReason_t> (_wakeupReason);
            return result;
        }

        uint32_t Network::PLAT_DS_GetLastWakeupKeyCode(uint32_t &KeyCode) const
        {
            DeepSleepMgr_WakeupKeyCode_Param_t st_wakeupKeyCode;
            TEST_LOG("Entry");
            uint32_t result = static_cast<uint32_t>(_powerManagerHalMock->PLAT_DS_GetLastWakeupKeyCode(&st_wakeupKeyCode));
            KeyCode = st_wakeupKeyCode.keyCode;
            return result;
        }

        uint32_t Network::PLAT_DS_TERM(void)
        {
            TEST_LOG("Entry");
            uint32_t result = static_cast<uint32_t>(_powerManagerHalMock->PLAT_DS_TERM());
            return result;
        }

//
/* ********************************     PowerManager HAL related mocks Ends. *********************************/
//

//
/* ********************************     MFR related mocks starts. *********************************/
//

        uint32_t Network::mfrGetTempThresholds(int &high /* @out  */, int &critical /* @out  */) const
        {
            TEST_LOG("Entry");
            uint32_t result = static_cast<uint32_t>(_mfrImpl->mfrGetTempThresholds(&high,&critical));
            return result;
        }

        uint32_t Network::mfrSetTempThresholds(int tempHigh /* @in  */, int tempCritical /* @in  */)
        {
            TEST_LOG("Entry");
            uint32_t result = static_cast<uint32_t>(_mfrImpl->mfrSetTempThresholds(tempHigh,tempCritical));
            return result;
        }

        uint32_t Network::mfrGetTemperature(uint32_t &curState /* @out  */, int &curTemperature /* @out  */, int &wifiTemperature /* @out  */) const
        {
            TEST_LOG("Entry");
            mfrTemperatureState_t _curState;
            uint32_t result = static_cast<uint32_t>(_mfrImpl->mfrGetTemperature(&_curState,&curTemperature,&wifiTemperature));
            curState = _curState;
            return result;
        }

//
/* ********************************     Rfc related mocks Ends. *********************************/
//

    }
}
