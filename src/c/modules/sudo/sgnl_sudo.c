/**
 * SGNL Sudo Plugin
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <stdbool.h>
#include <sudo_plugin.h>
#include <json-c/json.h>

// SGNL library and common config
#include "../../lib/libsgnl.h"
#include "../../common/config.h"

// Define sudo_dso_public if not already defined
#ifndef sudo_dso_public
#define sudo_dso_public __attribute__((visibility("default")))
#endif

// Sudo plugin return codes (not defined in header, these are standard)
#define SUDO_RC_OK          1   /* success */
#define SUDO_RC_ACCEPT      1   /* policy accepts */
#define SUDO_RC_REJECT      0   /* policy rejects */
#define SUDO_RC_ERROR      -1   /* error occurred */
#define SUDO_RC_USAGE_ERROR -2  /* usage error */

// Plugin-specific configuration structure
typedef struct {
    bool debug_enabled;
    bool access_msg_enabled;
    char command_attribute[64];  // Which SGNL response attribute to use for command names
} sudo_plugin_settings_t;

// Plugin state
static struct {
    char **envp;
    char * const *settings;       // Sudo's settings array
    char * const *user_info;
    sgnl_client_t *sgnl_client;
    sudo_plugin_settings_t config;  // Our settings struct
} plugin_state = {0};

static sudo_conv_t sudo_conv;
static sudo_printf_t sudo_log;

/**
 * Load sudo plugin settings using common config system
 */
static bool load_sudo_settings(sudo_plugin_settings_t *settings) {
    // Set defaults first
    settings->debug_enabled = false;
    settings->access_msg_enabled = true;  // Default to showing success messages
    strcpy(settings->command_attribute, "id");  // Default to using asset ID
    
    // Load configuration using common config system
    sgnl_config_t *config = sgnl_config_create();
    if (!config) {
        if (sudo_log) {
            sudo_log(SUDO_CONV_ERROR_MSG, "SGNL: Failed to create config object\n");
        }
        return false;
    }
    
    sgnl_config_options_t options = SGNL_CONFIG_DEFAULT_OPTIONS;
    options.module_name = "sudo";
    
    sgnl_config_result_t result = sgnl_config_load(config, &options);
    if (result != SGNL_CONFIG_OK) {
        if (sudo_log) {
            sudo_log(SUDO_CONV_ERROR_MSG, "SGNL: Failed to load config: %s\n", 
                     sgnl_config_result_to_string(result));
        }
        sgnl_config_destroy(config);
        return false;
    }
    
    // Extract sudo-specific settings
    settings->debug_enabled = sgnl_config_is_debug_enabled(config);
    settings->access_msg_enabled = sgnl_config_get_sudo_access_msg(config);
    const char *cmd_attr = sgnl_config_get_sudo_command_attribute(config);
    if (cmd_attr) {
        strncpy(settings->command_attribute, cmd_attr, sizeof(settings->command_attribute) - 1);
        settings->command_attribute[sizeof(settings->command_attribute) - 1] = '\0';
    }
    
    sgnl_config_destroy(config);
    return true;
}



/**
 * Get current username from plugin context
 */
static const char* get_current_username(void) {
    static char username_buffer[256];
    const char *username = "unknown";
    
    // Try to get from user_info first
    if (plugin_state.user_info) {
        for (int i = 0; plugin_state.user_info[i] != NULL; i++) {
            if (strncmp(plugin_state.user_info[i], "user=", 5) == 0) {
                username = plugin_state.user_info[i] + 5;
                break;
            }
        }
    }
    
    // Fallback to environment
    if (strcmp(username, "unknown") == 0) {
        username = getenv("SUDO_USER");
        if (!username) {
            struct passwd *pw = getpwuid(getuid());
            username = pw ? pw->pw_name : "unknown";
        }
    }
    
    strncpy(username_buffer, username, sizeof(username_buffer) - 1);
    username_buffer[sizeof(username_buffer) - 1] = '\0';
    return username_buffer;
}

/**
 * Show allowed commands for user
 */
