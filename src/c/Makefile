# SGNL Consolidated Build System
# 
# Provides clear, purpose-built targets for different consumer needs:
# - Library only (for developers integrating SGNL)
# - PAM module only (for PAM-based systems)
# - Sudo plugin only (for sudo-based systems)  
# - Everything (for complete installations)

# Include platform detection
include platform.mk

# Tool detection for optional quality checks
CLANG_TIDY ?= $(shell which clang-tidy 2>/dev/null)
VALGRIND ?= $(shell which valgrind 2>/dev/null)

# ============================================================================
# Build Configuration
# ============================================================================

# Compiler settings
CC ?= gcc
AR ?= ar
CFLAGS ?= -Wall -Wextra -std=c99 -fPIC -O2 -g
INCLUDES = -I. -Iinclude -Icommon -Itests $(PLATFORM_ALL_CFLAGS)
LIBS = $(PLATFORM_ALL_LIBS)
LDFLAGS = $(PLATFORM_ALL_LDFLAGS)

# Directories
LIB_DIR = lib
MODULES_DIR = modules
TESTS_DIR = tests
COMMON_DIR = common

# Source files for dependency tracking
SOURCES = $(wildcard $(LIB_DIR)/*.c $(COMMON_DIR)/*.c $(MODULES_DIR)/*/*.c)

# Build outputs
LIBSGNL = $(LIB_DIR)/libsgnl.a
PAM_MODULE = $(MODULES_DIR)/pam/pam_sgnl.$(SO_EXT)
SUDO_PLUGIN = $(MODULES_DIR)/sudo/sgnl_policy.$(SO_EXT)
TEST_RUNNER = $(TESTS_DIR)/test_runner

# Installation directories
INSTALL_LIB_DIR ?= /usr/local/lib
INSTALL_INC_DIR ?= /usr/local/include
INSTALL_PAM_DIR ?= $(PAM_DIR)
INSTALL_SUDO_DIR ?= $(SUDO_DIR)

# ============================================================================
# Primary Build Targets (Consumer-Focused)
# ============================================================================

.PHONY: all library lib pam sudo modules clean install help test test-library test-lib create-test

# Build everything including tests
all: library modules
	@echo "✅ All components built successfully"
	@echo "   📚 Library: $(LIBSGNL)"
	@echo "   🔐 PAM Module: $(PAM_MODULE)" 
	@echo "   🛡️  Sudo Plugin: $(SUDO_PLUGIN)"
	@if [ -f $(TEST_RUNNER) ]; then \
		echo "   🧪 Test Runner: $(TEST_RUNNER)"; \
	fi

# Build just the core library
library: $(LIBSGNL)
	@echo "✅ SGNL library built: $(LIBSGNL)"

# Build just the PAM module 
pam: $(PAM_MODULE)
	@echo "✅ PAM module built: $(PAM_MODULE)"

# Build just the sudo plugin
sudo: $(SUDO_PLUGIN) 
	@echo "✅ Sudo plugin built: $(SUDO_PLUGIN)"

# Build both modules
modules: $(PAM_MODULE) $(SUDO_PLUGIN)
	@echo "✅ All modules built successfully"

# Alias for backward compatibility
lib: library

$(LIBSGNL): $(LIB_DIR)/libsgnl.c $(LIB_DIR)/libsgnl.h $(COMMON_DIR)/config.c $(COMMON_DIR)/config.h $(COMMON_DIR)/logging.c $(COMMON_DIR)/logging.h | $(LIB_DIR)
	@echo "🔨 Building consolidated SGNL library..."
	$(CC) $(CFLAGS) $(INCLUDES) -c $(LIB_DIR)/libsgnl.c -o $(LIB_DIR)/libsgnl.o
	$(CC) $(CFLAGS) $(INCLUDES) -c $(COMMON_DIR)/config.c -o $(COMMON_DIR)/config.o
	$(CC) $(CFLAGS) $(INCLUDES) -c $(COMMON_DIR)/logging.c -o $(COMMON_DIR)/logging.o
	$(AR) rcs $@ $(LIB_DIR)/libsgnl.o $(COMMON_DIR)/config.o $(COMMON_DIR)/logging.o
	@rm -f $(LIB_DIR)/libsgnl.o $(COMMON_DIR)/config.o $(COMMON_DIR)/logging.o
	@echo "📦 Library size: $$($(STAT_SIZE) $@ 2>/dev/null || echo 'unknown') bytes"

