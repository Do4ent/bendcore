# BendCore v1.0.0-PRODUCTION: Deployment Procedures

**Last Updated:** October 30, 2026  
**Version:** 1.0.0-FINAL  
**Audience:** DevOps engineers, system administrators, deployment engineers

---

## Table of Contents
1. Pre-Deployment Checklist
2. Deployment Architecture
3. Step-by-Step Deployment Process
4. Post-Deployment Verification
5. Rollback Procedures
6. Troubleshooting Guide
7. Performance Baseline

---

## 1. Pre-Deployment Checklist

### Infrastructure Prerequisites
- [ ] AWS account with appropriate IAM permissions
- [ ] Terraform or CloudFormation access
- [ ] PostgreSQL database provisioned (RDS)
- [ ] S3 buckets created for storage
- [ ] CloudFront distribution configured
- [ ] SSL/TLS certificates installed
- [ ] Redis cluster deployed for caching
- [ ] CloudWatch log groups created

### Code Prerequisites
- [ ] All code merged to main branch
- [ ] Version tag created (v1.0.0-PRODUCTION)
- [ ] All tests passing (unit, integration, E2E)
- [ ] Code review approved
- [ ] Security scan passed
- [ ] Build artifacts ready

### Documentation Prerequisites
- [ ] Deployment runbook reviewed
- [ ] Rollback procedures documented
- [ ] Incident response plan confirmed
- [ ] On-call rotation established
- [ ] Communication plan approved

### Team Prerequisites
- [ ] Deployment engineer assigned
- [ ] Operations team on-call
- [ ] Support team briefed
- [ ] Management approval obtained
- [ ] Deployment window scheduled (off-peak hours)

---

## 2. Deployment Architecture

### Network Topology
```
Internet
    ↓
CloudFront (CDN, 150+ edge locations)
    ↓
Application Load Balancer (us-east-1)
    ├→ Auto Scaling Group (2-10 EC2 instances)
    │   ├→ App Server 1 (t3.large)
    │   ├→ App Server 2 (t3.large)
    │   └→ App Server N (auto-scaling)
    ↓
Backend Services
    ├→ PostgreSQL RDS (Multi-AZ, 100GB initial)
    ├→ Redis ElastiCache (Cluster mode, 10GB cache)
    ├→ S3 Storage (Versioning enabled, Lifecycle policies)
    └→ Elasticsearch (Log indexing, monitoring)
```

### Infrastructure Components
- **Region 1 (Primary):** US East (N. Virginia)
- **Region 2 (Failover):** US West (Oregon)
- **Database:** PostgreSQL 14 (Multi-AZ)
- **Cache:** Redis 7.x (Cluster mode)
- **Monitoring:** Prometheus + Grafana
- **Logging:** CloudWatch + ELK Stack

---

## 3. Step-by-Step Deployment Process

### Phase 1: Pre-Deployment (1 hour before launch)

**Step 1.1: Backup All Systems**
```bash
#!/bin/bash
# Backup production database
aws rds create-db-snapshot \
  --db-instance-identifier bendcore-prod \
  --db-snapshot-identifier bendcore-prod-$(date +%Y%m%d-%H%M%S)

# Backup S3 data
aws s3 sync s3://bendcore-prod-data s3://bendcore-prod-backup-$(date +%Y%m%d)

# Verify backups
echo "Backup Status:"
aws rds describe-db-snapshots --db-instance-identifier bendcore-prod
```

**Step 1.2: Clear CDN Cache**
```bash
# Invalidate all CloudFront cache
aws cloudfront create-invalidation \
  --distribution-id E12345EXAMPLE \
  --paths "/*"

# Wait for invalidation to complete
aws cloudfront wait invalidation-completed \
  --distribution-id E12345EXAMPLE
```

**Step 1.3: Pre-Warm Cache**
```bash
# Pre-load critical assets into cache
curl -I https://api.bendcore.io/health
curl -I https://bendcore.io/
curl -I https://bendcore.io/features
curl -I https://api.bendcore.io/docs
```

**Step 1.4: Verify Staging Environment**
```bash
# Run smoke tests on staging
npm run test:e2e --env=staging

# Check staging API health
curl -v https://staging-api.bendcore.io/health
```

**Step 1.5: Database Verification**
```bash
# Connect to production database
psql -h bendcore-prod.cdmqpf1jzd1r.us-east-1.rds.amazonaws.com \
  -U admin -d bendcore

-- Verify database integrity
SELECT COUNT(*) FROM pg_class;
SELECT * FROM pg_stat_statements;

-- Check for long-running queries
SELECT * FROM pg_stat_activity WHERE query_start IS NOT NULL;
```

