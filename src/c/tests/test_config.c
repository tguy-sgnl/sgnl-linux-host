/*
 * SGNL Configuration Management Tests
 *
 * Tests for the unified configuration system used by all SGNL modules.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
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

// Test configuration creation and destruction
static int test_config_lifecycle(void) {
    TEST_SECTION("Configuration Lifecycle");
    
    // Test creation
    sgnl_config_t *config = sgnl_config_create();
    TEST_ASSERT(config != NULL, "Configuration creation");
    TEST_ASSERT(!config->initialized, "Initial state is not initialized");
    
    // Test destruction
    sgnl_config_destroy(config);
    printf("✅ PASS: Configuration destruction\n");
    
    return 0;
}

// Test default configuration values
static int test_config_defaults(void) {
    TEST_SECTION("Default Configuration Values");
    
    sgnl_config_t *config = sgnl_config_create();
    TEST_ASSERT(config != NULL, "Configuration creation");
    
    // Set defaults
    sgnl_config_set_defaults(config, "test-module");
    
    // Verify HTTP defaults
    TEST_ASSERT(config->http.timeout_seconds == 10, "Default timeout");
    TEST_ASSERT(config->http.connect_timeout_seconds == 3, "Default connect timeout");
    TEST_ASSERT(config->http.ssl_verify_peer == true, "Default SSL verify peer");
    TEST_ASSERT(config->http.ssl_verify_host == true, "Default SSL verify host");
    TEST_ASSERT(strcmp(config->http.user_agent, "SGNL-Client/1.0") == 0, "Default user agent");
    
    // Verify logging defaults
    TEST_ASSERT(config->logging.debug_mode == false, "Default debug mode");
    TEST_ASSERT(strcmp(config->logging.log_level, "info") == 0, "Default log level");
    
    // Verify sudo defaults
    TEST_ASSERT(config->sudo.access_msg == true, "Default access message");
    TEST_ASSERT(strcmp(config->sudo.command_attribute, "id") == 0, "Default command attribute");
    
    sgnl_config_destroy(config);
    return 0;
}

// Test configuration loading from file
static int test_config_loading(void) {
    TEST_SECTION("Configuration Loading");
    
    // Use static test config file
    const char *test_config_file = "tests/test_config.json";
    
    sgnl_config_t *config = sgnl_config_create();
    TEST_ASSERT(config != NULL, "Configuration creation");
    
    // Test loading with options
    sgnl_config_options_t options = {
        .config_path = test_config_file,
        .strict_validation = true,
        .module_name = "test-module"
    };
    
    sgnl_config_result_t result = sgnl_config_load(config, &options);
    TEST_ASSERT(result == SGNL_CONFIG_OK, "Configuration loading success");
    TEST_ASSERT(config->initialized, "Configuration marked as initialized");
    
    // Verify loaded values
    TEST_ASSERT(strcmp(config->api_url, "https://sgnlapis.cloud") == 0, "API URL loaded");
    TEST_ASSERT(strcmp(config->api_token, "test-token-12345") == 0, "API token loaded");
    TEST_ASSERT(strcmp(config->tenant, "test-tenant") == 0, "Tenant loaded");
    TEST_ASSERT(config->http.timeout_seconds == 15, "HTTP timeout loaded");
    TEST_ASSERT(config->http.connect_timeout_seconds == 5, "HTTP connect timeout loaded");
    TEST_ASSERT(strcmp(config->http.user_agent, "SGNL-Test/1.0") == 0, "User agent loaded");
    TEST_ASSERT(config->sudo.access_msg == true, "Sudo access message loaded");
    TEST_ASSERT(strcmp(config->sudo.command_attribute, "name") == 0, "Command attribute loaded");
    TEST_ASSERT(config->logging.debug_mode == true, "Debug mode loaded");
    TEST_ASSERT(strcmp(config->logging.log_level, "debug") == 0, "Log level loaded");
    
    sgnl_config_destroy(config);
    
    return 0;
}

// Test configuration validation
static int test_config_validation(void) {
    TEST_SECTION("Configuration Validation");
    
    sgnl_config_t *config = sgnl_config_create();
    TEST_ASSERT(config != NULL, "Configuration creation");
    
    // Test empty configuration (should fail)
    sgnl_config_result_t result = sgnl_config_validate(config);
    TEST_ASSERT(result == SGNL_CONFIG_MISSING_REQUIRED, "Empty config validation fails");
    
    // Test minimal valid configuration
    sgnl_config_set_defaults(config, "test-module");  // Set defaults first
    strcpy(config->api_url, "https://sgnlapis.cloud");
    strcpy(config->api_token, "test-token");
    result = sgnl_config_validate(config);
    TEST_ASSERT(result == SGNL_CONFIG_OK, "Minimal config validation passes");
    
    // Test invalid timeout values
    config->http.timeout_seconds = 0;
    result = sgnl_config_validate(config);
    TEST_ASSERT(result == SGNL_CONFIG_INVALID_VALUE, "Invalid timeout validation fails");
    
    config->http.timeout_seconds = 301;
    result = sgnl_config_validate(config);
    TEST_ASSERT(result == SGNL_CONFIG_INVALID_VALUE, "Too large timeout validation fails");
    
    // Test invalid connect timeout
    config->http.timeout_seconds = 30;
    config->http.connect_timeout_seconds = 0;
    result = sgnl_config_validate(config);
    TEST_ASSERT(result == SGNL_CONFIG_INVALID_VALUE, "Invalid connect timeout validation fails");
    
    sgnl_config_destroy(config);
    return 0;
}

// Test accessor functions
static int test_config_accessors(void) {
    TEST_SECTION("Configuration Accessors");
    
    sgnl_config_t *config = sgnl_config_create();
    TEST_ASSERT(config != NULL, "Configuration creation");
    
    // Set some values
    strcpy(config->api_url, "https://sgnlapis.cloud");
    strcpy(config->api_token, "test-token-12345");
    strcpy(config->tenant, "test-tenant");
    strcpy(config->sudo.command_attribute, "name");
    config->sudo.access_msg = true;
    strcpy(config->http.user_agent, "SGNL-Test/1.0");
    config->http.timeout_seconds = 25;
    config->http.connect_timeout_seconds = 8;
    config->logging.debug_mode = true;
    
    // Test accessors
    TEST_ASSERT(strcmp(sgnl_config_get_api_url(config), "https://sgnlapis.cloud") == 0, "API URL accessor");
    TEST_ASSERT(strcmp(sgnl_config_get_api_token(config), "test-token-12345") == 0, "API token accessor");
    TEST_ASSERT(strcmp(sgnl_config_get_tenant(config), "test-tenant") == 0, "Tenant accessor");
    TEST_ASSERT(strcmp(sgnl_config_get_sudo_command_attribute(config), "name") == 0, "Command attribute accessor");
    TEST_ASSERT(sgnl_config_get_sudo_access_msg(config) == true, "Access message accessor");
    TEST_ASSERT(strcmp(sgnl_config_get_user_agent(config), "SGNL-Test/1.0") == 0, "User agent accessor");
    TEST_ASSERT(sgnl_config_get_timeout(config) == 25, "Timeout accessor");
    TEST_ASSERT(sgnl_config_get_connect_timeout(config) == 8, "Connect timeout accessor");
    
    // Test convenience functions
    TEST_ASSERT(sgnl_config_is_debug_enabled(config) == true, "Debug enabled check");
    
    sgnl_config_destroy(config);
    return 0;
}

// Test error handling
static int test_config_errors(void) {
    TEST_SECTION("Configuration Error Handling");
    
    // Test NULL configuration
    sgnl_config_result_t result = sgnl_config_validate(NULL);
    TEST_ASSERT(result == SGNL_CONFIG_MEMORY_ERROR, "NULL config validation");
    
    // Test file not found
    sgnl_config_t *config = sgnl_config_create();
    TEST_ASSERT(config != NULL, "Configuration creation");
    
    sgnl_config_options_t options = {
        .config_path = "/nonexistent/file.json",
        .strict_validation = true,
        .module_name = "test-module"
    };
    
    result = sgnl_config_load(config, &options);
    TEST_ASSERT(result == SGNL_CONFIG_FILE_NOT_FOUND, "File not found error");
    TEST_ASSERT(strlen(sgnl_config_get_last_error(config)) > 0, "Error message set");
    
    sgnl_config_destroy(config);
    
    return 0;
}

// Test result code conversion
static int test_result_codes(void) {
    TEST_SECTION("Result Code Conversion");
    
    TEST_ASSERT(strcmp(sgnl_config_result_to_string(SGNL_CONFIG_OK), "Success") == 0, "OK result string");
    TEST_ASSERT(strcmp(sgnl_config_result_to_string(SGNL_CONFIG_FILE_NOT_FOUND), "Configuration file not found") == 0, "File not found result string");
    TEST_ASSERT(strcmp(sgnl_config_result_to_string(SGNL_CONFIG_INVALID_JSON), "Invalid JSON in configuration file") == 0, "Invalid JSON result string");
    TEST_ASSERT(strcmp(sgnl_config_result_to_string(SGNL_CONFIG_MISSING_REQUIRED), "Missing required configuration field") == 0, "Missing required result string");
    TEST_ASSERT(strcmp(sgnl_config_result_to_string(SGNL_CONFIG_INVALID_VALUE), "Invalid configuration value") == 0, "Invalid value result string");
    TEST_ASSERT(strcmp(sgnl_config_result_to_string(SGNL_CONFIG_MEMORY_ERROR), "Memory allocation error") == 0, "Memory error result string");
    
    return 0;
}

// Test non-strict validation
static int test_non_strict_validation(void) {
    TEST_SECTION("Non-Strict Validation");
    
    sgnl_config_t *config = sgnl_config_create();
    TEST_ASSERT(config != NULL, "Configuration creation");
    
    // Test loading with non-strict validation
    sgnl_config_options_t options = {
        .config_path = "tests/test_config.json",
        .strict_validation = false,  // Allow missing optional fields
        .module_name = "test-module"
    };
    
    sgnl_config_result_t result = sgnl_config_load(config, &options);
    TEST_ASSERT(result == SGNL_CONFIG_OK, "Non-strict validation allows missing fields");
    TEST_ASSERT(config->initialized, "Configuration marked as initialized");
    
    sgnl_config_destroy(config);
    
    return 0;
}

#ifdef SGNL_TEST_RUNNER
int test_config_main(void)
#else
static int test_config_main(void)
#endif
{
    int failures = 0;
    failures += test_config_lifecycle();
    failures += test_config_defaults();
    failures += test_config_loading();
    failures += test_config_validation();
    failures += test_config_accessors();
    failures += test_config_errors();
    failures += test_result_codes();
    failures += test_non_strict_validation();
    printf("\n📊 Test Summary\n");
    printf("==============\n");
    if (failures == 0) {
        printf("✅ All configuration tests passed!\n");
    } else {
        printf("❌ %d configuration test(s) failed\n", failures);
    }
    return failures;
}
#ifndef SGNL_TEST_RUNNER
int main(void) {
    printf("🧪 SGNL Configuration Management Tests\n");
    printf("====================================\n");
    return test_config_main();
}
#endif 