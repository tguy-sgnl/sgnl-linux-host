# Platform Detection and Configuration
# Include this file in all component Makefiles for consistent multi-platform support

# Detect operating system and architecture
UNAME_S := $(shell uname -s 2>/dev/null || echo Unknown)
UNAME_M := $(shell uname -m 2>/dev/null || echo unknown)

# Default tools (can be overridden)
CC ?= gcc
AR ?= ar
INSTALL ?= install
PKG_CONFIG ?= pkg-config

# Platform-specific settings
ifeq ($(UNAME_S),Darwin)
    # macOS
    PLATFORM = macos
    SO_EXT = dylib
    SHARED_FLAGS = -dynamiclib
    PAM_DIR = /usr/lib/pam
    SUDO_DIR = /usr/local/libexec/sudo
    STAT_SIZE = stat -f%z
    
    # macOS-specific compiler flags
    PLATFORM_CFLAGS = -mmacosx-version-min=10.9
    PLATFORM_LDFLAGS = 
    
    # Library paths
    BREW_PREFIX = $(shell brew --prefix 2>/dev/null || echo /usr/local)
    PLATFORM_INCLUDES = -I$(BREW_PREFIX)/include
    PLATFORM_LIBDIRS = -L$(BREW_PREFIX)/lib
    
else ifeq ($(UNAME_S),Linux)
    # Linux
    PLATFORM = linux
    SO_EXT = so
    SHARED_FLAGS = -shared
    PAM_DIR = /lib/security
    SUDO_DIR = /usr/lib/sudo
    STAT_SIZE = stat -c%s
    
    # Linux-specific compiler flags
    PLATFORM_CFLAGS = -D_GNU_SOURCE
    PLATFORM_LDFLAGS = -Wl,--as-needed
    
    # Standard Linux paths
    PLATFORM_INCLUDES = -I/usr/include -I/usr/local/include
    PLATFORM_LIBDIRS = -L/usr/lib -L/usr/local/lib
    
else ifeq ($(UNAME_S),FreeBSD)
    # FreeBSD
    PLATFORM = freebsd
    SO_EXT = so
    SHARED_FLAGS = -shared
    PAM_DIR = /usr/local/lib/pam
    SUDO_DIR = /usr/local/libexec/sudo
    STAT_SIZE = stat -f%z
    
    # FreeBSD-specific settings
    PLATFORM_CFLAGS = -D_BSD_SOURCE
    PLATFORM_LDFLAGS = 
    PLATFORM_INCLUDES = -I/usr/local/include
    PLATFORM_LIBDIRS = -L/usr/local/lib
    
else ifeq ($(UNAME_S),OpenBSD)
    # OpenBSD
    PLATFORM = openbsd
    SO_EXT = so
    SHARED_FLAGS = -shared
    PAM_DIR = /usr/lib/pam
    SUDO_DIR = /usr/libexec/sudo
    STAT_SIZE = stat -f%z
    
    PLATFORM_CFLAGS = -D_BSD_SOURCE
    PLATFORM_LDFLAGS = 
    PLATFORM_INCLUDES = -I/usr/local/include
    PLATFORM_LIBDIRS = -L/usr/local/lib
    
else
    # Unknown/Generic Unix
    PLATFORM = generic
    SO_EXT = so
    SHARED_FLAGS = -shared
    PAM_DIR = /lib/security
    SUDO_DIR = /usr/lib/sudo
    STAT_SIZE = ls -l  # Fallback - not exact size
    
    PLATFORM_CFLAGS = 
    PLATFORM_LDFLAGS = 
    PLATFORM_INCLUDES = 
    PLATFORM_LIBDIRS = 
endif

# Dependency detection functions
define check_pkg_config
$(shell $(PKG_CONFIG) --exists $(1) 2>/dev/null && echo yes || echo no)
endef

define get_pkg_cflags
$(shell $(PKG_CONFIG) --cflags $(1) 2>/dev/null || echo "")
endef

define get_pkg_libs
$(shell $(PKG_CONFIG) --libs $(1) 2>/dev/null || echo "-l$(1)")
endef

# Detect required libraries
HAS_JSON_C := $(call check_pkg_config,json-c)
HAS_LIBCURL := $(call check_pkg_config,libcurl)

# Set library flags based on availability
ifeq ($(HAS_JSON_C),yes)
    JSON_CFLAGS = $(call get_pkg_cflags,json-c)
    JSON_LIBS = $(call get_pkg_libs,json-c)
else
    JSON_CFLAGS = 
    JSON_LIBS = -ljson-c
endif

ifeq ($(HAS_LIBCURL),yes)
    CURL_CFLAGS = $(call get_pkg_cflags,libcurl)
    CURL_LIBS = $(call get_pkg_libs,libcurl)
else
    CURL_CFLAGS = 
    CURL_LIBS = -lcurl
endif

# Combined platform settings
PLATFORM_ALL_CFLAGS = $(PLATFORM_CFLAGS) $(PLATFORM_INCLUDES) $(JSON_CFLAGS) $(CURL_CFLAGS)
PLATFORM_ALL_LDFLAGS = $(PLATFORM_LDFLAGS) $(PLATFORM_LIBDIRS)
PLATFORM_ALL_LIBS = $(JSON_LIBS) $(CURL_LIBS)

# Debug info
platform-info:
	@echo "Platform: $(PLATFORM) ($(UNAME_S) $(UNAME_M))"
	@echo "Shared library extension: $(SO_EXT)"
	@echo "PAM directory: $(PAM_DIR)"
	@echo "Sudo directory: $(SUDO_DIR)"
	@echo "JSON-C available: $(HAS_JSON_C)"
	@echo "libcurl available: $(HAS_LIBCURL)"
	@echo "Platform CFLAGS: $(PLATFORM_ALL_CFLAGS)"
	@echo "Platform LDFLAGS: $(PLATFORM_ALL_LDFLAGS)"
	@echo "Platform LIBS: $(PLATFORM_ALL_LIBS)"

.PHONY: platform-info 