from typing import Dict, Any, Optional, Tuple
import logging

from auth import is_valid_token
from config import DatasourceConfig, EntityConfig, get_adapter_config

logger = logging.getLogger(__name__)


class ValidationError(Exception):
    """Exception raised when request validation fails."""
    
    def __init__(self, message: str, error_code: int = 1):
        self.message = message
        self.error_code = error_code
        super().__init__(self.message)


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


def validate_get_page_request(request: Any, token: Optional[str]) -> Tuple[DatasourceConfig, EntityConfig, int, str]:
    """
    Validate a complete GetPage request.
    
    Args:
        request: The gRPC GetPageRequest
        token: Authentication token from metadata
        
    Returns:
        Tuple of (datasource_config, entity_config, page_size, cursor)
        
    Raises:
        ValidationError: If any part of the request is invalid
    """
    # Validate authentication
    #validate_authentication(token)
    
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
