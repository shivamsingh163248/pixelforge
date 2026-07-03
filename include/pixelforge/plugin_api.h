/* pixelforge/plugin_api.h -- stable C ABI between the runtime and plugins.
 *
 * Industry pattern (think VST, OBS, Photoshop plugins): the host loads
 * a shared object at runtime and looks up ONE entry symbol that returns
 * a struct of function pointers describing the plugin's capabilities.
 *
 * Rules to keep the ABI stable across compiler versions:
 *   - Pure C, no C++ types in the interface.
 *   - Fixed-width integer types only.
 *   - Plugins declare an abi_version; the host refuses mismatches.
 *   - Memory crosses the boundary together with its destructor (free_fn),
 *     so the plugin can use whatever allocator it wants internally.
 *
 * A plugin shared object MUST export exactly one symbol:
 *   const PixelforgePlugin* pixelforge_plugin_entry(void);
 */
#ifndef PIXELFORGE_PLUGIN_API_H
#define PIXELFORGE_PLUGIN_API_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#  define PIXELFORGE_PLUGIN_EXPORT __declspec(dllexport)
#else
#  define PIXELFORGE_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

#define PIXELFORGE_PLUGIN_ABI_VERSION  1u
#define PIXELFORGE_PLUGIN_ENTRY_SYMBOL "pixelforge_plugin_entry"

/* Status codes returned by PixelforgeApplyFn. */
#define PIXELFORGE_OK            0
#define PIXELFORGE_ERR_GENERIC  -1
#define PIXELFORGE_ERR_FORMAT   -2
#define PIXELFORGE_ERR_PARAMS   -3
#define PIXELFORGE_ERR_NOMEM    -4

#ifdef __cplusplus
extern "C" {
#endif

/* Read-only view of an input image. The host owns `data`. */
typedef struct PixelforgeImageView {
    uint32_t       width;     /* pixels */
    uint32_t       height;    /* pixels */
    uint32_t       channels;  /* 1 (Gray), 3 (RGB), 4 (RGBA) */
    uint32_t       stride;    /* bytes per row, == width * channels */
    const uint8_t* data;      /* contiguous, row-major, interleaved */
} PixelforgeImageView;

/* Output buffer produced by a plugin. The plugin owns `data` and must
 * provide a destructor (`free_fn`) that the host calls when finished. */
typedef struct PixelforgeImageBuffer {
    uint32_t  width;
    uint32_t  height;
    uint32_t  channels;
    uint32_t  stride;
    uint8_t*  data;
    void    (*free_fn)(uint8_t* data);
} PixelforgeImageBuffer;

/* Plugin entry point function signature.
 *   input       -- read-only image view from the host
 *   output      -- on success, the plugin fills this with a new buffer
 *                  whose lifetime is governed by output->free_fn
 *   params_json -- optional JSON string of parameters, or NULL
 *   user_data   -- value of PixelforgePlugin::user_data, opaque to host
 *
 * Returns PIXELFORGE_OK on success, negative error code otherwise. */
typedef int (*PixelforgeApplyFn)(const PixelforgeImageView* input,
                                 PixelforgeImageBuffer*     output,
                                 const char*                params_json,
                                 void*                      user_data);

/* Self-description returned by the plugin's entry symbol. */
typedef struct PixelforgePlugin {
    uint32_t          abi_version;  /* must equal PIXELFORGE_PLUGIN_ABI_VERSION */
    const char*       name;         /* e.g. "blur" -- used by CLI / API */
    const char*       version;      /* "1.0.0" */
    const char*       description;  /* short human-readable description */
    PixelforgeApplyFn apply;        /* required */
    void*             user_data;    /* opaque, passed back into apply() */
} PixelforgePlugin;

/* Signature of the entry symbol every plugin must export. */
typedef const PixelforgePlugin* (*PixelforgePluginEntryFn)(void);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* PIXELFORGE_PLUGIN_API_H */
