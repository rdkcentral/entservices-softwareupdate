#pragma once


namespace WPEFramework {
namespace Plugin {

class MaintenanceManager : public PluginHost::IPlugin, public PluginHost::JSONRPC
{
public:
    uint32_t MaintenanceManager::isConnectedToInternet(const JsonObject& parameters, JsonObject& response);

};

} // namespace Plugin
} // namespace WPEFramework
