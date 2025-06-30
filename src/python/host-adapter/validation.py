from typing import Dict, Any, Optional, Tuple, List, Callable
import logging
import json
import sys
from pathlib import Path
import functools
import asyncio

from config import DatasourceConfig, EntityConfig, get_adapter_config

# Get the current directory (equivalent to __dirname in Node.js)
_current_dir = Path(__file__).parent

# Set tokens path from environment variable or default to tokens.json in current directory
TOKENS_PATH = _current_dir / '.adapter_tokens'

# Global variable to cache loaded tokens
_valid_tokens: Optional[List[str]] = None

logger = logging.getLogger(__name__)


class ValidationError(Exception):
    """Exception raised when request validation fails."""
    
    def __init__(self, message: str, error_code: int = 1):
        self.message = message
        self.error_code = error_code
        super().__init__(self.message)


def load_tokens() -> None:
    """
    Load authentication tokens from the tokens file.
    If the file doesn't exist or is empty, generate new tokens and save them.
    
    Raises:
        SystemExit: If tokens cannot be loaded or generated
    """
    global _valid_tokens
    
    tokens_path = Path(TOKENS_PATH)
    
    try:
        # Try to load existing tokens
        if tokens_path.exists() and tokens_path.stat().st_size > 0:
            with open(tokens_path, 'r', encoding='utf-8') as file:
                content = file.read().strip()
                if content:
                    _valid_tokens = json.loads(content)
                    if _valid_tokens and len(_valid_tokens) > 0:
                        logger.info(f"Loaded {len(_valid_tokens)} authentication tokens from {tokens_path}")
                        return
        
    except (json.JSONDecodeError, IOError, OSError) as err:
        logger.error(f'Failed to load tokens: {err}')
        print(f'Failed to load tokens: {err}', file=sys.stderr)
        sys.exit(1)


def is_valid_token(token: str) -> bool:
    """
    Check if a token is valid by comparing against loaded tokens.
    
    Args:
        token: The token to validate
        
    Returns:
        True if token is valid, False otherwise
    """
    global _valid_tokens
    
    if _valid_tokens is None:
        load_tokens()  # Lazy load on first use (may generate tokens)
    
    # After load_tokens(), _valid_tokens is guaranteed to be a list (or process exits)
    assert _valid_tokens is not None
    return token in _valid_tokens


def validate_authentication(token: Optional[str]) -> None:
    """
    Validate the authentication token.
    
    Args:
        token: Authentication token from request metadata
        
    Raises:
        ValidationError: If authentication fails
    """
    if not token:
        raise ValidationError("Missing authentication token", 1)
    
    if not is_valid_token(token):
        raise ValidationError("Invalid authentication token", 1)


def validate_datasource_config(datasource: DatasourceConfig) -> None:
    """
    Validate the datasource configuration.
    
    Args:
        datasource: Datasource configuration to validate
        
    Raises:
        ValidationError: If datasource config is invalid
    """
    logger.debug(f"Validating datasource config - id: '{datasource.id}', type: '{datasource.type}', address: '{datasource.address}'")
    
    if not datasource.is_valid():
        raise ValidationError(f"Invalid datasource configuration - missing required fields. ID: '{datasource.id}', Type: '{datasource.type}'", 2)
    
    # Add any additional datasource-specific validation here
    logger.debug(f"Validated datasource config: {datasource.id}")

    # Client authentication via Authorization header (Bearer token)
    #auth_header = metadata.get('authorization') or metadata.get('Authorization')
    #if auth_header and auth_header.startswith('Bearer '):
    #    client_token = auth_header[7:]  # Remove 'Bearer ' prefix
    #    logger.debug(f"Extracted client Bearer token from authorization header")
    #else:
    #    logger.warning(f"No Authorization header found. Available keys: {list(metadata.keys())}")


def validate_entity_config(entity: EntityConfig) -> None:
    """
    Validate the entity configuration.
    
    Args:
        entity: Entity configuration to validate
        
    Raises:
        ValidationError: If entity config is invalid
    """
    if not entity.is_valid():
        raise ValidationError("Invalid entity configuration", 4)
    
    # Check if entity type is supported
    adapter_config = get_adapter_config()
    if not adapter_config.is_entity_supported(entity.external_id):
        raise ValidationError(f"Unsupported entity type: {entity.external_id}", 4)
    
    # Validate attributes
    for attr in entity.attributes:
        if hasattr(attr, 'id') and not attr.id:
            raise ValidationError("Attribute missing required 'id' field", 5)
        elif isinstance(attr, dict) and not attr.get('id'):
            raise ValidationError("Attribute missing required 'id' field", 5)
    
    logger.debug(f"Validated entity config: {entity.id} ({entity.external_id})")


def validate_page_request(page_size: Optional[int], cursor: Optional[str]) -> Tuple[int, str]:
    """
    Validate and normalize page request parameters.
    
    Args:
        page_size: Requested page size
        cursor: Page cursor
        
    Returns:
        Tuple of (effective_page_size, normalized_cursor)
        
    Raises:
        ValidationError: If page parameters are invalid
    """
    adapter_config = get_adapter_config()
    
    # Validate and normalize page size
    if page_size is not None and page_size <= 0:
        raise ValidationError("Page size must be positive", 1)
    
    effective_page_size = adapter_config.get_effective_page_size(page_size)
    
    # Normalize cursor
    normalized_cursor = cursor or ""
    
    logger.debug(f"Validated page request: size={effective_page_size}, cursor='{normalized_cursor}'")
    
    return effective_page_size, normalized_cursor


