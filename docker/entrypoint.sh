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
        log "  docker exec <container> test-as [username]    - Test sudo with SGNL"
        log "  docker exec <container> shell-as [username]   - Interactive shell"
        log "  docker exec <container> status                - Show service status"
        log "  docker exec <container> logs                  - Show adapter logs"
        log "  docker exec <container> restart               - Restart services"
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
        log "  test-as [username]    - Test sudo with SGNL"
        log "  shell-as [username]   - Switch to user shell"
        log "  status                - Show service status"
        log "  logs                  - Show adapter logs"
        log "  restart               - Restart services"
        log ""
        log "Starting interactive shell as root..."
        log "Use 'test-as alice' or 'shell-as alice' to test with a user"
        
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
        error "Available commands: start, foreground, test-as, shell-as, status, logs, restart, stop"
        exit 1
        ;;
esac 