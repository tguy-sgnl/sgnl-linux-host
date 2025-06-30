# SGNL C Library Tests

This directory contains comprehensive tests for the SGNL C library components.

## Test Structure

The test suite is organized into the following test files:

### Core Test Files

- **`test_config.c`** - Configuration management tests
  - Tests JSON configuration loading and parsing
  - Tests configuration validation
  - Tests environment variable overrides
  - Tests error handling for invalid configurations
  - Tests accessor functions and convenience methods

- **`test_logging.c`** - Logging system tests
  - Tests log level filtering and conversion
  - Tests logging with different contexts
  - Tests secure logging macros
  - Tests variadic logging functions
  - Tests logging configuration options

- **`test_error_handling.c`** - Error handling and resource management tests
  - Tests RAII-style cleanup macros
  - Tests safe string operations
  - Tests automatic resource cleanup
  - Tests memory safety and leak prevention
  - Tests file descriptor safety

- **`test_libsgnl.c`** - Core library functionality tests
  - Tests client lifecycle management
  - Tests access evaluation functions
  - Tests asset search functionality
  - Tests validation functions
  - Tests memory management and cleanup

### Test Runner

- **`test_runner.c`** - Unified test runner
  - Runs all test suites
  - Provides individual test suite execution
  - Generates test reports and summaries
  - Supports CI/CD integration

## Building and Running Tests

### Prerequisites

- GCC or Clang compiler
- Make build system
- json-c library (for configuration tests)
- Standard C library

### Building Tests

From the `src/c/` directory:

```bash
# Build all tests
make test

# Build individual test files
make test-config
make test-logging  
make test-error-handling
make test-libsgnl
```

### Running Tests

#### Run All Tests
```bash
# From src/c/ directory
make test

# Or directly
./tests/test_runner
```

#### Run Individual Test Suites
```bash
# Run specific test suite
./tests/test_runner config
./tests/test_runner logging
./tests/test_runner error_handling
./tests/test_runner libsgnl

# List available test suites
./tests/test_runner --list

# Show help
./tests/test_runner --help
```

#### Run Individual Test Files
```bash
# Build and run individual test files
make test-config && ./tests/test_config
make test-logging && ./tests/test_logging
make test-error-handling && ./tests/test_error_handling
make test-libsgnl && ./tests/test_libsgnl
```

## Test Coverage

### Configuration Management (`test_config.c`)

Tests the unified configuration system used by all SGNL modules:

- ✅ **Configuration Lifecycle**: Creation, loading, validation, destruction
- ✅ **Default Values**: HTTP settings, logging, sudo plugin settings
- ✅ **JSON Loading**: File loading, parsing, error handling
- ✅ **Validation**: Required fields, value ranges, format validation
- ✅ **Accessors**: Safe access to configuration values
- ✅ **Error Handling**: File not found, invalid JSON, missing fields
- ✅ **Environment Variables**: Override configuration paths
- ✅ **Non-strict Validation**: Allow missing optional fields

### Logging System (`test_logging.c`)

Tests the structured logging system:

- ✅ **Logging Lifecycle**: Initialization, configuration, cleanup
- ✅ **Log Levels**: Conversion, filtering, enabled checks
- ✅ **Context Logging**: Component, function, request tracking
- ✅ **Output Capture**: Stdout redirection for testing
- ✅ **Null Handling**: NULL context, format, empty strings
- ✅ **Level Filtering**: Debug, info, warning, error levels
- ✅ **Secure Logging**: Conditional debug output
- ✅ **Variadic Logging**: Formatted message support
- ✅ **Request Tracking**: Stub implementations

### Error Handling (`test_error_handling.c`)

Tests the error handling and resource management utilities:

- ✅ **Safe String Operations**: Bounds checking, NULL handling
- ✅ **Validation Macros**: NULL checks, early returns
- ✅ **Automatic Cleanup**: Memory, file descriptors, JSON objects
- ✅ **RAII Patterns**: Scope-based resource management
- ✅ **Error Context**: Function, file, line tracking
- ✅ **Nested Scopes**: Complex cleanup scenarios
- ✅ **Early Returns**: Cleanup on function exit
- ✅ **Memory Safety**: Leak prevention, repeated allocations
- ✅ **File Descriptor Safety**: FD leak prevention
- ✅ **NULL Handling**: Safe cleanup of NULL pointers
- ✅ **Performance**: Minimal overhead verification

### Core Library (`test_libsgnl.c`)

Tests the main SGNL library functionality:

