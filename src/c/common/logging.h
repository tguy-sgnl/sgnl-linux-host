/*
 * SGNL Unified Logging System
 *
 * Structured logging with levels, contexts, and consistent formatting
 */

#ifndef SGNL_LOGGING_H
#define SGNL_LOGGING_H

#include <stdbool.h>
#include <stdarg.h>
#include <time.h>

// Log levels (matching syslog levels)
typedef enum {
    SGNL_LOG_DEBUG = 7,     // Debug messages
    SGNL_LOG_INFO = 6,      // Informational messages
    SGNL_LOG_NOTICE = 5,    // Normal but significant conditions
    SGNL_LOG_WARNING = 4,   // Warning conditions
    SGNL_LOG_ERROR = 3,     // Error conditions
    SGNL_LOG_CRITICAL = 2,  // Critical conditions
    SGNL_LOG_ALERT = 1,     // Action must be taken immediately
    SGNL_LOG_EMERGENCY = 0  // System is unusable
} sgnl_log_level_t;

// Log context for structured logging
typedef struct {
    const char *component;      // "libsgnl", "pam", "sudo"
    const char *function;       // Current function
    const char *request_id;     // Request tracking ID
    const char *principal_id;   // User/principal ID
    const char *asset_id;       // Asset being accessed
    const char *action;         // Action being performed
} sgnl_log_context_t;

// Logger configuration
typedef struct {
    sgnl_log_level_t min_level;     // Minimum level to log
    bool use_syslog;                // Use syslog vs stderr
    bool structured_format;         // JSON vs plain text
    bool include_timestamp;         // Include timestamp in output
    bool include_pid;               // Include process ID
    const char *facility;           // Syslog facility name
} sgnl_logger_config_t;

// Global logger instance
extern sgnl_logger_config_t sgnl_logger_config;

// Core logging functions
void sgnl_log_init(const sgnl_logger_config_t *config);
void sgnl_log_cleanup(void);

void sgnl_log_with_context(sgnl_log_level_t level, 
                          const sgnl_log_context_t *context,
                          const char *format, ...);

void sgnl_log_with_context_v(sgnl_log_level_t level,
                            const sgnl_log_context_t *context,
                            const char *format, va_list args);

// Convenience macros
#define SGNL_LOG_CONTEXT(comp) \
    ((sgnl_log_context_t){.component = (comp), .function = __func__})

#define SGNL_LOG_DEBUG(ctx, fmt, ...) \
    sgnl_log_with_context(SGNL_LOG_DEBUG, (ctx), fmt, ##__VA_ARGS__)

#define SGNL_LOG_INFO(ctx, fmt, ...) \
    sgnl_log_with_context(SGNL_LOG_INFO, (ctx), fmt, ##__VA_ARGS__)

#define SGNL_LOG_WARNING(ctx, fmt, ...) \
    sgnl_log_with_context(SGNL_LOG_WARNING, (ctx), fmt, ##__VA_ARGS__)

#define SGNL_LOG_ERROR(ctx, fmt, ...) \
    sgnl_log_with_context(SGNL_LOG_ERROR, (ctx), fmt, ##__VA_ARGS__)

// Security-aware logging macros (redact sensitive data)
#define SGNL_LOG_SECURE_DEBUG(ctx, fmt, ...) \
    do { if (sgnl_logger_config.min_level >= SGNL_LOG_DEBUG) \
        sgnl_log_with_context(SGNL_LOG_DEBUG, (ctx), fmt, ##__VA_ARGS__); } while(0)

// Request tracking helpers
typedef struct {
    char request_id[64];
    char principal_id[256];
    char asset_id[256];
    char action[64];
    time_t start_time;
} sgnl_request_tracker_t;

sgnl_request_tracker_t* sgnl_request_start(const char *principal_id,
                                          const char *asset_id,
                                          const char *action);
void sgnl_request_end(sgnl_request_tracker_t *tracker, 
                     const char *result);

// Level utilities
const char* sgnl_log_level_to_string(sgnl_log_level_t level);
sgnl_log_level_t sgnl_log_level_from_string(const char *level_str);
bool sgnl_log_level_enabled(sgnl_log_level_t level);

#endif /* SGNL_LOGGING_H */ 