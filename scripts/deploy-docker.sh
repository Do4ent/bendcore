#!/bin/bash

# BendCore Web - Docker Deployment Script
# Builds and deploys Docker images to Docker Hub

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}BendCore Web - Docker Deployment${NC}"
echo ""

# Check prerequisites
echo -e "${YELLOW}Checking prerequisites...${NC}"

if ! command -v docker &> /dev/null; then
    echo -e "${RED}Error: Docker is not installed${NC}"
    exit 1
fi

# Check Docker Hub credentials
if [ -z "$DOCKER_USERNAME" ] && [ -z "$DOCKER_PASSWORD" ]; then
    echo -e "${YELLOW}Docker Hub credentials not set in environment${NC}"
    read -p "Enter Docker Hub username: " DOCKER_USERNAME
    read -sp "Enter Docker Hub password: " DOCKER_PASSWORD
    echo ""
fi

echo -e "${GREEN}✓ Prerequisites check passed${NC}"
echo ""

# Version
VERSION=${1:-"latest"}
TIMESTAMP=$(date +%Y%m%d-%H%M%S)

echo -e "${YELLOW}Deployment Configuration:${NC}"
echo "- Docker Username: $DOCKER_USERNAME"
echo "- Version: $VERSION"
echo "- Timestamp: $TIMESTAMP"
echo ""

# Docker Hub registry
REGISTRY="docker.io"
BACKEND_IMAGE="$REGISTRY/$DOCKER_USERNAME/bendcore-backend"
FRONTEND_IMAGE="$REGISTRY/$DOCKER_USERNAME/bendcore-frontend"

# Build backend image
echo -e "${YELLOW}Building backend Docker image...${NC}"
docker build \
    -f docker/Dockerfile.backend \
    -t "$BACKEND_IMAGE:latest" \
    -t "$BACKEND_IMAGE:$VERSION" \
    -t "$BACKEND_IMAGE:$TIMESTAMP" \
    .

echo -e "${GREEN}✓ Backend image built${NC}"
echo ""

# Build frontend image
echo -e "${YELLOW}Building frontend Docker image...${NC}"
docker build \
    -f docker/Dockerfile.frontend \
    -t "$FRONTEND_IMAGE:latest" \
    -t "$FRONTEND_IMAGE:$VERSION" \
    -t "$FRONTEND_IMAGE:$TIMESTAMP" \
    .

echo -e "${GREEN}✓ Frontend image built${NC}"
echo ""

# Security scanning (optional)
if command -v trivy &> /dev/null; then
    echo -e "${YELLOW}Scanning images for vulnerabilities...${NC}"

    trivy image "$BACKEND_IMAGE:latest" || true
    trivy image "$FRONTEND_IMAGE:latest" || true

    echo ""
fi

# Login to Docker Hub
echo -e "${YELLOW}Logging in to Docker Hub...${NC}"
echo "$DOCKER_PASSWORD" | docker login -u "$DOCKER_USERNAME" --password-stdin

echo -e "${GREEN}✓ Docker Hub login successful${NC}"
echo ""

# Push backend image
echo -e "${YELLOW}Pushing backend image to Docker Hub...${NC}"
docker push "$BACKEND_IMAGE:latest"
docker push "$BACKEND_IMAGE:$VERSION"
docker push "$BACKEND_IMAGE:$TIMESTAMP"

echo -e "${GREEN}✓ Backend image pushed${NC}"
echo ""

# Push frontend image
echo -e "${YELLOW}Pushing frontend image to Docker Hub...${NC}"
docker push "$FRONTEND_IMAGE:latest"
docker push "$FRONTEND_IMAGE:$VERSION"
docker push "$FRONTEND_IMAGE:$TIMESTAMP"

echo -e "${GREEN}✓ Frontend image pushed${NC}"
echo ""

# Logout
docker logout

# Summary
echo -e "${GREEN}Deployment completed successfully!${NC}"
echo ""
echo "Docker Hub Images:"
echo "- Backend: $BACKEND_IMAGE:latest"
echo "- Frontend: $FRONTEND_IMAGE:latest"
echo ""
echo "Usage:"
echo "  docker pull $BACKEND_IMAGE:latest"
echo "  docker pull $FRONTEND_IMAGE:latest"
echo ""
echo "Run with docker-compose:"
echo "  docker-compose up"
echo ""
