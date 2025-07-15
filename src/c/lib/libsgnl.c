/*
 * libsgnl - SGNL Access Control Client Library Implementation
 *
 * Unified library providing client management, access evaluation, 
 * asset search, and configuration handling.
 */

#include "libsgnl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <dirent.h>
#include <curl/curl.h>
#include <json-c/json.h>

// Add common config and logging systems
#include "../common/config.h"
#include "../common/logging.h"

// ============================================================================
// Internal Data Structures
// ============================================================================

struct sgnl_client {
    // Configuration
    char api_url[256];
    char api_token[512];
    char tenant[128];
    
    // HTTP settings
    int timeout_seconds;
    int connect_timeout_seconds;
    bool ssl_verify_peer;
    bool ssl_verify_host;
    char user_agent[128];
    
    // Logging settings  
    bool debug_enabled;
    
    // Runtime state
    bool initialized;
    char last_error[512];
    char last_request_id[64];
};

// HTTP response structure for internal use
typedef struct {
    char *data;
    size_t size;
    long status_code;
    char *error_message;
} http_response_t;



// ============================================================================
// Internal Helper Functions
// ============================================================================

static void sgnl_log_debug(sgnl_client_t *client, const char *format, ...) {
    if (!client || !client->debug_enabled) {
        return;
    }
    
    sgnl_log_context_t log_ctx = SGNL_LOG_CONTEXT("libsgnl");
    va_list args;
    va_start(args, format);
    sgnl_log_with_context_v(SGNL_LOG_DEBUG, &log_ctx, format, args);
    va_end(args);
}

static void sgnl_log_error(sgnl_client_t *client, const char *format, ...) {
    sgnl_log_context_t log_ctx = SGNL_LOG_CONTEXT("libsgnl");
    va_list args;
    va_start(args, format);
    
    if (client) {
        va_list args_copy;
        va_copy(args_copy, args);
        vsnprintf(client->last_error, sizeof(client->last_error), format, args_copy);
        va_end(args_copy);
    }
    
    sgnl_log_with_context_v(SGNL_LOG_ERROR, &log_ctx, format, args);
    va_end(args);
}

// HTTP response callback
static size_t http_write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    http_response_t *response = (http_response_t *)userp;
    
    char *temp = realloc(response->data, response->size + realsize + 1);
    if (temp == NULL) {
        free(response->data);
        response->data = NULL;
        return 0;
    }
    
    response->data = temp;
    memcpy(&(response->data[response->size]), contents, realsize);
    response->size += realsize;
    response->data[response->size] = '\0';
    
    return realsize;
}

// Free HTTP response
static void http_response_free(http_response_t *response) {
    if (response) {
        if (response->data) {
            free(response->data);
        }
        if (response->error_message) {
            free(response->error_message);
        }
        free(response);
    }
}

// Generate request ID
static char* generate_request_id_internal(void) {
    static char request_id[64];
    time_t now = time(NULL);
    unsigned int pid = getpid();
    unsigned int random_val = (unsigned int)(now ^ pid);
    
    snprintf(request_id, sizeof(request_id), 
             "sgnl-%08x-%04x-%04x",
             (unsigned int)now,
             (unsigned int)(pid & 0xFFFF),
             (unsigned int)(random_val & 0xFFFF));
    
    return request_id;
}

// Get system device ID with fallback chain: machine-id -> hostname -> MAC address
static char* get_device_id(void) {
    static char device_id[256];
    
    // First try: /etc/machine-id
    FILE *machine_id_file = fopen("/etc/machine-id", "r");
    if (machine_id_file) {
        if (fgets(device_id, sizeof(device_id), machine_id_file)) {
            // Remove newline if present
            size_t len = strlen(device_id);
            if (len > 0 && device_id[len-1] == '\n') {
                device_id[len-1] = '\0';
            }
            fclose(machine_id_file);
            return device_id;
        }
        fclose(machine_id_file);
    }
    
    // Second try: hostname
    if (gethostname(device_id, sizeof(device_id)) == 0) {
        return device_id;
    }
    
    // Third try: MAC address of first network interface
    FILE *net_dev_file = fopen("/sys/class/net/eth0/address", "r");
    if (!net_dev_file) {
        net_dev_file = fopen("/sys/class/net/wlan0/address", "r");
    }
    if (!net_dev_file) {
        // Try to find any network interface
        DIR *net_dir = opendir("/sys/class/net");
        if (net_dir) {
            struct dirent *entry;
            while ((entry = readdir(net_dir)) != NULL) {
                if (entry->d_name[0] != '.' && strcmp(entry->d_name, "lo") != 0) {
                    char path[256];
                    snprintf(path, sizeof(path), "/sys/class/net/%s/address", entry->d_name);
                    net_dev_file = fopen(path, "r");
                    if (net_dev_file) break;
                }
            }
            closedir(net_dir);
        }
    }
    
    if (net_dev_file) {
        if (fgets(device_id, sizeof(device_id), net_dev_file)) {
            // Remove newline if present
            size_t len = strlen(device_id);
            if (len > 0 && device_id[len-1] == '\n') {
                device_id[len-1] = '\0';
            }
            fclose(net_dev_file);
            return device_id;
        }
        fclose(net_dev_file);
    }
    
    // Final fallback
    strcpy(device_id, "unknown-device");
    return device_id;
}

