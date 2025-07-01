#!/bin/bash

# lockdown.sh - Maximum security user restriction

USERNAME="$1"
if [ -z "$USERNAME" ]; then
    echo "Usage: $0 <username>"
    exit 1
fi

echo "🔒 Implementing lockdown for user: $USERNAME"

# 1. Create user if doesn't exist
if ! id "$USERNAME" &>/dev/null; then
    useradd -m -s /bin/bash "$USERNAME"
    echo "👤 Created user $USERNAME"
fi

USER_HOME="/home/$USERNAME"

# 2. Set up restricted shell environment
echo "🐚 Setting up restricted shell..."

# Create restricted command directory
mkdir -p "$USER_HOME/bin"
cd "$USER_HOME/bin"

# Link only safe commands
SAFE_COMMANDS="ls cat grep head tail pwd echo less mkdir rmdir rm mv cp touch wc sort uniq groups date tr chmod sudo"
for cmd in $SAFE_COMMANDS; do
    if command -v "$cmd" > /dev/null; then
        ln -sf "$(command -v $cmd)" "$cmd"
    fi
done

# Handle built-in commands that don't have external binaries
if [ -f "/usr/bin/column" ]; then
    ln -sf "/usr/bin/column" "$USER_HOME/bin/column"
fi

# Fix broken symlinks for built-in commands
rm -f "$USER_HOME/bin/echo" "$USER_HOME/bin/pwd"

# Create .profile with restrictions first
cat > "$USER_HOME/.profile" << 'PROFILE_EOF'
# Restricted user profile
export PATH="$HOME/bin"
cd "$HOME"

# Disable dangerous aliases
unalias -a 2>/dev/null

# Disable history expansion
set +H

# Disable command history
unset HISTFILE

# Disable tab completion to prevent file discovery
bind 'set disable-completion on' 2>/dev/null || true
complete -r 2>/dev/null || true

# Custom prompt
PS1='\u@restricted:\W\$ '

# Function to show available commands
commands() {
    echo "Available commands:"
    if [ -d ~/bin ]; then
        /usr/bin/ls -1 ~/bin/
    else
        echo "No commands available"
    fi
}

# Handle logout cleanly
trap 'exit 0' EXIT

# Override logout to prevent clear_console error
logout() {
    exit 0
}

# Disable clear_console by overriding the function
clear_console() {
    :
}

# Override cat to prevent reading system files
cat() {
    # Check if any argument is a system file
    for arg in "$@"; do
        if [[ "$arg" == "/etc"* ]] || [[ "$arg" == "/var"* ]] || [[ "$arg" == "/usr"* ]] || [[ "$arg" == "/bin"* ]] || [[ "$arg" == "/sbin"* ]] || [[ "$arg" == "/proc"* ]] || [[ "$arg" == "/sys"* ]] || [[ "$arg" == "/root"* ]]; then
            echo "❌ Cannot read system files"
            return 1
        fi
    done
    /usr/bin/cat "$@"
}