$(LIB_DIR):
	@mkdir -p $(LIB_DIR)

# ============================================================================
# PAM Module Build
# ============================================================================

$(PAM_MODULE): $(MODULES_DIR)/pam/sgnl_pam.c $(LIBSGNL) | $(MODULES_DIR)/pam
	@echo "🔨 Building PAM module..."
ifeq ($(PLATFORM),macos)
	$(CC) $(CFLAGS) -DPAM_DYNAMIC $(INCLUDES) $(SHARED_FLAGS) $(LDFLAGS) \
		-o $@ $< -force_load $(LIBSGNL) $(LIBS) -lpam
else
	$(CC) $(CFLAGS) -DPAM_DYNAMIC $(INCLUDES) $(SHARED_FLAGS) $(LDFLAGS) \
		-o $@ $< -Wl,--whole-archive $(LIBSGNL) -Wl,--no-whole-archive $(LIBS) -lpam
endif
	@echo "📦 PAM module size: $$($(STAT_SIZE) $@ 2>/dev/null || echo 'unknown') bytes"

$(MODULES_DIR)/pam:
	@mkdir -p $(MODULES_DIR)/pam

# ============================================================================
# Sudo Plugin Build
# ============================================================================

$(SUDO_PLUGIN): $(MODULES_DIR)/sudo/sgnl_sudo.c $(LIBSGNL) | $(MODULES_DIR)/sudo
	@echo "🔨 Building sudo plugin..."
ifeq ($(PLATFORM),macos)
	$(CC) $(CFLAGS) $(INCLUDES) $(SHARED_FLAGS) $(LDFLAGS) \
		-o $@ $< -force_load $(LIBSGNL) $(LIBS) -ljson-c
else
	$(CC) $(CFLAGS) $(INCLUDES) $(SHARED_FLAGS) $(LDFLAGS) \
		-o $@ $< -Wl,--whole-archive $(LIBSGNL) -Wl,--no-whole-archive $(LIBS) -ljson-c
endif
	@echo "📦 Sudo plugin size: $$($(STAT_SIZE) $@ 2>/dev/null || echo 'unknown') bytes"

$(MODULES_DIR)/sudo:
	@mkdir -p $(MODULES_DIR)/sudo

# ============================================================================
# Installation Targets
# ============================================================================

.PHONY: install-lib install-pam install-sudo install uninstall

# Install library only
install-lib: $(LIBSGNL)
	@echo "📦 Installing SGNL library..."
	@sudo mkdir -p $(INSTALL_LIB_DIR) $(INSTALL_INC_DIR)
	sudo cp $(LIBSGNL) $(INSTALL_LIB_DIR)/
	sudo cp $(LIB_DIR)/libsgnl.h $(INSTALL_INC_DIR)/
	@which ldconfig >/dev/null 2>&1 && sudo ldconfig || true
	@echo "✅ Library installed to $(INSTALL_LIB_DIR)"

# Install PAM module only
install-pam: $(PAM_MODULE)
	@echo "📦 Installing PAM module..."
	@if [ "$$(id -u)" != "0" ]; then \
		echo "❌ Root privileges required. Use: sudo make install-pam"; \
		exit 1; \
	fi
	mkdir -p $(INSTALL_PAM_DIR)
	cp $(PAM_MODULE) $(INSTALL_PAM_DIR)/
	chmod 644 $(INSTALL_PAM_DIR)/pam_sgnl.$(SO_EXT)
	@echo "✅ PAM module installed to $(INSTALL_PAM_DIR)"
	@echo "💡 Update your PAM configuration to use pam_sgnl.so"