// Load configuration using common config system
static sgnl_result_t load_config_from_common_system(sgnl_client_t *client, const char *config_path) {
    sgnl_log_context_t log_ctx = SGNL_LOG_CONTEXT("libsgnl");
    
    sgnl_config_options_t options = SGNL_CONFIG_DEFAULT_OPTIONS;
    if (config_path) {
        options.config_path = config_path;
        SGNL_LOG_DEBUG(&log_ctx, "Loading config from specified path: %s", config_path);
    }
    options.module_name = "libsgnl";
    
    sgnl_config_t *common_config = sgnl_config_create();
    if (!common_config) {
        sgnl_log_error(client, "Failed to create config object");
        return SGNL_MEMORY_ERROR;
    }
    
    sgnl_config_result_t result = sgnl_config_load(common_config, &options);
    if (result != SGNL_CONFIG_OK) {
        sgnl_log_error(client, "Failed to load config: %s", 
                      sgnl_config_result_to_string(result));
        sgnl_config_destroy(common_config);
        return SGNL_CONFIG_ERROR;
    }
    
    // Copy configuration values to client
    strncpy(client->api_url, sgnl_config_get_api_url(common_config), sizeof(client->api_url) - 1);
    client->api_url[sizeof(client->api_url) - 1] = '\0';
    strncpy(client->api_token, sgnl_config_get_api_token(common_config), sizeof(client->api_token) - 1);
    client->api_token[sizeof(client->api_token) - 1] = '\0';
    strncpy(client->tenant, sgnl_config_get_tenant(common_config), sizeof(client->tenant) - 1);
    client->tenant[sizeof(client->tenant) - 1] = '\0';
    
    // HTTP settings
    client->timeout_seconds = sgnl_config_get_timeout(common_config);
    client->connect_timeout_seconds = sgnl_config_get_connect_timeout(common_config);
    strncpy(client->user_agent, sgnl_config_get_user_agent(common_config), sizeof(client->user_agent) - 1);
    client->user_agent[sizeof(client->user_agent) - 1] = '\0';
    
    // Logging settings
    client->debug_enabled = sgnl_config_is_debug_enabled(common_config);
    
    sgnl_config_destroy(common_config);
    return SGNL_OK;
}

// Make HTTP request to SGNL API
static http_response_t* make_http_request(sgnl_client_t *client, const char *endpoint, const char *json_body) {
    CURL *curl;
    CURLcode res;
    http_response_t *response = NULL;
    
    curl = curl_easy_init();
    if (!curl) {
        return NULL;
    }
    
    response = calloc(1, sizeof(http_response_t));
    if (!response) {
        curl_easy_cleanup(curl);
        return NULL;
    }
    
    response->data = calloc(1, sizeof(char));
    if (!response->data) {
        free(response);
        curl_easy_cleanup(curl);
        return NULL;
    }
    response->size = 0;
    
    // Build full URL
    char url[512];
    snprintf(url, sizeof(url), "https://%s.%s%s", client->tenant, client->api_url, endpoint);
    
    sgnl_log_debug(client, "Making HTTP request to: %s", url);
    sgnl_log_debug(client, "Request body: %s", json_body ? json_body : "NULL");
    
    // Set curl options
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, client->user_agent);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)client->timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, (long)client->connect_timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, client->ssl_verify_peer ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, client->ssl_verify_host ? 2L : 0L);
    
    // Set POST data
    if (json_body) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
    }
    
    // Set headers
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    // Authorization header
    char auth_header[768];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", client->api_token);
    headers = curl_slist_append(headers, auth_header);
    
    // Request ID header
    char req_id_header[128];
    snprintf(req_id_header, sizeof(req_id_header), "X-Request-Id: %s", client->last_request_id);
    headers = curl_slist_append(headers, req_id_header);
    
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Perform request
    res = curl_easy_perform(curl);
    
    // Get response code
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->status_code);
    
    sgnl_log_debug(client, "HTTP response: status=%ld, curl_result=%d", response->status_code, res);
    if (response->data && response->size > 0) {
        sgnl_log_debug(client, "Response body: %.*s", (int)response->size, response->data);
    }
    
    // Handle curl errors
    if (res != CURLE_OK) {
        size_t error_len = strlen(curl_easy_strerror(res)) + 1;
        response->error_message = malloc(error_len);
        if (response->error_message) {
            strcpy(response->error_message, curl_easy_strerror(res));
        }
        response->status_code = 0;
    }
    
    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    return response;
}

