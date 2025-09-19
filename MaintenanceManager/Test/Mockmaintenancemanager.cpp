#pragma once
#include "../MaintenanceManager.h"

namespace WPEFramework {
namespace Plugin {

uint32_t MaintenanceManager::isConnectedToInternet(const JsonObject& parameters, JsonObject& response)
        { 
           uint32_t rc = Core::ERROR_NONE;
		   MM_LOGINFO("Inside isConnectedTointernet");
           //response["connectedToInternet"] = "true";
		   response["connectedToInternet"] = true;
           response["success"] = "true"; 
           return rc;           
        }

} // namespace Plugin
} // namespace WPEFramework
