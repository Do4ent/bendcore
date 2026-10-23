.PHONY: help install build dev test lint format docker-build docker-up docker-down \
         docker-logs deploy-docker clean

# Colors for output
RED := \033[0;31m
GREEN := \033[0;32m
YELLOW := \033[0;33m
BLUE := \033[0;34m
NC := \033[0m # No Color

# Default target
.DEFAULT_GOAL := help

help: ## Show this help message
	@echo "$(BLUE)BendCore Web - Development & Deployment Commands$(NC)"
	@echo ""
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "$(GREEN)%-20s$(NC) %s\n", $$1, $$2}'

# Installation targets
install: ## Install all dependencies
	@echo "$(YELLOW)Installing backend dependencies...$(NC)"
	cd backend && npm ci
	@echo "$(YELLOW)Installing frontend dependencies...$(NC)"
	cd ui && npm ci
	@echo "$(GREEN)Dependencies installed successfully$(NC)"

install-backend: ## Install backend dependencies only
	@echo "$(YELLOW)Installing backend dependencies...$(NC)"
	cd backend && npm ci

install-frontend: ## Install frontend dependencies only
	@echo "$(YELLOW)Installing frontend dependencies...$(NC)"
	cd ui && npm ci

# Build targets
build: build-backend build-frontend ## Build both backend and frontend

build-backend: ## Build backend TypeScript
	@echo "$(YELLOW)Building backend...$(NC)"
	cd backend && npm run build
	@echo "$(GREEN)Backend built successfully$(NC)"

build-frontend: ## Build frontend with Vite
	@echo "$(YELLOW)Building frontend...$(NC)"
	cd ui && npm run build
	@echo "$(GREEN)Frontend built successfully$(NC)"

# Development targets
dev: ## Start local development environment
	@echo "$(YELLOW)Starting development servers...$(NC)"
	docker-compose up

dev-backend: ## Start backend development server only
	@echo "$(YELLOW)Starting backend development server...$(NC)"
	cd backend && npm run dev

dev-frontend: ## Start frontend development server only
	@echo "$(YELLOW)Starting frontend development server...$(NC)"
	cd ui && npm run dev

# Testing targets
test: test-backend test-frontend ## Run all tests

test-backend: ## Run backend tests
	@echo "$(YELLOW)Running backend tests...$(NC)"
	cd backend && npm test

test-backend-watch: ## Run backend tests in watch mode
	@echo "$(YELLOW)Running backend tests (watch mode)...$(NC)"
	cd backend && npm run test:watch

test-backend-coverage: ## Run backend tests with coverage
	@echo "$(YELLOW)Running backend tests with coverage...$(NC)"
	cd backend && npm run test:coverage

test-frontend: ## Run frontend tests
	@echo "$(YELLOW)Running frontend tests...$(NC)"
	cd ui && npm test

# Linting and formatting
lint: lint-backend lint-frontend ## Lint all code

lint-backend: ## Lint backend code
	@echo "$(YELLOW)Linting backend...$(NC)"
	cd backend && npm run lint

lint-frontend: ## Lint frontend code
	@echo "$(YELLOW)Linting frontend...$(NC)"
	cd ui && npm run lint

format: format-backend format-frontend ## Format all code

format-backend: ## Format backend code
	@echo "$(YELLOW)Formatting backend...$(NC)"
	cd backend && npm run format

format-frontend: ## Format frontend code
	@echo "$(YELLOW)Formatting frontend...$(NC)"
	cd ui && npm run format

# Docker targets
docker-build: ## Build Docker images
	@echo "$(YELLOW)Building Docker images...$(NC)"
	docker-compose build
	@echo "$(GREEN)Docker images built successfully$(NC)"

docker-build-backend: ## Build backend Docker image
	@echo "$(YELLOW)Building backend Docker image...$(NC)"
	docker build -f docker/Dockerfile.backend -t bendcore-backend:latest .
	@echo "$(GREEN)Backend Docker image built successfully$(NC)"

docker-build-frontend: ## Build frontend Docker image
	@echo "$(YELLOW)Building frontend Docker image...$(NC)"
	docker build -f docker/Dockerfile.frontend -t bendcore-frontend:latest .
	@echo "$(GREEN)Frontend Docker image built successfully$(NC)"

docker-up: ## Start Docker services
	@echo "$(YELLOW)Starting Docker services...$(NC)"
	docker-compose up -d
	@echo "$(GREEN)Services started successfully$(NC)"
	@echo "$(BLUE)Backend: http://localhost:3001$(NC)"
	@echo "$(BLUE)Frontend: http://localhost$(NC)"

docker-down: ## Stop Docker services
	@echo "$(YELLOW)Stopping Docker services...$(NC)"
	docker-compose down
	@echo "$(GREEN)Services stopped$(NC)"

docker-logs: ## View Docker service logs
	docker-compose logs -f

docker-logs-backend: ## View backend service logs
	docker-compose logs -f backend

docker-logs-frontend: ## View frontend service logs
	docker-compose logs -f frontend

docker-ps: ## List running containers
	docker-compose ps

docker-clean: ## Remove Docker containers and volumes
	@echo "$(RED)Removing Docker containers and volumes...$(NC)"
	docker-compose down -v
	@echo "$(GREEN)Docker cleanup completed$(NC)"

# Production Docker targets
docker-prod-build: ## Build production Docker images
	@echo "$(YELLOW)Building production Docker images...$(NC)"
	docker-compose -f docker-compose.prod.yml build
	@echo "$(GREEN)Production Docker images built successfully$(NC)"

docker-prod-up: ## Start production Docker services
	@echo "$(YELLOW)Starting production Docker services...$(NC)"
	docker-compose -f docker-compose.prod.yml up -d
	@echo "$(GREEN)Production services started successfully$(NC)"

docker-prod-down: ## Stop production Docker services
	@echo "$(YELLOW)Stopping production Docker services...$(NC)"
	docker-compose -f docker-compose.prod.yml down
	@echo "$(GREEN)Production services stopped$(NC)"

# Deployment targets
deploy-docker: ## Deploy to Docker Hub
	@echo "$(YELLOW)Deploying to Docker Hub...$(NC)"
	./scripts/deploy-docker.sh
	@echo "$(GREEN)Docker Hub deployment completed$(NC)"

deploy-vercel: ## Deploy frontend to Vercel
	@echo "$(YELLOW)Deploying frontend to Vercel...$(NC)"
	./scripts/deploy-vercel.sh
	@echo "$(GREEN)Vercel deployment completed$(NC)"

deploy-railway: ## Deploy backend to Railway
	@echo "$(YELLOW)Deploying backend to Railway...$(NC)"
	./scripts/deploy-railway.sh
	@echo "$(GREEN)Railway deployment completed$(NC)"

# Cleanup targets
clean: ## Clean build artifacts
	@echo "$(YELLOW)Cleaning build artifacts...$(NC)"
	rm -rf backend/dist
	rm -rf ui/dist
	rm -rf backend/node_modules
	rm -rf ui/node_modules
	find . -name "*.js" -path "*/dist/*" -delete
	find . -name "*.map" -path "*/dist/*" -delete
	@echo "$(GREEN)Cleanup completed$(NC)"

clean-cache: ## Clean package manager cache
	@echo "$(YELLOW)Cleaning cache...$(NC)"
	npm cache clean --force
	@echo "$(GREEN)Cache cleaned$(NC)"

# Verification targets
verify-docker: ## Verify Docker installation
	@echo "$(YELLOW)Verifying Docker installation...$(NC)"
	@docker --version
	@docker-compose --version
	@echo "$(GREEN)Docker verification passed$(NC)"

verify-node: ## Verify Node.js installation
	@echo "$(YELLOW)Verifying Node.js installation...$(NC)"
	@node --version
	@npm --version
	@echo "$(GREEN)Node.js verification passed$(NC)"

# Quick start
quick-start: install docker-build docker-up ## Quick start: install dependencies, build and start services
	@echo "$(GREEN)BendCore Web is running!$(NC)"
	@echo "$(BLUE)Backend: http://localhost:3001$(NC)"
	@echo "$(BLUE)Frontend: http://localhost$(NC)"
	@echo "$(YELLOW)Press Ctrl+C to stop services$(NC)"
