"""
Tests for SGNL client functionality

Basic unit tests for the SGNL client module.
"""

import unittest
import json
import tempfile
import os
from unittest.mock import patch, MagicMock

# Add the parent directory to the path to import the module
import sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from sgnl_sudo_plugin.client import SgnlClient


class TestSgnlClient(unittest.TestCase):
    """Test cases for SgnlClient class."""
    
    def setUp(self):
        """Set up test fixtures."""
        self.user_env = {}
        self.settings = {}
        self.user_info = {"user": "testuser"}
        
        # Create a temporary config file
        self.config_fd, self.config_path = tempfile.mkstemp()
        self.addCleanup(self.cleanup_config)
        
    def cleanup_config(self):
        """Clean up temporary config file."""
        os.close(self.config_fd)
        os.unlink(self.config_path)
    
    def test_init_with_valid_config(self):
        """Test client initialization with valid config."""
        config_data = {
            "api_url": "test.sgnlapis.cloud",
            "api_token": "test_token_123"
        }
        os.write(self.config_fd, json.dumps(config_data).encode())
        os.fsync(self.config_fd)
        
        with patch.object(SgnlClient, 'CONFIG_PATH', self.config_path):
            client = SgnlClient(self.user_env, self.settings, self.user_info)
            
        self.assertEqual(client.config.api_url, "test.sgnlapis.cloud")
        self.assertEqual(client.config.api_token, "test_token_123")
    
    def test_init_with_missing_config(self):
        """Test client initialization with missing config file."""
        with patch.object(SgnlClient, 'CONFIG_PATH', '/nonexistent/path'):
            client = SgnlClient(self.user_env, self.settings, self.user_info)
            
        self.assertIsNone(client.config.api_url)
        self.assertIsNone(client.config.api_token)
    
    def test_get_principal_id(self):
        """Test getting principal ID from user info."""
        client = SgnlClient(self.user_env, self.settings, self.user_info)
        principal_id = client.get_principal_id()
        self.assertEqual(principal_id, "testuser")
    
    def test_get_principal_id_fallback(self):
        """Test getting principal ID with fallback values."""
        user_info_no_user = {"username": "fallback_user"}
        client = SgnlClient(self.user_env, self.settings, user_info_no_user)
        principal_id = client.get_principal_id()
        self.assertEqual(principal_id, "fallback_user")
    
    def test_get_principal_id_unknown(self):
        """Test getting principal ID when no user info is available."""
        user_info_empty = {}
        client = SgnlClient(self.user_env, self.settings, user_info_empty)
        principal_id = client.get_principal_id()
        self.assertEqual(principal_id, "unknown")


if __name__ == '__main__':
    unittest.main() 