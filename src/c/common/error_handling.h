/*
 * SGNL Error Handling and Resource Management
 * 
 * Centralized error handling with automatic cleanup patterns
 */

#ifndef SGNL_ERROR_HANDLING_H
#define SGNL_ERROR_HANDLING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <json-c/json.h>

// RAII-style cleanup macros
#define SGNL_CLEANUP_FUNC(func) __attribute__((cleanup(func)))
#define SGNL_AUTO_FREE SGNL_CLEANUP_FUNC(sgnl_auto_free_p)
#define SGNL_AUTO_CLOSE SGNL_CLEANUP_FUNC(sgnl_auto_close_p)
#define SGNL_AUTO_JSON SGNL_CLEANUP_FUNC(sgnl_auto_json_put_p)

// Cleanup helper functions
static inline void sgnl_auto_free_p(void *p) {
    void **pp = (void**)p;
    if (pp && *pp) {
        free(*pp);
        *pp = NULL;
    }
}

static inline void sgnl_auto_close_p(int *fd) {
    if (fd && *fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}

static inline void sgnl_auto_json_put_p(json_object **obj) {
    if (obj && *obj) {
        json_object_put(*obj);
        *obj = NULL;
    }
}

// Error context for better debugging
typedef struct {
    const char *function;
    const char *file;
    int line;
    const char *error_msg;
} sgnl_error_context_t;

#define SGNL_ERROR_CONTEXT(msg) \
    ((sgnl_error_context_t){__func__, __FILE__, __LINE__, (msg)})

// Safe string operations
#define SGNL_SAFE_STRNCPY(dest, src, size) do { \
    if ((src) && (dest)) { \
        strncpy((dest), (src), (size) - 1); \
        (dest)[(size) - 1] = '\0'; \
    } \
} while(0)

// Validation macros
#define SGNL_RETURN_IF_NULL(ptr, retval) \
    if (!(ptr)) { return (retval); }

#define SGNL_GOTO_IF_NULL(ptr, label) \
    if (!(ptr)) { goto label; }

#endif /* SGNL_ERROR_HANDLING_H */ 