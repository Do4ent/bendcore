# BendCore Web - DevOps Documentation

Complete DevOps infrastructure for BendCore Web production deployment.

## Quick Start

### 1. Local Development

```bash
make install
make docker-build
make docker-up

# Access:
# Frontend: http://localhost
# Backend: http://localhost:3001
```

### 2. Docker Hub Deployment

```bash
export DOCKER_USERNAME=your-username
export DOCKER_PASSWORD=your-password
./scripts/deploy-docker.sh v1.0.0
```

### 3. Cloud Deployment (Vercel + Railway)

```bash
export VERCEL_TOKEN=your-token
export VERCEL_PROJECT_ID=your-project-id
export RAILWAY_TOKEN=your-token
export RAILWAY_PROJECT_ID=your-project-id

./scripts/deploy-vercel.sh production
./scripts/deploy-railway.sh production
```

## File Structure

```
.
├── docker/                          # Docker configuration
│   ├── Dockerfile.backend           # Backend image definition
│   ├── Dockerfile.frontend          # Frontend image definition
│   ├── nginx.conf                   # Nginx configuration
│   ├── .dockerignore               # Files to exclude
│   └── ssl/                        # SSL certificates
├── .github/workflows/               # GitHub Actions CI/CD
│   ├── build-test.yml             # Build & test pipeline
│   ├── deploy-docker.yml          # Docker Hub deployment
│   ├── deploy-vercel.yml          # Vercel deployment
│   └── deploy-railway.yml         # Railway deployment
├── scripts/                         # Deployment scripts
│   ├── setup.sh                   # Initial setup
│   ├── build.sh                   # Build script
│   ├── deploy-docker.sh           # Docker Hub deploy
│   ├── deploy-vercel.sh           # Vercel deploy
│   └── deploy-railway.sh          # Railway deploy
├── terraform/                       # Infrastructure as Code
│   ├── main.tf                    # AWS infrastructure
│   ├── variables.tf               # Input variables
│   ├── outputs.tf                 # Output values
│   └── terraform.tfvars.example   # Example configuration
├── docs/                           # Documentation
│   ├── DEPLOYMENT.md             # Deployment guide
│   ├── DOCKER.md                 # Docker guide
│   ├── CLOUD_DEPLOYMENT.md       # Cloud platforms guide
│   ├── ARCHITECTURE.md           # System architecture
│   └── DEVOPS_README.md          # This file
├── docker-compose.yml             # Development configuration
├── docker-compose.prod.yml        # Production configuration
├── Makefile                       # Convenience commands
└── .env.example                   # Environment variables template
```

## Key Features

### Docker Setup
- ✅ Multi-stage Dockerfile for optimization
- ✅ Non-root user for security
- ✅ Health checks for all services
- ✅ Automatic restarts on failure
- ✅ Resource limits for production
- ✅ Volume management for data persistence

### CI/CD Pipelines
- ✅ Automated build and test on PR
- ✅ Docker image building and scanning
- ✅ Vulnerability scanning (Trivy)
- ✅ Automatic deployment to Docker Hub
- ✅ Vercel frontend deployment
- ✅ Railway backend deployment

### Deployment Scripts
- ✅ One-command setup
- ✅ One-command build
- ✅ One-command Docker deployment
- ✅ One-command Vercel deployment
- ✅ One-command Railway deployment

### Infrastructure as Code
- ✅ AWS infrastructure with Terraform
- ✅ VPC with public/private subnets
- ✅ ECS for container orchestration
- ✅ RDS for managed PostgreSQL
- ✅ ElastiCache for Redis
- ✅ ALB for load balancing
- ✅ CloudFront for CDN
- ✅ Auto-scaling configured

### Documentation
- ✅ Complete deployment guide
- ✅ Docker usage guide
- ✅ Cloud platform guides
- ✅ System architecture documentation
- ✅ Troubleshooting guides

## Common Commands

### Make Commands
```bash
make help              # Show all commands
make install          # Install dependencies
make build            # Build everything
make dev              # Start development environment
make test             # Run tests
make lint             # Lint code
make format           # Format code
make docker-build     # Build Docker images
make docker-up        # Start Docker services
make docker-down      # Stop Docker services
make deploy-docker    # Deploy to Docker Hub
make deploy-vercel    # Deploy to Vercel
make deploy-railway   # Deploy to Railway
make clean            # Clean artifacts
```

### Docker Commands
```bash
docker-compose up -d          # Start services
docker-compose down           # Stop services
docker-compose logs -f        # View logs
docker-compose ps             # List services
docker-compose exec backend npm test  # Run tests in container
```

### Deployment Scripts
```bash
./scripts/setup.sh               # Setup project
./scripts/build.sh               # Build for production
./scripts/deploy-docker.sh v1.0  # Deploy to Docker Hub
./scripts/deploy-vercel.sh       # Deploy to Vercel
./scripts/deploy-railway.sh      # Deploy to Railway
```

## Environment Variables

### Required (Backend)
```
NODE_ENV=production
API_PORT=3001
DATABASE_URL=postgresql://...
REDIS_URL=redis://...
JWT_SECRET=your-secret-key
```