// Parse SGNL API response and extract decision
static sgnl_result_t parse_api_response(const char *json_data, sgnl_access_result_t *result) {
    if (!json_data || !result) {
        return SGNL_ERROR;
    }
    
    json_object *root = json_tokener_parse(json_data);
    if (!root) {
        strncpy(result->error_message, "Failed to parse JSON response", sizeof(result->error_message) - 1);
        result->error_message[sizeof(result->error_message) - 1] = '\0';
        return SGNL_ERROR;
    }
    
    // Check for API errors
    json_object *error_obj;
    if (json_object_object_get_ex(root, "error", &error_obj)) {
        json_object *error_msg;
        if (json_object_object_get_ex(error_obj, "message", &error_msg)) {
            strncpy(result->error_message, json_object_get_string(error_msg), sizeof(result->error_message) - 1);
            result->error_message[sizeof(result->error_message) - 1] = '\0';
        }
        json_object_put(root);
        return SGNL_ERROR;
    }
    
    // Get decisions array
    json_object *decisions;
    if (!json_object_object_get_ex(root, "decisions", &decisions) ||
        !json_object_is_type(decisions, json_type_array)) {
        json_object_put(root);
        strncpy(result->error_message, "No decisions in response", sizeof(result->error_message) - 1);
        result->error_message[sizeof(result->error_message) - 1] = '\0';
        return SGNL_ERROR;
    }
    
    int decision_count = json_object_array_length(decisions);
    if (decision_count == 0) {
        json_object_put(root);
        strncpy(result->decision, "Deny", sizeof(result->decision) - 1);
        result->decision[sizeof(result->decision) - 1] = '\0';
        return SGNL_DENIED;
    }
    
    // Get first decision
    json_object *decision_obj = json_object_array_get_idx(decisions, 0);
    if (!decision_obj) {
        json_object_put(root);
        return SGNL_ERROR;
    }
    
    json_object *decision_value;
    if (json_object_object_get_ex(decision_obj, "decision", &decision_value)) {
        const char *decision_str = json_object_get_string(decision_value);
        if (decision_str) {
            strncpy(result->decision, decision_str, sizeof(result->decision) - 1);
            result->decision[sizeof(result->decision) - 1] = '\0';
            
            if (strcmp(decision_str, "Allow") == 0) {
                json_object_put(root);
                return SGNL_ALLOWED;
            }
        }
    }
    
    // Extract reason if available
    json_object *reason_value;
    if (json_object_object_get_ex(decision_obj, "reason", &reason_value)) {
        const char *reason_str = json_object_get_string(reason_value);
        if (reason_str) {
            strncpy(result->reason, reason_str, sizeof(result->reason) - 1);
            result->reason[sizeof(result->reason) - 1] = '\0';
        }
    }
    
    json_object_put(root);
    return SGNL_DENIED;
}

// ============================================================================
// Public API Implementation
// ============================================================================

