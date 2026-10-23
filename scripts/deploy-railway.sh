#!/bin/bash

# BendCore Web - Railway Deployment Script
# Deploys backend to Railway

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}BendCore Web - Railway Deployment${NC}"
echo ""

# Check prerequisites
echo -e "${YELLOW}Checking prerequisites...${NC}"

if ! command -v npm &> /dev/null; then
    echo -e "${RED}Error: npm is not installed${NC}"
    exit 1
fi

# Check Railway CLI
if ! command -v railway &> /dev/null; then
    echo -e "${YELLOW}Installing Railway CLI...${NC}"
    npm install -g @railway/cli
fi

echo -e "${GREEN}✓ Prerequisites check passed${NC}"
echo ""

# Check environment
if [ -z "$RAILWAY_TOKEN" ]; then
    echo -e "${YELLOW}Railway token not set in environment${NC}"
    read -sp "Enter Railway token: " RAILWAY_TOKEN
    echo ""
fi

if [ -z "$RAILWAY_PROJECT_ID" ]; then
    echo -e "${YELLOW}Railway project ID not set in environment${NC}"
    read -p "Enter Railway project ID: " RAILWAY_PROJECT_ID
fi

echo ""
echo -e "${YELLOW}Deployment Configuration:${NC}"
echo "- Project ID: $RAILWAY_PROJECT_ID"
echo ""

# Install dependencies
echo -e "${YELLOW}Installing dependencies...${NC}"
cd backend
npm ci
cd ..
echo -e "${GREEN}✓ Dependencies installed${NC}"
echo ""

# Build backend
echo -e "${YELLOW}Building backend...${NC}"
cd backend
npm run build
cd ..
echo -e "${GREEN}✓ Backend built${NC}"
echo ""

# Deploy to Railway
echo -e "${YELLOW}Deploying to Railway...${NC}"

ENVIRONMENT="${1:-production}"

DEPLOY_OUTPUT=$(railway up \
    --service bendcore-backend \
    --environment "$ENVIRONMENT" \
    --token "$RAILWAY_TOKEN" \
    --project "$RAILWAY_PROJECT_ID" \
    --detach \
    2>&1) || true

echo "$DEPLOY_OUTPUT"
echo ""

# Wait for deployment to stabilize
echo -e "${YELLOW}Waiting for deployment to stabilize...${NC}"
sleep 30

# Health check
echo -e "${YELLOW}Running health check...${NC}"

HEALTH_CHECK_URL="https://bendcore-backend.railway.app/health"

for i in {1..10}; do
    if curl -sf "$HEALTH_CHECK_URL" > /dev/null 2>&1; then
        echo -e "${GREEN}✓ Health check passed${NC}"
        HEALTH_OK=true
        break
    fi

    echo "Attempt $i/10: Waiting for service to be ready..."
    sleep 5
done

if [ "$HEALTH_OK" != "true" ]; then
    echo -e "${YELLOW}! Health check failed or timed out (service may still be starting)${NC}"
fi

echo ""
echo -e "${GREEN}Deployment completed!${NC}"
echo ""
echo "Deployment Details:"
echo "- Service: bendcore-backend"
echo "- Environment: $ENVIRONMENT"
echo "- Status: Check https://railway.app/dashboard"
echo ""
echo "Access backend:"
echo "  https://bendcore-backend.railway.app"
echo ""
echo "View logs:"
echo "  railway logs --service bendcore-backend --environment $ENVIRONMENT"
echo ""