### Required (Frontend)
```
VITE_API_URL=https://api.yourdomain.com
```

### Optional
```
LOG_LEVEL=error
SENTRY_DSN=...
AWS_REGION=us-east-1
```

See `.env.example` for complete configuration.

## Deployment Checklist

### Pre-Deployment
- [ ] Update version number
- [ ] Run tests: `make test`
- [ ] Build locally: `make build`
- [ ] Update CHANGELOG
- [ ] Review environment variables
- [ ] Create database backup

### Deployment
- [ ] Build: `./scripts/build.sh`
- [ ] Test in staging
- [ ] Deploy to production
- [ ] Monitor logs
- [ ] Verify health checks
- [ ] Test critical features

### Post-Deployment
- [ ] Verify endpoints
- [ ] Check error logs
- [ ] Monitor metrics
- [ ] Notify team
- [ ] Document issues

## Monitoring & Logging

### Health Checks
```bash
curl http://localhost:3001/health   # Backend
curl http://localhost/health         # Frontend
```

### View Logs
```bash
docker-compose logs -f backend      # Backend logs
docker-compose logs -f frontend     # Frontend logs
docker-compose logs -f postgres     # Database logs
```

### Resource Usage
```bash
docker stats                         # Real-time stats
docker-compose top backend          # Process details
```

## Security

### Best Practices Implemented
- ✅ Non-root container users
- ✅ Minimal base images (Alpine)
- ✅ SSL/TLS encryption
- ✅ Security headers in Nginx
- ✅ Health checks and auto-restart
- ✅ Resource limits
- ✅ Network isolation
- ✅ Vulnerability scanning
- ✅ Secret management
- ✅ Read-only filesystems (where possible)

### SSL/TLS Setup

**Development (self-signed):**
```bash
openssl req -x509 -newkey rsa:4096 \
  -keyout docker/ssl/key.pem \
  -out docker/ssl/cert.pem \
  -days 365 -nodes
```

**Production (Let's Encrypt):**
Use certbot with Docker or your cloud provider's SSL service.

## Scaling

### Horizontal Scaling (Docker)
```bash
docker-compose up -d --scale backend=3
```

### Vertical Scaling (AWS Terraform)
Increase ECS task CPU/memory in `terraform/variables.tf`:
```hcl
ecs_task_cpu = "512"        # Increase from 256
ecs_task_memory = "1024"    # Increase from 512
```

### Auto-Scaling (AWS)
Configured in Terraform:
- Min: 1 task
- Max: 4 tasks
- Trigger: 70% CPU or 80% memory

## Troubleshooting

### Docker Issues
```bash
# Clear cache and rebuild
docker system prune -a
docker-compose build --no-cache

# Check service health
docker-compose ps
docker-compose logs backend

# Restart service
docker-compose restart backend
```

### Deployment Issues
```bash
# Check GitHub Actions
# Visit: https://github.com/yourusername/bendcore/actions

# Check Vercel
# Visit: https://vercel.com/dashboard

# Check Railway
# Visit: https://railway.app/dashboard
```

### Database Issues
```bash
# Check PostgreSQL
docker-compose logs postgres
docker-compose exec postgres psql -U bendcore -d bendcore

# Check Redis
docker-compose logs redis
docker-compose exec redis redis-cli ping
```

## Performance Optimization

### Frontend
- Gzip compression enabled
- Static asset caching (1 year)
- CDN-ready (CloudFront/Vercel)
- Route-based code splitting

### Backend
- Connection pooling
- Query optimization
- Redis caching
- Async operations

### Database
- Indexed columns
- Connection pooling
- Slow query logs
- Regular maintenance

## Backup & Recovery

### Database Backups
```bash
# Automatic (daily in production)
docker-compose exec postgres pg_dump -U bendcore bendcore > backup.sql

# Restore
docker-compose exec -T postgres psql -U bendcore bendcore < backup.sql
```

### Volume Backups
```bash
docker run --rm -v bendcore_postgres_data:/data \
  -v $(pwd):/backup alpine tar czf /backup/backup.tar.gz /data
```

## References

- [Docker Documentation](https://docs.docker.com/)
- [Express.js Best Practices](https://expressjs.com/en/advanced/best-practice-performance.html)
- [React Performance](https://react.dev/learn)
- [Terraform AWS Provider](https://registry.terraform.io/providers/hashicorp/aws/latest/docs)
- [PostgreSQL Documentation](https://www.postgresql.org/docs/)

## Support

For issues or questions:
1. Check troubleshooting section
2. Review logs: `docker-compose logs -f`
3. Create GitHub issue
4. Check documentation files

## Next Steps

1. Complete `.env.example` → `.env`
2. Run `make install` to setup dependencies
3. Run `make docker-build` to build images
4. Run `make docker-up` to start services
5. Access http://localhost in your browser
6. Configure CI/CD secrets in GitHub
7. Deploy to cloud platform

---

**Version:** 1.0.0  
**Last Updated:** 2024-01-23  
**Maintained By:** BendCore Team
