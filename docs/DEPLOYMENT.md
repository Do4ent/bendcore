# BendCore Web - Deployment Guide

This guide provides comprehensive instructions for deploying BendCore Web in different environments.

## Quick Start

### Prerequisites
- Node.js >= 16.0.0
- npm >= 8.0.0
- Docker & Docker Compose (for containerized deployment)
- Git

### Local Development Setup

```bash
# Clone the repository
git clone https://github.com/Do4ent/bendcore.git
cd bendcore

# Run setup script
./scripts/setup.sh

# Or manually:
make install
make docker-build
make docker-up
```

Access the application:
- Frontend: http://localhost
- Backend API: http://localhost:3001

## Deployment Options

### 1. Docker Compose (Recommended for local/self-hosted)

#### Development Environment

```bash
# Build images
docker-compose build

# Start services
docker-compose up

# View logs
docker-compose logs -f

# Stop services
docker-compose down
```

**Services:**
- Frontend (Nginx): port 80, 443
- Backend (Node.js): port 3001
- PostgreSQL: port 5432
- Redis: port 6379

#### Production Environment

```bash
# Build with production config
docker-compose -f docker-compose.prod.yml build

# Start with production compose
docker-compose -f docker-compose.prod.yml up -d

# View logs
docker-compose -f docker-compose.prod.yml logs -f

# Stop services
docker-compose -f docker-compose.prod.yml down
```

### 2. Docker Hub Deployment

Push Docker images to Docker Hub and deploy from there:

```bash
# Prerequisites
export DOCKER_USERNAME=your-docker-username
export DOCKER_PASSWORD=your-docker-password

# Deploy
./scripts/deploy-docker.sh [version-tag]

# Example
./scripts/deploy-docker.sh v1.0.0
```

### 3. Vercel (Frontend Only)

Deploy the frontend to Vercel for serverless hosting:

```bash
# Prerequisites
export VERCEL_TOKEN=your-vercel-token
export VERCEL_PROJECT_ID=your-project-id

# Deploy
./scripts/deploy-vercel.sh [production|preview]

# Example
./scripts/deploy-vercel.sh production
```

**Features:**
- Automatic deployments on push to main
- Preview deployments on PRs
- SSL/TLS included
- CDN globally distributed

### 4. Railway (Backend Only)

Deploy the backend to Railway:

```bash
# Prerequisites
export RAILWAY_TOKEN=your-railway-token
export RAILWAY_PROJECT_ID=your-project-id

# Deploy
./scripts/deploy-railway.sh [production|staging]

# Example
./scripts/deploy-railway.sh production
```

**Features:**
- Automatic database provisioning
- Environment-based deployments
- Built-in SSL
- Zero-config deployments

### 5. Self-Hosted with Terraform (Advanced)

Deploy to AWS, GCP, or Azure using Terraform:

```bash
cd terraform
terraform init
terraform plan
terraform apply
```

See `terraform/README.md` for detailed instructions.

## Environment Configuration

### Environment Variables

Copy `.env.example` to `.env` and configure:

```bash
cp .env.example .env
```

**Required variables:**
```
NODE_ENV=production
API_PORT=3001
DATABASE_URL=postgresql://user:pass@host:5432/bendcore
REDIS_URL=redis://:password@host:6379/0
JWT_SECRET=your-secret-key
SESSION_SECRET=your-session-secret
```

See `.env.example` for all available options.

### Backend Configuration

Create `backend/.env`:

```
NODE_ENV=production
PORT=3001
LOG_LEVEL=error
DATABASE_URL=postgresql://...
REDIS_URL=redis://...
CORS_ORIGIN=https://yourdomain.com
JWT_SECRET=your-secret
```

### Frontend Configuration

Create `ui/.env`:

```
VITE_API_URL=https://api.yourdomain.com
VITE_APP_NAME=BendCore Web
VITE_APP_VERSION=1.0.0
```

## Continuous Integration & Deployment

### GitHub Actions Workflows

Automated CI/CD pipelines are configured:

**Workflows:**
- `build-test.yml`: Runs on every PR and push
- `deploy-docker.yml`: Builds and pushes Docker images
- `deploy-vercel.yml`: Deploys frontend to Vercel
- `deploy-railway.yml`: Deploys backend to Railway

### Required GitHub Secrets

```
DOCKER_USERNAME        # Docker Hub username
DOCKER_PASSWORD        # Docker Hub password
VERCEL_TOKEN          # Vercel API token
VERCEL_PROJECT_ID     # Vercel project ID
RAILWAY_TOKEN         # Railway API token
RAILWAY_PROJECT_ID    # Railway project ID
```

