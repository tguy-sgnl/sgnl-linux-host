#!/bin/bash
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log() {
    echo -e "${GREEN}[SGNL]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[SGNL]${NC} $1"
}

error() {
    echo -e "${RED}[SGNL]${NC} $1"
}

# Function to create user if it doesn't exist
create_user() {
    local username="$1"
    if ! id "$username" &>/dev/null; then
        log "Creating user: $username"
        useradd -m -s /bin/bash "$username"
        echo "$username:password" | chpasswd
        usermod -aG sudo "$username"
        log "User $username created with password 'password'"
    else
        log "User $username already exists"
    fi
}

# Function to create locked-down user
create_locked_user() {
    local username="$1"
    if [ -z "$username" ]; then
        error "Username required for lockdown"
        return 1
    fi
    
    log "Creating locked-down user: $username"
    
    # Create user first
    create_user "$username"
    
    # Apply lockdown
    /usr/local/bin/lockdown.sh "$username"
    
    log "✅ User $username is now locked down"
    log "🔒 Restrictions applied:"
    log "   - Restricted shell with limited commands"
    log "   - Confined to home directory only"
    log "   - No directory traversal allowed"
    log "   - No network access"
    log "   - Resource limits applied"
    log "   - Activity logging enabled"
}

# Function to test lockdown restrictions
test_lockdown() {
    local username="${1:-$TEST_USERNAME}"
    log "Testing lockdown restrictions for user: $username"
    
    # Ensure user exists and is locked down
    if ! id "$username" &>/dev/null; then
        create_locked_user "$username"
    fi
    
    log "🧪 Running lockdown tests..."
    
    # Test 1: Basic functionality
    log "Test 1: Basic file operations..."
    su - "$username" -c "pwd; ls -la; echo 'test' > testfile.txt; cat testfile.txt; rm testfile.txt" || warn "Some operations failed (expected)"
    
    # Test 2: Directory traversal
    log "Test 2: Directory traversal prevention..."
    su - "$username" -c "cd /etc 2>&1" && error "Directory traversal should be blocked" || log "✅ Directory traversal blocked"
    
    # Test 3: Dangerous commands
    log "Test 3: Dangerous command prevention..."
    su - "$username" -c "sudo whoami 2>&1" && error "Sudo should be blocked" || log "✅ Sudo blocked"
    
    # Test 4: Network access
    log "Test 4: Network access prevention..."
    su - "$username" -c "ping -c1 8.8.8.8 2>&1" && error "Network access should be blocked" || log "✅ Network access blocked"
    
    # Test 5: Command restrictions
    log "Test 5: Command restrictions..."
    su - "$username" -c "which bash 2>&1" && error "System commands should be restricted" || log "✅ System commands restricted"
    
    log "✅ Lockdown tests completed"
}

# Function to show lockdown status
lockdown_status() {
    local username="${1:-$TEST_USERNAME}"
    if ! id "$username" &>/dev/null; then
        warn "User $username does not exist"
        return 1
    fi
    
    log "Lockdown status for user: $username"
    echo "=================================="
    
    # Check shell
    local shell=$(grep "^$username:" /etc/passwd | cut -d: -f7)
    if [[ "$shell" == *"lockdown-shell"* ]]; then
        log "✅ Restricted shell: $shell"
    else
        warn "❌ Standard shell: $shell"
    fi
    
    # Check resource limits
    if grep -q "^$username" /etc/security/limits.conf; then
        log "✅ Resource limits applied"
    else
        warn "❌ No resource limits found"
    fi
    
    # Check SSH access
    if grep -q "DenyUsers.*$username" /etc/ssh/sshd_config* 2>/dev/null; then
        log "✅ SSH access denied"
    else
        warn "❌ SSH access not explicitly denied"
    fi
    
    # Check monitoring
    if [ -f "/var/log/restricted-users.log" ]; then
        log "✅ Activity logging enabled"
        echo "Recent activity:"
        tail -5 /var/log/restricted-users.log 2>/dev/null || echo "No recent activity"
    else
        warn "❌ Activity logging not found"
    fi
}

# Function to monitor locked user
monitor_locked_user() {
    local username="${1:-$TEST_USERNAME}"
    if ! id "$username" &>/dev/null; then
        error "User $username does not exist"
        return 1
    fi
    
    log "Monitoring locked user: $username"
    log "Press Ctrl+C to stop monitoring"
    
    if [ -f "/var/log/restricted-users.log" ]; then
        tail -f /var/log/restricted-users.log | grep "$username"
    else
        warn "No activity log found"
    fi
}

