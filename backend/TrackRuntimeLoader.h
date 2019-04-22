#pragma once
#ifndef PM_TRACK_RUNTIME_LOADER_H
#define PM_TRACK_RUNTIME_LOADER_H

#include "RuntimeLoader.h"

/* pvcam_helper_track */
#include <pvcam_helper_track.h>

namespace pm {

class TrackRuntimeLoader final : public RuntimeLoader
{
public:
    struct Api
    {
        ph_track_get_lib_version_fn get_lib_version;
        ph_track_get_last_error_message_fn get_last_error_message;
        ph_track_init_fn init;
        ph_track_link_particles_fn link_particles;
        ph_track_uninit_fn uninit;
    };

public:
    static TrackRuntimeLoader* m_instance;

    /// @brief Creates singleton instance
    static TrackRuntimeLoader* Get();
    static void Release();

private:
    TrackRuntimeLoader();
    virtual ~TrackRuntimeLoader();

    TrackRuntimeLoader(const TrackRuntimeLoader&) = delete;
    TrackRuntimeLoader(TrackRuntimeLoader&&) = delete;
    TrackRuntimeLoader& operator=(const TrackRuntimeLoader&) = delete;
    TrackRuntimeLoader& operator=(TrackRuntimeLoader&&) = delete;

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

#define PH_TRACK pm::TrackRuntimeLoader::Get()->GetApi()

#endif /* PM_TRACK_RUNTIME_LOADER_H */
