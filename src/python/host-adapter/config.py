import os
import json
from typing import Dict, Any, Optional, List
from pathlib import Path
import logging

logger = logging.getLogger(__name__)

# Configuration constants
DEFAULT_GRPC_PORT = '8082'
DEFAULT_PAGE_SIZE = 100
MAX_PAGE_SIZE = 1000

# Get current directory for configuration files
_current_dir = Path(__file__).parent


class DatasourceConfig:
    """Configuration for a datasource."""
    
    def __init__(self, config_dict: Dict[str, Any]):
        self.id = config_dict.get('id', '')
        self.type = config_dict.get('type', '')
        self.address = config_dict.get('address', '')
        self.config = config_dict.get('config', b'')
        self.auth = config_dict.get('auth', {})
    
    def is_valid(self) -> bool:
        """Check if the datasource configuration is valid."""
        return bool(self.id and self.type)


class EntityConfig:
    """Configuration for an entity."""
    
    def __init__(self, config_dict: Dict[str, Any]):
        self.id = config_dict.get('id', '')
        self.external_id = config_dict.get('external_id', '')
        self.ordered = config_dict.get('ordered', False)
        self.attributes = config_dict.get('attributes', [])
        self.child_entities = config_dict.get('child_entities', [])
    
    def is_valid(self) -> bool:
        """Check if the entity configuration is valid."""
        return bool(self.id and self.external_id)
    
    def get_attribute_map(self) -> Dict[str, Dict[str, Any]]:
        """Get a mapping of external_id to attribute configuration."""
        attr_map = {}
        for attr in self.attributes:
            if hasattr(attr, 'external_id'):
                attr_map[attr.external_id] = attr
            elif isinstance(attr, dict) and 'external_id' in attr:
                attr_map[attr['external_id']] = attr
        return attr_map


class AdapterConfig:
    """Main adapter configuration."""
    
    def __init__(self):
        self.grpc_port = os.environ.get('GRPC_PORT', DEFAULT_GRPC_PORT)
        self.log_level = os.environ.get('LOG_LEVEL', 'INFO')
        self.max_page_size = int(os.environ.get('MAX_PAGE_SIZE', MAX_PAGE_SIZE))
        self.default_page_size = int(os.environ.get('DEFAULT_PAGE_SIZE', DEFAULT_PAGE_SIZE))
        
        # Load supported entity types
        self.supported_entities = self._load_supported_entities()
    
    def _load_supported_entities(self) -> List[str]:
        """Load the list of supported entity types."""
        # These are the entity types we support in our datasource
        return [
            'users',
            'groups', 
            'executables',
            'pam_config',
            'sudoers_config',
            'host_info'
        ]
    
    def is_entity_supported(self, entity_external_id: str) -> bool:
        """Check if an entity type is supported."""
        return entity_external_id in self.supported_entities
    
    def get_effective_page_size(self, requested_size: Optional[int]) -> int:
        """Get the effective page size to use."""
        if requested_size is None:
            return self.default_page_size
        
        return min(requested_size, self.max_page_size)


def load_adapter_config() -> AdapterConfig:
    """Load the adapter configuration."""
    return AdapterConfig()


def parse_datasource_config(datasource_dict: Dict[str, Any]) -> DatasourceConfig:
    """Parse datasource configuration from request."""
    return DatasourceConfig(datasource_dict)


def parse_entity_config(entity_dict: Dict[str, Any]) -> EntityConfig:
    """Parse entity configuration from request."""
    return EntityConfig(entity_dict)


# Global adapter configuration instance
_adapter_config: Optional[AdapterConfig] = None


def get_adapter_config() -> AdapterConfig:
    """Get the global adapter configuration instance."""
    global _adapter_config
    if _adapter_config is None:
        _adapter_config = load_adapter_config()
    return _adapter_config