### Phase 2: Application Deployment (0 minutes = Launch time)

**Step 2.1: Build Production Artifacts**
```bash
#!/bin/bash
# Build Docker image
docker build -t bendcore:v1.0.0-PRODUCTION \
  --build-arg NODE_ENV=production \
  --build-arg VERSION=v1.0.0-PRODUCTION .

# Verify image
docker inspect bendcore:v1.0.0-PRODUCTION

# Tag for registry
docker tag bendcore:v1.0.0-PRODUCTION \
  123456789.dkr.ecr.us-east-1.amazonaws.com/bendcore:latest
  
docker tag bendcore:v1.0.0-PRODUCTION \
  123456789.dkr.ecr.us-east-1.amazonaws.com/bendcore:v1.0.0
```

**Step 2.2: Push to Container Registry**
```bash
# Login to ECR
aws ecr get-login-password --region us-east-1 | \
  docker login --username AWS --password-stdin \
  123456789.dkr.ecr.us-east-1.amazonaws.com

# Push images
docker push 123456789.dkr.ecr.us-east-1.amazonaws.com/bendcore:latest
docker push 123456789.dkr.ecr.us-east-1.amazonaws.com/bendcore:v1.0.0

# Verify pushed images
aws ecr describe-images --repository-name bendcore
```

**Step 2.3: Update Load Balancer Target Groups**
```bash
# Get current ALB details
aws elbv2 describe-load-balancers --names bendcore-prod-alb

# Create new target group for blue-green deployment
aws elbv2 create-target-group \
  --name bendcore-prod-v1-0-0 \
  --protocol HTTP \
  --port 3000 \
  --vpc-id vpc-12345678 \
  --health-check-path /health

# Register targets
aws elbv2 register-targets \
  --target-group-arn arn:aws:elasticloadbalancing:... \
  --targets Id=i-0123456789abcdef0 Id=i-0123456789abcdef1
```

**Step 2.4: Launch New Auto Scaling Group**
```bash
# Create launch template
aws ec2 create-launch-template \
  --launch-template-name bendcore-v1-0-0 \
  --version-description "BendCore v1.0.0-PRODUCTION" \
  --launch-template-data '{
    "ImageId": "ami-0c55b159cbfafe1f0",
    "InstanceType": "t3.large",
    "SecurityGroupIds": ["sg-0123456789abcdef0"],
    "UserData": "IyEvYmluL2Jhc2gK..."
  }'

# Create auto scaling group
aws autoscaling create-auto-scaling-group \
  --auto-scaling-group-name bendcore-prod-v1-0-0 \
  --launch-template LaunchTemplateName=bendcore-v1-0-0 \
  --min-size 2 \
  --max-size 10 \
  --desired-capacity 3 \
  --target-group-arns arn:aws:elasticloadbalancing:...
```

**Step 2.5: Health Check Verification (5-minute grace period)**
```bash
#!/bin/bash
# Wait for instances to be healthy
for i in {1..30}; do
  healthy=$(aws elbv2 describe-target-health \
    --target-group-arn arn:aws:elasticloadbalancing:... \
    | jq '.TargetHealthDescriptions[] | select(.TargetHealth.State=="healthy") | .Target.Id' \
    | wc -l)
  
  echo "Healthy instances: $healthy/3"
  
  if [ $healthy -eq 3 ]; then
    echo "All instances healthy!"
    break
  fi
  
  sleep 10
done
```

**Step 2.6: Switch Load Balancer Traffic (Blue-Green Deployment)**
```bash
# Get current listener rule
aws elbv2 describe-listeners --load-balancer-arn arn:aws:elasticloadbalancing:...

# Modify listener rule to point to new target group
aws elbv2 modify-rule \
  --rule-arn arn:aws:elasticloadbalancing:... \
  --actions Type=forward,TargetGroupArn=arn:aws:elasticloadbalancing:...bendcore-prod-v1-0-0

# Verify traffic is flowing
sleep 30
curl -v https://api.bendcore.io/health
```

**Step 2.7: Database Migrations**
```bash
# Run database migrations
npm run migrate:prod

# Verify migrations completed
psql -h bendcore-prod.cdmqpf1jzd1r.us-east-1.rds.amazonaws.com \
  -U admin -d bendcore -c \
  "SELECT version, installed_on FROM schema_migrations ORDER BY version DESC LIMIT 5;"
```

