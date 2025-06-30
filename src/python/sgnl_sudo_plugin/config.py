"""
Configuration management for SGNL sudo plugin

Handles loading and validation of plugin configuration.
Unified with C plugin configuration format.
Enhanced with advanced HTTP client options.
"""

import json
import os
from typing import Dict, Optional


class SgnlConfig:
    """Configuration manager for SGNL plugin."""
    
    DEFAULT_CONFIG_PATH = "/etc/sgnl/config.json"
    
    def __init__(self, config_path: Optional[str] = None):
        # Support SGNL_CONFIG_PATH environment variable (matches C plugin)
        env_config_path = os.getenv("SGNL_CONFIG_PATH")
        self.config_path = config_path or env_config_path or self.DEFAULT_CONFIG_PATH
        self.config: Dict = {}
        self.loaded = False
        
    def load(self) -> bool:
        """Load configuration from file."""
        try:
            if not os.path.exists(self.config_path):
                return False
                
            with open(self.config_path, 'r') as f:
                self.config = json.load(f)
                
            # Apply environment variable overrides (matches C plugin behavior)
            self._apply_env_overrides()
                
            # Validate required fields
            if self.validate():
                self.loaded = True
                return True
            return False
            
        except (json.JSONDecodeError, IOError) as e:
            # Log error if sudo module is available
            try:
                import sudo  # type: ignore
                sudo.log_error(f"SGNL: Failed to load config from {self.config_path}: {e}")
            except ImportError:
                # sudo module not available (e.g., during testing)
                pass
            return False
    
    def _apply_env_overrides(self):
        """Apply environment variable overrides (matches C plugin)."""
        env_overrides = {
            'SGNL_API_URL': 'api_url',
            'SGNL_API_TOKEN': 'api_token', 
            'SGNL_TENANT': 'tenant'
        }
        
        for env_var, config_key in env_overrides.items():
            env_value = os.getenv(env_var)
            if env_value:
                self.config[config_key] = env_value
    
    def validate(self) -> bool:
        """Validate configuration has required fields."""
        required_fields = ['api_url', 'api_token']
        for field in required_fields:
            if not self.config.get(field):
                return False
        return True
    
    def get(self, key: str, default=None):
        """Get configuration value."""
        return self.config.get(key, default)
    
    @property
    def api_url(self) -> Optional[str]:
        """Get API URL."""
        return self.config.get('api_url')
    
    @property
    def api_token(self) -> Optional[str]:
        """Get API token."""
        return self.config.get('api_token')
    
    @property
    def tenant(self) -> Optional[str]:
        """Get tenant ID."""
        return self.config.get('tenant')
    
    @property
    def asset_attribute(self) -> Optional[str]:
        """Get asset attribute (unified with C plugin)."""
        return self.config.get('asset_attribute', 'type')  # Default to 'type' like C plugin
    
    @property
    def enable_debug_logging(self) -> bool:
        """Check if debug logging is enabled (matches C plugin)."""
        # Environment variable takes precedence
        env_debug = os.getenv('SGNL_DEBUG')
        if env_debug:
            return env_debug.lower() in ('1', 'true', 'yes', 'on')
        
        # Fall back to config file
        return self.config.get('enable_debug_logging', False)
    
    @property 
    def enable_access_logging(self) -> bool:
        """Check if access logging is enabled (matches C plugin)."""
        # Environment variable takes precedence
        env_access = os.getenv('SGNL_ACCESS_LOGGING')
        if env_access:
            return env_access.lower() in ('1', 'true', 'yes', 'on')
            
        # Fall back to config file
        return self.config.get('enable_access_logging', False)
    
    # HTTP Client Configuration Options (enhanced beyond C plugin)
    
    @property
    def timeout_seconds(self) -> int:
        """Get request timeout in seconds (matches C plugin default: 30s)."""
        env_timeout = os.getenv('SGNL_TIMEOUT')
        if env_timeout:
            try:
                return int(env_timeout)
            except ValueError:
                pass
        return self.config.get('timeout_seconds', 30)
    
    @property
    def connect_timeout_seconds(self) -> int:
        """Get connection timeout in seconds (matches C plugin default: 10s)."""
        env_timeout = os.getenv('SGNL_CONNECT_TIMEOUT')
        if env_timeout:
            try:
                return int(env_timeout)
            except ValueError:
                pass
        return self.config.get('connect_timeout_seconds', 10)
    
    @property
    def retry_count(self) -> int:
        """Get retry count (matches C plugin default: 2)."""
        env_retry = os.getenv('SGNL_RETRY_COUNT')
        if env_retry:
            try:
                return int(env_retry)
            except ValueError:
                pass
        return self.config.get('retry_count', 2)
    
    @property
    def retry_delay_ms(self) -> int:
        """Get retry delay in milliseconds (matches C plugin default: 1000ms)."""
        env_delay = os.getenv('SGNL_RETRY_DELAY')
        if env_delay:
            try:
                return int(env_delay)
            except ValueError:
                pass
        return self.config.get('retry_delay_ms', 1000)
    
    @property
    def validate_ssl(self) -> bool:
        """Check if SSL certificate validation is enabled (default: True)."""
        env_ssl = os.getenv('SGNL_VALIDATE_SSL')
        if env_ssl:
            return env_ssl.lower() in ('1', 'true', 'yes', 'on')
        return self.config.get('validate_ssl', True)
    
    @property
    def user_agent(self) -> str:
        """Get custom user agent string."""
        env_ua = os.getenv('SGNL_USER_AGENT')
        if env_ua:
            return env_ua
        return self.config.get('user_agent', 'sgnl-sudo-plugin-python/1.0')
    
    @property
    def ca_bundle_path(self) -> Optional[str]:
        """Get custom CA bundle path for SSL validation."""
        env_ca = os.getenv('SGNL_CA_BUNDLE')
        if env_ca:
            return env_ca
        return self.config.get('ca_bundle_path')
    
    def is_valid(self) -> bool:
        """Check if configuration is valid and loaded."""
        return self.loaded and self.validate() 