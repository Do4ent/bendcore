#!/bin/bash

# BendCore Web - Setup Script
# Initializes the project for development or production

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}BendCore Web - Setup Script${NC}"
echo ""

# Check prerequisites
echo -e "${YELLOW}Checking prerequisites...${NC}"

if ! command -v node &> /dev/null; then
    echo -e "${RED}Error: Node.js is not installed${NC}"
    exit 1
fi

if ! command -v npm &> /dev/null; then
    echo -e "${RED}Error: npm is not installed${NC}"
    exit 1
fi

if ! command -v docker &> /dev/null; then
    echo -e "${YELLOW}Warning: Docker is not installed. Docker features will not be available.${NC}"
fi

echo -e "${GREEN}✓ Prerequisites check passed${NC}"
echo ""

# Install dependencies
echo -e "${YELLOW}Installing dependencies...${NC}"

echo "Installing backend dependencies..."
cd backend
npm ci
cd ..

echo "Installing frontend dependencies..."
cd ui
npm ci
cd ..

echo -e "${GREEN}✓ Dependencies installed${NC}"
echo ""

# Setup environment files
echo -e "${YELLOW}Setting up environment files...${NC}"

if [ ! -f ".env" ]; then
    echo "Creating .env file..."
    cat > .env << 'EOF'
# BendCore Web Environment Variables
NODE_ENV=development
LOG_LEVEL=debug

# Database
DATABASE_URL=postgresql://bendcore:bendcore_dev_password@postgres:5432/bendcore
REDIS_URL=redis://:@redis:6379/0

# API
API_PORT=3001
API_HOST=0.0.0.0

# Frontend
VITE_API_URL=http://localhost:3001/api
VITE_APP_NAME=BendCore Web
VITE_APP_VERSION=1.0.0

# CORS
CORS_ORIGIN=http://localhost

# Secrets (generate with: openssl rand -base64 32)
JWT_SECRET=your-secret-key-change-me
SESSION_SECRET=your-session-secret-change-me
EOF
    echo -e "${GREEN}✓ .env file created${NC}"
else
    echo -e "${YELLOW}! .env file already exists${NC}"
fi

if [ ! -f "backend/.env" ]; then
    echo "Creating backend/.env file..."
    cat > backend/.env << 'EOF'
NODE_ENV=development
PORT=3001
LOG_LEVEL=debug
DATABASE_URL=postgresql://bendcore:bendcore_dev_password@postgres:5432/bendcore
REDIS_URL=redis://:@redis:6379/0
CORS_ORIGIN=http://localhost
JWT_SECRET=your-secret-key-change-me
SESSION_SECRET=your-session-secret-change-me
EOF
    echo -e "${GREEN}✓ backend/.env file created${NC}"
else
    echo -e "${YELLOW}! backend/.env file already exists${NC}"
fi

if [ ! -f "ui/.env" ]; then
    echo "Creating ui/.env file..."
    cat > ui/.env << 'EOF'
VITE_API_URL=http://localhost:3001/api
VITE_APP_NAME=BendCore Web
VITE_APP_VERSION=1.0.0
EOF
    echo -e "${GREEN}✓ ui/.env file created${NC}"
else
    echo -e "${YELLOW}! ui/.env file already exists${NC}"
fi

echo ""

# Build applications
echo -e "${YELLOW}Building applications...${NC}"

echo "Building backend..."
cd backend
npm run build
cd ..
echo -e "${GREEN}✓ Backend built${NC}"

echo "Building frontend..."
cd ui
npm run build
cd ..
echo -e "${GREEN}✓ Frontend built${NC}"

echo ""

# Docker setup (if available)
if command -v docker &> /dev/null; then
    echo -e "${YELLOW}Setting up Docker...${NC}"

    if [ ! -f "docker/.dockerignore" ]; then
        echo -e "${YELLOW}Warning: docker/.dockerignore not found${NC}"
    fi

    echo "Building Docker images..."
    docker-compose build --no-cache 2>&1 | tail -20

    echo -e "${GREEN}✓ Docker images built${NC}"
    echo ""
fi

# Final instructions
echo -e "${GREEN}Setup completed successfully!${NC}"
echo ""
echo -e "${BLUE}Next steps:${NC}"
echo ""
echo "1. Update environment variables in .env file"
echo "2. Start services:"
echo ""
echo -e "   ${YELLOW}Option A (Docker):${NC}"
echo "   docker-compose up"
echo ""
echo -e "   ${YELLOW}Option B (Development):${NC}"
echo "   make dev-backend  # Terminal 1"
echo "   make dev-frontend # Terminal 2"
echo ""
echo "3. Access the application:"
echo "   Frontend: http://localhost:5173 (dev) or http://localhost (prod)"
echo "   Backend:  http://localhost:3001"
echo ""
