# Cloud Deployment Guide

Deploy BendCore Web to major cloud platforms.

## Vercel (Frontend Deployment)

### Setup

1. **Create Vercel Account**
   - Visit https://vercel.com
   - Sign up with GitHub
   - Authorize Vercel

2. **Create Project**
   - Click "Add New" → "Project"
   - Import GitHub repository
   - Select `bendcore` repo
   - Choose "Other" → "Create a monorepo"

3. **Configure Build Settings**
   - Root Directory: `ui`
   - Build Command: `npm run build`
   - Output Directory: `dist`
   - Install Command: `npm install`

4. **Environment Variables**
   ```
   VITE_API_URL=https://api.yourdomain.com
   VITE_APP_NAME=BendCore Web
   VITE_APP_VERSION=1.0.0
   ```

### Deploy

**Automatic (GitHub Integration)**
- Pushes to `main` branch → Production deployment
- PRs → Preview deployments (automatic)

**Manual**
```bash
# Install Vercel CLI
npm install -g vercel

# Deploy
vercel deploy --prod

# Or use script
./scripts/deploy-vercel.sh production
```

### Verify Deployment

```bash
# Test endpoints
curl https://your-app.vercel.app/
curl https://your-app.vercel.app/health

# View logs
vercel logs
```

### Custom Domain

1. In Vercel Dashboard
2. Settings → Domains
3. Add your domain
4. Update DNS records
5. Verify SSL certificate (automatic)

### Pricing
- **Hobby**: Free (bandwidth limited)
- **Pro**: $20/month (unlimited bandwidth)
- **Enterprise**: Custom pricing

---

## Railway (Backend Deployment)

### Setup

1. **Create Railway Account**
   - Visit https://railway.app
   - Sign up with GitHub
   - Authorize Railway

2. **Create Project**
   - New Project → Deploy from GitHub Repo
   - Select `bendcore` repository
   - Select `backend` folder path (optional)

3. **Add Services**
   ```bash
   # Via Railway CLI
   railway init
   
   # Add backend
   railway service add
   ```

4. **Configure Environment**
   - Click "Variables"
   - Add environment variables:
   ```
   NODE_ENV=production
   PORT=3001
   DATABASE_URL=postgres://...
   REDIS_URL=redis://...
   JWT_SECRET=your-secret
   ```

### Deploy

**Automatic (GitHub Integration)**
- Pushes to `main` → Production deployment
- Pushes to `develop` → Staging deployment

**Manual**
```bash
# Install Railway CLI
npm install -g @railway/cli

# Login
railway login

# Deploy
railway up

# Or use script
./scripts/deploy-railway.sh production
```

### Database Setup

1. **Add PostgreSQL Service**
   ```bash
   railway service add
   # Select PostgreSQL
   ```

2. **Get Connection String**
   - Railway Dashboard → Postgres service
   - Copy connection string
   - Add to backend environment variables

3. **Create Database**
   ```bash
   railway run psql -c "CREATE DATABASE bendcore;"
   ```

### Verify Deployment

```bash
# Check logs
railway logs

# Test API
curl https://your-railway-app.railway.app/health

# Check environment
railway variables
```

### Custom Domain

1. In Railway Dashboard
2. Settings → Domains
3. Add domain
4. Update DNS records
5. SSL auto-enabled

### Pricing
- **Pay as you go**: From $5/month
- **Per-service pricing**: CPU + Memory + Storage
- **First month free** ($5 credit)

---

## AWS (Full Stack)

### Setup with Docker

1. **Push to ECR**
   ```bash
   # Login
   aws ecr get-login-password --region us-east-1 | docker login --username AWS --password-stdin <account-id>.dkr.ecr.us-east-1.amazonaws.com
   
   # Build and push
   docker build -t bendcore-backend:latest -f docker/Dockerfile.backend .
   docker tag bendcore-backend:latest <account-id>.dkr.ecr.us-east-1.amazonaws.com/bendcore-backend:latest
   docker push <account-id>.dkr.ecr.us-east-1.amazonaws.com/bendcore-backend:latest
   ```

