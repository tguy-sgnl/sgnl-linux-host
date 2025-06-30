"""
SGNL Client for Python sudo plugin

Handles SGNL API communication and access evaluation.
Unified with C plugin configuration and logging behavior.
Enhanced with comprehensive error handling, timeouts, and retry logic.
"""

import errno
import sys
import os
import http.client
import json
import uuid
import time
import socket
import ssl
from typing import Optional, Dict, Any, List
from .config import SgnlConfig
from .errors import (
    SgnlResult, SgnlError, SgnlConfigError, SgnlNetworkError, 
    SgnlAuthError, SgnlTimeoutError, SgnlAccessResult,
    handle_http_error, validate_principal_id, validate_asset_id,
    result_to_sudo_code
)


class SgnlClient:
    """SGNL API client for access evaluation."""

    def __init__(self, user_env, settings, user_info):
        self.user_env = user_env
        self.settings = settings
        self.user_info = user_info
        self.config = SgnlConfig()
        
        # State tracking
        self.last_error: Optional[str] = None
        self.last_request_id: Optional[str] = None
        
        self._load_config()

    def _load_config(self):
        """Load configuration using unified config system."""
        if not self.config.load():
            self.last_error = "Failed to load SGNL configuration"
            self._log_error(self.last_error)
            return
            
        if not self.config.is_valid():
            self.last_error = "Invalid SGNL configuration"
            self._log_error(self.last_error)
            return
            
        if self.config.enable_debug_logging:
            self._log_debug(f"Config loaded: tenant={self.config.tenant}, "
                          f"api_url={self.config.api_url}, "
                          f"asset_attribute={self.config.asset_attribute}, "
                          f"timeout={self.config.timeout_seconds}s, "
                          f"retry_count={self.config.retry_count}, "
                          f"validate_ssl={self.config.validate_ssl}")

    def _log_error(self, message: str):
        """Log error message (always enabled)."""
        try:
            import sudo  # type: ignore
            sudo.log_error(f"SGNL: {message}")
        except ImportError:
            print(f"SGNL ERROR: {message}", file=sys.stderr)

    def _log_info(self, message: str):
        """Log info message (only if access logging enabled)."""
        if self.config.enable_access_logging:
            try:
                import sudo  # type: ignore
                sudo.log_info(f"SGNL: {message}")
            except ImportError:
                print(f"SGNL INFO: {message}", file=sys.stderr)

    def _log_debug(self, message: str):
        """Log debug message (only if debug logging enabled)."""
        if self.config.enable_debug_logging:
            try:
                import sudo  # type: ignore
                sudo.log_info(f"SGNL DEBUG: {message}")
            except ImportError:
                print(f"SGNL DEBUG: {message}", file=sys.stderr)

    def _generate_request_id(self) -> str:
        """Generate unique request ID for tracking (matches C plugin)."""
        request_id = str(uuid.uuid4())
        self.last_request_id = request_id
        return request_id

    def get_last_error(self) -> Optional[str]:
        """Get last error message (matches C plugin API)."""
        return self.last_error

    def is_debug_enabled(self) -> bool:
        """Check if debug logging is enabled (matches C plugin API)."""
        return self.config.enable_debug_logging

    def is_access_logging_enabled(self) -> bool:
        """Check if access logging is enabled (matches C plugin API)."""
        return self.config.enable_access_logging

    def validate(self) -> SgnlResult:
        """Validate client configuration (matches C plugin API)."""
        if not self.config.is_valid():
            self.last_error = "Configuration validation failed"
            return SgnlResult.CONFIG_ERROR
        return SgnlResult.OK

    def _post_with_retry(self, endpoint: str, payload: str) -> Dict[str, Any]:
        """
        Send POST request with retry logic and comprehensive error handling.
        Matches C plugin timeout and retry behavior.
        """
        if not self.config.is_valid():
            raise SgnlConfigError("API URL or token not configured")
            
        request_id = self._generate_request_id()
        api_url = self.config.api_url
        tenant = self.config.tenant
        
        if not api_url:
            raise SgnlConfigError("API URL not configured", request_id)
        
        # Construct full API URL as tenant.api_url
        full_api_url = f"{tenant}.{api_url}" if tenant else api_url
        
        self._log_debug(f"Making API call to {full_api_url}{endpoint} (request_id: {request_id})")
        self._log_debug(f"Request payload: {payload}")
        
        last_error = None
        
        # Retry loop (matches C plugin retry behavior)
        for attempt in range(self.config.retry_count + 1):
            if attempt > 0:
                delay_ms = self.config.retry_delay_ms
                self._log_debug(f"Retrying request (attempt {attempt + 1}/{self.config.retry_count + 1}) "
                              f"after {delay_ms}ms delay")
                time.sleep(delay_ms / 1000.0)
            
            try:
                return self._post_single_attempt(full_api_url, endpoint, payload, request_id)
                
            except SgnlTimeoutError as e:
                last_error = e
                self._log_debug(f"Request timeout on attempt {attempt + 1}: {e}")
                if attempt == self.config.retry_count:
                    break
                continue
                
            except SgnlNetworkError as e:
                last_error = e
                # Don't retry on authentication errors
                if e.result_code == SgnlResult.AUTH_ERROR:
                    break
                self._log_debug(f"Network error on attempt {attempt + 1}: {e}")
                if attempt == self.config.retry_count:
                    break
                continue
                
            except Exception as e:
                # Unexpected error - don't retry
                last_error = SgnlError(f"Unexpected error: {e}", request_id=request_id)
                break
        
        # All retries exhausted
        if last_error:
            self.last_error = str(last_error)
            raise last_error
        else:
            error = SgnlNetworkError("All retry attempts failed", request_id)
            self.last_error = str(error)
            raise error

    def _post_single_attempt(self, api_url: str, endpoint: str, payload: str, request_id: str) -> Dict[str, Any]:
        """Single HTTP POST attempt with timeout and SSL validation."""
        conn = None
        try:
            # Create SSL context based on configuration
            if self.config.validate_ssl:
                # Use default SSL context with certificate validation
                ssl_context = ssl.create_default_context()
                if self.config.ca_bundle_path:
                    ssl_context.load_verify_locations(self.config.ca_bundle_path)
            else:
                # Disable SSL verification (not recommended for production)
                ssl_context = ssl.create_default_context()
                ssl_context.check_hostname = False
                ssl_context.verify_mode = ssl.CERT_NONE
                self._log_debug("SSL certificate validation disabled")
            
            # Create connection with timeout and SSL context
            conn = http.client.HTTPSConnection(
                api_url, 
                timeout=self.config.connect_timeout_seconds,
                context=ssl_context
            )
            
            headers = {
                'Content-Type': "application/json",
                'Accept': "application/json",
                'X-Request-Id': request_id,
                'Authorization': f"Bearer {self.config.api_token}",
                'User-Agent': self.config.user_agent
            }
            
            # Set overall timeout by using socket timeout
            conn.sock = None  # Ensure clean connection
            conn.connect()
            if conn.sock:
                conn.sock.settimeout(self.config.timeout_seconds)
            
            conn.request("POST", endpoint, payload, headers)
            res = conn.getresponse()
            data = res.read()
            
            self._log_debug(f"API response status: {res.status} (request_id: {request_id})")
            
            if res.status != 200:
                response_text = data.decode("utf-8", errors='replace')
                self._log_debug(f"Error response: {response_text}")
                raise handle_http_error(res.status, response_text, request_id)
                
            try:
                response_json = json.loads(data.decode("utf-8"))
                self._log_debug(f"API response: {response_json}")
                return response_json
            except json.JSONDecodeError as e:
                raise SgnlNetworkError(f"Failed to decode API response: {e}", request_id)
                
        except socket.timeout:
            raise SgnlTimeoutError(f"Request timed out after {self.config.timeout_seconds}s", 
                                 request_id, self.config.timeout_seconds)
        except socket.error as e:
            raise SgnlNetworkError(f"Network error: {e}", request_id)
        except Exception as e:
            if "timeout" in str(e).lower():
                raise SgnlTimeoutError(f"Connection timeout: {e}", request_id)
            raise SgnlNetworkError(f"HTTP request failed: {e}", request_id)
        finally:
            if conn:
                try:
                    conn.close()
                except:
                    pass

    def get_principal_id(self) -> str:
        """Get the principal (user) ID from user_info."""
        principal_id = self.user_info.get("user") or self.user_info.get("username") or "unknown"
        
        if not validate_principal_id(principal_id):
            self._log_error(f"Invalid principal ID: {principal_id}")
            return "unknown"
        
        return principal_id

    def check_access(self, principal_id: str, asset_id: str, action: str = "execute") -> SgnlResult:
        """
        Simple access check (matches C plugin sgnl_check_access).
        Returns standardized result codes.
        """
        try:
            result = self.evaluate_access(principal_id, asset_id, action)
            return result.result
        except SgnlError as e:
            self.last_error = str(e)
            return e.result_code
        except Exception as e:
            self.last_error = f"Unexpected error: {e}"
            return SgnlResult.ERROR

    def evaluate_access(self, principal_id: str, asset_id: str, action: str = "execute") -> SgnlAccessResult:
        """
        Detailed access evaluation (matches C plugin sgnl_evaluate_access).
        Returns comprehensive result with full details.
        """
        result = SgnlAccessResult()
        result.principal_id = principal_id
        result.asset_id = asset_id
        result.action = action
        result.request_id = self._generate_request_id()
        
        try:
            # Validate inputs
            if not validate_principal_id(principal_id):
                result.result = SgnlResult.INVALID_REQUEST
                result.error_message = f"Invalid principal ID: {principal_id}"
                return result
            
            if not validate_asset_id(asset_id):
                result.result = SgnlResult.INVALID_REQUEST
                result.error_message = f"Invalid asset ID: {asset_id}"
                return result
            
            # Build request payload
            payload_data = {
                "principal": {
                    "id": principal_id,
                },
                "queries": [
                    {
                        "action": action,
                        "assetId": asset_id
                    }
                ]
            }
            
            # Add asset attributes if configured
            if self.config.asset_attribute and self.config.asset_attribute != "id":
                asset_attrs = {self.config.asset_attribute: asset_id}
                payload_data["queries"][0]["asset"] = asset_attrs
            
            payload = json.dumps(payload_data)
            
            # Make API call
            response = self._post_with_retry("/access/v2/evaluations", payload)
            
            # Parse response
            decisions = response.get("decisions", [])
            if not decisions:
                result.result = SgnlResult.ERROR
                result.error_message = "No decisions in response"
                return result
            
            decision_data = decisions[0]
            result.decision = decision_data.get("decision", "")
            result.reason = decision_data.get("reason", "")
            
            if result.decision == "Allow":
                result.result = SgnlResult.ALLOWED
            elif result.decision == "Deny":
                result.result = SgnlResult.DENIED
            else:
                result.result = SgnlResult.ERROR
                result.error_message = f"Unknown decision: {result.decision}"
            
            return result
            
        except SgnlError as e:
            result.result = e.result_code
            result.error_message = str(e)
            result.error_code = e.result_code
            self.last_error = str(e)
            return result
        except Exception as e:
            result.result = SgnlResult.ERROR
            result.error_message = f"Unexpected error: {e}"
            self.last_error = str(e)
            return result

    def check_policy(self, argv: List[str]) -> int:
        """
        Check if the policy allows the given command (legacy interface).
        Returns sudo return codes for compatibility.
        """
        if not argv or not argv[0]:
            self._log_error("Invalid command arguments")
            return result_to_sudo_code(SgnlResult.ERROR)
            
        cmd = argv[0]
        principal_id = self.get_principal_id()
        
        self._log_debug(f"Checking policy for user '{principal_id}' command: {cmd}")
        
        try:
            result = self.evaluate_access(principal_id, cmd, "execute")
            
            self._log_debug(f"Decision for '{cmd}': {result.decision}")
            
            if result.is_allowed():
                self._log_info(f"Access granted for {principal_id} to run {cmd}")
                return result_to_sudo_code(SgnlResult.ALLOWED)
            elif result.is_denied():
                self._log_info(f"Access denied for {principal_id} to run {cmd}: {result.reason}")
                return result_to_sudo_code(SgnlResult.DENIED)
            else:
                self._log_error(f"Access evaluation error: {result.error_message}")
                return result_to_sudo_code(result.result)
                
        except Exception as e:
            self._log_error(f"Policy check failed: {e}")
            return result_to_sudo_code(SgnlResult.ERROR)

    def asset_search(self, action: str = "execute") -> List[str]:
        """Search for assets the principal is allowed to access for the given action."""
        principal_id = self.get_principal_id()
        
        try:
            payload = json.dumps({
                "principal": {
                    "id": principal_id,
                },
                "queries": [
                    {
                        "action": action
                    }
                ]
            })
            
            response = self._post_with_retry("/access/v2/search", payload)
            
            if response is None:
                self._log_error("Asset search returned no response")
                return []
                
            asset_ids = [d.get("assetId") for d in response.get("decisions", []) 
                        if d.get("decision") == "Allow"]
            return asset_ids
            
        except SgnlError as e:
            self._log_error(f"Asset search failed: {e}")
            return []
        except Exception as e:
            self._log_error(f"Asset search error: {e}")
            return [] 