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

// hal includes
#include "deepSleepMgr.h" // for deepsleep hal
#include "plat_power.h"

class PowerManagerImpl {
public:
    virtual pmStatus_t PLAT_INIT() = 0;
    virtual pmStatus_t PLAT_API_SetPowerState(PWRMgr_PowerState_t newState/* @in */) = 0;
    virtual pmStatus_t PLAT_API_GetPowerState(PWRMgr_PowerState_t* curState/* @out */) = 0;
    virtual pmStatus_t PLAT_API_SetWakeupSrc(PWRMGR_WakeupSrcType_t srcType/* @in */, bool  enable/* @in */) = 0;
    virtual pmStatus_t PLAT_API_GetWakeupSrc(PWRMGR_WakeupSrcType_t srcType/* @in */, bool*  enable/* @out */) = 0;
    virtual pmStatus_t PLAT_Reset(PWRMgr_PowerState_t newState/* @in */) = 0;
    virtual pmStatus_t PLAT_TERM() = 0;

    virtual DeepSleep_Return_Status_t PLAT_DS_INIT() = 0;
    virtual DeepSleep_Return_Status_t PLAT_DS_SetDeepSleep(uint32_t deep_sleep_timeout/* @in */, bool* isGPIOWakeup/* @out */, bool networkStandby/* @in */) = 0;
    virtual DeepSleep_Return_Status_t PLAT_DS_DeepSleepWakeup() = 0;
    virtual DeepSleep_Return_Status_t PLAT_DS_GetLastWakeupReason(DeepSleep_WakeupReason_t* wakeupReason/* @out */) = 0;
    virtual DeepSleep_Return_Status_t PLAT_DS_GetLastWakeupKeyCode(DeepSleepMgr_WakeupKeyCode_Param_t *keyCode/* @out */) = 0;
    virtual DeepSleep_Return_Status_t PLAT_DS_TERM() = 0;

    virtual ~PowerManagerImpl() = default;
};

class PowerManagerAPI {
    protected:
        static PowerManagerImpl* impl;
    public:
        PowerManagerAPI();
        PowerManagerAPI(const PowerManagerAPI &obj) = delete; // deleted copy constructor so that copy of the instance cannot be created.
        static void setImpl(PowerManagerImpl* newImpl);
        static pmStatus_t PLAT_INIT();
        static pmStatus_t PLAT_API_SetPowerState(PWRMgr_PowerState_t newState/* @in */);
        static pmStatus_t PLAT_API_GetPowerState(PWRMgr_PowerState_t* curState/* @out */);
        static pmStatus_t PLAT_API_SetWakeupSrc(PWRMGR_WakeupSrcType_t srcType/* @in */, bool  enable/* @in */);
        static pmStatus_t PLAT_API_GetWakeupSrc(PWRMGR_WakeupSrcType_t srcType/* @in */, bool*  enable/* @out */);
        static pmStatus_t PLAT_Reset(PWRMgr_PowerState_t newState/* @in */);
        static pmStatus_t PLAT_TERM();

        static DeepSleep_Return_Status_t PLAT_DS_INIT();
        static DeepSleep_Return_Status_t PLAT_DS_SetDeepSleep(uint32_t deep_sleep_timeout/* @in */, bool* isGPIOWakeup/* @out */, bool networkStandby/* @in */);
        static DeepSleep_Return_Status_t PLAT_DS_DeepSleepWakeup();
        static DeepSleep_Return_Status_t PLAT_DS_GetLastWakeupReason(DeepSleep_WakeupReason_t* wakeupReason/* @out */);
        static DeepSleep_Return_Status_t PLAT_DS_GetLastWakeupKeyCode(DeepSleepMgr_WakeupKeyCode_Param_t *keyCode/* @out */);
        static DeepSleep_Return_Status_t PLAT_DS_TERM();
};
