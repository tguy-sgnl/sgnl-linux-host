/*
 * SGNL Configuration Management Implementation
 *
 * Unified configuration system for SGNL modules
 * from PAM module, sudo plugin, and libsgnl (API client).
 */

#include "config.h"
#include "error_handling.h"
#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <json-c/json.h>

// Default configuration options
const sgnl_config_options_t SGNL_CONFIG_DEFAULT_OPTIONS = {
    .config_path = NULL,         // Will use SGNL_DEFAULT_CONFIG
    .strict_validation = true,
    .module_name = "default"
};

// Core configuration management functions
sgnl_config_t* sgnl_config_create(void) {
    sgnl_config_t *config = calloc(1, sizeof(sgnl_config_t));
    if (config) {
        // Initialize with safe defaults
        config->initialized = false;
        config->last_error[0] = '\0';
    }
    return config;
}

void sgnl_config_destroy(sgnl_config_t *config) {
    if (config) {
        // Clear sensitive data
        memset(config->api_token, 0, sizeof(config->api_token));
        free(config);
    }
}

void sgnl_config_set_defaults(sgnl_config_t *config, const char *module_name) {
    if (!config) return;
    (void)module_name;  // Unused for now, but reserved for future module-specific defaults
    
    // Set default HTTP settings
    config->http.timeout_seconds = 10;
    config->http.connect_timeout_seconds = 3;
    config->http.ssl_verify_peer = true;
    config->http.ssl_verify_host = true;
    
    // Set user agent
    strncpy(config->http.user_agent, "SGNL-Client/1.0", sizeof(config->http.user_agent) - 1);
    config->http.user_agent[sizeof(config->http.user_agent) - 1] = '\0';
    
    // Set default logging
    config->logging.debug_mode = false;
    strncpy(config->logging.log_level, "info", sizeof(config->logging.log_level) - 1);
    config->logging.log_level[sizeof(config->logging.log_level) - 1] = '\0';
    
    // Set default sudo settings
    config->sudo.access_msg = true;
    strcpy(config->sudo.command_attribute, "id");  // Default to using asset ID
    config->sudo.batch_evaluation = false;  // Default to single query evaluation
}

// Forward declaration
static void apply_config_values(sgnl_config_t *config, json_object *root);

// Helper to load and parse a single JSON config file
static sgnl_config_result_t load_config_file(const char *config_path, json_object **root_out, char *error_buffer, size_t error_size) {
    SGNL_RETURN_IF_NULL(config_path, SGNL_CONFIG_MEMORY_ERROR);
    SGNL_RETURN_IF_NULL(root_out, SGNL_CONFIG_MEMORY_ERROR);
    
    *root_out = NULL;
    
    FILE *fp = fopen(config_path, "r");
    if (!fp) {
        if (error_buffer) {
            snprintf(error_buffer, error_size, "Could not open config file: %s", config_path);
        }
        return SGNL_CONFIG_FILE_NOT_FOUND;
    }
    
    // Read file content with automatic cleanup
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    rewind(fp);
    
    SGNL_AUTO_FREE char *buffer = malloc(file_size + 1);
    if (!buffer) {
        fclose(fp);
        if (error_buffer) {
            snprintf(error_buffer, error_size, "Memory allocation failed for file: %s", config_path);
        }
        return SGNL_CONFIG_MEMORY_ERROR;
    }
    
    size_t bytes_read = fread(buffer, 1, file_size, fp);
    buffer[bytes_read] = '\0';
    fclose(fp);
    
    // Parse JSON with automatic cleanup
    json_object *root = json_tokener_parse(buffer);
    if (!root) {
        if (error_buffer) {
            snprintf(error_buffer, error_size, "Invalid JSON in config file: %s", config_path);
        }
        return SGNL_CONFIG_INVALID_JSON;
    }
    
    *root_out = root;  // Transfer ownership to caller
    return SGNL_CONFIG_OK;
}

