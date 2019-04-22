#pragma once
#ifndef PM_COLOR_RUNTIME_LOADER_H
#define PM_COLOR_RUNTIME_LOADER_H

#include "RuntimeLoader.h"

/* pvcam_helper_color */
#include <pvcam_helper_color.h>

namespace pm {

class ColorRuntimeLoader final : public RuntimeLoader
{
public:
    struct Api
    {
        ph_color_get_lib_version_fn get_lib_version;
        ph_color_context_create_fn context_create;
        ph_color_context_release_fn context_release;
        ph_color_context_apply_changes_fn context_apply_changes;
        ph_color_debayer_fn debayer;
        ph_color_white_balance_fn white_balance;
        ph_color_auto_exposure_fn auto_exposure;
        ph_color_auto_exposure_abort_fn auto_exposure_abort;
        ph_color_auto_white_balance_fn auto_white_balance;
        ph_color_auto_exposure_and_white_balance_fn auto_exposure_and_white_balance;
        ph_color_convert_format_fn convert_format;
    };

public:
    static ColorRuntimeLoader* m_instance;

    /// @brief Creates singleton instance
    static ColorRuntimeLoader* Get();
    static void Release();

private:
    ColorRuntimeLoader();
    virtual ~ColorRuntimeLoader();

    ColorRuntimeLoader(const ColorRuntimeLoader&) = delete;
    ColorRuntimeLoader(ColorRuntimeLoader&&) = delete;
    ColorRuntimeLoader& operator=(const ColorRuntimeLoader&) = delete;
    ColorRuntimeLoader& operator=(ColorRuntimeLoader&&) = delete;

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

#define PH_COLOR pm::ColorRuntimeLoader::Get()->GetApi()

#endif /* PM_COLOR_RUNTIME_LOADER_H */
