/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
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

/**
 * @addtogroup HPK Hardware Porting Kit
 * @{
 * @par The Hardware Porting Kit
 * HPK is the next evolution of the well-defined Hardware Abstraction Layer
 * (HAL), but augmented with more comprehensive documentation and test suites
 * that OEM or SOC vendors can use to self-certify their ports before taking
 * them to RDKM for validation or to an operator for final integration and
 * deployment. The Hardware Porting Kit effectively enables an OEM and/or SOC
 * vendor to self-certify their own Video Accelerator devices, with minimal RDKM
 * assistance.
 *
 */

/**
 * @defgroup Deepsleep_Manager Deep Sleep Manager
 * @{
 * @par Application API Specification
 * Deep sleep is a power saving mode which turns off STB 
 * subsystems such as A/V, HDMI, front panels, HDD etc.
 * Provides API defines the structures and functions for the  Deep Sleep Manager HAL.
 * - The main purpose is to bring down the power consumption based on the actual usage
 * - It can also be triggered in other scenarios such as 
 *   thermal shutdown in case of the temperature is above threshold for certain period.
 * - When the STB goes in a normal power saving mode, it may be woken up 
 *   for scheduled maintenance ina predetermined time.
 */

/**
 * @defgroup Deepsleep_Manager_HAL Deep Sleep Manager HAL
 * @{
 */

/**
 * @defgroup Deepsleep_Mgr_H Deep Sleep Manager Header
 * @{
 */


/**
 * @file deepSleepMgr.h
 *
 * @brief  Deep Sleep Manager HAL Public API
 *
 * This API defines the structures and functions for the  Deep Sleep Manager HAL
 *
 * @par Abbreviations
 * - HAL:     Hardware Abstraction Layer
 * - CPE:     Customer Premises Equipment
 * - IR:      Infra-Red
 * - CEC:     Consumer Electronic Control
 * - LAN:     Local Area Network
 * - WLAN:    Wireless Local Area Network
 * - STB:     Set-top Box
 * - RCU:     Remote Control Unit
 * - STR:     Suspend To RAM
 * - HDMI:    High-Definition Multimedia Interface
 * - GPIO:    General Purpose Input/Output
 * - RCU:     Remote Control Unit
 * - BT:      Bluetooth
 * - RF4CE:   Radio Frequency for Consumer Electronics
 */


#ifndef __DEEP_SLEEP_MGR_H__
#define __DEEP_SLEEP_MGR_H__

#include <stdbool.h>
#include <inttypes.h>
#include "Iarm.h"


#ifdef __cplusplus
extern "C" 
{
#endif

/**
 * @brief Deep Sleep manager API Status return codes
 * 
 * A non-zero value indicates an error
*/
 typedef enum
 {
     DEEPSLEEPMGR_SUCCESS = 0,         /*!< Success */
     DEEPSLEEPMGR_INVALID_ARGUMENT,    /*!< Invalid Argument */
     DEEPSLEEPMGR_ALREADY_INITIALIZED, /*!< Module already initialized */
     DEEPSLEEPMGR_NOT_INITIALIZED,     /*!< Module is not initialized */
     DEEPSLEEPMGR_INIT_FAILURE,        /*!< Initialization has failed */
     DEEPSLEEPMGR_SET_FAILURE,         /*!< Failed to enter Deep Sleep */
     DEEPSLEEPMGR_WAKEUP_FAILURE,      /*!< Failed to perform wake up actions */
     DEEPSLEEPMGR_TERM_FAILURE,        /*!< Module has failed to terminate */
     DEEPSLEEPMGR_MAX                  /*!< Out of range - required to be the last item of the enum */
 } DeepSleep_Return_Status_t;

/**
 * @brief Structure which holds the HAL timeout in seconds
 */
typedef struct _DeepSleepMgr_SetDeepSleepTimer_Param_t 
{
    unsigned int timeout;   /*!< Timeout for deep sleep in seconds */
} DeepSleepMgr_SetDeepSleepTimer_Param_t;


extern DeepSleep_Return_Status_t (*PLAT_DS_INIT)(void);
extern DeepSleep_Return_Status_t (*PLAT_DS_SetDeepSleep)(uint32_t, bool*, bool);
extern DeepSleep_Return_Status_t (*PLAT_DS_DeepSleepWakeup)(void);
extern DeepSleep_Return_Status_t (*PLAT_DS_GetLastWakeupReason)(DeepSleep_WakeupReason_t*);
extern DeepSleep_Return_Status_t (*PLAT_DS_GetLastWakeupKeyCode)(DeepSleepMgr_WakeupKeyCode_Param_t*);
extern DeepSleep_Return_Status_t (*PLAT_DS_TERM)(void);

#ifdef __cplusplus
}
#endif
#endif  // End of __DEEP_SLEEP_MGR_H__

/** @} */ // End of Deepsleep_Mgr_H
/** @} */ // End of Deepsleep_Manager_HAL
/** @} */ // End Deepsleep_Manager
/** @} */ // End of HPK