static void show_allowed_commands(const char *username) {
    if (!plugin_state.sgnl_client) {
        sudo_log(SUDO_CONV_INFO_MSG, "SGNL client not available\n");
        return;
    }
    
    // Search for allowed assets
    int asset_count = 0;
    char **allowed_commands = sgnl_search_assets(plugin_state.sgnl_client, 
                                               username, "execute", &asset_count);
    
    if (allowed_commands && asset_count > 0) {
        sudo_log(SUDO_CONV_INFO_MSG, "Allowed commands:\n");
        for (int i = 0; i < asset_count && allowed_commands[i]; i++) {
            sudo_log(SUDO_CONV_INFO_MSG, "  - %s\n", allowed_commands[i]);
        }
        sgnl_asset_ids_free(allowed_commands, asset_count);
    } else {
        sudo_log(SUDO_CONV_INFO_MSG, "No commands are currently allowed.\n");
    }
}

/**
 * Build command info for sudo
 */
static char* resolve_command_path(const char *command) {
    // If command already has a path, return it as-is
    if (strchr(command, '/')) {
        return strdup(command);
    }
    
    // Search in PATH
    const char *path_env = getenv("PATH");
    if (!path_env) {
        path_env = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";
    }
    
    // Debug: log the PATH we're using (commented out for clean output)
    // if (sudo_log) {
    //     sudo_log(SUDO_CONV_INFO_MSG, "[SGNL-DEBUG] Resolving command '%s' using PATH: %s\n", command, path_env);
    // }
    
    char *path_copy = strdup(path_env);
    if (!path_copy) return NULL;
    
    char *resolved_path = NULL;
    char *dir = strtok(path_copy, ":");
    
    while (dir) {
        size_t path_len = strlen(dir) + strlen(command) + 2;
        char *full_path = malloc(path_len);
        if (full_path) {
            snprintf(full_path, path_len, "%s/%s", dir, command);
            if (access(full_path, X_OK) == 0) {
                resolved_path = full_path;
                // if (sudo_log) {
                //     sudo_log(SUDO_CONV_INFO_MSG, "[SGNL-DEBUG] Command resolved to: %s\n", resolved_path);
                // }
                break;
            }
            free(full_path);
        }
        dir = strtok(NULL, ":");
    }
    
    if (!resolved_path && sudo_log) {
        sudo_log(SUDO_CONV_ERROR_MSG, "SGNL: Command not found: %s\n", command);
    }
    
    free(path_copy);
    return resolved_path;
}

/**
 * Free command_info array and its contents
 */
static void free_command_info(char **command_info) {
    if (!command_info) return;
    
    for (int i = 0; command_info[i] != NULL; i++) {
        free(command_info[i]);
    }
    free(command_info);
}

static char** build_command_info(const char *command) {
    char **command_info = calloc(16, sizeof(char *)); // Increased size for more fields
    if (!command_info) {
        return NULL;
    }
    
    int idx = 0;
    
    // Resolve the full path to the command
    char *resolved_command = resolve_command_path(command);
    if (!resolved_command) {
        free(command_info);
        return NULL;
    }
    
    // Set command path (required)
    size_t len = strlen(resolved_command) + 9; // "command=" + command + '\0'
    command_info[idx] = malloc(len);
    if (command_info[idx]) {
        snprintf(command_info[idx], len, "command=%s", resolved_command);
        idx++;
    }
    
    // Set runas_uid (required) - default to root (0)
    command_info[idx] = malloc(32);
    if (command_info[idx]) {
        snprintf(command_info[idx], 32, "runas_uid=0");
        idx++;
    }
    
    // Set runas_gid (required) - default to root (0)  
    command_info[idx] = malloc(32);
    if (command_info[idx]) {
        snprintf(command_info[idx], 32, "runas_gid=0");
        idx++;
    }
    
    // Set current working directory
    char *cwd = getcwd(NULL, 0);
    if (cwd) {
        size_t cwd_len = strlen(cwd) + 5; // "cwd=" + cwd + '\0'
        command_info[idx] = malloc(cwd_len);
        if (command_info[idx]) {
            snprintf(command_info[idx], cwd_len, "cwd=%s", cwd);
            idx++;
        }
        free(cwd);
    }
    
    // Add timeout (optional but recommended)
    command_info[idx] = malloc(32);
    if (command_info[idx]) {
        snprintf(command_info[idx], 32, "timeout=300"); // 5 minute timeout
        idx++;
    }
    
    free(resolved_command);
    return command_info;
}

