/*
 * SGNL Logging System Tests
 *
 * Tests for the unified logging system used by all SGNL modules.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
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

// Capture stdout for testing
static char captured_output[4096];
static size_t captured_size = 0;
static int original_stdout = -1;
static int pipe_read_fd = -1;
static int pipe_write_fd = -1;

static void capture_stdout(void) {
    captured_size = 0;
    memset(captured_output, 0, sizeof(captured_output));
    
    // Create pipe
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        printf("❌ FAIL: Could not create pipe for stdout capture\n");
        return;
    }
    
    pipe_read_fd = pipefd[0];
    pipe_write_fd = pipefd[1];
    
    // Save original stdout and redirect
    original_stdout = dup(STDOUT_FILENO);
    if (dup2(pipe_write_fd, STDOUT_FILENO) == -1) {
        printf("❌ FAIL: Could not redirect stdout\n");
        return;
    }
    
    // Close write end in parent process
    close(pipe_write_fd);
    pipe_write_fd = -1;
}

static void restore_stdout(void) {
    if (original_stdout != -1) {
        // Flush any buffered output
        fflush(stdout);
        
        // Restore original stdout
        dup2(original_stdout, STDOUT_FILENO);
        close(original_stdout);
        original_stdout = -1;
        
        // Read captured output
        if (pipe_read_fd != -1) {
            captured_size = read(pipe_read_fd, captured_output, sizeof(captured_output) - 1);
            close(pipe_read_fd);
            pipe_read_fd = -1;
            
            if (captured_size > 0) {
                captured_output[captured_size] = '\0';
            }
        }
    }
}

// Test logging initialization and cleanup
static int test_logging_lifecycle(void) {
    TEST_SECTION("Logging Lifecycle");
    
    // Test default initialization
    sgnl_log_init(NULL);
    TEST_ASSERT(sgnl_logger_config.min_level == SGNL_LOG_INFO, "Default log level");
    TEST_ASSERT(sgnl_logger_config.use_syslog == false, "Default syslog setting");
    TEST_ASSERT(sgnl_logger_config.structured_format == false, "Default structured format");
    
    // Test custom initialization
    sgnl_logger_config_t custom_config = {
        .min_level = SGNL_LOG_DEBUG,
        .use_syslog = true,
        .structured_format = true,
        .include_timestamp = true,
        .include_pid = true,
        .facility = "local1"
    };
    
    sgnl_log_init(&custom_config);
    TEST_ASSERT(sgnl_logger_config.min_level == SGNL_LOG_DEBUG, "Custom log level");
    TEST_ASSERT(sgnl_logger_config.use_syslog == true, "Custom syslog setting");
    TEST_ASSERT(sgnl_logger_config.structured_format == true, "Custom structured format");
    TEST_ASSERT(sgnl_logger_config.include_timestamp == true, "Custom timestamp setting");
    TEST_ASSERT(sgnl_logger_config.include_pid == true, "Custom PID setting");
    TEST_ASSERT(strcmp(sgnl_logger_config.facility, "local1") == 0, "Custom facility");
    
    // Test cleanup
    sgnl_log_cleanup();
    printf("✅ PASS: Logging cleanup\n");
    
    return 0;
}

// Test log level conversion
static int test_log_level_conversion(void) {
    TEST_SECTION("Log Level Conversion");
    
    // Test string to level conversion
    TEST_ASSERT(sgnl_log_level_from_string("debug") == SGNL_LOG_DEBUG, "Debug level from string");
    TEST_ASSERT(sgnl_log_level_from_string("info") == SGNL_LOG_INFO, "Info level from string");
    TEST_ASSERT(sgnl_log_level_from_string("notice") == SGNL_LOG_NOTICE, "Notice level from string");
    TEST_ASSERT(sgnl_log_level_from_string("warning") == SGNL_LOG_WARNING, "Warning level from string");
    TEST_ASSERT(sgnl_log_level_from_string("warn") == SGNL_LOG_WARNING, "Warn level from string");
    TEST_ASSERT(sgnl_log_level_from_string("error") == SGNL_LOG_ERROR, "Error level from string");
    TEST_ASSERT(sgnl_log_level_from_string("critical") == SGNL_LOG_CRITICAL, "Critical level from string");
    TEST_ASSERT(sgnl_log_level_from_string("alert") == SGNL_LOG_ALERT, "Alert level from string");
    TEST_ASSERT(sgnl_log_level_from_string("emergency") == SGNL_LOG_EMERGENCY, "Emergency level from string");
    
    // Test invalid strings
    TEST_ASSERT(sgnl_log_level_from_string("invalid") == SGNL_LOG_INFO, "Invalid level defaults to info");
    TEST_ASSERT(sgnl_log_level_from_string(NULL) == SGNL_LOG_INFO, "NULL level defaults to info");
    
    // Test level to string conversion
    TEST_ASSERT(strcmp(sgnl_log_level_to_string(SGNL_LOG_DEBUG), "DEBUG") == 0, "Debug level to string");
    TEST_ASSERT(strcmp(sgnl_log_level_to_string(SGNL_LOG_INFO), "INFO") == 0, "Info level to string");
    TEST_ASSERT(strcmp(sgnl_log_level_to_string(SGNL_LOG_NOTICE), "NOTICE") == 0, "Notice level to string");
    TEST_ASSERT(strcmp(sgnl_log_level_to_string(SGNL_LOG_WARNING), "WARNING") == 0, "Warning level to string");
    TEST_ASSERT(strcmp(sgnl_log_level_to_string(SGNL_LOG_ERROR), "ERROR") == 0, "Error level to string");
    TEST_ASSERT(strcmp(sgnl_log_level_to_string(SGNL_LOG_CRITICAL), "CRITICAL") == 0, "Critical level to string");
    TEST_ASSERT(strcmp(sgnl_log_level_to_string(SGNL_LOG_ALERT), "ALERT") == 0, "Alert level to string");
    TEST_ASSERT(strcmp(sgnl_log_level_to_string(SGNL_LOG_EMERGENCY), "EMERGENCY") == 0, "Emergency level to string");
    
    return 0;
}

// Test log level filtering
static int test_log_level_filtering(void) {
    TEST_SECTION("Log Level Filtering");
    
    // Set up logging with debug level
    sgnl_logger_config_t config = {
        .min_level = SGNL_LOG_DEBUG,
        .use_syslog = false,
        .structured_format = false,
        .include_timestamp = false,
        .include_pid = false,
        .facility = "local0"
    };
    sgnl_log_init(&config);
    
    // Test level enabled checks
    TEST_ASSERT(sgnl_log_level_enabled(SGNL_LOG_DEBUG) == true, "Debug level enabled");
    TEST_ASSERT(sgnl_log_level_enabled(SGNL_LOG_INFO) == true, "Info level enabled");
    TEST_ASSERT(sgnl_log_level_enabled(SGNL_LOG_WARNING) == true, "Warning level enabled");
    TEST_ASSERT(sgnl_log_level_enabled(SGNL_LOG_ERROR) == true, "Error level enabled");
    
    // Change to warning level
    config.min_level = SGNL_LOG_WARNING;
    sgnl_log_init(&config);
    
    TEST_ASSERT(sgnl_log_level_enabled(SGNL_LOG_DEBUG) == false, "Debug level disabled");
    TEST_ASSERT(sgnl_log_level_enabled(SGNL_LOG_INFO) == false, "Info level disabled");
    TEST_ASSERT(sgnl_log_level_enabled(SGNL_LOG_WARNING) == true, "Warning level enabled");
    TEST_ASSERT(sgnl_log_level_enabled(SGNL_LOG_ERROR) == true, "Error level enabled");
    
    return 0;
}

// Test basic logging without context
static int test_basic_logging(void) {
    TEST_SECTION("Basic Logging");
    
    // Set up logging
    sgnl_logger_config_t config = {
        .min_level = SGNL_LOG_DEBUG,
        .use_syslog = false,
        .structured_format = false,
        .include_timestamp = false,
        .include_pid = false,
        .facility = "local0"
    };
    sgnl_log_init(&config);
    
    // Capture output
    capture_stdout();
    
    // Test logging with context
    sgnl_log_context_t ctx = SGNL_LOG_CONTEXT("test");
    sgnl_log_with_context(SGNL_LOG_INFO, &ctx, "Test message");
    
    restore_stdout();
    
    // Check output contains expected content
    TEST_ASSERT(strstr(captured_output, "[test]") != NULL, "Component name in output");
    TEST_ASSERT(strstr(captured_output, "Test message") != NULL, "Message in output");
    
    return 0;
}

// Test logging with different levels
static int test_logging_levels(void) {
    TEST_SECTION("Logging Levels");
    
    // Set up logging to capture all levels
    sgnl_logger_config_t config = {
        .min_level = SGNL_LOG_DEBUG,
        .use_syslog = false,
        .structured_format = false,
        .include_timestamp = false,
        .include_pid = false,
        .facility = "local0"
    };
    sgnl_log_init(&config);
    
    sgnl_log_context_t ctx = SGNL_LOG_CONTEXT("test");
    
    // Test each level
    capture_stdout();
    SGNL_LOG_DEBUG(&ctx, "Debug message");
    restore_stdout();
    TEST_ASSERT(strstr(captured_output, "Debug message") != NULL, "Debug message logged");
    
    capture_stdout();
    SGNL_LOG_INFO(&ctx, "Info message");
    restore_stdout();
    TEST_ASSERT(strstr(captured_output, "Info message") != NULL, "Info message logged");
    
    capture_stdout();
    SGNL_LOG_WARNING(&ctx, "Warning message");
    restore_stdout();
    TEST_ASSERT(strstr(captured_output, "Warning message") != NULL, "Warning message logged");
    
    capture_stdout();
    SGNL_LOG_ERROR(&ctx, "Error message");
    restore_stdout();
    TEST_ASSERT(strstr(captured_output, "Error message") != NULL, "Error message logged");
    
    return 0;
}

// Test logging with NULL context
static int test_null_context_logging(void) {
    TEST_SECTION("Null Context Logging");
    
    sgnl_logger_config_t config = {
        .min_level = SGNL_LOG_INFO,
        .use_syslog = false,
        .structured_format = false,
        .include_timestamp = false,
        .include_pid = false,
        .facility = "local0"
    };
    sgnl_log_init(&config);
    
    capture_stdout();
    sgnl_log_with_context(SGNL_LOG_INFO, NULL, "Message without context");
    restore_stdout();
    
    TEST_ASSERT(strstr(captured_output, "[SGNL]") != NULL, "Default component name used");
    TEST_ASSERT(strstr(captured_output, "Message without context") != NULL, "Message logged");
    
    return 0;
}

// Test logging with NULL format
static int test_null_format_logging(void) {
    TEST_SECTION("Null Format Logging");
    
    sgnl_logger_config_t config = {
        .min_level = SGNL_LOG_INFO,
        .use_syslog = false,
        .structured_format = false,
        .include_timestamp = false,
        .include_pid = false,
        .facility = "local0"
    };
    sgnl_log_init(&config);
    
    sgnl_log_context_t ctx = SGNL_LOG_CONTEXT("test");
    
    capture_stdout();
    sgnl_log_with_context(SGNL_LOG_INFO, &ctx, NULL);
    restore_stdout();
    
    TEST_ASSERT(strstr(captured_output, "Log message") != NULL, "Default message used");
    
    return 0;
}

// Test logging with empty format
static int test_empty_format_logging(void) {
    TEST_SECTION("Empty Format Logging");
    
    sgnl_logger_config_t config = {
        .min_level = SGNL_LOG_INFO,
        .use_syslog = false,
        .structured_format = false,
        .include_timestamp = false,
        .include_pid = false,
        .facility = "local0"
    };
    sgnl_log_init(&config);
    
    sgnl_log_context_t ctx = SGNL_LOG_CONTEXT("test");
    
    capture_stdout();
    sgnl_log_with_context(SGNL_LOG_INFO, &ctx, "");
    restore_stdout();
    
    TEST_ASSERT(strstr(captured_output, "Log message") != NULL, "Default message used for empty format");
    
    return 0;
}

// Test logging level filtering
static int test_logging_filtering(void) {
    TEST_SECTION("Logging Level Filtering");
    
    // Set up logging to only show warnings and above
    sgnl_logger_config_t config = {
        .min_level = SGNL_LOG_WARNING,
        .use_syslog = false,
        .structured_format = false,
        .include_timestamp = false,
        .include_pid = false,
        .facility = "local0"
    };
    sgnl_log_init(&config);
    
    sgnl_log_context_t ctx = SGNL_LOG_CONTEXT("test");
    
    // Debug and info should be filtered out
    capture_stdout();
    SGNL_LOG_DEBUG(&ctx, "This should not appear");
    SGNL_LOG_INFO(&ctx, "This should not appear either");
    restore_stdout();
    
    TEST_ASSERT(strstr(captured_output, "This should not appear") == NULL, "Debug message filtered out");
    TEST_ASSERT(strstr(captured_output, "This should not appear either") == NULL, "Info message filtered out");
    
    // Warning and error should appear
    capture_stdout();
    SGNL_LOG_WARNING(&ctx, "This should appear");
    SGNL_LOG_ERROR(&ctx, "This should also appear");
    restore_stdout();
    
    TEST_ASSERT(strstr(captured_output, "This should appear") != NULL, "Warning message not filtered");
    TEST_ASSERT(strstr(captured_output, "This should also appear") != NULL, "Error message not filtered");
    
    return 0;
}

// Test secure logging macro
static int test_secure_logging(void) {
    TEST_SECTION("Secure Logging");
    
    // Set up logging with debug disabled
    sgnl_logger_config_t config = {
        .min_level = SGNL_LOG_INFO,
        .use_syslog = false,
        .structured_format = false,
        .include_timestamp = false,
        .include_pid = false,
        .facility = "local0"
    };
    sgnl_log_init(&config);
    
    sgnl_log_context_t ctx = SGNL_LOG_CONTEXT("test");
    
    // Secure debug should not appear when debug is disabled
    capture_stdout();
    SGNL_LOG_SECURE_DEBUG(&ctx, "Sensitive debug info");
    restore_stdout();
    
    TEST_ASSERT(strstr(captured_output, "Sensitive debug info") == NULL, "Secure debug filtered when debug disabled");
    
    // Enable debug
    config.min_level = SGNL_LOG_DEBUG;
    sgnl_log_init(&config);
    
    // Secure debug should appear when debug is enabled
    capture_stdout();
    SGNL_LOG_SECURE_DEBUG(&ctx, "Sensitive debug info");
    restore_stdout();
    
    TEST_ASSERT(strstr(captured_output, "Sensitive debug info") != NULL, "Secure debug appears when debug enabled");
    
    return 0;
}

// Test request tracking stubs
static int test_request_tracking(void) {
    TEST_SECTION("Request Tracking");
    
    // These are stubs, so we just test they don't crash
    sgnl_request_tracker_t *tracker = sgnl_request_start("test-user", "test-asset", "test-action");
    TEST_ASSERT(tracker == NULL, "Request tracker stub returns NULL");
    
    sgnl_request_end(tracker, "test-result");
    printf("✅ PASS: Request tracking stubs don't crash\n");
    
    return 0;
}

// Test logging with variadic arguments
static int test_variadic_logging(void) {
    TEST_SECTION("Variadic Logging");
    
    sgnl_logger_config_t config = {
        .min_level = SGNL_LOG_INFO,
        .use_syslog = false,
        .structured_format = false,
        .include_timestamp = false,
        .include_pid = false,
        .facility = "local0"
    };
    sgnl_log_init(&config);
    
    sgnl_log_context_t ctx = SGNL_LOG_CONTEXT("test");
    
    capture_stdout();
    sgnl_log_with_context(SGNL_LOG_INFO, &ctx, "Formatted message: %s, %d", "test", 42);
    restore_stdout();
    
    TEST_ASSERT(strstr(captured_output, "Formatted message: test, 42") != NULL, "Variadic logging works");
    
    return 0;
}

// Test logging context macro
static int test_logging_context_macro(void) {
    TEST_SECTION("Logging Context Macro");
    
    sgnl_logger_config_t config = {
        .min_level = SGNL_LOG_INFO,
        .use_syslog = false,
        .structured_format = false,
        .include_timestamp = false,
        .include_pid = false,
        .facility = "local0"
    };
    sgnl_log_init(&config);
    
    // Test the SGNL_LOG_CONTEXT macro
    sgnl_log_context_t ctx = SGNL_LOG_CONTEXT("test-component");
    TEST_ASSERT(strcmp(ctx.component, "test-component") == 0, "Component set correctly");
    TEST_ASSERT(strcmp(ctx.function, "test_logging_context_macro") == 0, "Function set correctly");
    
    capture_stdout();
    sgnl_log_with_context(SGNL_LOG_INFO, &ctx, "Test message");
    restore_stdout();
    
    TEST_ASSERT(strstr(captured_output, "[test-component]") != NULL, "Component name in output");
    
    return 0;
}

#ifdef SGNL_TEST_RUNNER
int test_logging_main(void)
#else
static int test_logging_main(void)
#endif
{
    printf("🧪 SGNL Logging System Tests\n");
    printf("===========================\n");
    
    int failures = 0;
    
    failures += test_logging_lifecycle();
    failures += test_log_level_conversion();
    failures += test_log_level_filtering();
    failures += test_basic_logging();
    failures += test_logging_levels();
    failures += test_null_context_logging();
    failures += test_null_format_logging();
    failures += test_empty_format_logging();
    failures += test_logging_filtering();
    failures += test_secure_logging();
    failures += test_request_tracking();
    failures += test_variadic_logging();
    failures += test_logging_context_macro();
    
    printf("\n📊 Test Summary\n");
    printf("==============\n");
    if (failures == 0) {
        printf("✅ All logging tests passed!\n");
    } else {
        printf("❌ %d logging test(s) failed\n", failures);
    }
    
    return failures;
}

#ifndef SGNL_TEST_RUNNER
int main(void) {
    return test_logging_main();
}
#endif 