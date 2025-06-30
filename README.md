# SGNL Linux Host 
Examples for 


## Quick Start

### 1. Configure Your Credentials
```bash
# Copy the example and edit with your SGNL credentials
cp config.yaml.example config.yaml
# Edit config.yaml with your tenant name and API token
```

### 2. Generate All Configurations
```bash
# This populates all templates with your credentials
./setup.sh
```

### 3. Test Safely in Docker
```bash
# Build container that mirrors real deployment
docker build --build-arg GRPC_PORT=$GRPC_PORT -f docker/Dockerfile -t sgnl-linux-host .

# Start container for testing
docker run -it sgnl-linux-host

```