/**
 * Build batch access evaluation for sudo command and arguments
 */
static sgnl_result_t check_sudo_access_with_args(sgnl_client_t *client, 
                                                const char *username, 
                                                int argc, char * const argv[]) {
    if (!client || !username || !argc || !argv || !argv[0]) {
        return SGNL_ERROR;
    }
    
    // Count how many queries we need:
    // 1. sudo access for the command itself
    // 2. command-specific access for each argument (if any)
    int query_count = 1; // Always at least 1 for the command itself
    
    // Add queries for arguments if they exist
    for (int i = 1; i < argc; i++) {
        if (argv[i] && strlen(argv[i]) > 0) {
            query_count++;
        }
    }
    
    if (query_count == 1) {
        // No arguments, just check the command
        return sgnl_check_access(client, username, argv[0], "sudo");
    }
    
    // Allocate arrays for batch evaluation
    const char **asset_ids = calloc(query_count, sizeof(char*));
    const char **actions = calloc(query_count, sizeof(char*));
    
    if (!asset_ids || !actions) {
        if (asset_ids) free(asset_ids);
        if (actions) free(actions);
        return SGNL_MEMORY_ERROR;
    }
    
    // Build the queries
    int query_idx = 0;
    
    // First query: sudo access for the command
    asset_ids[query_idx] = argv[0];
    actions[query_idx] = "sudo";
    query_idx++;
    
    // Additional queries: command-specific access for each argument
    for (int i = 1; i < argc && query_idx < query_count; i++) {
        if (argv[i] && strlen(argv[i]) > 0) {
            asset_ids[query_idx] = argv[i];
            actions[query_idx] = argv[0]; // Use command name as action
            query_idx++;
        }
    }
    
    // Perform batch evaluation
    sgnl_access_result_t **results = sgnl_evaluate_access_batch(client, username, 
                                                              asset_ids, actions, query_count);
    
    // Clean up arrays
    free(asset_ids);
    free(actions);
    
    if (!results) {
        return SGNL_ERROR;
    }
    
    // Check all results - ALL must be allowed for the overall request to succeed
    sgnl_result_t overall_result = SGNL_ALLOWED;
    for (int i = 0; i < query_count && results[i]; i++) {
        if (results[i]->result != SGNL_ALLOWED) {
            overall_result = results[i]->result;
            break;
        }
    }
    
    // Clean up results
    sgnl_access_result_array_free(results, query_count);
    
    return overall_result;
}

// ============================================================================
// Sudo Plugin Interface Implementation
// ============================================================================

/**
 * Plugin open/initialization
 */
static int policy_open(unsigned int version, sudo_conv_t conversation, 
                      sudo_printf_t sudo_plugin_printf, char * const settings[], 
                      char * const user_info[], char * const user_env[], 
                      char * const args[], const char **errstr) {
    (void)args;
    (void)errstr;
    
    sudo_conv = conversation;
    sudo_log = sudo_plugin_printf;
    
    if (SUDO_API_VERSION_GET_MAJOR(version) != SUDO_API_VERSION_MAJOR) {
        sudo_log(SUDO_CONV_ERROR_MSG, "SGNL plugin requires API version %d.x\n", 
                 SUDO_API_VERSION_MAJOR);
        return SUDO_RC_ERROR; // API version mismatch
    }
    
    // Store plugin state
    plugin_state.envp = (char **)user_env;
    plugin_state.settings = settings;
    plugin_state.user_info = user_info;
    
    // Load sudo-specific settings using common config system
    if (!load_sudo_settings(&plugin_state.config)) {
        sudo_log(SUDO_CONV_ERROR_MSG, "SGNL: Failed to load sudo settings\n");
        return SUDO_RC_ERROR;
    }
    
    // Create SGNL client (libsgnl will load its own config using common config system)
    plugin_state.sgnl_client = sgnl_client_create(NULL);  // NULL = use defaults
    
    if (!plugin_state.sgnl_client) {
        sudo_log(SUDO_CONV_ERROR_MSG, "SGNL: Failed to initialize client\n");
        return SUDO_RC_ERROR; // Do not continue with invalid state
    }
    
    // libsgnl will load API connection from common config system
    
    // Validate client
    if (sgnl_client_validate(plugin_state.sgnl_client) != SGNL_OK) {
        sudo_log(SUDO_CONV_ERROR_MSG, "SGNL: Invalid configuration: %s\n",
                 sgnl_client_get_last_error(plugin_state.sgnl_client));
        sgnl_client_destroy(plugin_state.sgnl_client);
        plugin_state.sgnl_client = NULL;
        return SUDO_RC_ERROR; // Do not continue with invalid state
    }
    
    // Only log initialization in debug mode
    if (plugin_state.config.debug_enabled) {
        sudo_log(SUDO_CONV_INFO_MSG, "SGNL: Plugin initialized successfully\n");
    }
    return SUDO_RC_OK;
}

