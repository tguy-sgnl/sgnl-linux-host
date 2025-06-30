/*
 * libsgnl - SGNL Access Control Client Library
 *
 * Unified library that provides a simple interface for SGNL
 * access control operations.
 *
 * This library is designed to be consumed by:
 * - PAM modules
 * - Sudo plugins  
 * - Custom applications
 * - Other access control systems
 */

#ifndef LIBSGNL_H
#define LIBSGNL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Core Types and Constants
// ============================================================================

// Library version
#define LIBSGNL_VERSION_MAJOR 1
#define LIBSGNL_VERSION_MINOR 0
#define LIBSGNL_VERSION_PATCH 0

// Result codes
typedef enum {
    SGNL_OK = 0,                    // Success
    SGNL_DENIED = 1,                // Access denied
    SGNL_ALLOWED = 2,               // Access allowed
    SGNL_ERROR = 3,                 // General error
    SGNL_CONFIG_ERROR = 4,          // Configuration error
    SGNL_NETWORK_ERROR = 5,         // Network/HTTP error
    SGNL_AUTH_ERROR = 6,            // Authentication error
    SGNL_TIMEOUT_ERROR = 7,         // Timeout error
    SGNL_INVALID_REQUEST = 8,       // Invalid request
    SGNL_MEMORY_ERROR = 9           // Memory allocation error
} sgnl_result_t;

// Forward declarations (opaque types)
typedef struct sgnl_client sgnl_client_t;
typedef struct sgnl_access_result sgnl_access_result_t;
typedef struct sgnl_search_result sgnl_search_result_t;

// Client configuration options  
typedef struct {
    const char *config_path;        // Path to config file (NULL = auto-detect)
    int timeout_seconds;            // Request timeout (0 = default: 30s)
    int retry_count;                // Number of retries (0 = default: 2)
    int retry_delay_ms;             // Delay between retries (0 = default: 1000ms)
    bool enable_debug_logging;      // Enable debug output
    bool validate_ssl;              // Validate SSL certificates
    const char *user_agent;         // Custom user agent (NULL = default)
} sgnl_client_config_t;

// Access evaluation result (detailed)
struct sgnl_access_result {
    sgnl_result_t result;           // Overall result
    char decision[16];              // "Allow", "Deny", etc.
    char reason[512];               // Reason for decision
    char asset_id[256];             // Asset that was evaluated
    char action[64];                // Action that was evaluated  
    char principal_id[256];         // Principal (user) that was evaluated
    int64_t timestamp;              // Evaluation timestamp
    char request_id[64];            // Request ID for tracking
    char error_message[512];        // Error message if result != SGNL_OK
    int error_code;                 // Detailed error code
    void *attributes;               // Additional attributes (internal)
};

// Asset search result
struct sgnl_search_result {
    sgnl_result_t result;           // Overall result
    char **asset_ids;               // Array of asset IDs (null-terminated)
    int asset_count;                // Number of assets
    char *next_page_token;          // Token for pagination (if any)
    bool has_more_pages;            // Whether more pages are available
    char principal_id[256];         // Principal that was searched for
    char action[64];                // Action that was searched for
    char request_id[64];            // Request ID for tracking
    char error_message[512];        // Error message if result != SGNL_OK
    int error_code;                 // Detailed error code
};

// ============================================================================
// Client Management
// ============================================================================

/**
 * Create SGNL client with configuration from file
 * 
 * @param config Configuration options (NULL = use defaults)
 * @return Client instance or NULL on error
 */
sgnl_client_t* sgnl_client_create(const sgnl_client_config_t *config);

/**
 * Destroy client and cleanup resources
 * 
 * @param client Client to destroy
 */
void sgnl_client_destroy(sgnl_client_t *client);

/**
 * Validate client configuration
 * 
 * @param client Client to validate
 * @return SGNL_OK if valid, error code otherwise
 */
sgnl_result_t sgnl_client_validate(sgnl_client_t *client);