# Function to setup configuration
setup_config() {
    log "SGNL configuration already installed from build"
    
    # Verify config exists
    if [ ! -f "/etc/sgnl/config.json" ]; then
        error "Configuration not found! Run generate-configs.sh before building Docker image"
        exit 1
    fi
    
    if [ ! -f "/etc/sudo.conf" ]; then
        error "Sudo configuration not found! Run generate-configs.sh before building Docker image"
        exit 1
    fi
    
    log "✅ Configuration verified"
}

# Function to start host adapter
start_adapter() {
    log "Starting SGNL host adapter..."
    cd /app/host-adapter
    export PYTHONPATH=/app/host-adapter
    python3 adapter.py > /var/log/sgnl-adapter.log 2>&1 &
    local adapter_pid=$!
    echo $adapter_pid > /var/run/sgnl-adapter.pid
    
    # Wait a moment and check if it started
    sleep 2
    if kill -0 $adapter_pid 2>/dev/null; then
        log "Host adapter started (PID: $adapter_pid)"
        return 0
    else
        error "Failed to start host adapter"
        cat /var/log/sgnl-adapter.log
        return 1
    fi
}

# Function to stop adapter
stop_adapter() {
    if [ -f /var/run/sgnl-adapter.pid ]; then
        local pid=$(cat /var/run/sgnl-adapter.pid)
        if kill -0 $pid 2>/dev/null; then
            log "Stopping host adapter (PID: $pid)"
            kill $pid
            rm -f /var/run/sgnl-adapter.pid
        fi
    fi
}

# Function to show adapter status
adapter_status() {
    if [ -f /var/run/sgnl-adapter.pid ]; then
        local pid=$(cat /var/run/sgnl-adapter.pid)
        if kill -0 $pid 2>/dev/null; then
            log "Host adapter is running (PID: $pid)"
            return 0
        else
            warn "Host adapter PID file exists but process is not running"
            rm -f /var/run/sgnl-adapter.pid
            return 1
        fi
    else
        warn "Host adapter is not running"
        return 1
    fi
}

# Function to show logs
show_logs() {
    if [ -f /var/log/sgnl-adapter.log ]; then
        tail -f /var/log/sgnl-adapter.log
    else
        warn "No adapter logs found"
    fi
}

# Function to test sudo with SGNL
test_sudo() {
    local username="${1:-$TEST_USERNAME}"
    log "Testing sudo with SGNL for user: $username"
    
    # Ensure user exists
    create_user "$username"
    
    log "Attempting sudo command as $username..."
    log "Note: This will use SGNL policy plugin for authorization"
    
    # Switch to user and try sudo
    su - "$username" -c "sudo whoami" || {
        warn "Sudo command failed - this is expected if SGNL denies access"
        warn "Check adapter logs for authorization details"
    }
}

# Function to run interactive shell as user
shell_as() {
    local username="${1:-$TEST_USERNAME}"
    create_user "$username"
    log "Starting shell as user: $username"
    log "Use 'sudo <command>' to test SGNL authorization"
    exec su - "$username"
}

# Function to run interactive shell as locked user
shell_as_locked() {
    local username="${1:-$TEST_USERNAME}"
    if ! id "$username" &>/dev/null; then
        create_locked_user "$username"
    fi
    log "Starting restricted shell as locked user: $username"
    log "This user has severe restrictions - only basic file operations allowed"
    exec su - "$username"
}

# Cleanup function
cleanup() {
    log "Shutting down SGNL services..."
    stop_adapter
    exit 0
}

# Set up signal handlers
trap cleanup SIGTERM SIGINT