/**
 * Policy check - main access control logic
 */
static int policy_check(int argc, char * const argv[], char *env_add[], 
                       char **command_info_out[], char **argv_out[], 
                       char **user_env_out[], const char **errstr) {
    (void)env_add;
    
    // Validate required output parameters
    if (!command_info_out || !argv_out || !user_env_out) {
        if (errstr) *errstr = "Invalid output parameters";
        return SUDO_RC_ERROR;
    }
    
    // Initialize outputs to NULL for safety
    *command_info_out = NULL;
    *argv_out = NULL;
    *user_env_out = NULL;
    
    if (!argc || !argv || !argv[0]) {
        sudo_log(SUDO_CONV_ERROR_MSG, "SGNL: No command specified\n");
        if (errstr) *errstr = "No command specified";
        return SUDO_RC_REJECT;
    }
    
    if (!plugin_state.sgnl_client) {
        sudo_log(SUDO_CONV_ERROR_MSG, "SGNL: Client not initialized\n");
        if (errstr) *errstr = "SGNL client not initialized";
        return SUDO_RC_ERROR;
    }
    
    const char *username = get_current_username();
    if (!username || strlen(username) == 0) {
        sudo_log(SUDO_CONV_ERROR_MSG, "SGNL: Cannot determine username\n");
        if (errstr) *errstr = "Cannot determine username";
        return SUDO_RC_ERROR;
    }
    
    // Check access using batch evaluation for command and arguments
    sgnl_result_t result = check_sudo_access_with_args(plugin_state.sgnl_client, 
                                                      username, argc, argv);
    
    if (result != SGNL_ALLOWED) {
        // Build a more descriptive error message
        char command_line[1024] = "";
        int pos = 0;
        for (int i = 0; i < argc && pos < sizeof(command_line) - 1; i++) {
            int written = snprintf(command_line + pos, sizeof(command_line) - pos, 
                                 "%s%s", i > 0 ? " " : "", argv[i]);
            if (written > 0) pos += written;
        }
        
        sudo_log(SUDO_CONV_ERROR_MSG, "SGNL: Access denied for %s to run '%s': %s\n", 
                 username, command_line, sgnl_result_to_string(result));
        if (errstr) *errstr = "Access denied by SGNL policy";
        return SUDO_RC_REJECT;
    }
    
    // Log access granted message based on configuration
    if (plugin_state.config.access_msg_enabled) {
        sudo_log(SUDO_CONV_INFO_MSG, "SGNL: Access granted for %s to run %s\n", 
                 username, argv[0]);
    }
    
    // Build command info - this must be done before setting outputs
    char **command_info = build_command_info(argv[0]);
    if (!command_info) {
        sudo_log(SUDO_CONV_ERROR_MSG, "SGNL: Failed to build command info\n");
        if (errstr) *errstr = "Failed to build command information";
        return SUDO_RC_ERROR;
    }
    
    // Set outputs only after all validations pass
    *argv_out = (char **)argv;
    *user_env_out = plugin_state.envp;
    *command_info_out = command_info;
    
    return SUDO_RC_ACCEPT;
}

/**
 * List allowed commands
 */
