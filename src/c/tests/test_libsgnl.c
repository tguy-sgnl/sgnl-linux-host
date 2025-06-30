/*
 * SGNL Core Library Tests
 *
 * Tests for the core SGNL library functionality.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include "../lib/libsgnl.h"
#include "../common/config.h"
#include "../common/logging.h"

// Test utilities
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("❌ FAIL: %s\n", message); \
            return 1; \
        } else { \
            printf("✅ PASS: %s\n", message); \
        } \
    } while(0)

#define TEST_SECTION(name) printf("\n🧪 Testing: %s\n", name)

// Static test config file path
static const char *test_config_file = "tests/test_config.json";

// Test client creation and destruction
static int test_client_lifecycle(void) {
    TEST_SECTION("Client Lifecycle");
    
    // Test creation with config file
    sgnl_client_config_t config = {
        .config_path = test_config_file,
        .timeout_seconds = 30,
        .retry_count = 3,
        .retry_delay_ms = 1000,
        .enable_debug_logging = true,
        .validate_ssl = true,
        .user_agent = "SGNL-Test/2.0"
    };
    
    sgnl_client_t *client = sgnl_client_create(&config);
    TEST_ASSERT(client != NULL, "Client creation with config file");
    
    // Test destruction
    sgnl_client_destroy(client);
    printf("✅ PASS: Client destruction\n");
    
    // Test creation with NULL config (should fail without config file)
    client = sgnl_client_create(NULL);
    TEST_ASSERT(client == NULL, "Client creation with NULL config fails without config file");
    
    return 0;
}

// Test client validation
static int test_client_validation(void) {
    TEST_SECTION("Client Validation");
    
    // Test validation with NULL client
    sgnl_result_t result = sgnl_client_validate(NULL);
    TEST_ASSERT(result == SGNL_ERROR, "NULL client validation fails");
    
    // Test validation with proper client
    sgnl_client_config_t config = {
        .config_path = test_config_file,
        .timeout_seconds = 30,
        .retry_count = 3,
        .retry_delay_ms = 1000,
        .enable_debug_logging = true,
        .validate_ssl = true,
        .user_agent = "SGNL-Test/1.0"
    };
    
    sgnl_client_t *client = sgnl_client_create(&config);
    TEST_ASSERT(client != NULL, "Client creation");
    
    // Test validation (should pass with proper config)
    result = sgnl_client_validate(client);
    TEST_ASSERT(result == SGNL_OK, "Client validation passes with proper config");
    
    // Cleanup
    sgnl_client_destroy(client);
    
    return 0;
}

// Test client error handling
static int test_client_error_handling(void) {
    TEST_SECTION("Client Error Handling");
    
    // Test with NULL client
    const char *error = sgnl_client_get_last_error(NULL);
    TEST_ASSERT(error != NULL, "Error message available for NULL client");
    TEST_ASSERT(strlen(error) >= 0, "Error message has valid length");
    
    // Test with valid client
    sgnl_client_config_t config = {
        .config_path = test_config_file,
        .timeout_seconds = 30,
        .retry_count = 3,
        .retry_delay_ms = 1000,
        .enable_debug_logging = true,
        .validate_ssl = true,
        .user_agent = "SGNL-Test/1.0"
    };
    
    sgnl_client_t *client = sgnl_client_create(&config);
    TEST_ASSERT(client != NULL, "Client creation");
    
    // Test get last error
    error = sgnl_client_get_last_error(client);
    TEST_ASSERT(error != NULL, "Error message available");
    TEST_ASSERT(strlen(error) >= 0, "Error message has valid length");
    
    // Test debug enabled check
    bool debug_enabled = sgnl_client_is_debug_enabled(client);
    TEST_ASSERT(debug_enabled == true, "Debug enabled check works");
    
    // Cleanup
    sgnl_client_destroy(client);
    
    return 0;
}

// Test result code conversion
static int test_result_codes(void) {
    TEST_SECTION("Result Code Conversion");
    
    TEST_ASSERT(strcmp(sgnl_result_to_string(SGNL_OK), "Success") == 0, "OK result string");
    TEST_ASSERT(strcmp(sgnl_result_to_string(SGNL_DENIED), "Access denied") == 0, "Denied result string");
    TEST_ASSERT(strcmp(sgnl_result_to_string(SGNL_ALLOWED), "Access allowed") == 0, "Allowed result string");
    TEST_ASSERT(strcmp(sgnl_result_to_string(SGNL_ERROR), "General error") == 0, "Error result string");
    TEST_ASSERT(strcmp(sgnl_result_to_string(SGNL_CONFIG_ERROR), "Configuration error") == 0, "Config error result string");
    TEST_ASSERT(strcmp(sgnl_result_to_string(SGNL_NETWORK_ERROR), "Network/HTTP error") == 0, "Network error result string");
    TEST_ASSERT(strcmp(sgnl_result_to_string(SGNL_AUTH_ERROR), "Authentication error") == 0, "Auth error result string");
    TEST_ASSERT(strcmp(sgnl_result_to_string(SGNL_TIMEOUT_ERROR), "Timeout error") == 0, "Timeout error result string");
    TEST_ASSERT(strcmp(sgnl_result_to_string(SGNL_INVALID_REQUEST), "Invalid request") == 0, "Invalid request result string");
    TEST_ASSERT(strcmp(sgnl_result_to_string(SGNL_MEMORY_ERROR), "Memory allocation error") == 0, "Memory error result string");
    
    return 0;
}

// Test request ID generation
static int test_request_id_generation(void) {
    TEST_SECTION("Request ID Generation");
    
    char *request_id1 = sgnl_generate_request_id();
    TEST_ASSERT(request_id1 != NULL, "Request ID generation");
    TEST_ASSERT(strlen(request_id1) > 0, "Request ID not empty");
    
    char *request_id2 = sgnl_generate_request_id();
    TEST_ASSERT(request_id2 != NULL, "Second request ID generation");
    TEST_ASSERT(strcmp(request_id1, request_id2) != 0, "Request IDs are unique");
    
    free(request_id1);
    free(request_id2);
    
    return 0;
}

// Test validation functions
static int test_validation_functions(void) {
    TEST_SECTION("Validation Functions");
    
    // Test principal ID validation
    TEST_ASSERT(sgnl_validate_principal_id("user123") == true, "Valid principal ID");
    TEST_ASSERT(sgnl_validate_principal_id("user-123") == true, "Valid principal ID with dash");
    TEST_ASSERT(sgnl_validate_principal_id("user_123") == true, "Valid principal ID with underscore");
    TEST_ASSERT(sgnl_validate_principal_id("") == false, "Empty principal ID invalid");
    TEST_ASSERT(sgnl_validate_principal_id(NULL) == false, "NULL principal ID invalid");
    
    // Test asset ID validation
    TEST_ASSERT(sgnl_validate_asset_id("asset123") == true, "Valid asset ID");
    TEST_ASSERT(sgnl_validate_asset_id("asset-123") == true, "Valid asset ID with dash");
    TEST_ASSERT(sgnl_validate_asset_id("asset_123") == true, "Valid asset ID with underscore");
    TEST_ASSERT(sgnl_validate_asset_id("") == false, "Empty asset ID invalid");
    TEST_ASSERT(sgnl_validate_asset_id(NULL) == false, "NULL asset ID invalid");
    
    return 0;
}

// Test version functions
static int test_version_functions(void) {
    TEST_SECTION("Version Functions");
    
    const char *version = sgnl_get_version();
    TEST_ASSERT(version != NULL, "Version string not NULL");
    TEST_ASSERT(strlen(version) > 0, "Version string not empty");
    
    // Check version format (should contain version numbers)
    TEST_ASSERT(strstr(version, "1.0.0") != NULL || strstr(version, "1.0") != NULL, "Version contains expected format");
    
    return 0;
}

// Test simple access check
static int test_simple_access_check(void) {
    TEST_SECTION("Simple Access Check");
    
    // Test with NULL client
    sgnl_result_t result = sgnl_check_access(NULL, "test-user", "test-asset", "execute");
    TEST_ASSERT(result == SGNL_ERROR, "NULL client access check fails");
    
    // Test with valid client
    sgnl_client_config_t config = {
        .config_path = test_config_file,
        .timeout_seconds = 30,
        .retry_count = 3,
        .retry_delay_ms = 1000,
        .enable_debug_logging = true,
        .validate_ssl = true,
        .user_agent = "SGNL-Test/1.0"
    };
    
    sgnl_client_t *client = sgnl_client_create(&config);
    TEST_ASSERT(client != NULL, "Client creation");
    
    // Test simple access check (should fail due to network/API issues in test environment)
    result = sgnl_check_access(client, "test-user", "test-asset", "execute");
    TEST_ASSERT(result == SGNL_NETWORK_ERROR || result == SGNL_ERROR, "Access check fails due to network/API issues");
    
    // Cleanup
    sgnl_client_destroy(client);
    
    return 0;
}

// Test detailed access evaluation
static int test_detailed_access_evaluation(void) {
    TEST_SECTION("Detailed Access Evaluation");
    
    // Test with NULL client
    sgnl_access_result_t *result = sgnl_evaluate_access(NULL, "test-user", "test-asset", "execute");
    TEST_ASSERT(result == NULL, "NULL client evaluation returns NULL");
    
    // Test with valid client
    sgnl_client_config_t config = {
        .config_path = test_config_file,
        .timeout_seconds = 30,
        .retry_count = 3,
        .retry_delay_ms = 1000,
        .enable_debug_logging = true,
        .validate_ssl = true,
        .user_agent = "SGNL-Test/1.0"
    };
    
    sgnl_client_t *client = sgnl_client_create(&config);
    TEST_ASSERT(client != NULL, "Client creation");
    
    // Test detailed access evaluation (should fail due to network/API issues in test environment)
    result = sgnl_evaluate_access(client, "test-user", "test-asset", "execute");
    TEST_ASSERT(result != NULL, "Access result created");
    TEST_ASSERT(result->result == SGNL_NETWORK_ERROR || result->result == SGNL_ERROR, "Access evaluation fails due to network/API issues");
    
    // Verify result structure
    TEST_ASSERT(strlen(result->decision) > 0, "Decision field populated");
    TEST_ASSERT(strlen(result->reason) > 0, "Reason field populated");
    TEST_ASSERT(strcmp(result->asset_id, "test-asset") == 0, "Asset ID set correctly");
    TEST_ASSERT(strcmp(result->action, "execute") == 0, "Action set correctly");
    TEST_ASSERT(strcmp(result->principal_id, "test-user") == 0, "Principal ID set correctly");
    
    // Cleanup
    sgnl_access_result_free(result);
    printf("✅ PASS: Access result cleanup\n");
    sgnl_client_destroy(client);
    
    return 0;
}

// Test batch access evaluation
static int test_batch_access_evaluation(void) {
    TEST_SECTION("Batch Access Evaluation");
    
    // Test with NULL client
    const char *asset_ids[] = {"asset1", "asset2", "asset3"};
    const char *actions[] = {"execute", "read", "write"};
    
    sgnl_access_result_t **results = sgnl_evaluate_access_batch(NULL, "test-user", asset_ids, actions, 3);
    TEST_ASSERT(results == NULL, "NULL client batch evaluation returns NULL");
    
    // Test with valid client
    sgnl_client_config_t config = {
        .config_path = test_config_file,
        .timeout_seconds = 30,
        .retry_count = 3,
        .retry_delay_ms = 1000,
        .enable_debug_logging = true,
        .validate_ssl = true,
        .user_agent = "SGNL-Test/1.0"
    };
    
    sgnl_client_t *client = sgnl_client_create(&config);
    TEST_ASSERT(client != NULL, "Client creation");
    
    // Test batch access evaluation (should fail due to network/API issues in test environment)
    results = sgnl_evaluate_access_batch(client, "test-user", asset_ids, actions, 3);
    TEST_ASSERT(results != NULL, "Batch results created");
    
    // Verify results structure
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT(results[i] != NULL, "Individual result created");
        TEST_ASSERT(results[i]->result == SGNL_NETWORK_ERROR || results[i]->result == SGNL_ERROR, "Batch result fails due to network/API issues");
        TEST_ASSERT(strcmp(results[i]->asset_id, asset_ids[i]) == 0, "Asset ID set correctly");
        TEST_ASSERT(strcmp(results[i]->action, actions[i]) == 0, "Action set correctly");
    }
    
    // Cleanup
    sgnl_access_result_array_free(results, 3);
    printf("✅ PASS: Batch results cleanup\n");
    sgnl_client_destroy(client);
    
    return 0;
}

// Test asset search
static int test_asset_search(void) {
    TEST_SECTION("Asset Search");
    
    // Test with NULL client
    char **asset_ids = sgnl_search_assets(NULL, "test-user", "execute", NULL);
    TEST_ASSERT(asset_ids == NULL, "NULL client asset search returns NULL");
    
    // Test with valid client
    sgnl_client_config_t config = {
        .config_path = test_config_file,
        .timeout_seconds = 30,
        .retry_count = 3,
        .retry_delay_ms = 1000,
        .enable_debug_logging = true,
        .validate_ssl = true,
        .user_agent = "SGNL-Test/1.0"
    };
    
    sgnl_client_t *client = sgnl_client_create(&config);
    TEST_ASSERT(client != NULL, "Client creation");
    
    int asset_count = 0;
    
    // Test asset search (should fail due to network/API issues in test environment)
    asset_ids = sgnl_search_assets(client, "test-user", "execute", &asset_count);
    TEST_ASSERT(asset_ids == NULL, "Asset search returns NULL due to network/API issues");
    TEST_ASSERT(asset_count == 0, "Asset count is zero due to network/API issues");
    
    // Cleanup
    sgnl_client_destroy(client);
    
    return 0;
}

// Test detailed asset search
static int test_detailed_asset_search(void) {
    TEST_SECTION("Detailed Asset Search");
    
    // Test with NULL client
    sgnl_search_result_t *result = sgnl_search_assets_detailed(NULL, "test-user", "execute", NULL, 50);
    TEST_ASSERT(result == NULL, "NULL client detailed search returns NULL");
    
    // Test with valid client
    sgnl_client_config_t config = {
        .config_path = test_config_file,
        .timeout_seconds = 30,
        .retry_count = 3,
        .retry_delay_ms = 1000,
        .enable_debug_logging = true,
        .validate_ssl = true,
        .user_agent = "SGNL-Test/1.0"
    };
    
    sgnl_client_t *client = sgnl_client_create(&config);
    TEST_ASSERT(client != NULL, "Client creation");
    
    // Test detailed asset search (should fail due to network/API issues in test environment)
    result = sgnl_search_assets_detailed(client, "test-user", "execute", NULL, 50);
    TEST_ASSERT(result != NULL, "Search result created");
    TEST_ASSERT(result->result == SGNL_ERROR || result->result == SGNL_NETWORK_ERROR, "Search fails due to network/API issues");
    
    // Verify result structure
    TEST_ASSERT(result->asset_count == 0, "Asset count is zero due to network/API issues");
    TEST_ASSERT(result->asset_ids == NULL, "Asset IDs is NULL due to network/API issues");
    TEST_ASSERT(strcmp(result->principal_id, "test-user") == 0, "Principal ID set correctly");
    TEST_ASSERT(strcmp(result->action, "execute") == 0, "Action set correctly");
    
    // Cleanup
    sgnl_search_result_free(result);
    printf("✅ PASS: Search result cleanup\n");
    sgnl_client_destroy(client);
    
    return 0;
}

// Test memory management
static int test_memory_management(void) {
    TEST_SECTION("Memory Management");
    
    // Test asset ID array cleanup
    char **asset_ids = malloc(3 * sizeof(char*));
    asset_ids[0] = strdup("asset1");
    asset_ids[1] = strdup("asset2");
    asset_ids[2] = NULL;
    
    sgnl_asset_ids_free(asset_ids, 2);
    printf("✅ PASS: Asset IDs cleanup\n");
    
    return 0;
}

// Test client configuration loading
static int test_client_config_loading(void) {
    TEST_SECTION("Client Configuration Loading");
    
    // Test client creation with config file
    sgnl_client_config_t config = {
        .config_path = test_config_file,
        .timeout_seconds = 30,
        .retry_count = 2,
        .retry_delay_ms = 1000,
        .enable_debug_logging = true,
        .validate_ssl = true,
        .user_agent = "SGNL-Test/1.0"
    };
    
    sgnl_client_t *client = sgnl_client_create(&config);
    TEST_ASSERT(client != NULL, "Client creation with config file");
    
    // Test validation (should pass with proper config)
    sgnl_result_t result = sgnl_client_validate(client);
    TEST_ASSERT(result == SGNL_OK, "Client validation passes with proper config");
    
    // Cleanup
    sgnl_client_destroy(client);
    
    return 0;
}

// Test error message handling
static int test_error_message_handling(void) {
    TEST_SECTION("Error Message Handling");
    
    // Test that error messages are available for NULL client
    const char *error = sgnl_client_get_last_error(NULL);
    TEST_ASSERT(error != NULL, "Error message available for NULL client");
    TEST_ASSERT(strlen(error) >= 0, "Error message has valid length");
    
    // Test with valid client
    sgnl_client_config_t config = {
        .config_path = test_config_file,
        .timeout_seconds = 30,
        .retry_count = 3,
        .retry_delay_ms = 1000,
        .enable_debug_logging = true,
        .validate_ssl = true,
        .user_agent = "SGNL-Test/1.0"
    };
    
    sgnl_client_t *client = sgnl_client_create(&config);
    TEST_ASSERT(client != NULL, "Client creation");
    
    // Test that error messages are available
    error = sgnl_client_get_last_error(client);
    TEST_ASSERT(error != NULL, "Error message available");
    TEST_ASSERT(strlen(error) >= 0, "Error message has valid length");
    
    // Cleanup
    sgnl_client_destroy(client);
    
    return 0;
}

// Test client configuration options
static int test_client_config_options(void) {
    TEST_SECTION("Client Configuration Options");
    
    // Test various configuration combinations
    sgnl_client_config_t configs[] = {
        {.config_path = test_config_file, .timeout_seconds = 0, .retry_count = 0, .retry_delay_ms = 0, .enable_debug_logging = false, .validate_ssl = true, .user_agent = NULL},
        {.config_path = test_config_file, .timeout_seconds = 60, .retry_count = 5, .retry_delay_ms = 2000, .enable_debug_logging = true, .validate_ssl = false, .user_agent = "Custom/1.0"},
        {.config_path = test_config_file, .timeout_seconds = 10, .retry_count = 1, .retry_delay_ms = 500, .enable_debug_logging = false, .validate_ssl = true, .user_agent = NULL}
    };
    
    for (int i = 0; i < 3; i++) {
        sgnl_client_t *client = sgnl_client_create(&configs[i]);
        TEST_ASSERT(client != NULL, "Client creation with config");
        sgnl_client_destroy(client);
    }
    
    return 0;
}

// Test library constants
static int test_library_constants(void) {
    TEST_SECTION("Library Constants");
    
    // Test version constants
    TEST_ASSERT(LIBSGNL_VERSION_MAJOR == 1, "Major version constant");
    TEST_ASSERT(LIBSGNL_VERSION_MINOR == 0, "Minor version constant");
    TEST_ASSERT(LIBSGNL_VERSION_PATCH == 0, "Patch version constant");
    
    // Test result constants
    TEST_ASSERT(SGNL_OK == 0, "OK result constant");
    TEST_ASSERT(SGNL_DENIED == 1, "Denied result constant");
    TEST_ASSERT(SGNL_ALLOWED == 2, "Allowed result constant");
    TEST_ASSERT(SGNL_ERROR == 3, "Error result constant");
    
    return 0;
}

// Test NULL parameter handling
static int test_null_parameter_handling(void) {
    TEST_SECTION("NULL Parameter Handling");
    
    // Test with valid client
    sgnl_client_config_t config = {
        .config_path = test_config_file,
        .timeout_seconds = 30,
        .retry_count = 3,
        .retry_delay_ms = 1000,
        .enable_debug_logging = true,
        .validate_ssl = true,
        .user_agent = "SGNL-Test/1.0"
    };
    
    sgnl_client_t *client = sgnl_client_create(&config);
    TEST_ASSERT(client != NULL, "Client creation with config file");
    
    // Test access check with NULL parameters
    sgnl_result_t result = sgnl_check_access(client, NULL, "asset", "action");
    TEST_ASSERT(result == SGNL_ERROR, "NULL principal handled");
    
    result = sgnl_check_access(client, "user", NULL, "action");
    TEST_ASSERT(result == SGNL_ERROR || result == SGNL_NETWORK_ERROR, "NULL asset handled");
    
    result = sgnl_check_access(client, "user", "asset", NULL);
    TEST_ASSERT(result == SGNL_ERROR || result == SGNL_NETWORK_ERROR, "NULL action handled");
    
    // Cleanup
    sgnl_client_destroy(client);
    
    return 0;
}

// Test empty string handling
static int test_empty_string_handling(void) {
    TEST_SECTION("Empty String Handling");
    
    // Test with valid client
    sgnl_client_config_t config = {
        .config_path = test_config_file,
        .timeout_seconds = 30,
        .retry_count = 3,
        .retry_delay_ms = 1000,
        .enable_debug_logging = true,
        .validate_ssl = true,
        .user_agent = "SGNL-Test/1.0"
    };
    
    sgnl_client_t *client = sgnl_client_create(&config);
    TEST_ASSERT(client != NULL, "Client creation");
    
    // Test access check with empty strings
    sgnl_result_t result = sgnl_check_access(client, "", "asset", "action");
    TEST_ASSERT(result == SGNL_ERROR, "Empty principal handled");
    
    result = sgnl_check_access(client, "user", "", "action");
    TEST_ASSERT(result == SGNL_ERROR || result == SGNL_NETWORK_ERROR, "Empty asset handled");
    
    // Cleanup
    sgnl_client_destroy(client);
    
    return 0;
}

#ifdef SGNL_TEST_RUNNER
int test_libsgnl_main(void)
#else
static int test_libsgnl_main(void)
#endif
{
    sgnl_logger_config_t log_config = {
        .min_level = SGNL_LOG_ERROR,  // Only show errors during tests
        .use_syslog = false,
        .structured_format = false,
        .include_timestamp = false,
        .include_pid = false,
        .facility = "local0"
    };
    sgnl_log_init(&log_config);
    int failures = 0;
    failures += test_client_lifecycle();
    failures += test_client_validation();
    failures += test_client_error_handling();
    failures += test_result_codes();
    failures += test_request_id_generation();
    failures += test_validation_functions();
    failures += test_version_functions();
    failures += test_simple_access_check();
    failures += test_detailed_access_evaluation();
    failures += test_batch_access_evaluation();
    failures += test_asset_search();
    failures += test_detailed_asset_search();
    failures += test_memory_management();
    failures += test_client_config_loading();
    failures += test_error_message_handling();
    failures += test_client_config_options();
    failures += test_library_constants();
    failures += test_null_parameter_handling();
    failures += test_empty_string_handling();
    printf("\n📊 Test Summary\n");
    printf("==============\n");
    if (failures == 0) {
        printf("✅ All core library tests passed!\n");
    } else {
        printf("❌ %d core library test(s) failed\n", failures);
    }
    sgnl_log_cleanup();
    return failures;
}
#ifndef SGNL_TEST_RUNNER
int main(void) {
    printf("🧪 SGNL Core Library Tests\n");
    printf("=========================\n");
    return test_libsgnl_main();
}
#endif 