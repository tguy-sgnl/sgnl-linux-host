import os
import json
import logging
import asyncio
from concurrent import futures
from pathlib import Path
from typing import Dict, List, Any, Optional

import grpc
from grpc import aio

# Import generated protobuf classes
import adapter_pb2
import adapter_pb2_grpc

# Import our local modules
from config import get_adapter_config
from validation import AuthenticatedServicerMixin, validate_get_page_request, ValidationError, create_error_response_from_exception
from datasource import get_users, get_groups, get_executables, get_pam_config, get_sudoers_config, get_host_info

# Get current directory and set up paths
_current_dir = Path(__file__).parent
PROTO_PATH = _current_dir / 'proto' / 'adapter.proto'

# Set up logging
logging.basicConfig(level=logging.DEBUG)
logger = logging.getLogger(__name__)


class AdapterServicer(AuthenticatedServicerMixin, adapter_pb2_grpc.AdapterServicer):
    """gRPC service implementation for the SGNL adapter."""
    
    def _create_auth_error_response(self, validation_error: ValidationError):
        """Create an authentication error response."""
        error_response = adapter_pb2.GetPageResponse()
        error_response.error.message = validation_error.message
        error_response.error.code = adapter_pb2.ERROR_CODE_INTERNAL
        return error_response
    
    async def GetPage(self, request, context):
        """
        Handle GetPage gRPC requests.
        
        Args:
            request: GetPageRequest from client
            context: gRPC context containing metadata
            
        Returns:
            GetPageResponse with success or error
        """
        try:
            logger.info(f"GetPage request received for entity: {request.entity.external_id}")
            
            # Validate request using validation module
            try:
                datasource_config, entity_config, page_size, cursor = validate_get_page_request(request)
            except ValidationError as ve:
                logger.warning(f"Validation failed: {ve.message}")
                error_response = adapter_pb2.GetPageResponse()
                error_response.error.message = ve.message
                error_response.error.code = adapter_pb2.ERROR_CODE_INVALID_PAGE_REQUEST_CONFIG
                return error_response
            
            # Route to appropriate data source based on entity external_id
            sor_response = []
            entity_id = entity_config.external_id
            
            if entity_id == 'users':
                sor_response = await get_users()
            elif entity_id == 'groups':
                sor_response = await get_groups()
            elif entity_id == 'executables':
                # Extract follow_symlinks from datasource config
                follow_symlinks = False
                if hasattr(datasource_config, 'config') and datasource_config.config:
                    try:
                        if isinstance(datasource_config.config, bytes):
                            config_data = json.loads(datasource_config.config.decode('utf-8'))
                        else:
                            config_data = datasource_config.config
                        follow_symlinks = config_data.get('follow_symlinks', False)
                    except (json.JSONDecodeError, AttributeError, TypeError):
                        logger.warning("Could not parse datasource config for follow_symlinks, using default (False)")
                
                sor_response = await get_executables(follow_symlinks=follow_symlinks)
            elif entity_id == 'pam_config':
                sor_response = await get_pam_config()
            elif entity_id == 'sudoers_config':
                sor_response = await get_sudoers_config()
            elif entity_id == 'host_info':
                sor_response = [await get_host_info()]
            else:
                # This should not happen due to validation, but just in case
                error_response = adapter_pb2.GetPageResponse()
                error_response.error.message = f'Unsupported entity ID: {entity_id}'
                error_response.error.code = adapter_pb2.ERROR_CODE_INVALID_ENTITY_CONFIG
                return error_response
            
            logger.info(f"Retrieved {len(sor_response)} records for entity {entity_id}")
            
            # Implement pagination
            start_index = 0
            if cursor:
                try:
                    start_index = int(cursor)
                except (ValueError, TypeError):
                    start_index = 0
            
            # Apply pagination to the results
            end_index = start_index + page_size
            paginated_response = sor_response[start_index:end_index]
            
            # Determine next cursor
            next_cursor = ""
            if end_index < len(sor_response):
                next_cursor = str(end_index)
            
            logger.info(f"Returning page {start_index}-{min(end_index, len(sor_response))} of {len(sor_response)} total records (page_size={page_size})")
            
            # Transform data directly to protobuf objects
            objects = []
            for entity_data in paginated_response:
                obj = adapter_pb2.Object()
                
                # Map attributes directly from entity configuration
                for attr_config in entity_config.attributes:
                    attr = adapter_pb2.Attribute()
                    attr.id = attr_config.id
                    
                    # Extract value from entity data using external_id as key
                    external_id = attr_config.external_id
                    if external_id in entity_data:
                        raw_value = entity_data[external_id]
                        
                        # Handle list vs single values
                        values_to_process = raw_value if attr_config.list else [raw_value]
                        
                        for value in values_to_process:
                            if value is not None:
                                attr_value = adapter_pb2.AttributeValue()
                                self._set_protobuf_value(attr_value, value, attr_config.type)
                                attr.values.append(attr_value)
                    
                    # Only add attribute if it has values
                    if attr.values:
                        obj.attributes.append(attr)
                
                objects.append(obj)
            
            logger.info(f"Returning {len(objects)} objects for entity {entity_id} (cursor: '{cursor}' -> '{next_cursor}')")
            
            # Create successful response
            response = adapter_pb2.GetPageResponse()
            response.success.objects.extend(objects)
            response.success.next_cursor = next_cursor
            
            return response
            
        except Exception as err:
            logger.error(f'Error in GetPage: {err}', exc_info=True)
            error_response = adapter_pb2.GetPageResponse()
            error_response.error.message = str(err)
            error_response.error.code = adapter_pb2.ERROR_CODE_DATASOURCE_FAILED
            return error_response

    def _set_protobuf_value(self, attr_value, value, attr_type):
        """Set the appropriate protobuf field based on the attribute type."""
        # Handle protobuf enum types (numeric) and string types
        if isinstance(attr_type, int):
            # Protobuf enum values
            type_map = {
                0: 'ATTRIBUTE_TYPE_UNSPECIFIED',
                1: 'ATTRIBUTE_TYPE_BOOL',
                2: 'ATTRIBUTE_TYPE_DATE_TIME', 
                3: 'ATTRIBUTE_TYPE_DOUBLE',
                4: 'ATTRIBUTE_TYPE_DURATION',
                5: 'ATTRIBUTE_TYPE_INT64',
                6: 'ATTRIBUTE_TYPE_STRING'
            }
            type_name = type_map.get(attr_type, 'ATTRIBUTE_TYPE_STRING')
        else:
            # String type names
            type_name = str(attr_type)

        try:
            if type_name == 'ATTRIBUTE_TYPE_STRING':
                attr_value.string_value = str(value)
            elif type_name == 'ATTRIBUTE_TYPE_BOOL':
                if isinstance(value, bool):
                    attr_value.bool_value = value
                elif isinstance(value, str) and value.lower() in ('true', 'false'):
                    attr_value.bool_value = value.lower() == 'true'
                else:
                    attr_value.bool_value = bool(value)
            elif type_name == 'ATTRIBUTE_TYPE_INT64':
                attr_value.int64_value = int(value)
            elif type_name == 'ATTRIBUTE_TYPE_DOUBLE':
                attr_value.double_value = float(value)
            elif type_name == 'ATTRIBUTE_TYPE_DATE_TIME':
                # Handle datetime - for now, convert timestamp to protobuf format
                if isinstance(value, (int, float)):
                    attr_value.datetime_value.timestamp.seconds = int(value)
                    attr_value.datetime_value.timestamp.nanos = int((value % 1) * 1e9)
                else:
                    # Default to string if we can't parse the datetime
                    attr_value.string_value = str(value)
            else:
                # Default to string for unknown types
                attr_value.string_value = str(value)
        except (ValueError, TypeError):
            # Fallback to string if conversion fails
            attr_value.string_value = str(value)


async def serve():
    """Start the gRPC server."""   
    # Get adapter configuration
    adapter_config = get_adapter_config()
    
    server = aio.server(futures.ThreadPoolExecutor(max_workers=10))
    
    # Add the service to the server
    adapter_pb2_grpc.add_AdapterServicer_to_server(AdapterServicer(), server)
    
    listen_addr = f'0.0.0.0:{adapter_config.grpc_port}'
    server.add_insecure_port(listen_addr)
    
    logger.info(f"Starting gRPC server on {listen_addr}")
    await server.start()
    
    try:
        await server.wait_for_termination()
    except KeyboardInterrupt:
        logger.info("Shutting down gRPC server...")
        await server.stop(5)


def main():
    """Main entry point."""
    try:
        asyncio.run(serve())
    except KeyboardInterrupt:
        logger.info("Server interrupted by user")
    except Exception as e:
        logger.error(f"Server error: {e}", exc_info=True)


if __name__ == '__main__':
    main()