static int policy_list(int argc, char * const argv[], int verbose, 
                      const char *list_user, const char **errstr) {
    (void)verbose;
    (void)errstr;
    
    const char *username = get_current_username();
    
    if (!plugin_state.sgnl_client) {
        sudo_log(SUDO_CONV_ERROR_MSG, "SGNL: Client not initialized\n");
        return SUDO_RC_ERROR;
    }
    
    if (argc > 0 && argv[0]) {
        // Check specific command
        sgnl_result_t result = sgnl_check_access(plugin_state.sgnl_client, 
                                               username, argv[0], "execute");
        
        const char *as_user_text = list_user ? list_user : "";
        if (result == SGNL_ALLOWED) {
            sudo_log(SUDO_CONV_INFO_MSG, "You are allowed to execute '%s'%s\n", 
                     argv[0], as_user_text);
        } else {
            sudo_log(SUDO_CONV_INFO_MSG, "You are NOT allowed to execute '%s'%s\n", 
                     argv[0], as_user_text);
        }
    } else {
        // List all allowed commands
        show_allowed_commands(username);
    }
    
    return SUDO_RC_OK;
}

/**
 * Show plugin version
 */
static int policy_version(int verbose) {
    (void)verbose;
    sudo_log(SUDO_CONV_INFO_MSG, "SGNL sudo policy plugin version %s\n", 
             sgnl_get_version());
    return SUDO_RC_OK;
}

/**
 * Initialize session - called before uid/gid changes
 */
static int policy_init_session(struct passwd *pwd, char **user_env_out[], const char **errstr) {
    (void)pwd;
    (void)errstr;
    
    if (!plugin_state.sgnl_client) {
        sudo_log(SUDO_CONV_ERROR_MSG, "SGNL: Client not initialized in init_session\n");
        return SUDO_RC_ERROR;
    }
    
    // Validate user environment
    if (!plugin_state.envp) {
        sudo_log(SUDO_CONV_ERROR_MSG, "SGNL: No user environment available\n");
        return SUDO_RC_ERROR;
    }
    
    // Pass through the environment (could add filtering here)
    if (user_env_out) {
        *user_env_out = plugin_state.envp;
    }
    
    // Log session initialization in debug mode
    if (plugin_state.config.debug_enabled) {
        const char *username = get_current_username();
        sudo_log(SUDO_CONV_INFO_MSG, "SGNL: Session initialized for user %s\n", 
                 username ? username : "unknown");
    }
    
    return SUDO_RC_OK;
}

/**
 * Plugin cleanup
 */
static void policy_close(int exit_status, int error) {
    
    // Log command completion if debug enabled
    if (plugin_state.config.debug_enabled) {
        const char *username = get_current_username();
        if (exit_status >= 0) {
            sudo_log(SUDO_CONV_INFO_MSG, "SGNL: Command completed for %s with exit status %d\n",
                     username ? username : "unknown", exit_status);
        } else {
            sudo_log(SUDO_CONV_INFO_MSG, "SGNL: Command execution failed for %s\n",
                     username ? username : "unknown");
        }
    }
    
    // Report any execution errors
    if (error != 0) {
        sudo_log(SUDO_CONV_ERROR_MSG, "SGNL: Command execution error: %s\n", strerror(error));
    }
    
    // Clean up SGNL client
    if (plugin_state.sgnl_client) {
        sgnl_client_destroy(plugin_state.sgnl_client);
        plugin_state.sgnl_client = NULL;
    }
    
    // Clear plugin state
    plugin_state.envp = NULL;
    plugin_state.settings = NULL;
    plugin_state.user_info = NULL;
}

// ============================================================================
// Plugin Export Structure
// ============================================================================

sudo_dso_public struct policy_plugin sgnl_policy = {
    SUDO_POLICY_PLUGIN,
    SUDO_API_VERSION,
    policy_open,
    policy_close,
    policy_version,
    policy_check,
    policy_list,
    NULL, /* validate */
    NULL, /* invalidate */
    policy_init_session, /* init_session */
    NULL, /* register_hooks */
    NULL, /* deregister_hooks */
    NULL  /* event_alloc() filled in by sudo */
};