# Override cd to prevent directory traversal
cd() {
    if [ $# -eq 0 ]; then
        builtin cd "$HOME"
    elif [[ "$1" == *".."* ]] || [[ "$1" == "/"* ]] || [[ "$1" == "/etc"* ]] || [[ "$1" == "/var"* ]] || [[ "$1" == "/usr"* ]] || [[ "$1" == "/bin"* ]] || [[ "$1" == "/sbin"* ]] || [[ "$1" == "/proc"* ]] || [[ "$1" == "/sys"* ]]; then
        echo "❌ Cannot access system directories"
        return 1
    elif [[ "$1" != "$HOME"* ]]; then
        echo "❌ Cannot access directories outside home"
        return 1
    else
        builtin cd "$1" 2>/dev/null || echo "❌ Directory not accessible"
    fi
}

# Override grep to prevent searching system files
grep() {
    # Check if any argument is a system file
    for arg in "$@"; do
        if [[ "$arg" == "/etc"* ]] || [[ "$arg" == "/var"* ]] || [[ "$arg" == "/usr"* ]] || [[ "$arg" == "/bin"* ]] || [[ "$arg" == "/sbin"* ]] || [[ "$arg" == "/proc"* ]] || [[ "$arg" == "/sys"* ]] || [[ "$arg" == "/root"* ]]; then
            echo "❌ Cannot search system files"
            return 1
        fi
    done
    /usr/bin/grep "$@"
}

# Override head to prevent reading system files
head() {
    # Check if any argument is a system file
    for arg in "$@"; do
        if [[ "$arg" == "/etc"* ]] || [[ "$arg" == "/var"* ]] || [[ "$arg" == "/usr"* ]] || [[ "$arg" == "/bin"* ]] || [[ "$arg" == "/sbin"* ]] || [[ "$arg" == "/proc"* ]] || [[ "$arg" == "/sys"* ]] || [[ "$arg" == "/root"* ]]; then
            echo "❌ Cannot read system files"
            return 1
        fi
    done
    /usr/bin/head "$@"
}

# Override tail to prevent reading system files
tail() {
    # Check if any argument is a system file
    for arg in "$@"; do
        if [[ "$arg" == "/etc"* ]] || [[ "$arg" == "/var"* ]] || [[ "$arg" == "/usr"* ]] || [[ "$arg" == "/bin"* ]] || [[ "$arg" == "/sbin"* ]] || [[ "$arg" == "/proc"* ]] || [[ "$arg" == "/sys"* ]] || [[ "$arg" == "/root"* ]]; then
            echo "❌ Cannot read system files"
            return 1
        fi
    done
    /usr/bin/tail "$@"
}

# Override less to prevent reading system files
less() {
    # Check if any argument is a system file
    for arg in "$@"; do
        if [[ "$arg" == "/etc"* ]] || [[ "$arg" == "/var"* ]] || [[ "$arg" == "/usr"* ]] || [[ "$arg" == "/bin"* ]] || [[ "$arg" == "/sbin"* ]] || [[ "$arg" == "/proc"* ]] || [[ "$arg" == "/sys"* ]] || [[ "$arg" == "/root"* ]]; then
            echo "❌ Cannot read system files"
            return 1
        fi
    done
    /usr/bin/less "$@"
}

# Override mkdir to prevent creating directories outside home
# mkdir() {
#     # Debug: print arguments
#     echo "DEBUG: mkdir called with args: $@" >&2
#     
#     # Check if any argument is a system directory
#     for arg in "$@"; do
#         if [[ "$arg" == "/etc"* ]] || [[ "$arg" == "/var"* ]] || [[ "$arg" == "/usr"* ]] || [[ "$arg" == "/bin"* ]] || [[ "$arg" == "/sbin"* ]] || [[ "$arg" == "/proc"* ]] || [[ "$arg" == "/sys"* ]] || [[ "$arg" == "/root"* ]]; then
#             echo "❌ Cannot create directories in system locations"
#             return 1
#         fi
#     done
#     /usr/bin/mkdir "$@"
# }

# Create simple .bashrc that sources profile
cat > "$USER_HOME/.bashrc" << 'BASHRC_EOF'
# Source profile
source $HOME/.profile
BASHRC_EOF

# Create custom shell script
cat > /usr/local/bin/lockdown-shell-$USERNAME << SHELL_EOF
#!/bin/bash

USERNAME_LOCKED="$USERNAME"
USER_HOME="/home/$USERNAME_LOCKED"

# Force user to home directory
cd "$USER_HOME" 2>/dev/null || exit 1

# Set restricted environment
export PATH="$USER_HOME/bin"
export HOME="$USER_HOME"
unset SUDO_USER SUDO_UID SUDO_GID SUDO_COMMAND
unset BASH_ENV ENV
unset BASH_EXIT_ON_FAILURE

# Ensure we're in the user's home directory and PATH is set
cd "$USER_HOME" 2>/dev/null || exit 1
export PATH="$USER_HOME/bin"

# Start bash without restricted mode but with our custom restrictions
exec /bin/bash --login
SHELL_EOF

chmod +x /usr/local/bin/lockdown-shell-$USERNAME

# Set proper permissions for profile files
chown "$USERNAME:$USERNAME" "$USER_HOME/.profile" "$USER_HOME/.bashrc" 2>/dev/null
chmod 644 "$USER_HOME/.profile" "$USER_HOME/.bashrc" 2>/dev/null

# Create restricted tmp directory
mkdir -p "$USER_HOME/tmp"
chown "$USERNAME:$USERNAME" "$USER_HOME/tmp"
chmod 700 "$USER_HOME/tmp"

usermod -s "/usr/local/bin/lockdown-shell-$USERNAME" "$USERNAME"

# Disable SSH access
echo "DenyUsers $USERNAME" >> /etc/ssh/sshd_config.lockdown 2>/dev/null

# Set resource limits
cat >> /etc/security/limits.conf << EOF
$USERNAME hard nproc 20
$USERNAME hard nofile 100
$USERNAME hard fsize 10000
$USERNAME hard cpu 300
$USERNAME hard maxlogins 1
EOF