2. **ECS (Elastic Container Service)**
   - Create cluster
   - Create task definition
   - Create service
   - Configure load balancer

3. **RDS (Relational Database)**
   - Create PostgreSQL instance
   - Configure security groups
   - Create database and user

4. **S3 + CloudFront**
   - Create S3 bucket for frontend
   - Build and upload assets
   - Create CloudFront distribution
   - Set custom domain

5. **Environment Configuration**
   ```
   DATABASE_URL=postgres://user:pass@rds-endpoint:5432/bendcore
   REDIS_ELASTICACHE=redis-endpoint:6379
   S3_BUCKET=bendcore-assets
   CloudFront_DOMAIN=d123.cloudfront.net
   ```

### Terraform (Recommended)

See `terraform/` directory for full infrastructure as code.

### AWS Pricing
- **ECS**: ~$30-50/month (compute)
- **RDS**: ~$20-100/month (database)
- **CloudFront**: ~$10/month (CDN)
- **S3**: ~$5/month (storage)
- **Load Balancer**: ~$15/month
- **Total**: ~$80-200/month

---

## Google Cloud (Full Stack)

### Setup

1. **Create GCP Project**
   - Visit https://console.cloud.google.com
   - Create new project
   - Enable required APIs:
     - Cloud Run
     - Cloud SQL
     - Cloud Storage
     - Cloud CDN

2. **Push to Artifact Registry**
   ```bash
   # Enable API
   gcloud services enable artifactregistry.googleapis.com
   
   # Create repository
   gcloud artifacts repositories create bendcore --repository-format=docker --location=us-central1
   
   # Build and push
   gcloud builds submit --config cloudbuild.yaml
   ```

3. **Deploy Backend (Cloud Run)**
   ```bash
   gcloud run deploy bendcore-backend \
     --image us-central1-docker.pkg.dev/PROJECT_ID/bendcore/backend \
     --platform managed \
     --region us-central1 \
     --set-env-vars DATABASE_URL=$DB_URL,REDIS_URL=$REDIS_URL
   ```

4. **Deploy Frontend (Cloud Storage + CDN)**
   ```bash
   # Create bucket
   gsutil mb gs://bendcore-frontend
   
   # Upload assets
   gsutil -m cp -r ui/dist/* gs://bendcore-frontend/
   
   # Set as website
   gsutil web set -m index.html -e 404.html gs://bendcore-frontend
   
   # Enable CDN
   gcloud compute backend-buckets create bendcore-cdn --gcs-bucket-name=bendcore-frontend
   ```

5. **Cloud SQL (PostgreSQL)**
   ```bash
   gcloud sql instances create bendcore-postgres \
     --database-version POSTGRES_15 \
     --region us-central1 \
     --tier db-f1-micro
   
   # Create database
   gcloud sql databases create bendcore --instance=bendcore-postgres
   ```

6. **Environment**
   ```
   DATABASE_URL=cloudsql://PROJECT_ID:REGION:INSTANCE/bendcore
   REDIS_URL=redis://MEMORYSTORE_IP:6379
   ```

### Google Cloud Pricing
- **Cloud Run**: ~$10-30/month
- **Cloud SQL**: ~$20-50/month
- **Cloud Storage**: ~$5/month
- **Cloud CDN**: ~$0.12/GB
- **Total**: ~$35-85/month

---

## Azure (Full Stack)

### Setup

1. **Create Azure Account**
   - Visit https://azure.microsoft.com
   - Sign up
   - Create subscription

2. **Container Registry**
   ```bash
   # Create registry
   az acr create -g bendcore -n bendcoreregistry --sku Basic
   
   # Build and push
   az acr build --registry bendcoreregistry --image bendcore-backend:latest -f docker/Dockerfile.backend .
   ```

3. **App Service (Backend)**
   ```bash
   # Create App Service plan
   az appservice plan create -g bendcore -n bendcore-plan --sku B1 --is-linux
   
   # Create web app
   az webapp create -g bendcore -p bendcore-plan -n bendcore-backend --deployment-container-image-name bendcoreregistry.azurecr.io/bendcore-backend:latest
   ```

