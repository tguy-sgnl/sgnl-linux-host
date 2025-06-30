"""
Tests for common validation utilities

Unit tests for the validation functions used by both C and Python implementations.
"""

import unittest
import sys
import os

# Add the common directory to the path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'common'))

from utils.validation import (
    validate_command,
    validate_username,
    validate_api_url,
    validate_api_token,
    sanitize_command,
    parse_command_args
)


class TestValidation(unittest.TestCase):
    """Test cases for validation utilities."""
    
    def test_validate_command_valid(self):
        """Test command validation with valid commands."""
        valid_commands = [
            "ls",
            "ls -la",
            "/usr/bin/python3",
            "echo 'hello world'",
            "sudo apt-get update"
        ]
        for cmd in valid_commands:
            with self.subTest(command=cmd):
                self.assertTrue(validate_command(cmd))
    
    def test_validate_command_invalid(self):
        """Test command validation with invalid commands."""
        invalid_commands = [
            None,
            "",
            "   ",
            "ls\x00",
            "echo\x01test",
            "\x00\x01\x02"
        ]
        for cmd in invalid_commands:
            with self.subTest(command=cmd):
                self.assertFalse(validate_command(cmd))
    
    def test_validate_username_valid(self):
        """Test username validation with valid usernames."""
        valid_usernames = [
            "user",
            "user123",
            "user_name",
            "user-name",
            "user123_name",
            "_user",
            "a",
            "user123456789"
        ]
        for username in valid_usernames:
            with self.subTest(username=username):
                self.assertTrue(validate_username(username))
    
    def test_validate_username_invalid(self):
        """Test username validation with invalid usernames."""
        invalid_usernames = [
            None,
            "",
            "123user",  # Starts with number
            "user@domain",  # Contains @
            "user.name",  # Contains .
            "user name",  # Contains space
            "user\x00",  # Contains null byte
            "-user"  # Starts with dash
        ]
        for username in invalid_usernames:
            with self.subTest(username=username):
                self.assertFalse(validate_username(username))
    
    def test_validate_api_url_valid(self):
        """Test API URL validation with valid URLs."""
        valid_urls = [
            "api.sgnl.com",
            "test-api.example.org",
            "sgnl.cloud",
            "a.b.c.d",
            "api-123.sgnl.com"
        ]
        for url in valid_urls:
            with self.subTest(url=url):
                self.assertTrue(validate_api_url(url))
    
    def test_validate_api_url_invalid(self):
        """Test API URL validation with invalid URLs."""
        invalid_urls = [
            None,
            "",
            "http://api.sgnl.com",  # Contains protocol
            "api.sgnl.com/path",  # Contains path
            "api.sgnl.com:8080",  # Contains port
            "api..sgnl.com",  # Double dots
            ".api.sgnl.com",  # Starts with dot
            "api.sgnl.com.",  # Ends with dot
            "api_sgnl.com"  # Contains underscore
        ]
        for url in invalid_urls:
            with self.subTest(url=url):
                self.assertFalse(validate_api_url(url))
    
    def test_validate_api_token_valid(self):
        """Test API token validation with valid tokens."""
        valid_tokens = [
            "token123",
            "token_123",
            "token-123",
            "token.123",
            "a" * 50,  # 50 characters
            "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyfQ.SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c"
        ]
        for token in valid_tokens:
            with self.subTest(token=token):
                self.assertTrue(validate_api_token(token))
    
    def test_validate_api_token_invalid(self):
        """Test API token validation with invalid tokens."""
        invalid_tokens = [
            None,
            "",
            "short",  # Too short
            "a" * 1001,  # Too long
            "token@123",  # Contains @
            "token 123",  # Contains space
            "token\x00123",  # Contains null byte
            "token(123)",  # Contains parentheses
        ]
        for token in invalid_tokens:
            with self.subTest(token=token):
                self.assertFalse(validate_api_token(token))
    
    def test_sanitize_command(self):
        """Test command sanitization for logging."""
        test_cases = [
            ("ls -la", "ls -la"),
            ("echo 'hello\nworld'", "echo 'hello\\nworld'"),
            ("echo 'hello\rworld'", "echo 'hello\\rworld'"),
            ("echo 'hello\x00world'", "echo 'hello\\x00world'"),
            ("a" * 300, "a" * 200 + "..."),  # Truncated
            ("", ""),
            (None, "")
        ]
        for input_cmd, expected in test_cases:
            with self.subTest(command=input_cmd):
                result = sanitize_command(input_cmd)
                self.assertEqual(result, expected)
    
    def test_parse_command_args(self):
        """Test command argument parsing."""
        test_cases = [
            ("ls", ("ls", [])),
            ("ls -la", ("ls", ["-la"])),
            ("echo hello world", ("echo", ["hello", "world"])),
            ("/usr/bin/python3 script.py", ("/usr/bin/python3", ["script.py"])),
            ("", ("", [])),
            ("   ", ("", [])),
            ("  ls  -la  ", ("ls", ["-la"]))
        ]
        for input_line, expected in test_cases:
            with self.subTest(command_line=input_line):
                result = parse_command_args(input_line)
                self.assertEqual(result, expected)


if __name__ == '__main__':
    unittest.main() 