sgnl_client_t* sgnl_client_create(const sgnl_client_config_t *config) {
    sgnl_log_context_t log_ctx = SGNL_LOG_CONTEXT("libsgnl");
    
    SGNL_LOG_DEBUG(&log_ctx, "Creating SGNL client");
    
    sgnl_client_t *client = calloc(1, sizeof(sgnl_client_t));
    if (!client) {
        SGNL_LOG_ERROR(&log_ctx, "Failed to allocate memory for client");
        return NULL;
    }
    
    // Set defaults
    client->timeout_seconds = 30;
    client->connect_timeout_seconds = 10;
    client->ssl_verify_peer = true;
    client->ssl_verify_host = true;
    strcpy(client->user_agent, "SGNL-Client/1.0");
    client->debug_enabled = false;
    
    // Apply configuration
    if (config) {
        if (config->timeout_seconds > 0) {
            client->timeout_seconds = config->timeout_seconds;
        }
        if (config->retry_count >= 0) {
            // Note: retry logic not implemented in this simplified version
        }
        client->debug_enabled = config->enable_debug_logging;
        client->ssl_verify_peer = config->validate_ssl;
        client->ssl_verify_host = config->validate_ssl;
        
        if (config->user_agent) {
            strncpy(client->user_agent, config->user_agent, sizeof(client->user_agent) - 1);
            client->user_agent[sizeof(client->user_agent) - 1] = '\0';
        }
    }
    
    // Load configuration from common config system
    const char *config_path = config ? config->config_path : NULL;
    if (load_config_from_common_system(client, config_path) != SGNL_OK) {
        SGNL_LOG_ERROR(&log_ctx, "Failed to load configuration from common system");
        free(client);
        return NULL;
    }
    
    // Validate required fields
    if (strlen(client->api_url) == 0 || strlen(client->api_token) == 0) {
        sgnl_log_error(client, "Missing required configuration: api_url or api_token");
        free(client);
        return NULL;
    }
    
    // Initialize common logging system with client's debug setting
    sgnl_logger_config_t logging_config = sgnl_logger_config;
    if (client->debug_enabled) {
        logging_config.min_level = SGNL_LOG_DEBUG;
    }
    sgnl_log_init(&logging_config);
    
    client->initialized = true;
    sgnl_log_debug(client, "SGNL client initialized successfully");
    sgnl_log_debug(client, "Config: tenant=%s, api_url=%s", 
                   client->tenant, client->api_url);
    
    SGNL_LOG_DEBUG(&log_ctx, "SGNL client created and initialized successfully");
    
    return client;
}

void sgnl_client_destroy(sgnl_client_t *client) {
    if (client) {
        sgnl_log_context_t log_ctx = SGNL_LOG_CONTEXT("libsgnl");
        SGNL_LOG_DEBUG(&log_ctx, "Destroying SGNL client");
        
        // Clear sensitive data
        memset(client->api_token, 0, sizeof(client->api_token));
        free(client);
        
        // Cleanup logging system (only if no other clients are using it)
        sgnl_log_cleanup();
    }
}

sgnl_result_t sgnl_client_validate(sgnl_client_t *client) {
    if (!client || !client->initialized) {
        return SGNL_ERROR;
    }
    
    if (strlen(client->api_url) == 0) {
        return SGNL_CONFIG_ERROR;
    }
    
    if (strlen(client->api_token) == 0) {
        return SGNL_CONFIG_ERROR;
    }
    
    return SGNL_OK;
}

const char* sgnl_client_get_last_error(sgnl_client_t *client) {
    return client ? client->last_error : "No client";
}

bool sgnl_client_is_debug_enabled(sgnl_client_t *client) {
    return client ? client->debug_enabled : false;
}



sgnl_result_t sgnl_check_access(sgnl_client_t *client,
                               const char *principal_id,
                               const char *asset_id,
                               const char *action) {
    sgnl_access_result_t *result = sgnl_evaluate_access(client, principal_id, asset_id, action);
    if (!result) {
        return SGNL_ERROR;
    }
    
    sgnl_result_t res = result->result;
    sgnl_access_result_free(result);
    
    return res;
}

