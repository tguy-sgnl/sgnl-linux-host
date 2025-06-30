"""
SGNL Error Handling System

Standardized error codes and handling that matches the C plugin's libsgnl error system.
Provides consistent error reporting and graceful fallback behavior.
"""

from enum import IntEnum
from typing import Optional, Dict, Any
import time


class SgnlResult(IntEnum):
    """SGNL result codes (matches C plugin libsgnl.h)."""
    
    # Success codes
    OK = 0                    # Success
    ALLOWED = 2               # Access allowed
    
    # Denial codes  
    DENIED = 1                # Access denied
    
    # Error codes
    ERROR = 3                 # General error
    CONFIG_ERROR = 4          # Configuration error
    NETWORK_ERROR = 5         # Network/HTTP error
    AUTH_ERROR = 6            # Authentication error
    TIMEOUT_ERROR = 7         # Timeout error
    INVALID_REQUEST = 8       # Invalid request
    MEMORY_ERROR = 9          # Memory allocation error


class SgnlError(Exception):
    """Base exception for SGNL operations."""
    
    def __init__(self, message: str, result_code: SgnlResult = SgnlResult.ERROR, 
                 request_id: Optional[str] = None, details: Optional[Dict[str, Any]] = None):
        super().__init__(message)
        self.result_code = result_code
        self.request_id = request_id
        self.details = details or {}
        self.timestamp = time.time()
    
    def __str__(self) -> str:
        base_msg = super().__str__()
        if self.request_id:
            return f"{base_msg} (request_id: {self.request_id})"
        return base_msg


class SgnlConfigError(SgnlError):
    """Configuration-related errors."""
    
    def __init__(self, message: str, request_id: Optional[str] = None, details: Optional[Dict[str, Any]] = None):
        super().__init__(message, SgnlResult.CONFIG_ERROR, request_id, details)


class SgnlNetworkError(SgnlError):
    """Network/HTTP-related errors."""
    
    def __init__(self, message: str, request_id: Optional[str] = None, 
                 status_code: Optional[int] = None, details: Optional[Dict[str, Any]] = None):
        details = details or {}
        if status_code:
            details['status_code'] = status_code
        super().__init__(message, SgnlResult.NETWORK_ERROR, request_id, details)


class SgnlAuthError(SgnlError):
    """Authentication-related errors."""
    
    def __init__(self, message: str, request_id: Optional[str] = None, details: Optional[Dict[str, Any]] = None):
        super().__init__(message, SgnlResult.AUTH_ERROR, request_id, details)


class SgnlTimeoutError(SgnlError):
    """Timeout-related errors."""
    
    def __init__(self, message: str, request_id: Optional[str] = None, 
                 timeout_seconds: Optional[int] = None, details: Optional[Dict[str, Any]] = None):
        details = details or {}
        if timeout_seconds:
            details['timeout_seconds'] = timeout_seconds
        super().__init__(message, SgnlResult.TIMEOUT_ERROR, request_id, details)


class SgnlAccessResult:
    """
    Access evaluation result (matches C plugin sgnl_access_result_t).
    Provides detailed information about access decisions.
    """
    
    def __init__(self):
        self.result: SgnlResult = SgnlResult.ERROR
        self.decision: str = ""
        self.reason: str = ""
        self.asset_id: str = ""
        self.action: str = ""
        self.principal_id: str = ""
        self.timestamp: float = time.time()
        self.request_id: str = ""
        self.error_message: str = ""
        self.error_code: int = 0
        self.attributes: Dict[str, Any] = {}
    
    def is_allowed(self) -> bool:
        """Check if access is allowed."""
        return self.result == SgnlResult.ALLOWED or self.decision == "Allow"
    
    def is_denied(self) -> bool:
        """Check if access is denied."""
        return self.result == SgnlResult.DENIED or self.decision == "Deny"
    
    def is_error(self) -> bool:
        """Check if result represents an error."""
        return self.result not in (SgnlResult.OK, SgnlResult.ALLOWED, SgnlResult.DENIED)
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for logging/debugging."""
        return {
            'result': self.result.name,
            'decision': self.decision,
            'reason': self.reason,
            'asset_id': self.asset_id,
            'action': self.action,
            'principal_id': self.principal_id,
            'timestamp': self.timestamp,
            'request_id': self.request_id,
            'error_message': self.error_message,
            'error_code': self.error_code,
            'attributes': self.attributes
        }


def result_to_string(result: SgnlResult) -> str:
    """Convert result code to human-readable string (matches C plugin)."""
    result_strings = {
        SgnlResult.OK: "Success",
        SgnlResult.ALLOWED: "Access Allowed", 
        SgnlResult.DENIED: "Access Denied",
        SgnlResult.ERROR: "General Error",
        SgnlResult.CONFIG_ERROR: "Configuration Error",
        SgnlResult.NETWORK_ERROR: "Network Error",
        SgnlResult.AUTH_ERROR: "Authentication Error",
        SgnlResult.TIMEOUT_ERROR: "Timeout Error",
        SgnlResult.INVALID_REQUEST: "Invalid Request",
        SgnlResult.MEMORY_ERROR: "Memory Error"
    }
    return result_strings.get(result, f"Unknown Error ({result})")


def result_to_sudo_code(result: SgnlResult) -> int:
    """
    Convert SGNL result to sudo return code.
    Matches C plugin behavior for sudo integration.
    """
    try:
        import sudo  # type: ignore
        if result == SgnlResult.ALLOWED:
            return sudo.RC.ACCEPT
        elif result == SgnlResult.DENIED:
            return sudo.RC.REJECT
        else:
            return sudo.RC.ERROR
    except ImportError:
        # Fallback for testing without sudo module
        if result == SgnlResult.ALLOWED:
            return 1  # Accept
        elif result == SgnlResult.DENIED:
            return 0  # Reject
        else:
            return -1  # Error


def handle_http_error(status_code: int, response_text: str, request_id: str) -> SgnlError:
    """
    Create appropriate error based on HTTP status code.
    Provides detailed error classification for better debugging.
    """
    if status_code == 401:
        return SgnlAuthError(
            f"Authentication failed: Invalid API token",
            request_id=request_id,
            details={'response': response_text}
        )
    elif status_code == 403:
        return SgnlAuthError(
            f"Authorization failed: Insufficient permissions",
            request_id=request_id,
            details={'response': response_text}
        )
    elif status_code == 408:
        return SgnlTimeoutError(
            f"Request timeout",
            request_id=request_id,
            details={'response': response_text}
        )
    elif status_code >= 500:
        return SgnlNetworkError(
            f"Server error: {status_code}",
            request_id=request_id,
            status_code=status_code,
            details={'response': response_text}
        )
    else:
        return SgnlNetworkError(
            f"HTTP error: {status_code}",
            request_id=request_id,
            status_code=status_code,
            details={'response': response_text}
        )


def validate_principal_id(principal_id: str) -> bool:
    """Validate principal ID format (matches C plugin)."""
    if not principal_id or not isinstance(principal_id, str):
        return False
    if len(principal_id.strip()) == 0:
        return False
    if len(principal_id) > 255:  # Match C plugin limit
        return False
    return True


def validate_asset_id(asset_id: str) -> bool:
    """Validate asset ID format (matches C plugin)."""
    if not asset_id or not isinstance(asset_id, str):
        return False
    if len(asset_id.strip()) == 0:
        return False
    if len(asset_id) > 255:  # Match C plugin limit
        return False
    return True 