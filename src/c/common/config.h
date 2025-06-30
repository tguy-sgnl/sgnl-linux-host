/*
 * SGNL Configuration Management
 *
 * Unified configuration system for all SGNL modules.
 * Supports JSON configuration files, environment variable overrides, and validation.
 */

#ifndef SGNL_CONFIG_H
#define SGNL_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

// Configuration structure - unified for all modules
typedef struct {
    // Core SGNL API settings
    char tenant[128];
    char api_url[256];
    char api_token[512];
    
    // HTTP client settings
    struct {
        int timeout_seconds;
        int connect_timeout_seconds;
        bool ssl_verify_peer;
        bool ssl_verify_host;
        char user_agent[128];
    } http;
    
    // Global logging settings
    struct {
        bool debug_mode;
        char log_level[16];          // "debug", "info", "warn", "error"
    } logging;
    
    // Sudo plugin specific settings
    struct {
        bool access_msg;             // Show user-visible message when access granted
        char command_attribute[64];  // SGNL response attribute to use as command name in sudo -l
    } sudo;
    
    // Internal state
    bool initialized;
    char last_error[256];
} sgnl_config_t;

// Configuration loading options
typedef struct {
    const char *config_path;         // Path to config file (NULL = use default)
    bool strict_validation;          // Require all fields to be present
    const char *module_name;         // For logging/debugging ("pam", "sudo", etc.)
} sgnl_config_options_t;

// Environment variable for config path only (for testing)
#define SGNL_ENV_CONFIG_PATH    "SGNL_CONFIG_PATH"

// Default configuration path
#define SGNL_DEFAULT_CONFIG     "/etc/sgnl/config.json"

// Configuration validation result
typedef enum {
    SGNL_CONFIG_OK = 0,
    SGNL_CONFIG_FILE_NOT_FOUND = 1,
    SGNL_CONFIG_INVALID_JSON = 2,
    SGNL_CONFIG_MISSING_REQUIRED = 3,
    SGNL_CONFIG_INVALID_VALUE = 4,
    SGNL_CONFIG_MEMORY_ERROR = 5
} sgnl_config_result_t;

// Function declarations

// Core configuration management
sgnl_config_t* sgnl_config_create(void);
void sgnl_config_destroy(sgnl_config_t *config);

sgnl_config_result_t sgnl_config_load(sgnl_config_t *config, const sgnl_config_options_t *options);
sgnl_config_result_t sgnl_config_validate(const sgnl_config_t *config);
void sgnl_config_set_defaults(sgnl_config_t *config, const char *module_name);

// Accessors
const char* sgnl_config_get_api_url(const sgnl_config_t *config);
const char* sgnl_config_get_api_token(const sgnl_config_t *config);
const char* sgnl_config_get_tenant(const sgnl_config_t *config);
const char* sgnl_config_get_sudo_command_attribute(const sgnl_config_t *config);
bool sgnl_config_get_sudo_access_msg(const sgnl_config_t *config);
const char* sgnl_config_get_user_agent(const sgnl_config_t *config);
int sgnl_config_get_timeout(const sgnl_config_t *config);
int sgnl_config_get_connect_timeout(const sgnl_config_t *config);

// Convenience functions for common operations
bool sgnl_config_is_valid(const sgnl_config_t *config);
bool sgnl_config_is_debug_enabled(const sgnl_config_t *config);

const char* sgnl_config_get_last_error(const sgnl_config_t *config);

// Result code helpers
const char* sgnl_config_result_to_string(sgnl_config_result_t result);

// Default configuration options
extern const sgnl_config_options_t SGNL_CONFIG_DEFAULT_OPTIONS;



#endif /* SGNL_CONFIG_H */ 