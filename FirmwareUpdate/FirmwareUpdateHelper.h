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

#ifndef FIRMWARE_UPDATE_HELPER_H_
#define FIRMWARE_UPDATE_HELPER_H_
#include "UtilsfileExists.h"
#include "rfcapi.h"
#include <regex>
#include <sys/prctl.h>
#include "UtilsJsonRpc.h"
#include <mutex>
#include "tracing/Logging.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <cstdlib>
#include <secure_wrapper.h>
#include <fstream>
#include <sstream>
#include "UtilsController.h"
#include "UtilsIarm.h"
#include "maintenanceMGR.h"

#define MAX_BUFF_SIZE 512
#define MAX_BUFF_SIZE1 256
#define MIN_BUFF_SIZE 64
#define MIN_BUFF_SIZE1 32
#define MAX_DEVICE_PROP_BUFF_SIZE 80
#define PCI_UPGRADE 0
#define PDRI_UPGRADE 1
#define SMALL_SIZE_BUFF 8
#define UTILS_SUCCESS 1
#define UTILS_FAIL -1
#define DEVICE_PROPERTIES_FILE  "/etc/device.properties"
#define INCLUDE_PROPERTIES_FILE "/etc/include.properties"
#define IMAGE_DETAILS           "/tmp/version.txt"
#define USB_TMP_COPY            "/tmp/USBCopy"
#define FIRMWARE_UPDATE_STATE   "/tmp/FirmwareUpdateStatus.txt"
#define HTTP_CDL_FLAG "/tmp/device_initiated_rcdl_in_progress"
#define RCDL_FLAG "/tmp/device_initiated_rcdl_in_progress"
#define SNMP_CDL_FLAG "/tmp/device_initiated_snmp_cdl_in_progress"
#define STATUS_FILE "/opt/fwdnldstatus.txt"
#define SUCCESS 1
#define FAILURE -1
#define MAINTENANCE_MGR_RECORD_FILE "/opt/maintenance_mgr_record.conf"
//Below file is use for update MAINTENANCE_MGR_RECORD_FILE file data.
#define MAINTENANCE_MGR_RECORD_UPDATE_FILE "/opt/.mm_record_update.conf"
#define FACTORYPROTECT_CALLSIGN_VER "org.rdk.FactoryProtect.1"

#define RFC_VALUE_BUF_SIZE 512
#define READ_RFC_SUCCESS 1
#define READ_RFC_FAILURE -1
#define READ_RFC_NOTAPPLICABLE 0
#define WRITE_RFC_SUCCESS 1
#define WRITE_RFC_FAILURE -1
#define WRITE_RFC_NOTAPPLICABLE 0
#define REBOOT_PENDING_DELAY "2"
#define STATEREDFLAG "/tmp/stateRedEnabled"
#define IARM_BUS_SYSMGR_SYSSTATE_FIRMWARE_UPDATE_STATE 1
#define IARM_BUS_SYSMGR_SYSSTATE_FIRMWARE_DWNLD 2

#define IMG_DWL_EVENT "ImageDwldEvent"
#define FW_STATE_EVENT "FirmwareStateEvent"

//Image Download States
#define IMAGE_FWDNLD_UNINITIALIZED "0"
#define IMAGE_FWDNLD_DOWNLOAD_INPROGRESS "1"
#define IMAGE_FWDNLD_DOWNLOAD_COMPLETE "2"
#define IMAGE_FWDNLD_DOWNLOAD_FAILED "3"
#define IMAGE_FWDNLD_FLASH_INPROGRESS "4"
#define IMAGE_FWDNLD_FLASH_COMPLETE "5"
#define IMAGE_FWDNLD_FLASH_FAILED "6"

//maintaince states
#define MAINT_FWDOWNLOAD_COMPLETE_ "8"
#define MAINT_FWDOWNLOAD_ERROR_ "9"
#define MAINT_FWDOWNLOAD_ABORTED_ "10"
#define MAINT_CRITICAL_UPDATE_ "11"
#define MAINT_REBOOT_REQUIRED_ "12"
#define MAINT_FWDOWNLOAD_INPROGRESS_ "15"
#define MAINT_FWDOWNLOAD_FG_ "17"
#define MAINT_FWDOWNLOAD_BG_ "18"

//Firmware Upgrade states
#define FW_STATE_UNINITIALIZED "0"
#define FW_STATE_REQUESTING "1"
#define FW_STATE_DOWNLOADING "2"
#define FW_STATE_FAILED "3"
#define FW_STATE_DOWNLOAD_COMPLETE "4"
#define FW_STATE_VALIDATION_COMPLETE "5"
#define FW_STATE_PREPARING_TO_REBOOT "6"
#define FW_STATE_ONHOLD_FOR_OPTOUT "7"
#define FW_STATE_CRITICAL_REBOOT "8"
#define FW_STATE_NO_UPGRADE_REQUIRED "9"

