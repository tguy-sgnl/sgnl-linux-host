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

info() {
    echo -e "${BLUE}[SGNL]${NC} $1"
}

# Check if config.yaml exists
if [ ! -f "config.yaml" ]; then
    error "config.yaml not found!"
    error "Copy config.yaml.example to config.yaml and edit with your credentials"
    exit 1
fi

log "🔧 Generating SGNL configurations..."

# Parse config.yaml (simple approach - could use yq for complex parsing)
TENANT=$(grep "^tenant:" config.yaml | sed 's/tenant: *//; s/"//g; s/'\''//g')
API_TOKEN=$(grep "^api_token:" config.yaml | sed 's/api_token: *//; s/"//g; s/'\''//g')

# Extract settings
API_URL=$(grep -A 10 "^settings:" config.yaml | grep "base_api_url:" | sed 's/.*base_api_url: *//; s/"//g; s/'\''//g' || echo "sgnlapis.cloud")
ASSET_ATTRIBUTE=$(grep -A 10 "^settings:" config.yaml | grep "asset_attribute:" | sed 's/.*asset_attribute: *//; s/"//g; s/'\''//g' || echo "name")
DEBUG=$(grep -A 10 "^settings:" config.yaml | grep "debug:" | sed 's/.*debug: *//; s/"//g; s/'\''//g' || echo "false")
ACCESS_MSG=$(grep -A 10 "^settings:" config.yaml | grep "access_msg:" | sed 's/.*access_msg: *//; s/"//g; s/'\''//g' || echo "true")
GRPC_PORT=$(grep -A 10 "^settings:" config.yaml | grep "grpc_port:" | sed 's/.*grpc_port: *//; s/"//g; s/'\''//g' || echo "8080")

# Validate required fields
if [ -z "$TENANT" ] || [ -z "$API_TOKEN" ]; then
    error "Missing required fields in config.yaml"
    error "Required: tenant, api_token"
    exit 1
fi

log "📋 Configuration:"
log "   Tenant: $TENANT"
log "   API URL: $API_URL"
log "   gRPC Port: $GRPC_PORT"
log "   Debug: $DEBUG"
log "   Access Logging: $ACCESS_MSG"

# Generate or load adapter tokens
TOKENS_FILE=".adapter_tokens"
if [ -f "$TOKENS_FILE" ]; then
    log "🔑 Using existing adapter tokens"
    ADAPTER_TOKENS=$(cat "$TOKENS_FILE")
else
    log "🔑 Generating new adapter tokens..."
    TOKEN1=$(openssl rand -hex 32)
    TOKEN2=$(openssl rand -hex 32)
    ADAPTER_TOKENS="[\"$TOKEN1\", \"$TOKEN2\"]"
    echo "$ADAPTER_TOKENS" > "$TOKENS_FILE"
    chmod 600 "$TOKENS_FILE"
fi

# Extract first token for datasource config
FIRST_TOKEN=$(echo "$ADAPTER_TOKENS" | sed 's/\["\([^"]*\)".*/\1/')

# Create output directories
mkdir -p output/for-SGNL-SOR
mkdir -p output/for-linux-host

log "📁 Populating templates..."

# Simple template substitution function
substitute_template() {
    local template_file="$1"
    local output_file="$2"
    
    if [ ! -f "$template_file" ]; then
        warn "Template not found: $template_file"
        return 1
    fi
    
    # Simple substitution (for complex templating, would use proper template engine)
    # Need to escape the JSON array for sed
    ESCAPED_TOKENS=$(echo "$ADAPTER_TOKENS" | sed 's/\[/\\[/g; s/\]/\\]/g; s/"/\\"/g')
    
    sed -e "s/{{TENANT}}/$TENANT/g" \
        -e "s/{{API_TOKEN}}/$API_TOKEN/g" \
        -e "s/{{API_URL}}/$API_URL/g" \
        -e "s/{{ASSET_ATTRIBUTE}}/$ASSET_ATTRIBUTE/g" \
        -e "s/{{DEBUG}}/$DEBUG/g" \
        -e "s/{{ACCESS_MSG}}/$ACCESS_MSG/g" \
        -e "s/{{GRPC_PORT}}/$GRPC_PORT/g" \
        -e "s/{{ADAPTER_TOKEN}}/$FIRST_TOKEN/g" \
        -e "s/{{ADAPTER_TOKENS}}/$ESCAPED_TOKENS/g" \
        -e "s/{{ADAPTER_ENDPOINT}}/https:\/\/your-host.example.com:$GRPC_PORT/g" \
        -e "s/{{LOG_LEVEL}}/$([ "$DEBUG" = "true" ] && echo "DEBUG" || echo "INFO")/g" \
        "$template_file" > "$output_file"
}

# Generate configs for SaaS upload
substitute_template "templates/sor.yaml.template" "output/for-SGNL-SOR/sor.yaml"

# Generate configs for Linux host
substitute_template "templates/sgnl-config.json.template" "output/for-linux-host/etc-sgnl-config.json"
substitute_template "templates/sudo.conf.template" "output/for-linux-host/etc-sudo.conf"

# Generate host-adapter config using template
substitute_template "templates/host-adapter-config.json.template" "output/for-linux-host/host-adapter-config.json"

# Create installation script
cat > "output/for-linux-host/install.sh" << 'EOF'
#!/bin/bash
set -e

echo "🔧 Installing SGNL Linux Host configuration..."

# Create directories
sudo mkdir -p /etc/sgnl
sudo mkdir -p /usr/lib/sudo
sudo mkdir -p /lib/security

# Install configuration files
if [ -f "etc-sgnl-config.json" ]; then
    sudo cp etc-sgnl-config.json /etc/sgnl/config.json
    sudo chmod 644 /etc/sgnl/config.json
    echo "✅ Installed /etc/sgnl/config.json"
fi

if [ -f "etc-sudo.conf" ]; then
    sudo cp etc-sudo.conf /etc/sudo.conf
    sudo chmod 644 /etc/sudo.conf
    echo "✅ Installed /etc/sudo.conf"
fi

# Install binary modules if they exist
if [ -f "sgnl_policy.so" ]; then
    sudo cp sgnl_policy.so /usr/lib/sudo/sgnl_policy_plugin.so
    sudo chmod 644 /usr/lib/sudo/sgnl_policy_plugin.so
    echo "✅ Installed sudo plugin"
fi

if [ -f "pam_sgnl.so" ]; then
    sudo cp pam_sgnl.so /lib/security/pam_sgnl.so
    sudo chmod 644 /lib/security/pam_sgnl.so
    echo "✅ Installed PAM module"
fi

echo "✅ SGNL installation complete!"
echo ""
echo "Next steps:"
echo "1. Start the host adapter service"
echo "2. Test with: sudo -l"
echo "3. Check logs for any issues"
EOF

chmod +x "output/for-linux-host/install.sh"

# Copy adapter tokens for reference
echo "$ADAPTER_TOKENS" > "output/for-linux-host/adapter-tokens.json"

log "✅ Configuration generation complete!"
log ""
log "📁 Generated files:"
log "   To create a SOR in SGNL:"
for file in output/for-SGNL-SOR/*; do
    [ -f "$file" ] && log "     $(basename "$file")"
done
log ""
log "   For Linux Host:"
for file in output/for-linux-host/*; do
    [ -f "$file" ] && log "     $(basename "$file")"
done
log ""
log "🔑 Adapter tokens saved to:"
log "   .adapter_tokens (for reuse)"
log "   output/for-linux-host/adapter-tokens.json (for reference)"
log ""
# Ask if user wants to build Docker image
log ""
log "🐳 Build Docker image now? [y/N]"
read -r BUILD_DOCKER

if [[ "$BUILD_DOCKER" =~ ^[Yy]$ ]]; then
    log "🔧 Building Docker image with configuration-based settings..."
    log "   gRPC Port: $GRPC_PORT (from config.yaml)"
    
    # Build Docker image with the port from config.yaml
    log "🐳 Running Docker build..."
    if docker build --build-arg GRPC_PORT="$GRPC_PORT" -f docker/Dockerfile -t sgnl-linux-host .; then
        log "✅ Docker image built successfully!"
        log ""
        log "🚀 Ready to run:"
        log "   docker run -it -p $GRPC_PORT:$GRPC_PORT sgnl-linux-host"
        log ""
        log "🔍 Or run with port mapping:"
        log "   docker run -it -p 8080:$GRPC_PORT sgnl-linux-host  # Map host port 8080 to container port $GRPC_PORT"
    else
        error "❌ Docker build failed!"
    fi
else
    log "🚀 Next steps:"
    log "   1. Upload files from output/for-SGNL-SOR/ to SGNL"
    log "   2. Build Docker: docker build --build-arg GRPC_PORT=$GRPC_PORT -f docker/Dockerfile -t sgnl-linux-host ."
    log "   3. Run Docker: docker run -it -p $GRPC_PORT:$GRPC_PORT sgnl-linux-host"
    log "   4. Deploy to real host: scp output/for-linux-host/* user@host:/tmp/"
fi 