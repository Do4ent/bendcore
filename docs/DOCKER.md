# Docker Guide for BendCore Web

Complete guide to building and running BendCore Web with Docker.

## Docker Basics

### What is Docker?

Docker containers provide:
- **Consistency**: Same environment across development, testing, and production
- **Isolation**: Services run independently without affecting others
- **Simplicity**: One command to start entire application stack
- **Scalability**: Easy to replicate and scale services

## Project Structure

```
docker/
├── Dockerfile.backend      # Backend service image
├── Dockerfile.frontend     # Frontend service image
├── nginx.conf             # Nginx configuration
├── .dockerignore          # Files to exclude from build
└── ssl/                   # SSL certificates (optional)

docker-compose.yml         # Development configuration
docker-compose.prod.yml    # Production configuration
```

## Building Images

### Build Backend Image

```bash
docker build -f docker/Dockerfile.backend -t bendcore-backend:latest .
```

**Multi-stage build process:**
1. **Build stage**: Install dependencies, compile TypeScript
2. **Runtime stage**: Copy only production artifacts, minimize image size

**Result:** ~250MB optimized image

### Build Frontend Image

```bash
docker build -f docker/Dockerfile.frontend -t bendcore-frontend:latest .
```

**Multi-stage build process:**
1. **Build stage**: Install dependencies, build with Vite
2. **Runtime stage**: Serve with optimized Nginx

**Result:** ~100MB optimized image

### Build All Images

```bash
docker-compose build
```

## Running Containers

### Development Mode

```bash
# Start all services
docker-compose up

# Start in background
docker-compose up -d

# View logs
docker-compose logs -f

# View specific service logs
docker-compose logs -f backend
docker-compose logs -f frontend

# Stop services
docker-compose down
```

### Production Mode

```bash
# Start with production configuration
docker-compose -f docker-compose.prod.yml up -d

# View logs
docker-compose -f docker-compose.prod.yml logs -f

# Stop services
docker-compose -f docker-compose.prod.yml down
```

### Individual Service

```bash
# Run single container
docker run -p 3001:3001 bendcore-backend:latest

# With environment variables
docker run -e NODE_ENV=production -p 3001:3001 bendcore-backend:latest

# With volume mapping
docker run -v $(pwd)/data:/app/data -p 3001:3001 bendcore-backend:latest
```

## Docker Compose Configuration

### Development (docker-compose.yml)

Services included:
- **backend**: Node.js Express API (port 3001)
- **frontend**: Nginx static server (port 80, 443)
- **postgres**: PostgreSQL database (port 5432)
- **redis**: Redis cache (port 6379)

Features:
- Source code volumes for live reloading
- Service health checks
- Automatic restart on failure
- Shared network for inter-service communication

### Production (docker-compose.prod.yml)

Optimizations:
- Resource limits (CPU, memory)
- Health checks with retries
- Structured logging
- Volume persistence
- Automatic restarts
- Environment-based configuration

Resource limits:
```
Backend:   1 CPU, 512MB RAM
Frontend:  2 CPU, 1GB RAM
Database:  2 CPU, 1GB RAM
Redis:     1 CPU, 256MB RAM
```

## Networking

### Service Communication

Services communicate via Docker network:
- **Backend → Frontend**: Via Nginx reverse proxy
- **Backend → Database**: `postgres:5432`
- **Backend → Redis**: `redis:6379`

### Port Mapping

```
Host Port → Container Port
localhost:80      → frontend:80
localhost:443     → frontend:443
localhost:3001    → backend:3001
localhost:5432    → postgres:5432
localhost:6379    → redis:6379
```

## Volumes & Persistence

### Development Volumes

Live reload during development:

```yaml
volumes:
  - ./backend/src:/app/src      # Backend source
  - ./ui/src:/app/src           # Frontend source
```

### Production Volumes

Data persistence:

```yaml
volumes:
  - postgres_data:/var/lib/postgresql/data
  - redis_data:/data
  - ./logs:/var/log/app
```

### Named Volumes

```bash
# List volumes
docker volume ls

# Inspect volume
docker volume inspect bendcore_postgres_data

# Remove volume
docker volume rm bendcore_postgres_data

# Backup volume
docker run --rm -v bendcore_postgres_data:/data -v $(pwd):/backup alpine tar czf /backup/postgres_backup.tar.gz /data
```

## Health Checks

### Configured Health Checks

```yaml
healthcheck:
  test: ["CMD", "curl", "-f", "http://localhost:3001/health"]
  interval: 30s
  timeout: 10s
  retries: 3
  start_period: 10s
```

### Manual Health Checks

```bash
# Check all services
docker-compose ps

# Check individual service
docker-compose exec backend curl http://localhost:3001/health

# Check database
docker-compose exec postgres pg_isready -U bendcore

# Check Redis
docker-compose exec redis redis-cli ping
```

## Image Optimization

### Size Optimization

**Backend:**
- Alpine Linux base: ~40MB
- Node.js: ~80MB
- Dependencies: ~130MB
- Total: ~250MB

**Frontend:**
- Nginx Alpine: ~10MB
- Built assets: ~2-5MB
- Total: ~15-20MB

### Performance Optimization

1. **Multi-stage builds**: Reduces final image size
2. **Alpine images**: Smaller base images
3. **Layer caching**: Reuses unchanged layers
4. **Dependency caching**: `npm ci` for reproducible builds