sgnl_config_result_t sgnl_config_load(sgnl_config_t *config, const sgnl_config_options_t *options) {
    SGNL_RETURN_IF_NULL(config, SGNL_CONFIG_MEMORY_ERROR);
    
    // Initialize logging context for this operation
    sgnl_log_context_t log_ctx = SGNL_LOG_CONTEXT("config");
    
    // Use default options if none provided
    const sgnl_config_options_t *opts = options ? options : &SGNL_CONFIG_DEFAULT_OPTIONS;
    
    SGNL_LOG_DEBUG(&log_ctx, "Loading configuration for module: %s", opts->module_name ? opts->module_name : "default");
    
    // Set defaults first
    sgnl_config_set_defaults(config, opts->module_name);
    
    // Determine config file path
    const char *config_path = opts->config_path;
    if (!config_path) {
        // Check environment variable first (only override allowed)
        config_path = getenv(SGNL_ENV_CONFIG_PATH);
        if (!config_path) {
            config_path = SGNL_DEFAULT_CONFIG;
        }
    }
    
    SGNL_LOG_DEBUG(&log_ctx, "Loading configuration from: %s", config_path);
    
    // Load configuration file
    SGNL_AUTO_JSON json_object *config_json = NULL;
    sgnl_config_result_t result = load_config_file(config_path, &config_json, config->last_error, sizeof(config->last_error));
    
    if (result != SGNL_CONFIG_OK) {
        SGNL_LOG_ERROR(&log_ctx, "Failed to load configuration file: %s", config->last_error);
        return result;
    }
    
    // Apply configuration values
    apply_config_values(config, config_json);
    
    // Validate final configuration
    sgnl_config_result_t validation_result = sgnl_config_validate(config);
    if (validation_result != SGNL_CONFIG_OK && opts->strict_validation) {
        SGNL_LOG_ERROR(&log_ctx, "Configuration validation failed: %s", 
                      sgnl_config_result_to_string(validation_result));
        return validation_result;
    }
    
    config->initialized = true;
    SGNL_LOG_DEBUG(&log_ctx, "Configuration loaded successfully for module: %s", opts->module_name ? opts->module_name : "default");
    return SGNL_CONFIG_OK;
}

// Helper function to apply JSON configuration values to config struct
static void apply_config_values(sgnl_config_t *config, json_object *root) {
    if (!config || !root) return;
    
    json_object *value;
    
    // API URL (required)
    if (json_object_object_get_ex(root, "api_url", &value) && json_object_is_type(value, json_type_string)) {
        SGNL_SAFE_STRNCPY(config->api_url, json_object_get_string(value), sizeof(config->api_url));
    }
    
    // API Token (required) - also try legacy name
    if (json_object_object_get_ex(root, "api_token", &value) && json_object_is_type(value, json_type_string)) {
        SGNL_SAFE_STRNCPY(config->api_token, json_object_get_string(value), sizeof(config->api_token));
    } else if (json_object_object_get_ex(root, "protected_system_token", &value) && json_object_is_type(value, json_type_string)) {
        SGNL_SAFE_STRNCPY(config->api_token, json_object_get_string(value), sizeof(config->api_token));
    }
    
    // Tenant (optional for some modules)
    if (json_object_object_get_ex(root, "tenant", &value) && json_object_is_type(value, json_type_string)) {
        SGNL_SAFE_STRNCPY(config->tenant, json_object_get_string(value), sizeof(config->tenant));
    }
    
    // Sudo plugin settings
    json_object *sudo_obj;
    if (json_object_object_get_ex(root, "sudo", &sudo_obj)) {
        // Access message setting
        if (json_object_object_get_ex(sudo_obj, "access_msg", &value)) {
            if (json_object_is_type(value, json_type_boolean)) {
                config->sudo.access_msg = json_object_get_boolean(value);
            } else if (json_object_is_type(value, json_type_string)) {
                const char *access_str = json_object_get_string(value);
                config->sudo.access_msg = (strcmp(access_str, "true") == 0 || strcmp(access_str, "1") == 0);
            }
        }
        
        // Command attribute setting
        if (json_object_object_get_ex(sudo_obj, "command_attribute", &value) && json_object_is_type(value, json_type_string)) {
            SGNL_SAFE_STRNCPY(config->sudo.command_attribute, json_object_get_string(value), sizeof(config->sudo.command_attribute));
        }
        
        // Batch evaluation setting
        if (json_object_object_get_ex(sudo_obj, "batch_evaluation", &value)) {
            if (json_object_is_type(value, json_type_boolean)) {
                config->sudo.batch_evaluation = json_object_get_boolean(value);
            } else if (json_object_is_type(value, json_type_string)) {
                const char *batch_str = json_object_get_string(value);
                config->sudo.batch_evaluation = (strcmp(batch_str, "true") == 0 || strcmp(batch_str, "1") == 0);
            }
        }
    }
    
    // HTTP settings (optional)
    json_object *http_obj;
    if (json_object_object_get_ex(root, "http", &http_obj)) {
        if (json_object_object_get_ex(http_obj, "timeout", &value) && json_object_is_type(value, json_type_int)) {
            config->http.timeout_seconds = json_object_get_int(value);
        }
        if (json_object_object_get_ex(http_obj, "connect_timeout", &value) && json_object_is_type(value, json_type_int)) {
            config->http.connect_timeout_seconds = json_object_get_int(value);
        }
        if (json_object_object_get_ex(http_obj, "ssl_verify_peer", &value) && json_object_is_type(value, json_type_boolean)) {
            config->http.ssl_verify_peer = json_object_get_boolean(value);
        }
        if (json_object_object_get_ex(http_obj, "ssl_verify_host", &value) && json_object_is_type(value, json_type_boolean)) {
            config->http.ssl_verify_host = json_object_get_boolean(value);
        }
        if (json_object_object_get_ex(http_obj, "user_agent", &value) && json_object_is_type(value, json_type_string)) {
            SGNL_SAFE_STRNCPY(config->http.user_agent, json_object_get_string(value), sizeof(config->http.user_agent));
        }
    }
    
    // Debug logging
    if (json_object_object_get_ex(root, "debug", &value)) {
        if (json_object_is_type(value, json_type_boolean)) {
            config->logging.debug_mode = json_object_get_boolean(value);
        } else if (json_object_is_type(value, json_type_string)) {
            const char *debug_str = json_object_get_string(value);
            config->logging.debug_mode = (strcmp(debug_str, "true") == 0 || strcmp(debug_str, "1") == 0);
        }
    }
    
    // Timeout settings
    if (json_object_object_get_ex(root, "timeout_seconds", &value) && json_object_is_type(value, json_type_int)) {
        config->http.timeout_seconds = json_object_get_int(value);
    }
    
    // Log level
    if (json_object_object_get_ex(root, "log_level", &value) && json_object_is_type(value, json_type_string)) {
        SGNL_SAFE_STRNCPY(config->logging.log_level, json_object_get_string(value), sizeof(config->logging.log_level));
    }

}

