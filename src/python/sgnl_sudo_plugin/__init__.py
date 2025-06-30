"""
SGNL sudo plugin package

This package provides SGNL integration for sudo access control.
Enhanced with comprehensive error handling and standardized result codes.
"""

__version__ = "1.0.0"
__author__ = "SGNL Team"

# Always available components (no sudo dependency)
from .config import SgnlConfig
from .errors import SgnlResult, SgnlError, SgnlAccessResult

# Conditionally import components that require sudo module
try:
    from .plugin import SudoPolicyPlugin
    from .client import SgnlClient
    __all__ = ['SudoPolicyPlugin', 'SgnlClient', 'SgnlConfig', 'SgnlResult', 'SgnlError', 'SgnlAccessResult']
except ImportError:
    # sudo module not available (e.g., during testing)
    __all__ = ['SgnlConfig', 'SgnlResult', 'SgnlError', 'SgnlAccessResult'] 