- ✅ **Client Lifecycle**: Creation, configuration, destruction
- ✅ **Client Validation**: Configuration validation
- ✅ **Error Handling**: Error messages, debug settings
- ✅ **Result Codes**: Conversion to strings
- ✅ **Request IDs**: Generation, uniqueness
- ✅ **Validation Functions**: Principal and asset ID validation
- ✅ **Version Functions**: Library version information
- ✅ **Access Evaluation**: Simple and detailed access checks
- ✅ **Batch Operations**: Multiple access evaluations
- ✅ **Asset Search**: Asset discovery functionality
- ✅ **Memory Management**: Result cleanup, resource management
- ✅ **Configuration Loading**: File-based configuration
- ✅ **Parameter Handling**: NULL and empty string handling

## Test Utilities

### Common Test Macros

All test files use consistent testing utilities:

```c
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
```

### Output Capture

The logging tests include utilities to capture stdout for verification:

```c
static void capture_stdout(void);
static void restore_stdout(void);
```

### Temporary File Creation

Configuration tests create temporary JSON files for testing:

```c
static char* create_temp_config_file(const char *content);
```

## Test Data

### Configuration Test Data

```json
{
  "api_url": "https://api.sgnl.ai/v1",
  "api_token": "test-token-12345",
  "tenant": "test-tenant",
  "http": {
    "timeout": 15,
    "connect_timeout": 5,
    "ssl_verify_peer": true,
    "ssl_verify_host": true,
    "user_agent": "SGNL-Test/1.0"
  },
  "sudo": {
    "access_msg": true,
    "command_attribute": "name"
  },
  "debug": true,
  "log_level": "debug"
}
```

## Continuous Integration

The test runner supports CI environments:

- Detects CI environment variables (`CI`, `GITHUB_ACTIONS`, `TRAVIS`, `CIRCLECI`)
- Disables output buffering for real-time test results
- Provides structured output for CI parsing
- Returns appropriate exit codes for CI systems

### CI Usage

```yaml
# GitHub Actions example
- name: Run SGNL Tests
  run: |
    cd src/c
    make test
```

## Debugging Tests

### Verbose Output

Enable debug logging during tests:

```bash
export SGNL_DEBUG=true
make test
```

### Individual Test Debugging

Run individual tests with debug output:

```bash
# Run with gdb
gdb --args ./tests/test_config

# Run with valgrind
valgrind --leak-check=full ./tests/test_config
```

### Test Isolation

Each test function is isolated and can be run independently:

```c
// In test_config.c
static int test_config_lifecycle(void) {
    // This test is completely independent
    // No shared state with other tests
}
```

## Adding New Tests

### Test File Structure

1. Include necessary headers
2. Define test utilities
3. Create test data
4. Write individual test functions
5. Create main function that runs all tests

### Example Test Function

```c
static int test_new_feature(void) {
    TEST_SECTION("New Feature");
    
    // Setup
    sgnl_config_t *config = sgnl_config_create();
    TEST_ASSERT(config != NULL, "Configuration creation");
    
    // Test
    sgnl_result_t result = sgnl_new_feature(config);
    TEST_ASSERT(result == SGNL_OK, "New feature works");
    
    // Cleanup
    sgnl_config_destroy(config);
    return 0;
}
```

### Integration with Test Runner

Add new test suites to `test_runner.c`:

```c
static test_suite_t test_suites[] = {
    // ... existing tests ...
    {
        .name = "new_feature",
        .description = "New Feature Tests",
        .test_function = test_new_feature_main
    }
};
```

## Performance Considerations

- Tests use minimal resources and run quickly
- Temporary files are cleaned up automatically
- Memory allocations are tracked and freed
- File descriptors are properly closed
- No network calls in unit tests

## Troubleshooting

### Common Issues

1. **Missing json-c library**: Install with `apt-get install libjson-c-dev` or `brew install json-c`
2. **Permission errors**: Ensure write access to `/tmp` directory
3. **Compiler warnings**: Fix any compiler warnings before running tests
4. **Memory leaks**: Use valgrind to detect memory issues

### Test Failures

1. Check test output for specific failure messages
2. Verify test data and expected results
3. Ensure all dependencies are installed
4. Check for environment-specific issues

## Contributing

When adding new functionality to the SGNL library:

1. Write corresponding tests in the appropriate test file
2. Ensure tests cover both success and failure cases
3. Test edge cases and error conditions
4. Update this README with new test descriptions
5. Run all tests to ensure no regressions

## License

Tests are covered by the same license as the SGNL library. 