4. **Database for PostgreSQL**
   ```bash
   az postgres flexible-server create \
     -g bendcore -n bendcore-db \
     --admin-user dbadmin \
     --location eastus
   ```

5. **Static Web App (Frontend)**
   ```bash
   az staticwebapp create \
     -g bendcore -n bendcore-frontend \
     --branch main \
     --repo-url https://github.com/yourusername/bendcore \
     --api-location api \
     --output-location ui/dist
   ```

### Azure Pricing
- **App Service**: ~$10-50/month
- **Database**: ~$30-100/month
- **Static Web App**: ~$10/month
- **Storage**: ~$5/month
- **Total**: ~$55-165/month

---

## Heroku (Simple Deployment)

### Setup

1. **Create Heroku Account**
   - Visit https://www.heroku.com
   - Sign up
   - Install Heroku CLI

2. **Deploy**
   ```bash
   # Login
   heroku login
   
   # Create app
   heroku create bendcore-app
   
   # Add PostgreSQL
   heroku addons:create heroku-postgresql:hobby-dev
   
   # Add Redis
   heroku addons:create heroku-redis:premium-0
   
   # Deploy
   git push heroku main
   ```

3. **Frontend to Vercel**
   - Already covered above

### Heroku Pricing
- **Dyno**: $7-25/month
- **Database**: $9-200/month
- **Redis**: $15-330/month
- **Total**: ~$30-550/month

---

## Comparison

| Platform | Backend | Frontend | Database | Price/mo | Ease |
|----------|---------|----------|----------|----------|------|
| **Docker Compose** | Self-hosted | Self-hosted | PostgreSQL | $10-50 | ★★★★ |
| **Vercel + Railway** | Railway | Vercel | Railway | $25-75 | ★★★★★ |
| **AWS** | ECS | S3+CDN | RDS | $80-200 | ★★★ |
| **Google Cloud** | Cloud Run | Storage+CDN | Cloud SQL | $35-85 | ★★★ |
| **Azure** | App Service | Static Web | Database | $55-165 | ★★★ |
| **Heroku** | Dyno | Vercel | Add-on | $30-550 | ★★★★ |

---

## Recommended Setup

**For startups/MVPs:**
```
Frontend: Vercel (Free tier available)
Backend: Railway (Free tier available)
Database: Railway PostgreSQL
Total: ~$0-25/month
```

**For scaling:**
```
Frontend: Vercel Pro
Backend: Railway Professional
Database: Railway PostgreSQL
Monitoring: Sentry
Total: ~$50-100/month
```

**For enterprise:**
```
Frontend: AWS S3 + CloudFront
Backend: AWS ECS
Database: AWS RDS
Load Balancer: AWS ELB
Total: ~$150-500/month
```

---

## Migration Path

1. **Start local**: `docker-compose up`
2. **Test on free tier**: Vercel + Railway
3. **Scale infrastructure**: AWS/GCP/Azure
4. **Add monitoring**: Sentry, New Relic
5. **Global CDN**: Cloudflare, CloudFront

---

## Monitoring & Logging

### Application Monitoring
- **Sentry**: Error tracking
- **DataDog**: Infrastructure monitoring
- **New Relic**: APM
- **Grafana**: Custom dashboards

### Log Aggregation
- **ELK Stack**: Open source
- **Splunk**: Enterprise
- **CloudWatch**: AWS
- **Stackdriver**: Google Cloud
- **Application Insights**: Azure

---

## Troubleshooting

### Deployment fails
```bash
# Check logs
vercel logs        # Vercel
railway logs       # Railway

# Rebuild
git push origin main
```

### Database connection errors
```bash
# Test connection
psql $DATABASE_URL

# Check environment variables
vercel env list
railway variables
```

### SSL/TLS issues
```bash
# Verify certificate
curl -v https://yourdomain.com

# Renew certificate
certbot renew
```

---

## References

- [Vercel Docs](https://vercel.com/docs)
- [Railway Docs](https://docs.railway.app)
- [AWS ECS](https://aws.amazon.com/ecs/)
- [Google Cloud Run](https://cloud.google.com/run)
- [Azure App Service](https://azure.microsoft.com/services/app-service/)
