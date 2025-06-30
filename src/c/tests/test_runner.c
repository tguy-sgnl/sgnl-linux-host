/*
 * SGNL Test Runner
 *
 * Comprehensive test runner for all SGNL C library components.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>
#include <stdbool.h>
#include "test_suites.h"

// Test suite structure
typedef struct {
    const char *name;
    const char *description;
    int (*test_function)(void);
} test_suite_t;

// Test suites
static test_suite_t test_suites[] = {
    {
        .name = "config",
        .description = "Configuration Management Tests",
        .test_function = test_config_main
    },
    {
        .name = "logging", 
        .description = "Logging System Tests",
        .test_function = test_logging_main
    },
    {
        .name = "error_handling",
        .description = "Error Handling Tests", 
        .test_function = test_error_handling_main
    },
    {
        .name = "libsgnl",
        .description = "Core Library Tests",
        .test_function = test_libsgnl_main
    }
};

static const int test_suite_count = sizeof(test_suites) / sizeof(test_suites[0]);

// Test results
typedef struct {
    const char *name;
    int exit_code;
    double duration;
    bool passed;
} test_result_t;

// Global test results
static test_result_t test_results[10];
static int test_result_count = 0;

// Test utilities
static void print_header(const char *title) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║ %-60s ║\n", title);
    printf("╚══════════════════════════════════════════════════════════════╝\n");
}

static void print_separator(void) {
    printf("──────────────────────────────────────────────────────────────────\n");
}

static void print_test_result(const test_result_t *result) {
    const char *status = result->passed ? "✅ PASS" : "❌ FAIL";
    printf("  %-20s %-8s %8.3fs\n", result->name, status, result->duration);
}

// Run a single test suite
static test_result_t run_test_suite(const test_suite_t *suite) {
    test_result_t result = {
        .name = suite->name,
        .exit_code = 0,
        .duration = 0.0,
        .passed = false
    };
    
    printf("\n🧪 Running %s tests...\n", suite->name);
    printf("   %s\n", suite->description);
    
    clock_t start = clock();
    
    // Run the test function
    result.exit_code = suite->test_function();
    
    clock_t end = clock();
    result.duration = ((double)(end - start)) / CLOCKS_PER_SEC;
    result.passed = (result.exit_code == 0);
    
    return result;
}

// Run all test suites
static int run_all_tests(void) {
    print_header("SGNL C Library Test Suite");
    printf("Running %d test suites...\n", test_suite_count);
    
    int total_failures = 0;
    int passed_tests = 0;
    
    for (int i = 0; i < test_suite_count; i++) {
        test_results[test_result_count] = run_test_suite(&test_suites[i]);
        
        if (test_results[test_result_count].passed) {
            passed_tests++;
        } else {
            total_failures++;
        }
        
        test_result_count++;
    }
    
    // Print summary
    print_header("Test Results Summary");
    printf("Suite                Status   Duration\n");
    print_separator();
    
    for (int i = 0; i < test_result_count; i++) {
        print_test_result(&test_results[i]);
    }
    
    print_separator();
    printf("Total: %d/%d test suites passed\n", passed_tests, test_suite_count);
    
    if (total_failures == 0) {
        printf("🎉 All tests passed successfully!\n");
    } else {
        printf("⚠️  %d test suite(s) failed\n", total_failures);
    }
    
    return total_failures;
}

// Run a specific test suite
static int run_specific_test(const char *test_name) {
    for (int i = 0; i < test_suite_count; i++) {
        if (strcmp(test_suites[i].name, test_name) == 0) {
            test_result_t result = run_test_suite(&test_suites[i]);
            return result.exit_code;
        }
    }
    
    printf("❌ Unknown test suite: %s\n", test_name);
    printf("Available test suites:\n");
    for (int i = 0; i < test_suite_count; i++) {
        printf("  - %s: %s\n", test_suites[i].name, test_suites[i].description);
    }
    return 1;
}

// List available test suites
static void list_test_suites(void) {
    print_header("Available Test Suites");
    printf("Name                 Description\n");
    print_separator();
    
    for (int i = 0; i < test_suite_count; i++) {
        printf("%-20s %s\n", test_suites[i].name, test_suites[i].description);
    }
    
    printf("\nUsage:\n");
    printf("  %s                    # Run all tests\n", "test_runner");
    printf("  %s <suite_name>       # Run specific test suite\n", "test_runner");
    printf("  %s --list             # List available test suites\n", "test_runner");
    printf("  %s --help             # Show this help\n", "test_runner");
}

// Show help
static void show_help(void) {
    print_header("SGNL Test Runner Help");
    printf("The SGNL test runner executes comprehensive tests for the SGNL C library.\n\n");
    
    printf("Test Suites:\n");
    for (int i = 0; i < test_suite_count; i++) {
        printf("  %-20s %s\n", test_suites[i].name, test_suites[i].description);
    }
    
    printf("\nUsage:\n");
    printf("  %s                    # Run all test suites\n", "test_runner");
    printf("  %s <suite_name>       # Run a specific test suite\n", "test_runner");
    printf("  %s --list             # List available test suites\n", "test_runner");
    printf("  %s --help             # Show this help message\n", "test_runner");
    
    printf("\nExamples:\n");
    printf("  %s config             # Run only configuration tests\n", "test_runner");
    printf("  %s logging            # Run only logging tests\n", "test_runner");
    printf("  %s error_handling     # Run only error handling tests\n", "test_runner");
    printf("  %s libsgnl            # Run only core library tests\n", "test_runner");
    
    printf("\nExit Codes:\n");
    printf("  0                    # All tests passed\n");
    printf("  1                    # One or more tests failed\n");
    printf("  2                    # Invalid arguments or test not found\n");
}

// Check if running in CI environment
static bool is_ci_environment(void) {
    return getenv("CI") != NULL || 
           getenv("GITHUB_ACTIONS") != NULL || 
           getenv("TRAVIS") != NULL ||
           getenv("CIRCLECI") != NULL;
}

// Main function
int main(int argc, char *argv[]) {
    // Set up output for CI if needed
    if (is_ci_environment()) {
        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);
    }
    
    // Parse command line arguments
    if (argc == 1) {
        // No arguments - run all tests
        return run_all_tests();
    } else if (argc == 2) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            show_help();
            return 0;
        } else if (strcmp(argv[1], "--list") == 0 || strcmp(argv[1], "-l") == 0) {
            list_test_suites();
            return 0;
        } else {
            // Run specific test suite
            return run_specific_test(argv[1]);
        }
    } else {
        printf("❌ Too many arguments\n");
        printf("Use --help for usage information\n");
        return 2;
    }
} 