/**
 * SGNL PAM Module
 * 
 * PAM module for SGNL access control integration.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

// PAM includes
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#ifndef __APPLE__
#include <security/pam_ext.h>
#endif

// SGNL library and common modules
#include "../../lib/libsgnl.h"
#include "../../common/logging.h"

// Global SGNL client (initialized once, reused)
static sgnl_client_t *sgnl_client = NULL;

// Logging compatibility macro
#ifdef __APPLE__
#define SGNL_LOG(pamh, level, fmt, ...) syslog(level, fmt, ##__VA_ARGS__)
#else  
#define SGNL_LOG(pamh, level, fmt, ...) pam_syslog(pamh, level, fmt, ##__VA_ARGS__)
#endif

/**
 * Initialize SGNL client with PAM-specific configuration
 */
static int init_sgnl_client(pam_handle_t *pamh) {
    if (sgnl_client != NULL) {
        return PAM_SUCCESS; // Already initialized
    }
    
    // Initialize logging context for PAM module
    sgnl_log_context_t log_ctx = SGNL_LOG_CONTEXT("pam");
    
    SGNL_LOG_DEBUG(&log_ctx, "Initializing SGNL client for PAM");
    
    // Create client configuration for PAM module
    sgnl_client_config_t pam_config = {
        .config_path = NULL,  // Use default config path
        .timeout_seconds = 0, // Use defaults
        .retry_count = 2,
        .retry_delay_ms = 1000,
        .enable_debug_logging = false,  // Will be overridden by config file
        .validate_ssl = true,
        .user_agent = "SGNL-PAM/1.0"
    };
    
    // Create client with PAM-specific configuration
    sgnl_client = sgnl_client_create(&pam_config);
    
    if (sgnl_client == NULL) {
        SGNL_LOG_ERROR(&log_ctx, "Failed to initialize SGNL client");
        SGNL_LOG(pamh, LOG_ERR, "SGNL PAM: Failed to initialize client");
        return PAM_AUTHINFO_UNAVAIL;
    }
    
    SGNL_LOG_INFO(&log_ctx, "SGNL client created successfully for PAM module");
    
    // Validate configuration
    if (sgnl_client_validate(sgnl_client) != SGNL_OK) {
        const char *error = sgnl_client_get_last_error(sgnl_client);
        SGNL_LOG_ERROR(&log_ctx, "Configuration validation failed: %s", error);
        SGNL_LOG(pamh, LOG_ERR, "SGNL PAM: Invalid configuration: %s", error);
        sgnl_client_destroy(sgnl_client);
        sgnl_client = NULL;
        return PAM_AUTHINFO_UNAVAIL;
    }
    
    SGNL_LOG_INFO(&log_ctx, "SGNL client initialized successfully");
    SGNL_LOG(pamh, LOG_INFO, "SGNL PAM: Client initialized successfully");
    return PAM_SUCCESS;
}

/**
 * Check SGNL access
 */
static int check_access(pam_handle_t *pamh, const char *username, const char *service) {
    sgnl_log_context_t log_ctx = SGNL_LOG_CONTEXT("pam");
    
    // Ensure client is initialized
    if (init_sgnl_client(pamh) != PAM_SUCCESS) {
        return PAM_AUTHINFO_UNAVAIL;
    }
    
    SGNL_LOG_INFO(&log_ctx, "Checking access for user [%s] service [%s]", username, service);
    SGNL_LOG(pamh, LOG_INFO, "SGNL PAM: Checking access for user [%s] service [%s]", 
             username, service);
    
    // Make SGNL API call
    sgnl_result_t result = sgnl_check_access(sgnl_client, username, service, NULL);
    
    switch (result) {
        case SGNL_ALLOWED:
            SGNL_LOG_INFO(&log_ctx, "Access granted for user [%s]", username);
            SGNL_LOG(pamh, LOG_INFO, "SGNL PAM: Access granted for [%s]", username);
            return PAM_SUCCESS;
            
        case SGNL_DENIED:
            SGNL_LOG_INFO(&log_ctx, "Access denied for user [%s]", username);
            SGNL_LOG(pamh, LOG_INFO, "SGNL PAM: Access denied for [%s]", username);
            return PAM_PERM_DENIED;
            
        default:
            SGNL_LOG_ERROR(&log_ctx, "Access check error for user [%s]: %s", 
                          username, sgnl_result_to_string(result));
            SGNL_LOG(pamh, LOG_ERR, "SGNL PAM: Error for [%s]: %s", 
                     username, sgnl_result_to_string(result));
            return PAM_AUTHINFO_UNAVAIL;
    }
}

// ============================================================================
// PAM Module Interface
// ============================================================================

/**
 * PAM Account Management Hook
 */
PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc, const char **argv) {
    const char *username = NULL;
    const char *service = NULL;
    const char *host = NULL;
    
    // Get PAM data
    (void) pam_get_user(pamh, &username, "Username: ");
    (void) pam_get_item(pamh, PAM_SERVICE, (const void **)&service);
    (void) pam_get_item(pamh, PAM_RHOST, (const void **)&host);
    
    // Validate inputs
    if (username == NULL || service == NULL) {
        SGNL_LOG(pamh, LOG_ERR, "SGNL PAM: Missing username or service");
        return PAM_AUTHINFO_UNAVAIL;
    }
    
    SGNL_LOG(pamh, LOG_INFO, "SGNL PAM: Processing account for [%s] service [%s] host [%s]", 
             username, service, host ? host : "local");
    
    // Check access
    return check_access(pamh, username, service);
}

/**
 * PAM Credential Management (not implemented)
 */
PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv) {
    SGNL_LOG(pamh, LOG_INFO, "SGNL PAM: pam_sm_setcred - returning success");
    return PAM_SUCCESS;
}

/**
 * PAM Authentication (not implemented)
 */
PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv) {
    SGNL_LOG(pamh, LOG_INFO, "SGNL PAM: pam_sm_authenticate - returning success");
    return PAM_SUCCESS;
}

/**
 * Module cleanup
 */
__attribute__((destructor))
static void pam_module_cleanup(void) {
    if (sgnl_client != NULL) {
        sgnl_client_destroy(sgnl_client);
        sgnl_client = NULL;
    }
}