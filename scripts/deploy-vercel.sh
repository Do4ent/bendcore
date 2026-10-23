#!/bin/bash

# BendCore Web - Vercel Deployment Script
# Deploys frontend to Vercel

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}BendCore Web - Vercel Deployment${NC}"
echo ""

# Check prerequisites
echo -e "${YELLOW}Checking prerequisites...${NC}"

if ! command -v npm &> /dev/null; then
    echo -e "${RED}Error: npm is not installed${NC}"
    exit 1
fi

# Check Vercel CLI
if ! command -v vercel &> /dev/null; then
    echo -e "${YELLOW}Installing Vercel CLI...${NC}"
    npm install -g vercel
fi

echo -e "${GREEN}✓ Prerequisites check passed${NC}"
echo ""

# Check environment
if [ -z "$VERCEL_TOKEN" ]; then
    echo -e "${YELLOW}Vercel token not set in environment${NC}"
    read -sp "Enter Vercel token: " VERCEL_TOKEN
    echo ""
fi

if [ -z "$VERCEL_PROJECT_ID" ]; then
    echo -e "${YELLOW}Vercel project ID not set in environment${NC}"
    read -p "Enter Vercel project ID: " VERCEL_PROJECT_ID
fi

echo ""
echo -e "${YELLOW}Deployment Configuration:${NC}"
echo "- Project ID: $VERCEL_PROJECT_ID"
echo ""

# Install dependencies
echo -e "${YELLOW}Installing dependencies...${NC}"
cd ui
npm ci
cd ..
echo -e "${GREEN}✓ Dependencies installed${NC}"
echo ""

# Build frontend
echo -e "${YELLOW}Building frontend...${NC}"
cd ui
npm run build
cd ..
echo -e "${GREEN}✓ Frontend built${NC}"
echo ""

# Deploy to Vercel
echo -e "${YELLOW}Deploying to Vercel...${NC}"

ENVIRONMENT="${1:-production}"

if [ "$ENVIRONMENT" = "preview" ]; then
    DEPLOY_OUTPUT=$(vercel deploy \
        --prod=false \
        --token="$VERCEL_TOKEN" \
        --project-id="$VERCEL_PROJECT_ID" \
        --cwd=ui \
        2>&1)
else
    DEPLOY_OUTPUT=$(vercel deploy \
        --prod=true \
        --token="$VERCEL_TOKEN" \
        --project-id="$VERCEL_PROJECT_ID" \
        --cwd=ui \
        2>&1)
fi

echo "$DEPLOY_OUTPUT"

# Extract deployment URL
DEPLOYMENT_URL=$(echo "$DEPLOY_OUTPUT" | grep -oP 'https://[^ ]+' | head -1)

if [ -z "$DEPLOYMENT_URL" ]; then
    echo -e "${RED}Error: Could not extract deployment URL${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}Deployment completed successfully!${NC}"
echo ""
echo "Deployment URL: $DEPLOYMENT_URL"
echo ""

# Optional: Create deployment record
if [ -f "DEPLOYMENT.md" ]; then
    echo "Recording deployment..."
    {
        echo ""
        echo "## Vercel Deployment"
        echo "- Date: $(date)"
        echo "- URL: $DEPLOYMENT_URL"
        echo "- Environment: $ENVIRONMENT"
    } >> DEPLOYMENT.md
fi

echo "For more details, visit: https://vercel.com/dashboard"
echo ""
