import json
import os
import sys
import secrets
import string
from pathlib import Path
from typing import List, Optional
import logging

logger = logging.getLogger(__name__)

# Get the current directory (equivalent to __dirname in Node.js)
_current_dir = Path(__file__).parent

# Set tokens path from environment variable or default to tokens.json in current directory
TOKENS_PATH = os.environ.get('AUTH_TOKENS_PATH', _current_dir / 'tokens.json')

# Global variable to cache loaded tokens
_valid_tokens: Optional[List[str]] = None


def generate_secure_token(length: int = 64) -> str:
    """
    Generate a cryptographically secure random token.
    
    Args:
        length: Length of the token to generate
        
    Returns:
        A secure random token string
    """
    # Use a mix of letters, digits, and some safe special characters
    alphabet = string.ascii_letters + string.digits + '-_'
    return ''.join(secrets.choice(alphabet) for _ in range(length))


def generate_default_tokens(count: int = 1) -> List[str]:
    """
    Generate a list of default authentication tokens.
    
    Args:
        count: Number of tokens to generate
        
    Returns:
        List of generated tokens
    """
    return [generate_secure_token() for _ in range(count)]


def save_tokens(tokens: List[str]) -> None:
    """
    Save authentication tokens to the tokens file.
    
    Args:
        tokens: List of tokens to save
        
    Raises:
        IOError: If tokens cannot be saved
    """
    try:
        # Ensure the directory exists
        tokens_path = Path(TOKENS_PATH)
        tokens_path.parent.mkdir(parents=True, exist_ok=True)
        
        # Save tokens to file with proper formatting
        with open(tokens_path, 'w', encoding='utf-8') as file:
            json.dump(tokens, file, indent=2)
        
        logger.info(f"Authentication tokens saved to {tokens_path}")
        
    except (IOError, OSError) as err:
        logger.error(f'Failed to save authentication tokens: {err}')
        raise


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
        
        # File doesn't exist, is empty, or contains no valid tokens - generate new ones
        logger.info(f"No valid tokens found in {tokens_path}, generating new authentication tokens...")
        
        # Generate new tokens
        new_tokens = generate_default_tokens(count=1)
        
        # Save them to the file
        save_tokens(new_tokens)
        
        # Set the global variable
        _valid_tokens = new_tokens
        
        logger.info(f"Generated and saved {len(new_tokens)} new authentication tokens")
        logger.info(f"Generated token: {new_tokens[0]}")  # Log the first token for reference
        
    except (json.JSONDecodeError, IOError, OSError) as err:
        logger.error(f'Failed to load or generate authentication tokens: {err}')
        print(f'Failed to load or generate authentication tokens: {err}', file=sys.stderr)
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


def get_valid_tokens() -> List[str]:
    """
    Get the list of valid authentication tokens.
    
    Returns:
        List of valid tokens
    """
    global _valid_tokens
    
    if _valid_tokens is None:
        load_tokens()
    
    assert _valid_tokens is not None
    return _valid_tokens.copy()


def add_token(token: str) -> None:
    """
    Add a new token to the valid tokens list and save to file.
    
    Args:
        token: Token to add
    """
    global _valid_tokens
    
    if _valid_tokens is None:
        load_tokens()
    
    assert _valid_tokens is not None
    
    if token not in _valid_tokens:
        _valid_tokens.append(token)
        save_tokens(_valid_tokens)
        logger.info(f"Added new authentication token")