### Security Hardening

1. **Non-root user**: Services run as unprivileged user
2. **Minimal base images**: Reduced attack surface
3. **Layer security scanning**: Uses Trivy for vulnerability scanning
4. **Read-only filesystems**: Production-grade hardening

## Networking Best Practices

### Service Discovery

Docker uses service names for DNS:
```
backend:3001       # Other services access backend this way
postgres:5432      # Database connection string
redis:6379         # Cache connection
```

### Network Isolation

Services communicate through `bendcore-network` bridge:
- Only expose necessary ports
- Internal communication via service names
- External access through reverse proxy

## Debugging

### View Running Containers

```bash
# List containers
docker-compose ps

# Detailed info
docker ps -a

# Resource usage
docker stats
```

### Execute Commands

```bash
# Run command in backend
docker-compose exec backend npm list

# Access backend shell
docker-compose exec backend sh

# Run database query
docker-compose exec postgres psql -U bendcore -d bendcore -c "SELECT version();"

# Redis CLI
docker-compose exec redis redis-cli
```

### View Logs

```bash
# All logs
docker-compose logs

# Specific service
docker-compose logs backend

# Follow logs
docker-compose logs -f frontend

# Last 100 lines
docker-compose logs --tail=100 backend

# Timestamps
docker-compose logs --timestamps backend
```

### Common Issues

**Container exits immediately:**
```bash
# Check logs
docker-compose logs backend

# Inspect container
docker inspect <container-id>
```

**Port already in use:**
```bash
# Find process
lsof -i :3001

# Or change port in .env
```

**Volume permission errors:**
```bash
# Fix permissions
docker-compose down
docker system prune -a
docker-compose build --no-cache
docker-compose up
```

## Production Deployment

### Pre-deployment Checklist

- [ ] All tests pass
- [ ] Dockerfiles are optimized
- [ ] Environment variables configured
- [ ] SSL certificates ready
- [ ] Database migrations prepared
- [ ] Backup strategy in place

### Deployment Steps

1. **Build images**
   ```bash
   docker-compose build
   ```

2. **Test locally**
   ```bash
   docker-compose up -d
   curl http://localhost:3001/health
   ```

3. **Push to registry**
   ```bash
   docker tag bendcore-backend:latest myregistry/bendcore-backend:v1.0
   docker push myregistry/bendcore-backend:v1.0
   ```

4. **Deploy to server**
   ```bash
   docker-compose -f docker-compose.prod.yml up -d
   ```

5. **Verify deployment**
   ```bash
   docker-compose logs -f
   curl https://yourdomain.com/health
   ```

## Monitoring & Logging

### Container Metrics

```bash
# Real-time resource usage
docker stats

# Container info
docker inspect <container-id>

# Container logs
docker logs --follow <container-id>
```

### Log Aggregation

Logs are stored in `/var/log/app`:

```bash
# View application logs
docker-compose exec backend cat logs/app.log

# Tail logs
tail -f /var/log/app/backend.log
```

### Performance Tuning

```yaml
# Adjust resource limits
deploy:
  resources:
    limits:
      cpus: '2'
      memory: 1G
    reservations:
      cpus: '1'
      memory: 512M
```

## Cleanup & Maintenance

### Remove Containers

```bash
# Stop and remove containers
docker-compose down

# Remove containers but keep volumes
docker-compose down

# Remove everything including volumes
docker-compose down -v
```

### Clean Docker System

```bash
# Remove unused images
docker image prune -a

# Remove unused volumes
docker volume prune

# Remove unused networks
docker network prune

# Full cleanup
docker system prune -a --volumes
```

### Backup Data

```bash
# Backup database
docker-compose exec -T postgres pg_dump -U bendcore bendcore > backup.sql

# Backup volumes
docker run --rm -v bendcore_postgres_data:/data -v $(pwd):/backup alpine tar czf /backup/postgres.tar.gz /data

# Restore backup
docker-compose exec -T postgres psql -U bendcore bendcore < backup.sql
```

## Advanced Topics

### Custom Networks

```bash
# Create network
docker network create bendcore-custom

# Run container on network
docker run --network bendcore-custom -d bendcore-backend
```

### Docker Swarm

For clustering multiple hosts:

```bash
# Initialize swarm
docker swarm init

# Deploy stack
docker stack deploy -c docker-compose.yml bendcore
```

### Kubernetes

For enterprise deployments:

```bash
# Convert compose to K8s manifests
kompose convert -f docker-compose.yml -o bendcore-k8s/
```

## Troubleshooting

### Image issues
```bash
# Rebuild without cache
docker-compose build --no-cache

# View image layers
docker history bendcore-backend:latest
```

### Container issues
```bash
# Check container logs
docker-compose logs <service>

# Restart service
docker-compose restart <service>

# Recreate service
docker-compose up -d --force-recreate <service>
```

### Network issues
```bash
# Inspect network
docker network inspect bendcore-network

# Test connectivity
docker-compose exec backend ping frontend
```

## References

- [Docker Documentation](https://docs.docker.com/)
- [Docker Compose Reference](https://docs.docker.com/compose/compose-file/)
- [Docker Best Practices](https://docs.docker.com/develop/dev-best-practices/)
- [Container Security](https://docs.docker.com/engine/security/)
