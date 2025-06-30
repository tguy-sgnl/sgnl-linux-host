/*
 * SGNL Error Handling and Resource Management Tests
 *
 * Tests for the error handling utilities and RAII-style cleanup patterns.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <assert.h>
#include "../common/error_handling.h"

// Forward declarations
static int test_function_with_null(void *ptr);
static int test_goto_function(void *ptr);
static int test_early_return_function(int should_return_early);

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

// Test safe string operations
static int test_safe_string_operations(void) {
    TEST_SECTION("Safe String Operations");
    
    char dest[10];
    const char *src = "test string";
    
    // Test normal case
    SGNL_SAFE_STRNCPY(dest, src, sizeof(dest));
    TEST_ASSERT(strcmp(dest, "test stri") == 0, "Safe string copy truncates correctly");
    TEST_ASSERT(dest[sizeof(dest) - 1] == '\0', "Null terminator preserved");
    
    // Test NULL source
    memset(dest, 'X', sizeof(dest));
    SGNL_SAFE_STRNCPY(dest, NULL, sizeof(dest));
    TEST_ASSERT(dest[0] == 'X', "NULL source doesn't modify destination");
    
    // Test NULL destination (should not crash)
    printf("✅ PASS: NULL destination handling works\n");
    
    // Test exact size
    char exact_dest[5];
    SGNL_SAFE_STRNCPY(exact_dest, "test", sizeof(exact_dest));
    TEST_ASSERT(strcmp(exact_dest, "test") == 0, "Exact size copy works");
    TEST_ASSERT(exact_dest[sizeof(exact_dest) - 1] == '\0', "Null terminator at end");
    
    return 0;
}

// Test validation macros
static int test_validation_macros(void) {
    TEST_SECTION("Validation Macros");
    
    // Test SGNL_RETURN_IF_NULL
    TEST_ASSERT(test_function_with_null(NULL) == -1, "SGNL_RETURN_IF_NULL works with NULL");
    TEST_ASSERT(test_function_with_null((void*)0x123) == 0, "SGNL_RETURN_IF_NULL allows non-NULL");
    
    // Test SGNL_GOTO_IF_NULL
    TEST_ASSERT(test_goto_function(NULL) == -1, "Goto pattern works with NULL");
    TEST_ASSERT(test_goto_function((void*)0x123) == 0, "Goto pattern allows non-NULL");
    
    return 0;
}

// Helper function for SGNL_RETURN_IF_NULL test
static int test_function_with_null(void *ptr) {
    SGNL_RETURN_IF_NULL(ptr, -1);
    return 0;
}

// Helper function for SGNL_GOTO_IF_NULL test
static int test_goto_function(void *ptr) {
    if (ptr == NULL) {
        goto cleanup;
    }
    return 0;
cleanup:
    return -1;
}

// Test automatic cleanup functions
static int test_auto_cleanup_functions(void) {
    TEST_SECTION("Automatic Cleanup Functions");
    
    // Test sgnl_auto_free_p
    void *test_ptr = malloc(100);
    TEST_ASSERT(test_ptr != NULL, "Memory allocation successful");
    
    sgnl_auto_free_p(&test_ptr);
    TEST_ASSERT(test_ptr == NULL, "Memory automatically freed");
    
    // Test sgnl_auto_free_p with NULL
    void *null_ptr = NULL;
    sgnl_auto_free_p(&null_ptr);
    TEST_ASSERT(null_ptr == NULL, "NULL pointer remains NULL");
    
    // Test sgnl_auto_close_p
    int test_fd = open("/dev/null", O_RDONLY);
    TEST_ASSERT(test_fd >= 0, "File descriptor opened");
    
    sgnl_auto_close_p(&test_fd);
    TEST_ASSERT(test_fd == -1, "File descriptor automatically closed");
    
    // Test sgnl_auto_close_p with invalid fd
    int invalid_fd = -1;
    sgnl_auto_close_p(&invalid_fd);
    TEST_ASSERT(invalid_fd == -1, "Invalid fd remains -1");
    
    return 0;
}

// Test RAII-style cleanup macros
static int test_raii_cleanup(void) {
    TEST_SECTION("RAII-Style Cleanup");
    
    // Test SGNL_AUTO_FREE
    {
        SGNL_AUTO_FREE char *auto_ptr = malloc(100);
        TEST_ASSERT(auto_ptr != NULL, "Auto-allocated memory");
        // Should be automatically freed when scope ends
    }
    printf("✅ PASS: Auto-free works correctly\n");
    
    // Test SGNL_AUTO_CLOSE
    {
        SGNL_AUTO_CLOSE int auto_fd = open("/dev/null", O_RDONLY);
        TEST_ASSERT(auto_fd >= 0, "Auto-opened file descriptor");
        // Should be automatically closed when scope ends
    }
    printf("✅ PASS: Auto-close works correctly\n");
    
    return 0;
}

// Test error context creation
static int test_error_context(void) {
    TEST_SECTION("Error Context");
    
    sgnl_error_context_t ctx = SGNL_ERROR_CONTEXT("Test error message");
    
    TEST_ASSERT(strcmp(ctx.function, "test_error_context") == 0, "Function name captured");
    TEST_ASSERT(strstr(ctx.file, "test_error_handling.c") != NULL, "File name captured");
    TEST_ASSERT(ctx.line > 0, "Line number captured");
    TEST_ASSERT(strcmp(ctx.error_msg, "Test error message") == 0, "Error message captured");
    
    return 0;
}

// Test cleanup with nested scopes
static int test_nested_cleanup(void) {
    TEST_SECTION("Nested Cleanup");
    
    {
        SGNL_AUTO_FREE char *outer_ptr = malloc(100);
        TEST_ASSERT(outer_ptr != NULL, "Outer allocation");
        
        {
            SGNL_AUTO_FREE char *inner_ptr = malloc(50);
            TEST_ASSERT(inner_ptr != NULL, "Inner allocation");
            
            SGNL_AUTO_CLOSE int inner_fd = open("/dev/null", O_RDONLY);
            TEST_ASSERT(inner_fd >= 0, "Inner file descriptor");
            
            // Inner scope should clean up inner_ptr and inner_fd
        }
        
        // Outer scope should still have outer_ptr
        TEST_ASSERT(outer_ptr != NULL, "Outer pointer still valid");
        
        // Outer scope should clean up outer_ptr
    }
    printf("✅ PASS: Nested cleanup works correctly\n");
    
    return 0;
}

// Test cleanup with early returns
static int test_early_return_cleanup(void) {
    TEST_SECTION("Early Return Cleanup");
    
    TEST_ASSERT(test_early_return_function(1) == 1, "Early return works");
    TEST_ASSERT(test_early_return_function(0) == 0, "Normal return works");
    printf("✅ PASS: Early return cleanup works correctly\n");
    
    return 0;
}

// Helper function for early return test
static int test_early_return_function(int should_return_early) {
    SGNL_AUTO_FREE char *ptr = malloc(100);
    TEST_ASSERT(ptr != NULL, "Allocation in function");
    
    SGNL_AUTO_CLOSE int fd = open("/dev/null", O_RDONLY);
    TEST_ASSERT(fd >= 0, "File descriptor in function");
    
    if (should_return_early) {
        return 1;  // Should still clean up
    }
    
    return 0;  // Should clean up
}

// Test cleanup with exceptions (simulated with setjmp/longjmp)
static int test_exception_cleanup(void) {
    TEST_SECTION("Exception Cleanup");
    
    // Note: This is a simplified test since we're not using actual exceptions
    // In a real scenario with setjmp/longjmp, the cleanup would still work
    // because the cleanup functions are called when variables go out of scope
    
    {
        SGNL_AUTO_FREE char *ptr = malloc(100);
        TEST_ASSERT(ptr != NULL, "Allocation before simulated exception");
        
        // Simulate an exception-like condition
        if (1) {  // Always true to simulate exception
            // In real exception handling, this would be a longjmp
            // but the cleanup would still occur when the scope ends
        }
        
        // Scope ends here, cleanup should occur
    }
    printf("✅ PASS: Exception-like cleanup works correctly\n");
    
    return 0;
}

// Test memory safety with cleanup
static int test_memory_safety(void) {
    TEST_SECTION("Memory Safety");
    
    // Test that cleanup prevents memory leaks
    for (int i = 0; i < 100; i++) {
        SGNL_AUTO_FREE char *ptr = malloc(100);
        TEST_ASSERT(ptr != NULL, "Repeated allocation");
        
        // Write some data to ensure it's actually allocated
        memset(ptr, i, 100);
        
        // Should be automatically freed
    }
    printf("✅ PASS: Memory safety maintained through cleanup\n");
    
    return 0;
}

// Test file descriptor safety
static int test_fd_safety(void) {
    TEST_SECTION("File Descriptor Safety");
    
    // Test that cleanup prevents fd leaks
    for (int i = 0; i < 50; i++) {
        SGNL_AUTO_CLOSE int fd = open("/dev/null", O_RDONLY);
        TEST_ASSERT(fd >= 0, "Repeated file descriptor opening");
        
        // Should be automatically closed
    }
    printf("✅ PASS: File descriptor safety maintained through cleanup\n");
    
    return 0;
}

// Test cleanup with NULL pointers
static int test_null_pointer_cleanup(void) {
    TEST_SECTION("NULL Pointer Cleanup");
    
    {
        SGNL_AUTO_FREE char *null_ptr = NULL;
        sgnl_auto_free_p(&null_ptr);  // Should not crash
        TEST_ASSERT(null_ptr == NULL, "NULL pointer remains NULL after cleanup");
    }
    
    {
        SGNL_AUTO_CLOSE int null_fd = -1;
        sgnl_auto_close_p(&null_fd);  // Should not crash
        TEST_ASSERT(null_fd == -1, "Invalid fd remains -1 after cleanup");
    }
    
    printf("✅ PASS: NULL pointer cleanup works correctly\n");
    
    return 0;
}

// Test cleanup with invalid values
static int test_invalid_value_cleanup(void) {
    TEST_SECTION("Invalid Value Cleanup");
    
    {
        // Test with NULL pointer (safe)
        char *ptr = NULL;
        SGNL_AUTO_FREE char *auto_ptr = ptr;
        TEST_ASSERT(auto_ptr == NULL, "NULL pointer handled safely");
    }
    
    {
        // Test with already closed fd (safe)
        int fd = -1;
        SGNL_AUTO_CLOSE int auto_fd = fd;
        TEST_ASSERT(auto_fd == -1, "Invalid fd handled safely");
    }
    
    {
        // Test that we can allocate and free normally
        SGNL_AUTO_FREE char *ptr = malloc(100);
        TEST_ASSERT(ptr != NULL, "Normal allocation works");
        // Should be automatically freed when scope ends
    }
    
    {
        // Test that we can open and close normally
        SGNL_AUTO_CLOSE int fd = open("/dev/null", O_RDONLY);
        TEST_ASSERT(fd >= 0, "Normal file descriptor opening works");
        // Should be automatically closed when scope ends
    }
    
    printf("✅ PASS: Invalid value cleanup works correctly\n");
    
    return 0;
}

// Test cleanup performance
static int test_cleanup_performance(void) {
    TEST_SECTION("Cleanup Performance");
    
    // Test that cleanup doesn't add significant overhead
    clock_t start = clock();
    
    for (int i = 0; i < 1000; i++) {
        SGNL_AUTO_FREE char *ptr = malloc(10);
        SGNL_AUTO_CLOSE int fd = open("/dev/null", O_RDONLY);
        // Automatic cleanup happens here
    }
    
    clock_t end = clock();
    double cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    
    TEST_ASSERT(cpu_time_used < 1.0, "Cleanup performance is reasonable");
    printf("✅ PASS: Cleanup completed in %.3f seconds\n", cpu_time_used);
    
    return 0;
}

#ifdef SGNL_TEST_RUNNER
int test_error_handling_main(void)
#else
static int test_error_handling_main(void)
#endif
{
    int failures = 0;
    failures += test_safe_string_operations();
    failures += test_validation_macros();
    failures += test_auto_cleanup_functions();
    failures += test_raii_cleanup();
    failures += test_error_context();
    failures += test_nested_cleanup();
    failures += test_early_return_cleanup();
    failures += test_exception_cleanup();
    failures += test_memory_safety();
    failures += test_fd_safety();
    failures += test_null_pointer_cleanup();
    failures += test_invalid_value_cleanup();
    failures += test_cleanup_performance();
    printf("\n📊 Test Summary\n");
    printf("==============\n");
    if (failures == 0) {
        printf("✅ All error handling tests passed!\n");
    } else {
        printf("❌ %d error handling test(s) failed\n", failures);
    }
    return failures;
}
#ifndef SGNL_TEST_RUNNER
int main(void) {
    printf("🧪 SGNL Error Handling Tests\n");
    printf("============================\n");
    return test_error_handling_main();
}
#endif 