### Phase 3: Post-Deployment Verification (Immediately after)

**Step 3.1: API Endpoint Verification**
```bash
#!/bin/bash
# Test all critical endpoints
endpoints=(
  "https://api.bendcore.io/health"
  "https://api.bendcore.io/v1/projects"
  "https://api.bendcore.io/v1/status"
  "https://bendcore.io/"
  "https://bendcore.io/docs"
)

for endpoint in "${endpoints[@]}"; do
  echo "Testing: $endpoint"
  response=$(curl -s -w "\n%{http_code}" "$endpoint")
  http_code=$(echo "$response" | tail -n 1)
  
  if [ "$http_code" = "200" ]; then
    echo "✓ $endpoint OK"
  else
    echo "✗ $endpoint FAILED (HTTP $http_code)"
  fi
done
```

**Step 3.2: Performance Baseline**
```bash
# Run load test (10 concurrent users, 1 minute)
ab -c 10 -t 60 https://api.bendcore.io/health

# Measure response times
wrk -t 4 -c 10 -d 60s https://api.bendcore.io/v1/projects

# Check database connection pool
psql -h bendcore-prod.cdmqpf1jzd1r.us-east-1.rds.amazonaws.com \
  -U admin -d bendcore -c \
  "SELECT datname, numbackends FROM pg_stat_database WHERE datname='bendcore';"
```

**Step 3.3: Monitor Metrics**
```bash
# Check CloudWatch metrics
aws cloudwatch get-metric-statistics \
  --namespace AWS/ApplicationELB \
  --metric-name TargetResponseTime \
  --start-time $(date -u -d '5 minutes ago' +%Y-%m-%dT%H:%M:%S) \
  --end-time $(date -u +%Y-%m-%dT%H:%M:%S) \
  --period 60 \
  --statistics Average,Maximum

# Check error rates
aws cloudwatch get-metric-statistics \
  --namespace AWS/ApplicationELB \
  --metric-name HTTPCode_Target_5XX_Count \
  --start-time $(date -u -d '5 minutes ago' +%Y-%m-%dT%H:%M:%S) \
  --end-time $(date -u +%Y-%m-%dT%H:%M:%S) \
  --period 60 \
  --statistics Sum
```

---

## 4. Post-Deployment Verification

### Health Checks (First 30 minutes)
- [ ] All API endpoints responding with HTTP 200
- [ ] Database connections stable
- [ ] No error rate spikes (>1% 5xx errors)
- [ ] Response times normal (<200ms p95)
- [ ] Cache hit rate >80%
- [ ] Memory usage stable (<70% of capacity)
- [ ] CPU usage normal (<60%)

### Functional Verification
- [ ] Can create new projects
- [ ] Can import/export STEP files
- [ ] Can generate G-code for bending operations
- [ ] Can generate BOM (Bill of Materials)
- [ ] Can create drawings
- [ ] Collaboration features working
- [ ] File uploads/downloads working

### User Experience
- [ ] Website loads in <3 seconds
- [ ] API responses in <200ms (p95)
- [ ] No JavaScript console errors
- [ ] All images loading correctly
- [ ] Mobile responsive design working
- [ ] Download functionality working

---

## 5. Rollback Procedures

### Immediate Rollback (If critical issue found < 10 minutes after deploy)

**Step 1: Switch Load Balancer Back**
```bash
# Revert listener rule to old target group
aws elbv2 modify-rule \
  --rule-arn arn:aws:elasticloadbalancing:... \
  --actions Type=forward,TargetGroupArn=arn:aws:elasticloadbalancing:...bendcore-prod

# Verify traffic reverted
sleep 10
curl https://api.bendcore.io/health
```

**Step 2: Scale Down New ASG**
```bash
# Set desired capacity to 0
aws autoscaling set-desired-capacity \
  --auto-scaling-group-name bendcore-prod-v1-0-0 \
  --desired-capacity 0
```

**Step 3: Verify Stability**
```bash
# Monitor old instances
aws elbv2 describe-target-health \
  --target-group-arn arn:aws:elasticloadbalancing:...bendcore-prod

# Run smoke tests
npm run test:e2e --env=production
```

### Gradual Rollback (If issue found after >10 minutes)

