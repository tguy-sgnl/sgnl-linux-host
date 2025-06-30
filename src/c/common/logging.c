/*
 * SGNL Minimal Logging Implementation
 *
 * Simple logging for the configuration system
 */

#include "logging.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// Global logger config with defaults
sgnl_logger_config_t sgnl_logger_config = {
    .min_level = SGNL_LOG_INFO,
    .use_syslog = false,
    .structured_format = false,
    .include_timestamp = false,
    .include_pid = false,
    .facility = "local0"
};

void sgnl_log_init(const sgnl_logger_config_t *config) {
    if (config) {
        sgnl_logger_config = *config;
    }
}

void sgnl_log_cleanup(void) {
    // Nothing to cleanup in this minimal implementation
}

void sgnl_log_with_context(sgnl_log_level_t level, 
                          const sgnl_log_context_t *context,
                          const char *format, ...) {
    if (level > sgnl_logger_config.min_level) {
        return;  // Log level too low
    }
    
    va_list args;
    va_start(args, format);
    
    // Simple output format: [COMPONENT] MESSAGE
    if (context && context->component) {
        printf("[%s] ", context->component);
    } else {
        printf("[SGNL] ");
    }
    
    // Use default message if format is NULL or empty
    if (!format || strlen(format) == 0) {
        printf("Log message");
    } else {
        vprintf(format, args);
    }
    printf("\n");
    
    va_end(args);
}

void sgnl_log_with_context_v(sgnl_log_level_t level,
                            const sgnl_log_context_t *context,
                            const char *format, va_list args) {
    if (level > sgnl_logger_config.min_level) {
        return;
    }
    
    if (context && context->component) {
        printf("[%s] ", context->component);
    } else {
        printf("[SGNL] ");
    }
    
    // Use default message if format is NULL or empty
    if (!format || strlen(format) == 0) {
        printf("Log message");
    } else {
        vprintf(format, args);
    }
    printf("\n");
}

const char* sgnl_log_level_to_string(sgnl_log_level_t level) {
    switch (level) {
        case SGNL_LOG_DEBUG: return "DEBUG";
        case SGNL_LOG_INFO: return "INFO";
        case SGNL_LOG_NOTICE: return "NOTICE";
        case SGNL_LOG_WARNING: return "WARNING";
        case SGNL_LOG_ERROR: return "ERROR";
        case SGNL_LOG_CRITICAL: return "CRITICAL";
        case SGNL_LOG_ALERT: return "ALERT";
        case SGNL_LOG_EMERGENCY: return "EMERGENCY";
        default: return "UNKNOWN";
    }
}

sgnl_log_level_t sgnl_log_level_from_string(const char *level_str) {
    if (!level_str) return SGNL_LOG_INFO;
    
    if (strcmp(level_str, "debug") == 0) return SGNL_LOG_DEBUG;
    if (strcmp(level_str, "info") == 0) return SGNL_LOG_INFO;
    if (strcmp(level_str, "notice") == 0) return SGNL_LOG_NOTICE;
    if (strcmp(level_str, "warning") == 0 || strcmp(level_str, "warn") == 0) return SGNL_LOG_WARNING;
    if (strcmp(level_str, "error") == 0) return SGNL_LOG_ERROR;
    if (strcmp(level_str, "critical") == 0) return SGNL_LOG_CRITICAL;
    if (strcmp(level_str, "alert") == 0) return SGNL_LOG_ALERT;
    if (strcmp(level_str, "emergency") == 0) return SGNL_LOG_EMERGENCY;
    
    return SGNL_LOG_INFO;  // Default
}

bool sgnl_log_level_enabled(sgnl_log_level_t level) {
    return level <= sgnl_logger_config.min_level;
}

// Request tracking stubs (not implemented)
sgnl_request_tracker_t* sgnl_request_start(const char *principal_id,
                                          const char *asset_id,
                                          const char *action) {
    (void)principal_id;
    (void)asset_id;
    (void)action;
    return NULL;
}

void sgnl_request_end(sgnl_request_tracker_t *tracker, const char *result) {
    (void)tracker;
    (void)result;
} 