#define RFC_MNG_NOTIFY "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.ManageableNotification.Enable"
#define RFC_FW_REBOOT_NOTIFY "Device.DeviceInfo.X_RDKCENTRAL-COM_xOpsDeviceMgmt.RPC.RebootPendingNotification"

//Firmware state
#define _VALIDATION_FAILED "VALIDATION_FAILED"
#define _FLASHING_STARTED "FLASHING_STARTED"
#define _FLASHING_FAILED "FLASHING_FAILED"
#define _FLASHING_SUCCEEDED "FLASHING_SUCCEEDED"
#define _WAITING_FOR_REBOOT "WAITING_FOR_REBOOT"

//Firmware substate
#define _FIRMWARE_NOT_FOUND "FIRMWARE_NOT_FOUND"
#define _FIRMWARE_INVALID "FIRMWARE_INVALID"
#define _FIRMWARE_OUTDATED "FIRMWARE_OUTDATED"
#define _FIRMWARE_UPTODATE "FIRMWARE_UPTODATE"
#define _FIRMWARE_INCOMPATIBLE "FIRMWARE_INCOMPATIBLE"
#define _PREWRITE_SIGNATURE_CHECK_FAILED "PREWRITE_SIGNATURE_CHECK_FAILED"
#define _FLASH_WRITE_FAILED "FLASH_WRITE_FAILED"
#define _POSTWRITE_FIRMWARE_CHECK_FAILED "POSTWRITE_FIRMWARE_CHECK_FAILED"
#define _POSTWRITE_SIGNATURE_CHECK_FAILED "POSTWRITE_SIGNATURE_CHECK_FAILED"

inline const char* ExtractFileName(const char* filePath) {
    const char* fileName = strrchr(filePath, '/');
    return fileName ? (fileName + 1) : filePath;
}

std::string GetCurrentTimestamp();

extern std::mutex logMutex;

#define SWUPDATEINFO(fmt, ...) do { \
    std::lock_guard<std::mutex> guard(logMutex); \
    std::ofstream logFile("/opt/logs/swupdate.log", std::ios::app); \
    if (logFile.is_open()) { \
        const char* filename_ = ExtractFileName(__FILE__); \
        pid_t pid = getpid(); \
        pid_t tid = syscall(SYS_gettid); \
        std::string timestamp = GetCurrentTimestamp(); \
        logFile << timestamp << " " \
                << "FirmwareUpdate WPEFramework[" << pid << "]: [" << tid << "] INFO [" \
                << filename_ << ":" << __LINE__ << "] " \
                << __FUNCTION__ << ": "; \
        char buffer[1024]; \
        snprintf(buffer, sizeof(buffer), fmt, ##__VA_ARGS__); \
        logFile << buffer << std::endl; \
        logFile.close(); \
    } else { \
        fprintf(stderr, "Failed to open log file /opt/logs/swupdate.log\n"); \
    } \
} while (0)

#define SWUPDATEERR(fmt, ...) do { \
    std::lock_guard<std::mutex> guard(logMutex); \
    std::ofstream logFile("/opt/logs/swupdate.log", std::ios::app); \
    if (logFile.is_open()) { \
        const char* filename_ = ExtractFileName(__FILE__); \
        pid_t pid = getpid(); \
        pid_t tid = syscall(SYS_gettid); \
        std::string timestamp = GetCurrentTimestamp(); \
        logFile << timestamp << " " \
                << "FirmwareUpdate WPEFramework[" << pid << "]: [" << tid << "] ERROR [" \
                << filename_ << ":" << __LINE__ << "] " \
                << __FUNCTION__ << ": "; \
        char buffer[1024]; \
        snprintf(buffer, sizeof(buffer), fmt, ##__VA_ARGS__); \
        logFile << buffer << std::endl; \
        logFile.close(); \
    } else { \
        fprintf(stderr, "Failed to open log file /opt/logs/swupdate.log\n"); \
    } \
} while (0)

std::map<std::string, WPEFramework::Exchange::IFirmwareUpdate::State> firmwareState = {
    {"VALIDATION_FAILED", WPEFramework::Exchange::IFirmwareUpdate::State::VALIDATION_FAILED},
    {"FLASHING_STARTED", WPEFramework::Exchange::IFirmwareUpdate::State::FLASHING_STARTED},
    {"FLASHING_FAILED", WPEFramework::Exchange::IFirmwareUpdate::State::FLASHING_FAILED},
    {"FLASHING_SUCCEEDED", WPEFramework::Exchange::IFirmwareUpdate::State::FLASHING_SUCCEEDED},
    {"WAITING_FOR_REBOOT", WPEFramework::Exchange::IFirmwareUpdate::State::WAITING_FOR_REBOOT}
};