sgnl_access_result_t* sgnl_evaluate_access(sgnl_client_t *client,
                                           const char *principal_id,
                                           const char *asset_id,
                                           const char *action) {
    if (!client || !client->initialized || !principal_id) {
        return NULL;
    }
    
    sgnl_access_result_t *result = calloc(1, sizeof(sgnl_access_result_t));
    if (!result) {
        return NULL;
    }
    
    // Initialize result
    result->result = SGNL_ERROR;
    result->timestamp = time(NULL);
    strncpy(result->principal_id, principal_id, sizeof(result->principal_id) - 1);
    result->principal_id[sizeof(result->principal_id) - 1] = '\0';
    if (asset_id) {
        strncpy(result->asset_id, asset_id, sizeof(result->asset_id) - 1);
        result->asset_id[sizeof(result->asset_id) - 1] = '\0';
    }
    if (action) {
        strncpy(result->action, action, sizeof(result->action) - 1);
        result->action[sizeof(result->action) - 1] = '\0';
    } else {
        strcpy(result->action, "execute");
    }
    
    // Generate request ID
    strcpy(client->last_request_id, generate_request_id_internal());
    strncpy(result->request_id, client->last_request_id, sizeof(result->request_id) - 1);
    result->request_id[sizeof(result->request_id) - 1] = '\0';
    
    sgnl_log_debug(client, "Evaluating access: principal=%s, asset=%s, action=%s",
                   principal_id, asset_id ? asset_id : "N/A", result->action);
    
    // Create JSON request
    json_object *request = json_object_new_object();
    json_object *principal = json_object_new_object();
    json_object *queries = json_object_new_array();
    json_object *query = json_object_new_object();
    
    if (!request || !principal || !queries || !query) {
        if (request) json_object_put(request);
        if (principal) json_object_put(principal);
        if (queries) json_object_put(queries);
        if (query) json_object_put(query);
        strncpy(result->error_message, "Failed to create JSON request", sizeof(result->error_message) - 1);
        result->error_message[sizeof(result->error_message) - 1] = '\0';
        return result;
    }
    
    // Build request
    json_object_object_add(principal, "id", json_object_new_string(principal_id));
    json_object_object_add(principal, "deviceId", json_object_new_string(get_device_id()));
    json_object_object_add(request, "principal", principal);
    
    if (asset_id) {
        json_object_object_add(query, "assetId", json_object_new_string(asset_id));
    }
    json_object_object_add(query, "action", json_object_new_string(result->action));
    json_object_array_add(queries, query);
    json_object_object_add(request, "queries", queries);
    
    const char *json_payload = json_object_to_json_string(request);
    
    // Make HTTP request
    http_response_t *response = make_http_request(client, "/access/v2/evaluations", json_payload);
    
    json_object_put(request);
    
    if (!response) {
        result->result = SGNL_NETWORK_ERROR;
        strncpy(result->error_message, "HTTP request failed", sizeof(result->error_message) - 1);
        result->error_message[sizeof(result->error_message) - 1] = '\0';
        return result;
    }
    
    // Handle HTTP errors
    if (response->status_code != 200) {
        if (response->status_code == 401 || response->status_code == 403) {
            result->result = SGNL_AUTH_ERROR;
        } else if (response->status_code >= 500) {
            result->result = SGNL_NETWORK_ERROR;
        } else {
            result->result = SGNL_ERROR;
        }
        result->error_code = response->status_code;
        snprintf(result->error_message, sizeof(result->error_message), 
                "HTTP %ld: %s", response->status_code, 
                response->error_message ? response->error_message : "Unknown error");
        http_response_free(response);
        return result;
    }
    
    // Parse response
    result->result = parse_api_response(response->data, result);
    
    http_response_free(response);
    
    sgnl_log_debug(client, "Access evaluation completed: decision=%s, result=%s",
                   result->decision, sgnl_result_to_string(result->result));
    
    return result;
}