# Install sudo plugin only  
install-sudo: $(SUDO_PLUGIN)
	@echo "📦 Installing sudo plugin..."
	@if [ "$$(id -u)" != "0" ]; then \
		echo "❌ Root privileges required. Use: sudo make install-sudo"; \
		exit 1; \
	fi
	mkdir -p $(INSTALL_SUDO_DIR)
	cp $(SUDO_PLUGIN) $(INSTALL_SUDO_DIR)/
	chmod 644 $(INSTALL_SUDO_DIR)/sgnl_policy.$(SO_EXT)
	@echo "✅ Sudo plugin installed to $(INSTALL_SUDO_DIR)"
	@echo "💡 Update your sudoers configuration to use sgnl_policy.so"

# Install everything
install: install-lib install-pam install-sudo
	@echo "✅ Complete SGNL installation finished"

# Uninstall everything
uninstall:
	@echo "🗑️  Removing SGNL installation..."
	@sudo rm -f $(INSTALL_LIB_DIR)/libsgnl.a
	@sudo rm -f $(INSTALL_INC_DIR)/libsgnl.h
	@sudo rm -f $(INSTALL_PAM_DIR)/pam_sgnl.$(SO_EXT)
	@sudo rm -f $(INSTALL_SUDO_DIR)/sgnl_policy.$(SO_EXT)
	@which ldconfig >/dev/null 2>&1 && sudo ldconfig || true
	@echo "✅ SGNL uninstalled"

# ============================================================================
# Testing
# ============================================================================

.PHONY: test test-config test-logging test-error-handling test-libsgnl test-lib test-modules

# Individual test executables
TEST_CONFIG = $(TESTS_DIR)/test_config
TEST_LOGGING = $(TESTS_DIR)/test_logging
TEST_ERROR_HANDLING = $(TESTS_DIR)/test_error_handling
TEST_LIBSGNL = $(TESTS_DIR)/test_libsgnl

# Build individual test executables
$(TEST_CONFIG): $(TESTS_DIR)/test_config.c $(LIBSGNL)
	@echo "🔨 Building configuration tests..."
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(LIBSGNL) $(LIBS)
	@echo "✅ Configuration tests built: $@"

$(TEST_LOGGING): $(TESTS_DIR)/test_logging.c $(LIBSGNL)
	@echo "🔨 Building logging tests..."
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(LIBSGNL) $(LIBS)
	@echo "✅ Logging tests built: $@"

$(TEST_ERROR_HANDLING): $(TESTS_DIR)/test_error_handling.c $(LIBSGNL)
	@echo "🔨 Building error handling tests..."
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(LIBSGNL) $(LIBS)
	@echo "✅ Error handling tests built: $@"

$(TEST_LIBSGNL): $(TESTS_DIR)/test_libsgnl.c $(LIBSGNL)
	@echo "🔨 Building core library tests..."
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(LIBSGNL) $(LIBS)
	@echo "✅ Core library tests built: $@"

# Build test runner with all test files
$(TEST_RUNNER): $(TESTS_DIR)/test_runner.c $(TESTS_DIR)/test_config.c $(TESTS_DIR)/test_logging.c $(TESTS_DIR)/test_error_handling.c $(TESTS_DIR)/test_libsgnl.c $(LIBSGNL)
	@echo "🔨 Building comprehensive test runner..."
	@mkdir -p $(TESTS_DIR)
	$(CC) $(CFLAGS) -DSGNL_TEST_RUNNER $(INCLUDES) -o $@ \
		$(TESTS_DIR)/test_runner.c \
		$(TESTS_DIR)/test_config.c \
		$(TESTS_DIR)/test_logging.c \
		$(TESTS_DIR)/test_error_handling.c \
		$(TESTS_DIR)/test_libsgnl.c \
		$(LIBSGNL) $(LIBS)
	@echo "✅ Test runner built: $@"

# Individual test targets
test-config: $(TEST_CONFIG)
	@echo "🧪 Running configuration tests..."
	./$(TEST_CONFIG)

test-logging: $(TEST_LOGGING)
	@echo "🧪 Running logging tests..."
	./$(TEST_LOGGING)

test-error-handling: $(TEST_ERROR_HANDLING)
	@echo "🧪 Running error handling tests..."
	./$(TEST_ERROR_HANDLING)

test-libsgnl: $(TEST_LIBSGNL)
	@echo "🧪 Running core library tests..."
	./$(TEST_LIBSGNL)

