#include "TrackRuntimeLoader.h"

/* System */
#include <cstring> // memset

pm::TrackRuntimeLoader* pm::TrackRuntimeLoader::m_instance = nullptr;

pm::TrackRuntimeLoader* pm::TrackRuntimeLoader::Get()
{
    if (!m_instance)
    {
        m_instance = new(std::nothrow) TrackRuntimeLoader();
    }

    return m_instance;
}

void pm::TrackRuntimeLoader::Release()
{
    delete m_instance;
    m_instance = nullptr;
}

pm::TrackRuntimeLoader::TrackRuntimeLoader()
    : RuntimeLoader(),
    m_api(nullptr)
{
}

pm::TrackRuntimeLoader::~TrackRuntimeLoader()
{
    delete m_api;
}

void pm::TrackRuntimeLoader::Unload()
{
    delete m_api;
    m_api = nullptr;

    RuntimeLoader::Unload();
}

bool pm::TrackRuntimeLoader::LoadSymbols(bool silent)
{
    if (m_api)
        return true;

    m_api = new(std::nothrow) Api();
    memset(m_api, 0, sizeof(*m_api));

    bool status = true;

    try {
        m_api->get_lib_version = (ph_track_get_lib_version_fn)
            LoadSymbol(ph_track_get_lib_version_fn_name, silent);
    } catch (const RuntimeLoader::Exception& /*ex*/)
    { if (!silent) throw; }
    status = status && !!m_api->get_lib_version;

    try {
        m_api->get_last_error_message = (ph_track_get_last_error_message_fn)
            LoadSymbol(ph_track_get_last_error_message_fn_name, silent);
    } catch (const RuntimeLoader::Exception& /*ex*/)
    { if (!silent) throw; }
    status = status && !!m_api->get_last_error_message;

    try {
        m_api->init = (ph_track_init_fn)
            LoadSymbol(ph_track_init_fn_name, silent);
    } catch (const RuntimeLoader::Exception& /*ex*/)
    { if (!silent) throw; }
    status = status && !!m_api->init;

    try {
        m_api->link_particles = (ph_track_link_particles_fn)
            LoadSymbol(ph_track_link_particles_fn_name, silent);
    } catch (const RuntimeLoader::Exception& /*ex*/)
    { if (!silent) throw; }
    status = status && !!m_api->link_particles;

    try {
        m_api->uninit = (ph_track_uninit_fn)
            LoadSymbol(ph_track_uninit_fn_name, silent);
    } catch (const RuntimeLoader::Exception& /*ex*/)
    { if (!silent) throw; }
    status = status && !!m_api->uninit;

    return status;
}

void pm::TrackRuntimeLoader::Load()
{
    const std::string nameBase = "pvcam_helper_track";
    const std::string majorVer = std::to_string(PH_TRACK_VERSION_MAJOR);
#if defined(_WIN32)
    const std::string name = nameBase + "_v" + majorVer + ".dll";
#elif defined(__linux__)
    const std::string name = "lib" + nameBase + ".so." + majorVer;
#elif defined(__APPLE__)
    const std::string name = "lib" + nameBase + "." + majorVer + ".dylib";
#endif

    RuntimeLoader::Load(name.c_str());
}