std::map<std::string, WPEFramework::Exchange::IFirmwareUpdate::SubState> firmwareSubState = {
    {"NOT_APPLICABLE", WPEFramework::Exchange::IFirmwareUpdate::SubState::NOT_APPLICABLE},
    {"FIRMWARE_NOT_FOUND", WPEFramework::Exchange::IFirmwareUpdate::SubState::FIRMWARE_NOT_FOUND},
    {"FIRMWARE_INVALID", WPEFramework::Exchange::IFirmwareUpdate::SubState::FIRMWARE_INVALID},
    {"FIRMWARE_OUTDATED", WPEFramework::Exchange::IFirmwareUpdate::SubState::FIRMWARE_OUTDATED},
    {"FIRMWARE_UPTODATE", WPEFramework::Exchange::IFirmwareUpdate::SubState::FIRMWARE_UPTODATE},
    {"FIRMWARE_INCOMPATIBLE", WPEFramework::Exchange::IFirmwareUpdate::SubState::FIRMWARE_INCOMPATIBLE},
    {"PREWRITE_SIGNATURE_CHECK_FAILED", WPEFramework::Exchange::IFirmwareUpdate::SubState::PREWRITE_SIGNATURE_CHECK_FAILED},
    {"FLASH_WRITE_FAILED", WPEFramework::Exchange::IFirmwareUpdate::SubState::FLASH_WRITE_FAILED},
    {"POSTWRITE_FIRMWARE_CHECK_FAILED", WPEFramework::Exchange::IFirmwareUpdate::SubState::POSTWRITE_FIRMWARE_CHECK_FAILED},
    {"POSTWRITE_SIGNATURE_CHECK_FAILED", WPEFramework::Exchange::IFirmwareUpdate::SubState::POSTWRITE_SIGNATURE_CHECK_FAILED}
};


const string REGEX_PATH = "^\\/([^~`.,!@#$%^&*:;()+={}<>\\[\\]\\s]+\\/)+$"; // here we are restriced Special Characters like (~`.,!@#$%^&*()+={}<>[]\\s(space))3
const string REGEX_BIN = "[\\w-]*\\.{0,1}[\\w-]*\\.bin";

typedef enum
{
    RFC_STRING=1,
    RFC_BOOL,
    RFC_UINT
}RFCVALDATATYPE;

struct FWDownloadStatus {
    char method[MIN_BUFF_SIZE1];
    char proto[MIN_BUFF_SIZE1];
    char status[MIN_BUFF_SIZE1];
    char reboot[MIN_BUFF_SIZE1];
    char failureReason[MAX_BUFF_SIZE];
    char dnldVersn[MAX_BUFF_SIZE1];
    char dnldfile[MAX_BUFF_SIZE1];
    char dnldurl[MAX_BUFF_SIZE];
    char lastrun[MAX_BUFF_SIZE1];
    char FwUpdateState[MIN_BUFF_SIZE];
    char DelayDownload[MIN_BUFF_SIZE1];
    char codeBig[MIN_BUFF_SIZE1];
};


void eventManager(const char *cur_event_name, const char *event_status) ;
int getDevicePropertyData(const char *dev_prop_name, char *out_data, unsigned int buff_size);
bool isMediaClientDevice(void);
void updateUpgradeFlag(int proto ,int action);
std::string extractField(const std::string& line, char delimiter, int fieldIndex) ;
int updateFWDownloadStatus(struct FWDownloadStatus *fwdls, const char *disableStatsUpdate , const char *initiated_type) ;
int read_RFCProperty(char* type, const char* key, char *out_value, size_t datasize) ;
int write_RFCProperty(char* type, const char* key, const char *value, RFCVALDATATYPE datatype) ;
bool isMmgbleNotifyEnabled(void);
bool updateOPTOUTFile(const char *optout_file_name);
int notifyDwnlStatus(const char *key, const char *value, RFCVALDATATYPE datatype) ;
void unsetStateRed(void);
string deviceSpecificRegexBin(); 
string deviceSpecificRegexPath();
bool createDirectory(const std::string &path) ;
bool copyFileToDirectory(const char *source_file, const char *destination_dir) ;
bool FirmwareStatus(std::string& state, std::string& substate, const std::string& mode) ;
std::string readProperty( std::string filename,std::string property, std::string delimiter) ;

#endif /* FIRMWARE_UPDATE_HELPER_H_ */