def validate_get_page_request(request: Any) -> Tuple[DatasourceConfig, EntityConfig, int, str]:
    """
    Validate a complete GetPage request.
    
    Args:
        request: The gRPC GetPageRequest
   
    Returns:
        Tuple of (datasource_config, entity_config, page_size, cursor)
        
    Raises:
        ValidationError: If any part of the request is invalid
    """
    
    # Extract and validate datasource config
    if not hasattr(request, 'datasource') or not request.datasource:
        raise ValidationError("Missing datasource configuration", 2)
    
    # Handle protobuf message - convert to dict
    if hasattr(request.datasource, 'id'):
        # It's a protobuf message, extract fields directly
        datasource_dict = {
            'id': getattr(request.datasource, 'id', ''),
            'type': getattr(request.datasource, 'type', ''),
            'address': getattr(request.datasource, 'address', ''),
            'config': getattr(request.datasource, 'config', b''),
            'auth': getattr(request.datasource, 'auth', {})
        }
    else:
        # It's already a dict
        datasource_dict = request.datasource
    
    datasource_config = DatasourceConfig(datasource_dict)
    validate_datasource_config(datasource_config)
    
    # Extract and validate entity config
    if not hasattr(request, 'entity') or not request.entity:
        raise ValidationError("Missing entity configuration", 4)
    
    # Handle protobuf message - convert to dict
    if hasattr(request.entity, 'id'):
        # It's a protobuf message, extract fields directly
        entity_dict = {
            'id': getattr(request.entity, 'id', ''),
            'external_id': getattr(request.entity, 'external_id', ''),
            'ordered': getattr(request.entity, 'ordered', False),
            'attributes': getattr(request.entity, 'attributes', []),
            'child_entities': getattr(request.entity, 'child_entities', [])
        }
    else:
        # It's already a dict
        entity_dict = request.entity
    
    entity_config = EntityConfig(entity_dict)
    validate_entity_config(entity_config)
    
    # Validate page parameters
    page_size = getattr(request, 'page_size', None)
    cursor = getattr(request, 'cursor', None)
    effective_page_size, normalized_cursor = validate_page_request(page_size, cursor)
    
    logger.info(f"Validated GetPage request for entity {entity_config.external_id}")
    
    return datasource_config, entity_config, effective_page_size, normalized_cursor


def create_error_response(error: ValidationError) -> Dict[str, Any]:
    """
    Create a standardized error response.
    
    Args:
        error: The validation error
        
    Returns:
        Error response dictionary
    """
    return {
        'error': {
            'message': error.message,
            'code': error.error_code
        }
    }


def create_error_response_from_exception(exc: Exception, default_code: int = 11) -> Dict[str, Any]:
    """
    Create an error response from a generic exception.
    
    Args:
        exc: The exception
        default_code: Default error code to use
        
    Returns:
        Error response dictionary
    """
    if isinstance(exc, ValidationError):
        return create_error_response(exc)
    
    return {
        'error': {
            'message': str(exc),
            'code': default_code
        }
    }


def require_auth(func: Callable) -> Callable:
    """
    Decorator that validates authentication token for any gRPC service method.
    
    Args:
        func: The gRPC service method to protect
        
    Returns:
        Wrapped function that validates authentication before execution
    """
    @functools.wraps(func)
    async def wrapper(self, request, context):
        try:
            # Extract token from metadata
            metadata = dict(context.invocation_metadata())
            token = metadata.get('token')
            
            # Validate authentication
            validate_authentication(token)
            
            # If validation passes, call the original method
            return await func(self, request, context)
            
        except ValidationError as ve:
            logger.warning(f"Authentication failed for {func.__name__}: {ve.message}")
            # Create error response - this needs to be customized per service
            return self._create_auth_error_response(ve)
        except Exception as e:
            logger.error(f"Unexpected authentication error for {func.__name__}: {e}")
            return self._create_auth_error_response(
                ValidationError(f"Internal authentication error: {str(e)}", 11)
            )
    
    return wrapper


class AuthenticatedServicerMixin:
    """
    Mixin class that provides authentication for all gRPC service methods.
    All methods in classes that inherit from this mixin will automatically
    require authentication.
    """
    
    def _create_auth_error_response(self, validation_error: ValidationError):
        """
        Create an authentication error response.
        Override this method in your service class to match your response format.
        """
        raise NotImplementedError("Override _create_auth_error_response in your service class")
    
    def __init_subclass__(cls, **kwargs):
        """
        Automatically apply authentication to all async methods in the class.
        """
        super().__init_subclass__(**kwargs)
        
        # Find all async methods and apply authentication
        for attr_name in dir(cls):
            attr = getattr(cls, attr_name)
            if callable(attr) and asyncio.iscoroutinefunction(attr) and not attr_name.startswith('_'):
                # Apply the authentication decorator
                setattr(cls, attr_name, require_auth(attr))