/**
 * Get last error message from client
 * 
 * @param client Client instance  
 * @return Error message or NULL if no error
 */
const char* sgnl_client_get_last_error(sgnl_client_t *client);

/**
 * Check if debug logging is enabled
 * 
 * @param client Client instance
 * @return true if debug logging is enabled
 */
bool sgnl_client_is_debug_enabled(sgnl_client_t *client);



// ============================================================================
// Access Evaluation
// ============================================================================

/**
 * Simple access check (for PAM/sudo modules)
 * 
 * @param client SGNL client
 * @param principal_id User/principal ID
 * @param asset_id Asset ID (command, service, etc.)
 * @param action Action to perform (NULL = "execute")
 * @return SGNL_ALLOWED, SGNL_DENIED, or error code
 */
sgnl_result_t sgnl_check_access(sgnl_client_t *client,
                               const char *principal_id,
                               const char *asset_id,
                               const char *action);

/**
 * Detailed access evaluation with full result
 * 
 * @param client SGNL client
 * @param principal_id User/principal ID
 * @param asset_id Asset ID
 * @param action Action to perform (NULL = "execute")
 * @return Access result (must be freed with sgnl_access_result_free)
 */
sgnl_access_result_t* sgnl_evaluate_access(sgnl_client_t *client,
                                           const char *principal_id,
                                           const char *asset_id, 
                                           const char *action);

/**
 * Batch access evaluation (multiple queries)
 * 
 * @param client SGNL client
 * @param principal_id User/principal ID
 * @param asset_ids Array of asset IDs
 * @param actions Array of actions (or NULL for all "execute")
 * @param query_count Number of queries
 * @return Array of results (must be freed with sgnl_access_result_array_free)
 */
sgnl_access_result_t** sgnl_evaluate_access_batch(sgnl_client_t *client,
                                                  const char *principal_id,
                                                  const char **asset_ids,
                                                  const char **actions,
                                                  int query_count);

// ============================================================================
// Asset Search
// ============================================================================

/**
 * Search for assets the principal can access
 * 
 * @param client SGNL client
 * @param principal_id User/principal ID
 * @param action Action to search for (NULL = "execute")
 * @param asset_count Output: number of assets found
 * @return Array of asset IDs (must be freed with sgnl_asset_ids_free)
 */
char** sgnl_search_assets(sgnl_client_t *client,
                          const char *principal_id,
                          const char *action,
                          int *asset_count);

/**
 * Detailed asset search with pagination
 * 
 * @param client SGNL client
 * @param principal_id User/principal ID
 * @param action Action to search for (NULL = "execute")
 * @param page_token Pagination token (NULL = first page)
 * @param page_size Page size (0 = default: 50)
 * @return Search result (must be freed with sgnl_search_result_free)
 */
sgnl_search_result_t* sgnl_search_assets_detailed(sgnl_client_t *client,
                                                  const char *principal_id,
                                                  const char *action,
                                                  const char *page_token,
                                                  int page_size);

// ============================================================================
// Memory Management
// ============================================================================

/**
 * Free access result
 */
void sgnl_access_result_free(sgnl_access_result_t *result);

/**
 * Free array of access results
 */
void sgnl_access_result_array_free(sgnl_access_result_t **results, int count);

/**
 * Free search result  
 */
void sgnl_search_result_free(sgnl_search_result_t *result);

/**
 * Free asset ID array
 */
void sgnl_asset_ids_free(char **asset_ids, int count);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Convert result code to human-readable string
 */
const char* sgnl_result_to_string(sgnl_result_t result);

/**
 * Generate unique request ID for tracking
 */
char* sgnl_generate_request_id(void);

/**
 * Validate principal ID format
 */
bool sgnl_validate_principal_id(const char *principal_id);

/**
 * Validate asset ID format
 */
bool sgnl_validate_asset_id(const char *asset_id);

/**
 * Get library version string
 */
const char* sgnl_get_version(void);





#ifdef __cplusplus
}
#endif

#endif /* LIBSGNL_H */ 