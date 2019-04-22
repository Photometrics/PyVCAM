#include "ParamBrowserRuntimeLoader.h"

/* System */
#include <cstring> // memset

pm::ParamBrowserRuntimeLoader* pm::ParamBrowserRuntimeLoader::m_instance = nullptr;

pm::ParamBrowserRuntimeLoader* pm::ParamBrowserRuntimeLoader::Get()
{
    if (!m_instance)
    {
        m_instance = new(std::nothrow) ParamBrowserRuntimeLoader();
    }

    return m_instance;
}

void pm::ParamBrowserRuntimeLoader::Release()
{
    delete m_instance;
    m_instance = nullptr;
}

pm::ParamBrowserRuntimeLoader::ParamBrowserRuntimeLoader()
    : RuntimeLoader(),
    m_api(nullptr)
{
}

pm::ParamBrowserRuntimeLoader::~ParamBrowserRuntimeLoader()
{
    delete m_api;
}

void pm::ParamBrowserRuntimeLoader::Unload()
{
    delete m_api;
    m_api = nullptr;

    RuntimeLoader::Unload();
}

bool pm::ParamBrowserRuntimeLoader::LoadSymbols(bool silent)
{
    if (m_api)
        return true;

    m_api = new(std::nothrow) Api();
    memset(m_api, 0, sizeof(*m_api));

    bool status = true;

    try {
        m_api->get_lib_version = (ph_param_browser_get_lib_version_fn)
            LoadSymbol(ph_param_browser_get_lib_version_fn_name, silent);
    } catch (const RuntimeLoader::Exception& /*ex*/)
    { if (!silent) throw; }
    status = status && !!m_api->get_lib_version;

    try {
        m_api->input_params = (ph_param_browser_input_params_fn)
            LoadSymbol(ph_param_browser_input_params_fn_name, silent);
    } catch (const RuntimeLoader::Exception& /*ex*/)
    { if (!silent) throw; }
    status = status && !!m_api->input_params;

    try {
        m_api->init = (ph_param_browser_init_fn)
            LoadSymbol(ph_param_browser_init_fn_name, silent);
    } catch (const RuntimeLoader::Exception& /*ex*/)
    { if (!silent) throw; }
    status = status && !!m_api->init;

    try {
        m_api->uninit = (ph_param_browser_uninit_fn)
            LoadSymbol(ph_param_browser_uninit_fn_name, silent);
    } catch (const RuntimeLoader::Exception& /*ex*/)
    { if (!silent) throw; }
    status = status && !!m_api->uninit;

    try {
        m_api->invoke = (ph_param_browser_invoke_fn)
            LoadSymbol(ph_param_browser_invoke_fn_name, silent);
    } catch (const RuntimeLoader::Exception& /*ex*/)
    { if (!silent) throw; }
    status = status && !!m_api->invoke;

    return status;
}

void pm::ParamBrowserRuntimeLoader::Load()
{
    const std::string nameBase = "pvcam_helper_param_browser";
    const std::string majorVer = std::to_string(PH_PARAM_BROWSER_VERSION_MAJOR);
#if defined(_WIN32)
    const std::string name = nameBase + "_v" + majorVer + ".dll";
#elif defined(__linux__)
    const std::string name = "lib" + nameBase + ".so." + majorVer;
#elif defined(__APPLE__)
    const std::string name = "lib" + nameBase + "." + majorVer + ".dylib";
#endif

    RuntimeLoader::Load(name.c_str());
}