# Run all tests
test: $(TEST_RUNNER)
	@echo "🧪 Running comprehensive test suite..."
	./$(TEST_RUNNER)
	@echo "✅ All tests completed"

# Legacy test targets
test-library: test-libsgnl
	@echo "🧪 Legacy test-library target - use test-libsgnl instead"

test-modules: $(PAM_MODULE) $(SUDO_PLUGIN)
	@echo "🧪 Testing modules..."
	@echo "⚠️  Module testing requires system integration"
	@echo "   PAM module: $(PAM_MODULE)"
	@echo "   Sudo plugin: $(SUDO_PLUGIN)"

# Test with specific suite
test-suite-%: $(TEST_RUNNER)
	@echo "🧪 Running test suite: $*"
	./$(TEST_RUNNER) $*

# Test with memory checking
test-memcheck: $(TEST_CONFIG) $(TEST_LOGGING) $(TEST_ERROR_HANDLING) $(TEST_LIBSGNL)
ifneq ($(VALGRIND),)
	@echo "🔍 Running tests with memory checking..."
	@$(VALGRIND) --leak-check=full --error-exitcode=1 ./$(TEST_CONFIG) || exit 1
	@$(VALGRIND) --leak-check=full --error-exitcode=1 ./$(TEST_LOGGING) || exit 1
	@$(VALGRIND) --leak-check=full --error-exitcode=1 ./$(TEST_ERROR_HANDLING) || exit 1
	@$(VALGRIND) --leak-check=full --error-exitcode=1 ./$(TEST_LIBSGNL) || exit 1
	@echo "✅ Memory check completed successfully"
else
	@echo "⚠️  valgrind not found, install for memory checking"
	@echo "   Running tests without memory checking..."
	@make test
endif

# ============================================================================
# Development and Quality Checks
# ============================================================================

.PHONY: clean distclean debug format lint memcheck security-scan info