# Handle different commands
case "${1:-interactive}" in
    "start")
        log "🚀 Starting SGNL Linux Host container..."
        setup_config
        start_adapter
        log "✅ SGNL services started"
        log "Container ready for testing!"
        log ""
        log "Available commands:"
        log "  docker exec <container> test-as [username]        - Test sudo with SGNL"
        log "  docker exec <container> shell-as [username]       - Interactive shell"
        log "  docker exec <container> lockdown [username]       - Create locked user"
        log "  docker exec <container> test-lockdown [username]  - Test lockdown"
        log "  docker exec <container> shell-locked [username]   - Shell as locked user"
        log "  docker exec <container> status                    - Show service status"
        log "  docker exec <container> logs                      - Show adapter logs"
        log "  docker exec <container> restart                   - Restart services"
        log ""
        
        # Keep container running
        while true; do
            sleep 30
            if ! adapter_status >/dev/null; then
                warn "Host adapter stopped, restarting..."
                start_adapter
            fi
        done
        ;;
    
    "interactive")
        log "🚀 Starting SGNL Linux Host container..."
        setup_config
        start_adapter
        log "✅ SGNL services started"
        log "Container ready for testing!"
        log ""
        log "Available commands:"
        log "  test-as [username]        - Test sudo with SGNL"
        log "  shell-as [username]       - Switch to user shell"
        log "  lockdown [username]       - Create locked-down user"
        log "  test-lockdown [username]  - Test lockdown restrictions"
        log "  shell-locked [username]   - Shell as locked user"
        log "  lockdown-status [username] - Show lockdown status"
        log "  monitor-locked [username] - Monitor locked user activity"
        log "  status                    - Show service status"
        log "  logs                      - Show adapter logs"
        log "  restart                   - Restart services"
        log ""
        log "Starting interactive shell as root..."
        log "Use 'lockdown alice' to create a locked-down user"
        
        # Create wrapper scripts for easy access to functions
        cat > /usr/local/bin/test-as << 'EOF'
#!/bin/bash
/app/entrypoint.sh test-as "$@"
EOF
        chmod +x /usr/local/bin/test-as
        
        cat > /usr/local/bin/shell-as << 'EOF'
#!/bin/bash
/app/entrypoint.sh shell-as "$@"
EOF
        chmod +x /usr/local/bin/shell-as
        
        cat > /usr/local/bin/lockdown << 'EOF'
#!/bin/bash
/app/entrypoint.sh lockdown "$@"
EOF
        chmod +x /usr/local/bin/lockdown
        
        cat > /usr/local/bin/test-lockdown << 'EOF'
#!/bin/bash
/app/entrypoint.sh test-lockdown "$@"
EOF
        chmod +x /usr/local/bin/test-lockdown
        
        cat > /usr/local/bin/shell-locked << 'EOF'
#!/bin/bash
/app/entrypoint.sh shell-locked "$@"
EOF
        chmod +x /usr/local/bin/shell-locked
        
        cat > /usr/local/bin/lockdown-status << 'EOF'
#!/bin/bash
/app/entrypoint.sh lockdown-status "$@"
EOF
        chmod +x /usr/local/bin/lockdown-status
        
        cat > /usr/local/bin/monitor-locked << 'EOF'
#!/bin/bash
/app/entrypoint.sh monitor-locked "$@"
EOF
        chmod +x /usr/local/bin/monitor-locked
        
        cat > /usr/local/bin/status << 'EOF'
#!/bin/bash
/app/entrypoint.sh status "$@"
EOF
        chmod +x /usr/local/bin/status
        
        cat > /usr/local/bin/logs << 'EOF'
#!/bin/bash
/app/entrypoint.sh logs "$@"
EOF
        chmod +x /usr/local/bin/logs
        
        cat > /usr/local/bin/restart << 'EOF'
#!/bin/bash
/app/entrypoint.sh restart "$@"
EOF
        chmod +x /usr/local/bin/restart
        
        # Change to /app directory and start interactive bash shell
        cd /app
        exec /bin/bash
        ;;
    
    "foreground")
        log "🚀 Starting SGNL in foreground mode..."
        setup_config
        cd /app/host-adapter
        export PYTHONPATH=/app/host-adapter
        exec python3 adapter.py
        ;;
    
    "test-as")
        test_sudo "$2"
        ;;
    
    "shell-as")
        shell_as "$2"
        ;;
    
    "lockdown")
        create_locked_user "$2"
        ;;
    
    "test-lockdown")
        test_lockdown "$2"
        ;;
    
    "shell-locked")
        shell_as_locked "$2"
        ;;
    
    "lockdown-status")
        lockdown_status "$2"
        ;;
    
    "monitor-locked")
        monitor_locked_user "$2"
        ;;
    
    "status")
        adapter_status
        ;;
    
    "logs")
        show_logs
        ;;
    
    "restart")
        stop_adapter
        sleep 1
        start_adapter
        ;;
    
    "stop")
        stop_adapter
        ;;
    
    *)
        error "Unknown command: $1"
        error "Available commands: start, foreground, test-as, shell-as, lockdown, test-lockdown, shell-locked, lockdown-status, monitor-locked, status, logs, restart, stop"
        exit 1
        ;;
esac 