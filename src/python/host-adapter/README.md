# THIS IS CURSED. DO NOT USE ON A SYSTEM YOU CARE ABOUT

## SGNL Host Adapter

This host adapter provides a gRPC interface to expose system information (users, groups, executables, etc.) for SGNL authorization policies.

### Authentication

The adapter uses a two-layer authentication system:
- **Client-to-Adapter**: gRPC clients authenticate to the adapter using the `Authorization: Bearer <token>` header
- **Adapter-to-Datasource**: The adapter uses the `token` metadata for datasource authentication (not needed for local system data)

The adapter automatically generates client authentication tokens if none are present:

1. **Token File**: Tokens are stored in `tokens.json` by default (configurable via `AUTH_TOKENS_PATH` environment variable)
2. **Auto-Generation**: If the token file doesn't exist or is empty, the adapter will generate a cryptographically secure random token
3. **Persistence**: Generated tokens are automatically saved to the token file for future use
4. **Docker Support**: Token generation works in Docker containers and persists across restarts if the token file is in a mounted volume

### Token Generation

When the adapter starts and no valid tokens are found:
1. A 64-character secure random token is generated using `secrets.choice()`
2. The token is saved to the configured token file (`tokens.json`)
3. The token is logged to the console for reference
4. Future gRPC requests must include this token in the Authorization header as `Bearer <token>` to authenticate to the adapter

### Usage

#### Direct Execution
```bash
cd src/python/host-adapter
python3 adapter.py
```

#### Docker
```bash
# Build and run - tokens will be generated automatically
docker run -p 8082:8082 sgnl-host-adapter cmd-adapter

# Mount a volume to persist tokens across container restarts
docker run -p 8082:8082 -v $(pwd)/tokens:/app/tokens -e AUTH_TOKENS_PATH=/app/tokens/tokens.json sgnl-host-adapter cmd-adapter
```

### Environment Variables

- `AUTH_TOKENS_PATH`: Path to the authentication tokens file (default: `./tokens.json`)
- `GRPC_PORT`: gRPC server port (default: `8082`)
- `LOG_LEVEL`: Logging level (default: `INFO`)

### Security Note

**THIS IS A DEVELOPMENT/DEMO TOOL ONLY.** The auto-generated tokens are for convenience in testing and development environments. For production use:

1. Generate strong tokens externally
2. Use proper token rotation and management
3. Implement additional security measures as needed
4. Consider using mutual TLS instead of bearer tokens

### Token File Format

The tokens file is a JSON array of strings:
```json
[
  "generated-token-here",
  "additional-token-if-needed"
]
```