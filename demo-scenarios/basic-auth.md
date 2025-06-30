# Basic Authentication Demo

This scenario demonstrates basic sudo authentication with SGNL policy enforcement.

## Setup

1. Generate configs with your SGNL credentials:
   ```bash
   ./generate-configs.sh
   ```

2. Start the test container:
   ```bash
   docker build -t sgnl-test .
   docker run -d --name sgnl-demo sgnl-test
   ```

## Demo Script

### 1. Show the configuration
```bash
# Show the sudo configuration
docker exec sgnl-demo cat /etc/sudo.conf

# Show the SGNL plugin config
docker exec sgnl-demo cat /etc/sgnl/config.json

# Verify plugins are installed
docker exec sgnl-demo ls -la /usr/lib/sudo/sgnl_policy_plugin.so
```

### 2. Test basic functionality
```bash
# Test with allowed user (alice)
docker exec sgnl-demo test-as alice

# Test with different user (bob)
docker exec sgnl-demo test-as bob

# Test with admin user
docker exec sgnl-demo test-as admin
```

### 3. Interactive testing
```bash
# Start interactive shell as alice
docker exec -it sgnl-demo shell-as alice

# Inside the container, try different commands:
alice@container:~$ sudo whoami        # Should work if allowed
alice@container:~$ sudo -l        # Should list allowed commands
alice@container:~$ sudo ls /root      # May work depending on policy
alice@container:~$ sudo rm /etc/passwd # Should be denied
alice@container:~$ exit
```

### 4. View logs and audit trail
```bash
# Check adapter status
docker exec sgnl-demo status

# View authorization logs
docker exec sgnl-demo logs

# Check sudo debug logs (if debug enabled)
docker exec sgnl-demo cat /var/log/sudo_debug 2>/dev/null || echo "Debug logging not enabled"
```

## Expected Results

- **Allowed users** (configured in SGNL policy): Commands succeed
- **Denied users**: Commands fail with SGNL policy denial
- **Audit trail**: All attempts logged with decision reasoning

## Troubleshooting

### Plugin not loading
```bash
# Check sudo configuration
docker exec sgnl-demo cat /etc/sudo.conf

# Verify plugin exists and has correct permissions
docker exec sgnl-demo ls -la /usr/lib/sudo/sgnl_policy_plugin.so
```

### Connection issues
```bash
# Check adapter service
docker exec sgnl-demo status
```

### Policy issues
- Verify policies are uploaded to SGNL SaaS
- Check user/asset mappings in SGNL console
- Review policy rules and conditions

## Cleanup

```bash
docker stop sgnl-demo
docker rm sgnl-demo
``` 