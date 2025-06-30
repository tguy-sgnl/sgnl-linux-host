import os
import pwd
import grp
import stat
import socket
import platform
import subprocess
import asyncio
from pathlib import Path
from typing import Dict, List, Any, Optional
import logging

logger = logging.getLogger(__name__)


async def get_users() -> List[Dict[str, Any]]:
    """
    Get all system users from /etc/passwd.
    
    Returns:
        List of user dictionaries with user information
    """
    users = []
    
    try:
        # Get all users from the system
        for user in pwd.getpwall():
            user_info = {
                'id': str(user.pw_uid),
                'name': user.pw_name,
                'uid': user.pw_uid,
                'gid': user.pw_gid,
                'gecos': user.pw_gecos,  # Full name/comment field
                'home_dir': user.pw_dir,
                'shell': user.pw_shell,
                'is_system_user': user.pw_uid < 1000,  # Common convention
                'has_login_shell': user.pw_shell not in ['/bin/false', '/usr/sbin/nologin', '/bin/sync', '/bin/halt']
            }
            
            # Add group information
            try:
                primary_group = grp.getgrgid(user.pw_gid)
                user_info['primary_group'] = primary_group.gr_name
            except KeyError:
                user_info['primary_group'] = str(user.pw_gid)
            
            users.append(user_info)
            
    except Exception as e:
        logger.error(f"Error getting users: {e}")
        
    return users


async def get_groups() -> List[Dict[str, Any]]:
    """
    Get all system groups from /etc/group.
    
    Returns:
        List of group dictionaries with group information
    """
    groups = []
    
    try:
        # Get all groups from the system
        for group in grp.getgrall():
            group_info = {
                'id': str(group.gr_gid),
                'name': group.gr_name,
                'gid': group.gr_gid,
                'members': list(group.gr_mem),
                'member_count': len(group.gr_mem),
                'is_system_group': group.gr_gid < 1000  # Common convention
            }
            groups.append(group_info)
            
    except Exception as e:
        logger.error(f"Error getting groups: {e}")
        
    return groups


async def get_executables(search_paths: Optional[List[str]] = None) -> List[Dict[str, Any]]:
    """
    Get executable files from specified directories.
    
    Args:
        search_paths: List of directories to search. Defaults to common bin directories.
        
    Returns:
        List of executable dictionaries with file information
    """
    if search_paths is None:
        search_paths = [
            '/usr/local/bin',
            '/usr/bin',
            '/bin',
            '/usr/sbin',
            '/sbin'
        ]
    
    executables = []
    
    for search_path in search_paths:
        try:
            path_obj = Path(search_path)
            if not path_obj.exists() or not path_obj.is_dir():
                continue
                
            for file_path in path_obj.iterdir():
                if file_path.is_file():
                    try:
                        file_stat = file_path.stat()
                        
                        # Check if file is executable
                        if file_stat.st_mode & stat.S_IEXEC:
                            executable_info = {
                                'id': str(file_path),
                                'name': file_path.name,
                                'path': str(file_path),
                                'directory': str(file_path.parent),
                                'size': file_stat.st_size,
                                'mode': oct(file_stat.st_mode),
                                'owner_uid': file_stat.st_uid,
                                'group_gid': file_stat.st_gid,
                                'modified_time': file_stat.st_mtime,
                                'is_executable': True
                            }
                            
                            # Try to get owner and group names
                            try:
                                owner = pwd.getpwuid(file_stat.st_uid)
                                executable_info['owner'] = owner.pw_name
                            except KeyError:
                                executable_info['owner'] = str(file_stat.st_uid)
                            
                            try:
                                group = grp.getgrgid(file_stat.st_gid)
                                executable_info['group'] = group.gr_name
                            except KeyError:
                                executable_info['group'] = str(file_stat.st_gid)
                            
                            executables.append(executable_info)
                            
                    except (OSError, PermissionError) as e:
                        logger.debug(f"Could not stat file {file_path}: {e}")
                        
        except (OSError, PermissionError) as e:
            logger.warning(f"Could not access directory {search_path}: {e}")
            
    return executables


async def get_pam_config() -> List[Dict[str, Any]]:
    """
    Get PAM configuration files and their contents.
    
    Returns:
        List of PAM configuration dictionaries
    """
    pam_configs = []
    pam_dir = Path('/etc/pam.d')
    
    try:
        if not pam_dir.exists():
            logger.warning("PAM directory /etc/pam.d does not exist")
            return pam_configs
            
        for config_file in pam_dir.iterdir():
            if config_file.is_file():
                try:
                    with open(config_file, 'r', encoding='utf-8', errors='ignore') as f:
                        content = f.read()
                    
                    file_stat = config_file.stat()
                    
                    pam_info = {
                        'id': str(config_file),
                        'name': config_file.name,
                        'path': str(config_file),
                        'content': content,
                        'size': file_stat.st_size,
                        'modified_time': file_stat.st_mtime,
                        'line_count': len(content.splitlines()),
                        'has_sgnl_module': 'pam_sgnl' in content.lower()
                    }
                    
                    pam_configs.append(pam_info)
                    
                except (OSError, PermissionError) as e:
                    logger.warning(f"Could not read PAM config {config_file}: {e}")
                    
    except (OSError, PermissionError) as e:
        logger.error(f"Error accessing PAM directory: {e}")
        
    return pam_configs


