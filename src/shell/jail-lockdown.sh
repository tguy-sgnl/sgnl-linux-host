#!/bin/bash

# proper-lockdown.sh - Actually effective user restriction

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
JAIL_DIR="/var/jail/$USERNAME"

# 2. Create chroot jail with ONLY allowed commands
echo "🏢 Creating chroot jail..."
mkdir -p "$JAIL_DIR"/{bin,lib,lib64,etc,home/$USERNAME,tmp,dev,usr/lib}

# Copy ONLY specific allowed binaries + sudo
ALLOWED_BINARIES="bash ls cat echo pwd sudo"
echo "📦 Installing only allowed commands: $ALLOWED_BINARIES"

for binary in $ALLOWED_BINARIES; do
    BINARY_PATH=$(which $binary)
    if [ -n "$BINARY_PATH" ]; then
        cp "$BINARY_PATH" "$JAIL_DIR/bin/"
        echo "✅ Installed: $binary"
        
        # Copy required libraries for this binary
        ldd "$BINARY_PATH" 2>/dev/null | grep -o '/lib[^ ]*' | while read lib; do
            if [ -f "$lib" ]; then
                mkdir -p "$JAIL_DIR$(dirname $lib)"
                cp "$lib" "$JAIL_DIR$lib" 2>/dev/null
            fi
        done
    fi
done

# Copy essential system libraries and sudo dependencies
if [ -f /lib64/ld-linux-x86-64.so.2 ]; then
    cp /lib64/ld-linux-x86-64.so.2 "$JAIL_DIR/lib64/"
fi

# Copy sudo configuration access (read-only)
mkdir -p "$JAIL_DIR/etc/sudoers.d"
cp /etc/sudoers "$JAIL_DIR/etc/" 2>/dev/null
cp -r /etc/sudoers.d/* "$JAIL_DIR/etc/sudoers.d/" 2>/dev/null
chmod -R 444 "$JAIL_DIR/etc/sudoers"*

# Copy PAM configuration for sudo
mkdir -p "$JAIL_DIR/etc/pam.d"
cp /etc/pam.d/sudo "$JAIL_DIR/etc/pam.d/" 2>/dev/null
cp /etc/pam.d/common-* "$JAIL_DIR/etc/pam.d/" 2>/dev/null

# Copy essential files sudo needs
cp /etc/shadow "$JAIL_DIR/etc/" 2>/dev/null
chmod 640 "$JAIL_DIR/etc/shadow"

# Create minimal device files
mknod "$JAIL_DIR/dev/null" c 1 3
mknod "$JAIL_DIR/dev/zero" c 1 5
chmod 666 "$JAIL_DIR/dev/null" "$JAIL_DIR/dev/zero"

# Create minimal /etc files needed for shell
echo "$USERNAME:x:$(id -u $USERNAME):$(id -g $USERNAME):Restricted:/home/$USERNAME:/bin/bash" > "$JAIL_DIR/etc/passwd"
echo "$(id -gn $USERNAME):x:$(id -g $USERNAME):" > "$JAIL_DIR/etc/group"

# Set up user's home in jail - ONLY writable area
mkdir -p "$JAIL_DIR/home/$USERNAME"
chown "$USERNAME:$(id -gn $USERNAME)" "$JAIL_DIR/home/$USERNAME"
chmod 755 "$JAIL_DIR/home/$USERNAME"

# Create restrictive .bashrc in jail
cat > "$JAIL_DIR/home/$USERNAME/.bashrc" << 'EOF'
# Restricted environment
export PATH="/bin"
cd /home/USERNAME_PLACEHOLDER

echo "🔒 RESTRICTED ENVIRONMENT"
echo "📁 Home directory: /home/USERNAME_PLACEHOLDER"
echo "💻 Available commands: ls, cat, echo, pwd, sudo"
echo "🚫 You are in a chroot jail - must use sudo for system access"
echo "💡 Try: sudo [command] to access system resources"
echo ""

# Override cd to only allow home directory navigation
cd() {
    local target="${1:-/home/USERNAME_PLACEHOLDER}"
    
    # Only allow paths within /home/USERNAME
    if [[ "$target" != /home/USERNAME_PLACEHOLDER* ]] && [[ "$target" != "." ]] && [[ "$target" != ".." ]] && [[ "$target" != "~" ]]; then
        echo "❌ Access denied: can only access /home/USERNAME_PLACEHOLDER"
        return 1
    fi
    
    builtin cd "$target" 2>/dev/null || {
        echo "❌ Directory not found: $target"
        return 1
    }
}

# Disable dangerous shell features
set +H  # Disable history expansion
enable -n source  # Disable source
enable -n .       # Disable .
enable -n exec    # Disable exec
enable -n eval    # Disable eval

# Custom prompt
PS1='restricted:\w\$ '
EOF

# Replace placeholder with actual username
sed -i "s/USERNAME_PLACEHOLDER/$USERNAME/g" "$JAIL_DIR/home/$USERNAME/.bashrc"

# 3. Create chroot wrapper script
cat > "/usr/local/bin/jail-$USERNAME" << EOF
#!/bin/bash
# Chroot jail wrapper for $USERNAME

# Log access attempt
echo "\$(date): $USERNAME jail access" >> /var/log/restricted-access.log

# Enter chroot jail
exec chroot "$JAIL_DIR" /bin/bash --login
EOF

chmod +x "/usr/local/bin/jail-$USERNAME"

# 4. Set user's shell to jail wrapper
usermod -s "/usr/local/bin/jail-$USERNAME" "$USERNAME"

# 7. Secure the jail
chown -R root:root "$JAIL_DIR"
chmod -R 755 "$JAIL_DIR"
# Only user's home is writable
chown "$USERNAME:$(id -gn $USERNAME)" "$JAIL_DIR/home/$USERNAME"
chmod 755 "$JAIL_DIR/home/$USERNAME"

echo ""
echo "✅ Lockdown completed for $USERNAME"
echo ""
echo "🔒 What user CANNOT do DIRECTLY:"
echo "   ❌ Access ANY system files (/etc, /var, /root, etc.) without sudo"
echo "   ❌ Use vi, wget, curl, or any other commands without sudo"
echo "   ❌ Leave their home directory without sudo"
echo "   ❌ Access network tools without sudo"
echo "   ❌ Execute scripts or programs without sudo"
echo "   ❌ Modify PATH to access system commands"
echo ""
echo "✅ What user CAN do:"
echo "   ✅ Use: bash, ls, cat, echo, pwd (basic commands)"
echo "   ✅ Use: sudo [anything] (goes through your plugin!)"
echo "   ✅ Create/edit files in their home directory"
echo "   ✅ Navigate within their home directory"
echo ""
echo "🎯 PERFECT SCENARIO:"
echo "   User must use 'sudo cat /etc/passwd' instead of 'cat /etc/passwd'"
echo "   User must use 'sudo systemctl status' instead of 'systemctl status'"
echo "   User must use 'sudo vi /etc/config' instead of 'vi /etc/config'"
echo "   ALL commands go through your plugin for authorization!"
echo ""
echo "🧪 Test the restrictions:"
echo "   /root/test-$USERNAME-lockdown.sh"
echo ""
echo "👁️ Monitor jail access:"
echo "   /usr/local/bin/monitor-jail-$USERNAME"
echo ""
echo "📁 Jail location: $JAIL_DIR"
echo "🏠 User's home in jail: $JAIL_DIR/home/$USERNAME"
echo ""
echo "💡 Now user is truly powerless without sudo!"
echo "   Your sudo plugin becomes their ONLY way to do anything useful!"