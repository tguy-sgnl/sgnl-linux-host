"""
SGNL Policy Plugin for sudo

Main plugin implementation that integrates with sudo's policy plugin API.
Unified with C plugin configuration and logging behavior.
Enhanced with comprehensive error handling and standardized result codes.
"""

import sudo
import errno
import sys
import os
import pwd
import grp
import shutil
from typing import List, Tuple, Optional

from .client import SgnlClient
from .errors import SgnlResult, result_to_sudo_code

class SudoPolicyPlugin(sudo.Plugin):
    """SGNL Policy Plugin for sudo."""

    # -- Plugin API functions --

    def __init__(self, user_env: tuple, settings: tuple,
                 version: str, user_info: tuple, **kwargs):
        """Constructor for the sudo policy plugin."""
        if not version.startswith("1."):
            raise sudo.PluginError(
                "This plugin is not compatible with python plugin"
                "API version {}".format(version))

        self.user_env = sudo.options_as_dict(user_env)
        self.settings = sudo.options_as_dict(settings)
        self.user_info = sudo.options_as_dict(user_info)
        self.sgnl_client = SgnlClient(self.user_env, self.settings, self.user_info)

    @staticmethod
    def _validate_argv_static(argv) -> bool:
        """Static helper to validate argv."""
        return bool(argv and isinstance(argv, (list, tuple)) and argv[0])

    def _validate_argv(self, argv) -> bool:
        """Instance helper to validate argv."""
        if not self._validate_argv_static(argv):
            # Use client's logging to respect configuration
            self.sgnl_client._log_error(f"Invalid argv: {argv}")
            return False
        return True

    def check_policy(self, argv: tuple, env_add: tuple) -> Tuple[int, tuple, tuple, tuple]:
        """Check if the command is allowed by policy."""
        if not self._validate_argv(argv):
            return (sudo.RC.ERROR, (), argv, ())
            
        cmd = argv[0]
        
        try:
            user_env_out = sudo.options_from_dict(self.user_env) + env_add
            command_info_out = sudo.options_from_dict({
                "command": self._find_on_path(cmd),  # Absolute path of command
                "runas_uid": self._runas_uid(),      # The user id
                "runas_gid": self._runas_gid(),      # The group id
            })
        except Exception as e:
            self.sgnl_client._log_error(f"Error preparing command info: {e}")
            return (sudo.RC.ERROR, (), argv, ())

        # Use new standardized error handling
        try:
            principal_id = self.sgnl_client.get_principal_id()
            result = self.sgnl_client.evaluate_access(principal_id, cmd, "execute")
            
            if result.is_allowed():
                self.sgnl_client._log_info(f"Access granted for {principal_id} to run {cmd}")
                return (sudo.RC.ACCEPT, command_info_out, argv, user_env_out)
            elif result.is_denied():
                self.sgnl_client._log_info(f"Access denied for {principal_id} to run {cmd}: {result.reason}")
                return (sudo.RC.REJECT, command_info_out, argv, user_env_out)
            else:
                self.sgnl_client._log_error(f"Access evaluation error: {result.error_message}")
                return (sudo.RC.ERROR, (), argv, ())
                
        except Exception as e:
            self.sgnl_client._log_error(f"Policy check failed: {e}")
            return (sudo.RC.ERROR, (), argv, ())

    def init_session(self, user_pwd, user_env: tuple) -> Tuple[int, tuple]:
        """Perform session setup."""
        # conversion example (not used, but left for reference):
        # user_pwd = pwd.struct_passwd(user_pwd) if user_pwd else None
        return (sudo.RC.OK, user_env + ("PLUGIN_EXAMPLE_ENV=1",))

    def list(self, argv: tuple, is_verbose: int, user: str):
        """List allowed commands or check if a specific command is allowed."""
        cmd = argv[0] if argv else None
        as_user_text = "as user '{}'".format(user) if user else ""
        
        if cmd:
            # Check specific command
            if self._is_command_allowed(cmd):
                allowed_text = ""
            else:
                allowed_text = "NOT "
            # Always show list results (not subject to logging controls)
            sudo.log_info("You are {}allowed to execute command '{}'{}"
                          .format(allowed_text, cmd, as_user_text))
        else:
            # List all allowed commands
            allowed_commands = self.sgnl_client.asset_search("execute")
            if allowed_commands:
                sudo.log_info("Allowed commands:")
                for cmd in allowed_commands:
                    sudo.log_info("  - {}".format(cmd))
            else:
                sudo.log_info("No commands are currently allowed.")

    def validate(self) -> int:
        """Validate the plugin configuration."""
        validation_result = self.sgnl_client.validate()
        if validation_result == SgnlResult.OK:
            return sudo.RC.OK
        else:
            self.sgnl_client._log_error(f"Plugin validation failed: {validation_result}")
            return sudo.RC.ERROR

    def invalidate(self, remove: int) -> int:
        """Invalidate cached data."""
        return sudo.RC.OK

    def show_version(self, is_verbose: int) -> int:
        """Show plugin version information."""
        sudo.log_info("SGNL sudo policy plugin version 1.0")
        if is_verbose:
            sudo.log_info("  - Python implementation")
            sudo.log_info("  - SGNL API integration")
            sudo.log_info("  - Unified configuration system")
            sudo.log_info("  - Enhanced error handling")
        return sudo.RC.OK

    def close(self, exit_status: int, error: int) -> None:
        """Clean up plugin resources."""
        if error:
            self.sgnl_client._log_error(f"Command error: {error}")
        else:
            self.sgnl_client._log_debug(f"Command exited with status {exit_status}")

    # -- Helper functions --

    def _is_command_allowed(self, cmd: str) -> bool:
        """Check if a specific command is allowed."""
        try:
            principal_id = self.sgnl_client.get_principal_id()
            result = self.sgnl_client.evaluate_access(principal_id, cmd, "execute")
            return result.is_allowed()
        except Exception as e:
            self.sgnl_client._log_error(f"Error checking command allowance: {e}")
            return False

    def _find_on_path(self, cmd: str) -> str:
        """Find the absolute path of a command."""
        if os.path.isabs(cmd):
            return cmd
        return shutil.which(cmd) or cmd

    def _runas_pwd(self) -> Optional[pwd.struct_passwd]:
        """Get the runas user's passwd entry."""
        runas_uid = self._runas_uid()
        if runas_uid is not None:
            return pwd.getpwuid(runas_uid)
        return None

    def _runas_uid(self) -> Optional[int]:
        """Get the runas user ID."""
        return self.user_info.get("runas_uid")

    def _runas_gid(self) -> Optional[int]:
        """Get the runas group ID."""
        return self.user_info.get("runas_gid")