async def get_sudoers_config() -> List[Dict[str, Any]]:
    """
    Get sudoers configuration files and their contents.
    
    Returns:
        List of sudoers configuration dictionaries
    """
    sudoers_configs = []
    
    # Main sudoers file
    sudoers_files = [Path('/etc/sudoers')]
    
    # Additional sudoers.d directory
    sudoers_d = Path('/etc/sudoers.d')
    if sudoers_d.exists() and sudoers_d.is_dir():
        try:
            sudoers_files.extend([f for f in sudoers_d.iterdir() if f.is_file()])
        except (OSError, PermissionError) as e:
            logger.warning(f"Could not access sudoers.d directory: {e}")
    
    for config_file in sudoers_files:
        try:
            # Use sudo to read sudoers files safely
            result = await asyncio.create_subprocess_exec(
                'sudo', 'cat', str(config_file),
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE
            )
            stdout, stderr = await result.communicate()
            
            if result.returncode == 0:
                content = stdout.decode('utf-8', errors='ignore')
                file_stat = config_file.stat()
                
                sudoers_info = {
                    'id': str(config_file),
                    'name': config_file.name,
                    'path': str(config_file),
                    'content': content,
                    'size': file_stat.st_size,
                    'modified_time': file_stat.st_mtime,
                    'line_count': len(content.splitlines()),
                    'has_sgnl_plugin': 'sgnl' in content.lower()
                }
                
                sudoers_configs.append(sudoers_info)
            else:
                logger.warning(f"Could not read sudoers file {config_file}: {stderr.decode()}")
                
        except (OSError, PermissionError, FileNotFoundError) as e:
            logger.warning(f"Could not read sudoers config {config_file}: {e}")
            
    return sudoers_configs


async def get_host_info() -> Dict[str, Any]:
    """
    Get host/system information.
    
    Returns:
        Dictionary with host information
    """
    host_info: Dict[str, Any] = {
        'id': socket.gethostname(),
        'hostname': socket.gethostname(),
        'fqdn': socket.getfqdn(),
        'platform': platform.platform(),
        'system': platform.system(),
        'release': platform.release(),
        'version': platform.version(),
        'machine': platform.machine(),
        'processor': platform.processor(),
        'architecture': platform.architecture()[0],
        'python_version': platform.python_version(),
    }
    
    # Get additional system information
    try:
        # Get system uptime
        with open('/proc/uptime', 'r') as f:
            uptime_seconds = float(f.read().split()[0])
            host_info['uptime_seconds'] = uptime_seconds
    except (OSError, FileNotFoundError):
        pass
    
    try:
        # Get load average
        load_avg = os.getloadavg()
        host_info['load_average'] = {
            '1min': load_avg[0],
            '5min': load_avg[1],
            '15min': load_avg[2]
        }
    except (OSError, AttributeError):
        pass
    
    try:
        # Get memory information
        with open('/proc/meminfo', 'r') as f:
            meminfo = {}
            for line in f:
                if ':' in line:
                    key, value = line.split(':', 1)
                    meminfo[key.strip()] = value.strip()
            host_info['memory_info'] = meminfo
    except (OSError, FileNotFoundError):
        pass
    
    # Get network interfaces
    try:
        result = await asyncio.create_subprocess_exec(
            'ip', 'addr', 'show',
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE
        )
        stdout, stderr = await result.communicate()
        
        if result.returncode == 0:
            host_info['network_interfaces'] = stdout.decode('utf-8')
    except (OSError, FileNotFoundError):
        pass
    
    return host_info


# Convenience function to get all system data
async def get_all_system_data() -> Dict[str, Any]:
    """
    Get all system data in a single call.
    
    Returns:
        Dictionary containing all system information
    """
    tasks = {
        'users': get_users(),
        'groups': get_groups(),
        'executables': get_executables(),
        'pam_config': get_pam_config(),
        'sudoers_config': get_sudoers_config(),
        'host_info': get_host_info()
    }
    
    results = {}
    for key, task in tasks.items():
        try:
            results[key] = await task
        except Exception as e:
            logger.error(f"Error getting {key}: {e}")
            results[key] = [] if key != 'host_info' else {}
    
    return results