Set secrets in GitHub Settings → Secrets and variables → Actions

## Deployment Checklist

### Pre-Deployment

- [ ] Update version number
- [ ] Run tests: `make test`
- [ ] Build locally: `make build`
- [ ] Update CHANGELOG.md
- [ ] Review environment variables
- [ ] Create backup of database

### Deployment

- [ ] Run build script: `./scripts/build.sh`
- [ ] Deploy to staging first
- [ ] Run smoke tests
- [ ] Deploy to production
- [ ] Monitor logs and metrics
- [ ] Verify health checks
- [ ] Test critical paths

### Post-Deployment

- [ ] Verify all endpoints
- [ ] Check error logs
- [ ] Monitor performance metrics
- [ ] Test database connections
- [ ] Verify backups
- [ ] Notify stakeholders
- [ ] Document any issues

## Troubleshooting

### Common Issues

#### Docker build fails
```bash
# Clear Docker cache
docker system prune -a

# Rebuild
docker-compose build --no-cache
```

#### Database connection errors
```bash
# Check PostgreSQL is running
docker-compose logs postgres

# Verify connection string
echo $DATABASE_URL

# Test connection
psql $DATABASE_URL
```

#### Port already in use
```bash
# Find process using port
lsof -i :3001
lsof -i :80

# Stop the process or change port in .env
```

#### Redis connection failed
```bash
# Check Redis is running
docker-compose logs redis

# Test connection
redis-cli ping
```

### Health Checks

All services expose health endpoints:

```bash
# Backend
curl http://localhost:3001/health

# Frontend
curl http://localhost/health

# Verify all services
curl http://localhost:3001/health && \
curl http://localhost/health && \
echo "All services healthy"
```

## Monitoring & Logging

### View Logs

```bash
# All services
docker-compose logs -f

# Specific service
docker-compose logs -f backend
docker-compose logs -f frontend
docker-compose logs -f postgres
```

### Enable Monitoring

Optional monitoring stack (Prometheus + Grafana):

```bash
# Start monitoring
docker-compose -f docker-compose.monitoring.yml up -d

# Access Grafana
# URL: http://localhost:3000
# User: admin
# Password: admin
```

### Performance Monitoring

Monitor resource usage:

```bash
# CPU, Memory, Network
docker stats

# Real-time dashboard
docker-compose top
```

## Scaling

### Horizontal Scaling

Scale backend services:

```bash
docker-compose up -d --scale backend=3
```

### Database Optimization

For large datasets:

```bash
# Create indexes
docker-compose exec postgres psql -U bendcore -d bendcore < /backups/indexes.sql

# Vacuum and analyze
docker-compose exec postgres vacuumdb -U bendcore -d bendcore -a
```

### Redis Caching

Enable caching in backend:

```
REDIS_ENABLED=true
REDIS_CACHE_TTL=3600
```

## Backup & Recovery

### Automated Backups

Daily backups configured in production:

```bash
# Manual backup
docker-compose exec postgres pg_dump -U bendcore bendcore > backup.sql

# Restore backup
docker-compose exec -T postgres psql -U bendcore bendcore < backup.sql
```

### Backup Scheduling

For automated backups, add to crontab:

```
0 2 * * * cd /home/bendcore && docker-compose exec -T postgres pg_dump -U bendcore bendcore > /backups/bendcore-$(date +\%Y\%m\%d).sql
```

## Security

### SSL/TLS

#### Self-signed certificates (development)

```bash
openssl req -x509 -newkey rsa:4096 -keyout docker/ssl/key.pem -out docker/ssl/cert.pem -days 365 -nodes
```

#### Production certificates (Let's Encrypt)

Use certbot with Docker:

```bash
docker run -it --rm -v /etc/letsencrypt:/etc/letsencrypt certbot/certbot certonly --standalone -d yourdomain.com
```

### Security Headers

Configured in `docker/nginx.conf`:
- HSTS (Strict-Transport-Security)
- CSP (Content-Security-Policy)
- X-Frame-Options
- X-Content-Type-Options
- X-XSS-Protection

### Database Security

- Always use strong passwords
- Enable SSL for remote connections
- Limit network access
- Regular backups with encryption
- Use environment variables for secrets

## Support & Documentation

- [Docker Documentation](https://docs.docker.com/)
- [Vercel Docs](https://vercel.com/docs)
- [Railway Docs](https://docs.railway.app/)
- [PostgreSQL Docs](https://www.postgresql.org/docs/)
- [Express.js Docs](https://expressjs.com/)

## Need Help?

Create an issue on GitHub or contact the maintainers.