sgnl_access_result_t** sgnl_evaluate_access_batch(sgnl_client_t *client,
                                                  const char *principal_id,
                                                  const char **asset_ids,
                                                  const char **actions,
                                                  int query_count) {
    if (!client || !client->initialized || !principal_id || !asset_ids || query_count <= 0) {
        return NULL;
    }
    
    sgnl_access_result_t **results = calloc(query_count + 1, sizeof(sgnl_access_result_t*));
    if (!results) {
        return NULL;
    }
    
    // Initialize all results to NULL
    for (int i = 0; i < query_count; i++) {
        results[i] = NULL;
    }
    
    // Generate request ID
    strcpy(client->last_request_id, generate_request_id_internal());
    
    sgnl_log_debug(client, "Batch evaluating access: principal=%s, queries=%d", 
                   principal_id, query_count);
    
    // Create JSON request with multiple queries
    json_object *request = json_object_new_object();
    json_object *principal = json_object_new_object();
    json_object *queries = json_object_new_array();
    
    if (!request || !principal || !queries) {
        if (request) json_object_put(request);
        if (principal) json_object_put(principal);
        if (queries) json_object_put(queries);
        for (int i = 0; i < query_count; i++) {
            if (results[i]) sgnl_access_result_free(results[i]);
        }
        free(results);
        return NULL;
    }
    
    // Build request
    json_object_object_add(principal, "id", json_object_new_string(principal_id));
    json_object_object_add(principal, "deviceId", json_object_new_string(get_device_id()));
    json_object_object_add(request, "principal", principal);
    
    // Add each query
    for (int i = 0; i < query_count; i++) {
        json_object *query = json_object_new_object();
        if (!query) {
            json_object_put(request);
            for (int j = 0; j < query_count; j++) {
                if (results[j]) sgnl_access_result_free(results[j]);
            }
            free(results);
            return NULL;
        }
        
        if (asset_ids[i]) {
            json_object_object_add(query, "assetId", json_object_new_string(asset_ids[i]));
        }
        
        const char *action = actions ? actions[i] : "execute";
        json_object_object_add(query, "action", json_object_new_string(action));
        
        json_object_array_add(queries, query);
    }
    
    json_object_object_add(request, "queries", queries);
    
    const char *json_payload = json_object_to_json_string(request);
    
    sgnl_log_debug(client, "Batch request payload: %s", json_payload);
    
    // Make HTTP request
    http_response_t *response = make_http_request(client, "/access/v2/evaluations", json_payload);
    
    json_object_put(request);
    
    if (!response) {
        sgnl_log_error(client, "HTTP request failed for batch evaluation");
        for (int i = 0; i < query_count; i++) {
            if (results[i]) sgnl_access_result_free(results[i]);
        }
        free(results);
        return NULL;
    }
    
    // Handle HTTP errors
    if (response->status_code != 200) {
        sgnl_log_error(client, "HTTP request failed with status %ld for batch evaluation", 
                      response->status_code);
        http_response_free(response);
        for (int i = 0; i < query_count; i++) {
            if (results[i]) sgnl_access_result_free(results[i]);
        }
        free(results);
        return NULL;
    }
    
    // Parse batch response
    json_object *root = json_tokener_parse(response->data);
    if (!root) {
        sgnl_log_error(client, "Failed to parse JSON response for batch evaluation");
        http_response_free(response);
        for (int i = 0; i < query_count; i++) {
            if (results[i]) sgnl_access_result_free(results[i]);
        }
        free(results);
        return NULL;
    }
    
    // Get decisions array
    json_object *decisions;
    if (!json_object_object_get_ex(root, "decisions", &decisions) ||
        !json_object_is_type(decisions, json_type_array)) {
        sgnl_log_error(client, "No decisions array in batch response");
        json_object_put(root);
        http_response_free(response);
        for (int i = 0; i < query_count; i++) {
            if (results[i]) sgnl_access_result_free(results[i]);
        }
        free(results);
        return NULL;
    }
    
    int decision_count = json_object_array_length(decisions);
    sgnl_log_debug(client, "Batch response contains %d decisions", decision_count);
    
    // Process each decision and create corresponding result
    for (int i = 0; i < decision_count && i < query_count; i++) {
        json_object *decision_obj = json_object_array_get_idx(decisions, i);
        if (!decision_obj) continue;
        
        // Create result object
        results[i] = calloc(1, sizeof(sgnl_access_result_t));
        if (!results[i]) continue;
        
        // Initialize result
        results[i]->result = SGNL_ERROR;
        results[i]->timestamp = time(NULL);
        strncpy(results[i]->principal_id, principal_id, sizeof(results[i]->principal_id) - 1);
        results[i]->principal_id[sizeof(results[i]->principal_id) - 1] = '\0';
        strncpy(results[i]->request_id, client->last_request_id, sizeof(results[i]->request_id) - 1);
        results[i]->request_id[sizeof(results[i]->request_id) - 1] = '\0';
        
        if (asset_ids[i]) {
            strncpy(results[i]->asset_id, asset_ids[i], sizeof(results[i]->asset_id) - 1);
            results[i]->asset_id[sizeof(results[i]->asset_id) - 1] = '\0';
        }
        
        const char *action = actions ? actions[i] : "execute";
        strncpy(results[i]->action, action, sizeof(results[i]->action) - 1);
        results[i]->action[sizeof(results[i]->action) - 1] = '\0';
        
        // Parse decision
        json_object *decision_value;
        if (json_object_object_get_ex(decision_obj, "decision", &decision_value)) {
            const char *decision_str = json_object_get_string(decision_value);
            if (decision_str) {
                strncpy(results[i]->decision, decision_str, sizeof(results[i]->decision) - 1);
                results[i]->decision[sizeof(results[i]->decision) - 1] = '\0';
                
                if (strcmp(decision_str, "Allow") == 0) {
                    results[i]->result = SGNL_ALLOWED;
                } else {
                    results[i]->result = SGNL_DENIED;
                }
            }
        }
        
        // Extract reason if available
        json_object *reason_value;
        if (json_object_object_get_ex(decision_obj, "reason", &reason_value)) {
            const char *reason_str = json_object_get_string(reason_value);
            if (reason_str) {
                strncpy(results[i]->reason, reason_str, sizeof(results[i]->reason) - 1);
                results[i]->reason[sizeof(results[i]->reason) - 1] = '\0';
            }
        }
        
        sgnl_log_debug(client, "Batch result[%d]: %s -> %s", i, 
                      asset_ids[i] ? asset_ids[i] : "N/A", 
                      sgnl_result_to_string(results[i]->result));
    }
    
    // For any remaining slots, create default denied results
    for (int i = decision_count; i < query_count; i++) {
        results[i] = calloc(1, sizeof(sgnl_access_result_t));
        if (results[i]) {
            results[i]->result = SGNL_DENIED;
            results[i]->timestamp = time(NULL);
            strncpy(results[i]->principal_id, principal_id, sizeof(results[i]->principal_id) - 1);
            results[i]->principal_id[sizeof(results[i]->principal_id) - 1] = '\0';
            strncpy(results[i]->request_id, client->last_request_id, sizeof(results[i]->request_id) - 1);
            results[i]->request_id[sizeof(results[i]->request_id) - 1] = '\0';
            strcpy(results[i]->decision, "Deny");
            
            if (asset_ids[i]) {
                strncpy(results[i]->asset_id, asset_ids[i], sizeof(results[i]->asset_id) - 1);
                results[i]->asset_id[sizeof(results[i]->asset_id) - 1] = '\0';
            }
            
            const char *action = actions ? actions[i] : "execute";
            strncpy(results[i]->action, action, sizeof(results[i]->action) - 1);
            results[i]->action[sizeof(results[i]->action) - 1] = '\0';
        }
    }
    
    json_object_put(root);
    http_response_free(response);
    
    sgnl_log_debug(client, "Batch access evaluation completed");
    
    return results;
}