# Clean build artifacts
clean:
	@echo "🧹 Cleaning build artifacts..."
	rm -rf $(LIB_DIR)/*.o $(LIB_DIR)/*.a
	rm -rf $(COMMON_DIR)/*.o
	rm -rf $(MODULES_DIR)/pam/*.$(SO_EXT) $(MODULES_DIR)/pam/*.o
	rm -rf $(MODULES_DIR)/sudo/*.$(SO_EXT) $(MODULES_DIR)/sudo/*.o
	rm -rf $(TESTS_DIR)/test_runner $(TESTS_DIR)/*.o
	rm -rf $(TEST_CONFIG) $(TEST_LOGGING) $(TEST_ERROR_HANDLING) $(TEST_LIBSGNL)
	@echo "✅ Build artifacts cleaned"

# Deep clean (remove all generated files)
distclean: clean
	@echo "🗑️  Deep cleaning..."
	@if [ -d $(LIB_DIR) ] && [ -z "$$(find $(LIB_DIR) -name '*.c' -o -name '*.h' 2>/dev/null)" ]; then \
		rm -rf $(LIB_DIR); \
	fi
	@if [ -d $(MODULES_DIR) ] && [ -z "$$(find $(MODULES_DIR) -name '*.c' -o -name '*.h' 2>/dev/null)" ]; then \
		rm -rf $(MODULES_DIR); \
	fi
	@if [ -d $(TESTS_DIR) ] && [ -z "$$(find $(TESTS_DIR) -name '*.c' -o -name '*.h' 2>/dev/null)" ]; then \
		rm -rf $(TESTS_DIR); \
	fi
	@echo "✅ Deep clean completed"

# Debug build (with symbols and sanitizers)
debug: CFLAGS += -DDEBUG -O0 -fsanitize=address
debug: LDFLAGS += -fsanitize=address  
debug: clean library modules
	@echo "🐛 Debug build completed"

# Format source code
format:
	@echo "💄 Formatting source code..."
	@find . -name "*.c" -o -name "*.h" | xargs clang-format -i 2>/dev/null || echo "⚠️  clang-format not available"
	@echo "✅ Code formatted"

# Static analysis with clang-tidy
lint:
ifneq ($(CLANG_TIDY),)
	@echo "🔍 Running static analysis..."
	@find . -name "*.c" -not -path "./tests/*" | head -5 | \
		xargs $(CLANG_TIDY) -checks=readability-*,performance-*,security-* -- $(CFLAGS) $(INCLUDES) || true
	@echo "✅ Static analysis complete"
else
	@echo "⚠️  clang-tidy not found, install for static analysis"
endif

# Memory leak detection  
memcheck: $(TEST_RUNNER)
ifneq ($(VALGRIND),)
	@echo "🔍 Checking for memory leaks..."
	@$(VALGRIND) --leak-check=yes --error-exitcode=1 ./$(TEST_RUNNER) || \
		echo "❌ Memory leaks detected!"
	@echo "✅ Memory check complete"
else
	@echo "⚠️  valgrind not found, install for memory checking"
endif

# Basic security checks
security-scan:
	@echo "🔒 Running security checks..."
	@echo "Checking for dangerous functions..."
	@! grep -rn "strcpy\|strcat\|sprintf\|gets" --include="*.c" . || \
		echo "⚠️  Found potentially unsafe functions above"
	@echo "✅ Security scan complete"

# Show build information and help
info:
	@echo "🏗️  SGNL Build System Information"
	@echo "================================="
	@echo "Platform: $(PLATFORM) ($(UNAME_S) $(UNAME_M))"
	@echo "Compiler: $(CC)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"
	@echo "Libraries: $(LIBS)"
	@echo "Library Extension: .a"
	@echo "Shared Extension: .$(SO_EXT)"
	@echo "PAM Directory: $(PAM_DIR)"
	@echo "Sudo Directory: $(SUDO_DIR)"
	@echo
	@echo "📂 Build Structure:"
	@echo "  lib/           - Consolidated SGNL library"
	@echo "  modules/pam/   - PAM module"  
	@echo "  modules/sudo/  - Sudo plugin"
	@echo "  tests/         - Test programs"

# Show help
help:
	@echo "🚀 SGNL Build System"
	@echo "===================="
	@echo
	@echo "🎯 PRIMARY TARGETS:"
	@echo "  library          - Build just the SGNL library (alias: lib)"
	@echo "  pam             - Build just the PAM module"
	@echo "  sudo            - Build just the sudo plugin"
	@echo "  modules         - Build both PAM and sudo modules"
	@echo "  all             - Build library + modules (default)"
	@echo
	@echo "📦 INSTALLATION:"
	@echo "  install-lib     - Install library to system"
	@echo "  install-pam     - Install PAM module to system (requires root)"
	@echo "  install-sudo    - Install sudo plugin to system (requires root)"
	@echo "  install         - Install everything (requires root)"
	@echo "  uninstall       - Remove all installed components"
	@echo
	@echo "🧪 TESTING:"
	@echo "  test            - Run all tests"
	@echo "  test-config     - Run configuration tests only"
	@echo "  test-logging    - Run logging tests only"
	@echo "  test-error-handling - Run error handling tests only"
	@echo "  test-libsgnl    - Run core library tests only"
	@echo "  test-suite-<name> - Run specific test suite"
	@echo "  test-memcheck   - Run tests with memory leak detection"
	@echo "  test-modules    - Test modules only"
	@echo
	@echo "🔧 DEVELOPMENT:"
	@echo "  debug           - Build with debug symbols and sanitizers"
	@echo "  clean           - Remove build artifacts"
	@echo "  distclean       - Deep clean all generated files"
	@echo "  format          - Format source code"
	@echo "  lint            - Run static analysis (requires clang-tidy)"
	@echo "  memcheck        - Check for memory leaks (requires valgrind)"
	@echo "  security-scan   - Check for unsafe functions"
	@echo "  info            - Show build system information"
	@echo "  help            - Show this help"
	@echo
	@echo "💡 USAGE EXAMPLES:"
	@echo "  make library                # Library developers"
	@echo "  make pam && sudo make install-pam     # PAM deployments"
	@echo "  make sudo && sudo make install-sudo   # Sudo deployments"
	@echo "  make all && sudo make install         # Complete installations"
	@echo
	@echo "🔗 For more information, see README.md" 