**Step 1: Shift 50% Traffic to Old Version**
```bash
# Create rule for gradual traffic shift
aws elbv2 modify-rule \
  --rule-arn arn:aws:elasticloadbalancing:... \
  --actions Type=forward,ForwardConfig="{TargetGroups=[{TargetGroupArn=arn:aws:elasticloadbalancing:...bendcore-prod,Weight=50},{TargetGroupArn=arn:aws:elasticloadbalancing:...bendcore-prod-v1-0-0,Weight=50}]}"

# Monitor for 10 minutes
```

**Step 2: Shift 100% Traffic to Old Version**
```bash
# Complete rollback
aws elbv2 modify-rule \
  --rule-arn arn:aws:elasticloadbalancing:... \
  --actions Type=forward,TargetGroupArn=arn:aws:elasticloadbalancing:...bendcore-prod
```

**Step 3: Database Rollback (If needed)**
```bash
# Restore from snapshot (only if database was corrupted)
aws rds restore-db-instance-from-db-snapshot \
  --db-instance-identifier bendcore-prod-restore \
  --db-snapshot-identifier bendcore-prod-$(date +%Y%m%d)

# Verify restored database
psql -h bendcore-prod-restore.cdmqpf1jzd1r.us-east-1.rds.amazonaws.com \
  -U admin -d bendcore -c "SELECT COUNT(*) FROM pg_tables WHERE schemaname='public';"

# Swap back to restored database
# (Requires DNS change and verification)
```

---

## 6. Troubleshooting Guide

### Issue: High Response Times (>500ms)

**Investigation:**
```bash
# Check database slow query log
psql -h bendcore-prod.cdmqpf1jzd1r.us-east-1.rds.amazonaws.com \
  -U admin -d bendcore -c \
  "SELECT * FROM pg_stat_statements ORDER BY mean_time DESC LIMIT 10;"

# Check cache hit rate
redis-cli -h bendcore-prod-cache.abc.ng.0001.use1.cache.amazonaws.com \
  INFO stats | grep -E "hits|misses"

# Check ALB metrics
aws cloudwatch get-metric-statistics \
  --namespace AWS/ApplicationELB \
  --metric-name TargetResponseTime \
  --dimensions Name=LoadBalancer,Value=app/bendcore-prod-alb/...
```

**Solution:**
- Enable query result caching
- Scale up RDS instance if CPU >70%
- Increase Redis cluster size
- Add database indexes for slow queries

### Issue: High Error Rate (>1%)

**Investigation:**
```bash
# Check error logs
aws logs tail /aws/ecs/bendcore-prod --follow --since 5m

# Check application metrics
curl https://api.bendcore.io/metrics | grep error

# Review recent code changes
git log --oneline -20
```

**Solution:**
- Review recent deployments
- Check for database connection issues
- Verify API keys and authentication
- Check third-party service availability

### Issue: High Memory Usage (>80%)

**Investigation:**
```bash
# Check memory usage
aws cloudwatch get-metric-statistics \
  --namespace AWS/EC2 \
  --metric-name MemoryUtilization \
  --dimensions Name=InstanceId,Value=i-0123456789abcdef0

# Check for memory leaks
ssh ec2-user@app-server-1
top -b -n 1 | grep -E "RES|VIRT"
```

**Solution:**
- Restart affected instances (auto-scaling will handle)
- Increase instance type (t3.large → t3.xlarge)
- Implement memory pooling/caching

---

## 7. Performance Baseline

### Expected Metrics at Launch

**API Performance:**
- Latency (p50): 50-100ms
- Latency (p95): 150-200ms
- Latency (p99): 250-300ms
- Error rate: <0.1%
- Availability: 99.9%+

**Database Performance:**
- Connection pool utilization: 20-30%
- Query execution time (avg): 10-50ms
- Disk I/O: <50% of capacity
- Replication lag: <1 second

**Cache Performance:**
- Hit rate: >85%
- Eviction rate: <1%
- Memory utilization: 60-80% of capacity

**Infrastructure:**
- CPU utilization (avg): 40-60%
- Memory utilization (avg): 50-70%
- Network throughput: <50% of capacity
- Disk usage growth: ~100 MB/hour

---

## Deployment Sign-Off

- [ ] All pre-deployment checks passed
- [ ] Deployment completed successfully
- [ ] All post-deployment verification passed
- [ ] Performance metrics within expected ranges
- [ ] No critical issues found
- [ ] Rollback procedures verified
- [ ] Team signed off on deployment

**Deployment Status:** ✅ SUCCESSFUL

**Next Steps:**
1. Monitor metrics for 24 hours
2. Compile launch metrics report
3. Conduct post-launch retrospective
4. Update documentation with lessons learned