char** sgnl_search_assets(sgnl_client_t *client,
                          const char *principal_id,
                          const char *action,
                          int *asset_count) {
    if (!client || !principal_id || !asset_count) {
        if (asset_count) *asset_count = 0;
        return NULL;
    }
    
    *asset_count = 0;
    
    if (!client->initialized) {
        sgnl_log_error(client, "Client not initialized");
        return NULL;
    }
    
    // Build endpoint path (correct SGNL API v2 endpoint)
    const char *endpoint = "/access/v2/search";
    const char *search_action = action ? action : "list";
    
    // Build JSON request body according to SearchRequest schema
    char json_body[1024];
    snprintf(json_body, sizeof(json_body), 
             "{"
             "\"principal\":{\"id\":\"%s\",\"deviceId\":\"%s\"},"
             "\"queries\":[{\"action\":\"%s\"}]"
             "}", 
             principal_id, get_device_id(), search_action);
    
    sgnl_log_debug(client, "Making asset search request to: %s.%s%s", client->tenant, client->api_url, endpoint);
    sgnl_log_debug(client, "Request body: %s", json_body);
    
    // Make HTTP request
    http_response_t *response = make_http_request(client, endpoint, json_body);
    if (!response) {
        sgnl_log_error(client, "Failed to make HTTP request");
        return NULL;
    }
    
    if (response->status_code != 200) {
        sgnl_log_error(client, "HTTP request failed with status %ld", response->status_code);
        http_response_free(response);
        return NULL;
    }
    
    sgnl_log_debug(client, "Received response: %s", response->data);
    
    // Parse JSON response
    json_object *root = json_tokener_parse(response->data);
    if (!root) {
        sgnl_log_error(client, "Failed to parse JSON response");
        http_response_free(response);
        return NULL;
    }
    
    // Extract decisions array
    json_object *decisions_array;
    if (!json_object_object_get_ex(root, "decisions", &decisions_array)) {
        sgnl_log_error(client, "No 'decisions' field in response");
        json_object_put(root);
        http_response_free(response);
        return NULL;
    }
    
    if (!json_object_is_type(decisions_array, json_type_array)) {
        sgnl_log_error(client, "'decisions' field is not an array");
        json_object_put(root);
        http_response_free(response);
        return NULL;
    }
    
    int decisions_count = json_object_array_length(decisions_array);
    sgnl_log_debug(client, "Found %d decisions in response", decisions_count);
    
    // First pass: count allowed assets
    int allowed_count = 0;
    for (int i = 0; i < decisions_count; i++) {
        json_object *decision = json_object_array_get_idx(decisions_array, i);
        if (!decision) continue;
        
        json_object *decision_field;
        if (json_object_object_get_ex(decision, "decision", &decision_field)) {
            const char *decision_value = json_object_get_string(decision_field);
            if (decision_value && strcmp(decision_value, "Allow") == 0) {
                allowed_count++;
            }
        }
    }
    
    sgnl_log_debug(client, "Found %d allowed assets", allowed_count);
    
    if (allowed_count == 0) {
        json_object_put(root);
        http_response_free(response);
        *asset_count = 0;
        return calloc(1, sizeof(char*)); // Empty NULL-terminated array
    }
    
    // Allocate array for asset IDs (+ 1 for NULL terminator)
    char **asset_ids = calloc(allowed_count + 1, sizeof(char*));
    if (!asset_ids) {
        sgnl_log_error(client, "Failed to allocate memory for asset IDs");
        json_object_put(root);
        http_response_free(response);
        return NULL;
    }
    
    // Second pass: extract asset IDs from allowed decisions
    int asset_index = 0;
    for (int i = 0; i < decisions_count && asset_index < allowed_count; i++) {
        json_object *decision = json_object_array_get_idx(decisions_array, i);
        if (!decision) continue;
        
        json_object *decision_field;
        if (json_object_object_get_ex(decision, "decision", &decision_field)) {
            const char *decision_value = json_object_get_string(decision_field);
            if (decision_value && strcmp(decision_value, "Allow") == 0) {
                // Extract assetId
                json_object *asset_id_field;
                if (json_object_object_get_ex(decision, "assetId", &asset_id_field)) {
                    const char *asset_id = json_object_get_string(asset_id_field);
                    if (asset_id) {
                        asset_ids[asset_index] = strdup(asset_id);
                        if (asset_ids[asset_index]) {
                            sgnl_log_debug(client, "Added allowed asset: %s", asset_id);
                            asset_index++;
                        }
                    }
                }
            }
        }
    }
    
    json_object_put(root);
    http_response_free(response);
    
    *asset_count = asset_index;
    sgnl_log_debug(client, "Asset search completed: %d assets found", asset_index);
    
    return asset_ids;
}