sgnl_config_result_t sgnl_config_validate(const sgnl_config_t *config) {
    if (!config) {
        return SGNL_CONFIG_MEMORY_ERROR;
    }
    
    // Check required fields
    if (strlen(config->api_url) == 0) {
        return SGNL_CONFIG_MISSING_REQUIRED;
    }
    
    if (strlen(config->api_token) == 0) {
        return SGNL_CONFIG_MISSING_REQUIRED;
    }
    
    // Validate timeout values
    if (config->http.timeout_seconds < 1 || config->http.timeout_seconds > 300) {
        return SGNL_CONFIG_INVALID_VALUE;
    }
    
    if (config->http.connect_timeout_seconds < 1 || config->http.connect_timeout_seconds > 60) {
        return SGNL_CONFIG_INVALID_VALUE;
    }
    
    return SGNL_CONFIG_OK;
}

// Accessor functions
const char* sgnl_config_get_api_url(const sgnl_config_t *config) {
    return config ? config->api_url : NULL;
}

const char* sgnl_config_get_api_token(const sgnl_config_t *config) {
    return config ? config->api_token : NULL;
}

const char* sgnl_config_get_tenant(const sgnl_config_t *config) {
    return config ? config->tenant : NULL;
}

const char* sgnl_config_get_sudo_command_attribute(const sgnl_config_t *config) {
    return config ? config->sudo.command_attribute : NULL;
}

bool sgnl_config_get_sudo_access_msg(const sgnl_config_t *config) {
    return config ? config->sudo.access_msg : false;
}

bool sgnl_config_get_sudo_batch_evaluation(const sgnl_config_t *config) {
    return config ? config->sudo.batch_evaluation : false;
}

const char* sgnl_config_get_user_agent(const sgnl_config_t *config) {
    return config ? config->http.user_agent : NULL;
}

int sgnl_config_get_timeout(const sgnl_config_t *config) {
    return config ? config->http.timeout_seconds : 30;
}

int sgnl_config_get_connect_timeout(const sgnl_config_t *config) {
    return config ? config->http.connect_timeout_seconds : 10;
}

// Convenience functions
bool sgnl_config_is_valid(const sgnl_config_t *config) {
    return config && config->initialized && (sgnl_config_validate(config) == SGNL_CONFIG_OK);
}

bool sgnl_config_is_debug_enabled(const sgnl_config_t *config) {
    return config && config->logging.debug_mode;
}

const char* sgnl_config_get_last_error(const sgnl_config_t *config) {
    return config ? config->last_error : NULL;
}

const char* sgnl_config_result_to_string(sgnl_config_result_t result) {
    switch (result) {
        case SGNL_CONFIG_OK: return "Success";
        case SGNL_CONFIG_FILE_NOT_FOUND: return "Configuration file not found";
        case SGNL_CONFIG_INVALID_JSON: return "Invalid JSON in configuration file";
        case SGNL_CONFIG_MISSING_REQUIRED: return "Missing required configuration field";
        case SGNL_CONFIG_INVALID_VALUE: return "Invalid configuration value";
        case SGNL_CONFIG_MEMORY_ERROR: return "Memory allocation error";
        default: return "Unknown error";
    }
}

 