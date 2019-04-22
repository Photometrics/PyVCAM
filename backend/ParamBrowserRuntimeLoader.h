#pragma once
#ifndef PM_PARAM_BROWSER_RUNTIME_LOADER_H
#define PM_PARAM_BROWSER_RUNTIME_LOADER_H

#include "RuntimeLoader.h"

/* pvcam_helper_param_browser */
#include <pvcam_helper_param_browser.h>

namespace pm {

class ParamBrowserRuntimeLoader final : public RuntimeLoader
{
public:
    struct Api
    {
        ph_param_browser_get_lib_version_fn get_lib_version;
        ph_param_browser_input_params_fn input_params;
        ph_param_browser_init_fn init;
        ph_param_browser_uninit_fn uninit;
        ph_param_browser_invoke_fn invoke;
    };

public:
    static ParamBrowserRuntimeLoader* m_instance;

    /// @brief Creates singleton instance
    static ParamBrowserRuntimeLoader* Get();
    static void Release();

private:
    ParamBrowserRuntimeLoader();
    virtual ~ParamBrowserRuntimeLoader();

    ParamBrowserRuntimeLoader(const ParamBrowserRuntimeLoader&) = delete;
    ParamBrowserRuntimeLoader(ParamBrowserRuntimeLoader&&) = delete;
    ParamBrowserRuntimeLoader& operator=(const ParamBrowserRuntimeLoader&) = delete;
    ParamBrowserRuntimeLoader& operator=(ParamBrowserRuntimeLoader&&) = delete;

public: // From RuntimeLoader
    virtual void Unload();
    virtual bool LoadSymbols(bool silent = false) override;

public:
    /// @brief Calls RuntimeLoader::Load with deduced library name.
    /// @throw RuntimeLoader::Exception with error message for logging.
    void Load();
    /// @brief Returns loaded Api structure or null if not loaded.
    inline const Api* GetApi() const
    { return m_api; }

private:
    Api* m_api;
};

} // namespace pm

#define PH_PARAM_BROWSER pm::ParamBrowserRuntimeLoader::Get()->GetApi()

#endif /* PM_PARAM_BROWSER_RUNTIME_LOADER_H */