sgnl_search_result_t* sgnl_search_assets_detailed(sgnl_client_t *client,
                                                  const char *principal_id,
                                                  const char *action,
                                                  const char *page_token,
                                                  int page_size) {
    // Simplified implementation
    (void)client;     // Suppress unused parameter warning
    (void)page_token; // Suppress unused parameter warning
    (void)page_size;  // Suppress unused parameter warning
    
    sgnl_search_result_t *result = calloc(1, sizeof(sgnl_search_result_t));
    if (result) {
        result->result = SGNL_OK;
        result->asset_count = 0;
        result->asset_ids = calloc(1, sizeof(char*)); // NULL-terminated empty array
        if (principal_id) {
            strncpy(result->principal_id, principal_id, sizeof(result->principal_id) - 1);
            result->principal_id[sizeof(result->principal_id) - 1] = '\0';
        }
        if (action) {
            strncpy(result->action, action, sizeof(result->action) - 1);
            result->action[sizeof(result->action) - 1] = '\0';
        }
    }
    return result;
}

void sgnl_access_result_free(sgnl_access_result_t *result) {
    if (result) {
        free(result);
    }
}

void sgnl_access_result_array_free(sgnl_access_result_t **results, int count) {
    if (results) {
        for (int i = 0; i < count; i++) {
            sgnl_access_result_free(results[i]);
        }
        free(results);
    }
}

void sgnl_search_result_free(sgnl_search_result_t *result) {
    if (result) {
        if (result->asset_ids) {
            sgnl_asset_ids_free(result->asset_ids, result->asset_count);
        }
        if (result->next_page_token) {
            free(result->next_page_token);
        }
        free(result);
    }
}

void sgnl_asset_ids_free(char **asset_ids, int count) {
    if (asset_ids) {
        for (int i = 0; i < count && asset_ids[i]; i++) {
            free(asset_ids[i]);
        }
        free(asset_ids);
    }
}

const char* sgnl_result_to_string(sgnl_result_t result) {
    switch (result) {
        case SGNL_OK:
            return "Success";
        case SGNL_ALLOWED:
            return "Access Allowed";
        case SGNL_DENIED:
            return "Access Denied";
        case SGNL_ERROR:
            return "Error";
        case SGNL_CONFIG_ERROR:
            return "Configuration Error";
        case SGNL_NETWORK_ERROR:
            return "Network Error";
        case SGNL_AUTH_ERROR:
            return "Authentication Error";
        case SGNL_TIMEOUT_ERROR:
            return "Timeout Error";
        case SGNL_INVALID_REQUEST:
            return "Invalid Request";
        case SGNL_MEMORY_ERROR:
            return "Memory Error";
        default:
            return "Unknown Error";
    }
}

char* sgnl_generate_request_id(void) {
    char *request_id = malloc(64);
    if (request_id) {
        strcpy(request_id, generate_request_id_internal());
    }
    return request_id;
}

bool sgnl_validate_principal_id(const char *principal_id) {
    if (!principal_id || strlen(principal_id) == 0) {
        return false;
    }
    
    size_t len = strlen(principal_id);
    return (len > 0 && len < 256);
}

bool sgnl_validate_asset_id(const char *asset_id) {
    if (!asset_id || strlen(asset_id) == 0) {
        return false;
    }
    
    size_t len = strlen(asset_id);
    return (len > 0 && len < 256);
}

const char* sgnl_get_version(void) {
    return "1.0.0";
}

 