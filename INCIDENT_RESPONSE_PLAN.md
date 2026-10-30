# BendCore v1.0.0-PRODUCTION: Incident Response Plan

**Last Updated:** October 30, 2026  
**Version:** 1.0.0  
**Status:** ACTIVE FOR LAUNCH

---

## Quick Reference

### Critical Contact Information
- **Incident Commander (On-Call):** +1-800-BENDCORE ext. 1
- **Engineering Lead:** engineering@bendcore.io
- **CTO:** cto@bendcore.io
- **Status Page:** https://status.bendcore.io
- **Slack Channel:** #incidents-production

### Severity Levels
| Level | Definition | Response Time | Resolution Target | Escalation |
|-------|-----------|----------------|-------------------|------------|
| P1 | Complete service outage | 15 minutes | 1 hour | CTO + Team |
| P2 | Major feature unavailable | 30 minutes | 4 hours | Engineering Lead |
| P3 | Minor feature degradation | 1 hour | 24 hours | Support + Engineer |
| P4 | Low-impact issues | 4 hours | 72 hours | Support |

---

## Incident Response Workflow

### Phase 1: Detection & Alert (0-15 minutes)

**Automated Detection:**
```
Monitoring System detects anomaly
  ↓
PagerDuty alert triggered
  ↓
SMS + Email + Slack notification
  ↓
On-call engineer acknowledges incident (target: <5 min)
```

**Manual Detection:**
```
User reports issue via support
  ↓
Support creates ticket
  ↓
Escalates to engineering
  ↓
Engineer confirms and declares incident
```

**Severity Classification:**
- **P1:** Service availability <99%, API error rate >5%, Download/export failures
- **P2:** Specific feature broken, response time >1 second, database issues
- **P3:** Minor UI issues, cosmetic bugs, documentation errors
- **P4:** Feature requests, low-impact bugs, performance tips

### Phase 2: Initial Response (15-30 minutes)

**Step 1: Incident Commander Assumes Command**
- Declare incident in Slack (#incidents-production)
- Create incident ticket in tracking system
- Establish communication channel
- Identify severity level
- Begin timeline logging

**Step 2: Gather Initial Information**
```bash
# Check system status
curl -s https://api.bendcore.io/health | jq
curl -s https://api.bendcore.io/metrics | jq

# Check logs
aws logs tail /aws/ecs/bendcore-prod --follow --since 5m

# Check metrics
aws cloudwatch get-metric-statistics \
  --namespace AWS/ApplicationELB \
  --metric-name HTTPCode_Target_5XX_Count \
  --start-time $(date -u -d '10 minutes ago' +%Y-%m-%dT%H:%M:%S) \
  --end-time $(date -u +%Y-%m-%dT%H:%M:%S) \
  --period 60 \
  --statistics Sum
```

**Step 3: Assemble Response Team**
- P1: Full team (Engineering Lead, Infrastructure, Database, Frontend, Support Lead)
- P2: Relevant experts (2-3 engineers)
- P3: One engineer + support
- P4: Support team only

**Step 4: Communicate Outage**
```
UPDATE 1 (Immediately):
"We're investigating reports of [issue]. Initial investigation underway.
Current status: Investigating. Updates every 10 minutes."
→ Post to status page, Twitter, email affected users
```

### Phase 3: Root Cause Analysis (30-90 minutes)

**For API/Database Issues:**
```bash
# 1. Check recent deployments
git log --oneline -10
git show --name-status HEAD

# 2. Monitor database
psql -h bendcore-prod.cdmqpf1jzd1r.us-east-1.rds.amazonaws.com -U admin -d bendcore
SELECT * FROM pg_stat_activity;
SELECT * FROM pg_stat_statements ORDER BY mean_time DESC LIMIT 10;

# 3. Check application logs
grep "ERROR\|FATAL\|exception" /var/log/bendcore/app.log | tail -50

# 4. Analyze metrics timeline
aws cloudwatch get-metric-widget-image \
  --metric-widget '{
    "metrics": [
      ["AWS/ApplicationELB", "TargetResponseTime"],
      [".", "HTTPCode_Target_5XX_Count"],
      [".", "RequestCount"]
    ],
    "period": 60,
    "stat": "Average",
    "region": "us-east-1",
    "title": "API Metrics Timeline"
  }'
```

**For Performance Issues:**
```bash
# 1. Check resource utilization
aws cloudwatch get-metric-statistics \
  --namespace AWS/EC2 --metric-name CPUUtilization \
  --dimensions Name=InstanceId,Value=i-... \
  --period 60 --statistics Average

# 2. Check cache hit rate
redis-cli -h bendcore-prod-cache.abc.ng.0001.use1.cache.amazonaws.com \
  INFO stats | grep -E "hits|misses|evicted"

# 3. Profile slowest endpoints
curl -w "@curl-format.txt" https://api.bendcore.io/v1/projects
```

**For Deployment Issues:**
```bash
# 1. Check deployment status
aws elbv2 describe-target-health \
  --target-group-arn arn:aws:elasticloadbalancing:...

# 2. Check container logs
docker logs bendcore-container-id | tail -100

# 3. Check recent commits
git diff HEAD~1..HEAD
```

### Phase 4: Resolution (Variable by issue type)

**Option A: Immediate Rollback (High-risk deployments)**
```bash
# If issue is deployment-related, rollback immediately
aws elbv2 modify-rule \
  --rule-arn arn:aws:elasticloadbalancing:... \
  --actions Type=forward,TargetGroupArn=arn:aws:elasticloadbalancing:...bendcore-prod

# Verify service restored
sleep 30
curl https://api.bendcore.io/health
```

**Option B: Emergency Hotfix (Database/logic issues)**
```bash
# 1. Fix issue in code
# (Modify source files, run tests)

# 2. Build & test locally
npm install && npm run test

# 3. Build production artifact
docker build -t bendcore:hotfix-v1.0.0-001 .

# 4. Deploy to staging first
docker push ...bendcore:hotfix-v1.0.0-001
# (Verify on staging)

# 5. Deploy to production
# (Blue-green deployment with 5-minute rollback window)
```

**Option C: Scale Up (Capacity issue)**
```bash
# If issue is capacity-related
aws autoscaling set-desired-capacity \
  --auto-scaling-group-name bendcore-prod \
  --desired-capacity 8

# Monitor scaling progress
watch -n 5 'aws autoscaling describe-auto-scaling-groups \
  --auto-scaling-group-names bendcore-prod | \
  jq ".AutoScalingGroups[0].Instances | length"'
```

**Option D: Database Repair (Data corruption)**
```bash
# If database is corrupted
# 1. Create snapshot
aws rds create-db-snapshot \
  --db-instance-identifier bendcore-prod \
  --db-snapshot-identifier bendcore-prod-recovery-$(date +%s)

# 2. Restore from known-good backup
aws rds restore-db-instance-from-db-snapshot \
  --db-instance-identifier bendcore-prod-restored \
  --db-snapshot-identifier bendcore-prod-2026-10-30-10-00

# 3. Verify restored database
psql -h bendcore-prod-restored.cdmqpf1jzd1r.us-east-1.rds.amazonaws.com \
  -U admin -d bendcore -c "SELECT COUNT(*) FROM pg_tables;"

# 4. Update RDS endpoint (DNS swap)
# (Update application connection string)

# 5. Verify application connects to restored database
curl https://api.bendcore.io/health
```

### Phase 5: Verification (15-30 minutes)

**Automated Verification:**
```bash
# Run smoke tests
npm run test:e2e --env=production

# Monitor error rates
watch -n 5 'aws cloudwatch get-metric-statistics \
  --namespace AWS/ApplicationELB \
  --metric-name HTTPCode_Target_5XX_Count \
  --period 60 \
  --statistics Sum'
```

**Manual Verification:**
- [ ] All API endpoints returning 200 OK
- [ ] Error rates <0.1% (normal baseline)
- [ ] Response times normal (<200ms p95)
- [ ] All critical features working
- [ ] No regressions in other features
- [ ] Database integrity verified

**User Communication:**
```
UPDATE 2 (After verification):
"We identified and fixed the issue causing the outage.
Service is now fully restored and operating normally.
Error rate: 0% | Response time: 98ms"
→ Post to status page, Twitter
```

### Phase 6: Post-Incident (Next 24-48 hours)

**Step 1: Gather Incident Details**
- Timeline of all events
- Actions taken and by whom
- Root cause identified
- Resolution implemented
- System behavior during and after incident

**Step 2: Write Post-Mortem Report**
```markdown
# Incident Post-Mortem: [Issue Description]

## Timeline
10:00 UTC - Alert triggered (error rate spike)
10:05 UTC - Engineering team engaged
10:15 UTC - Root cause identified (database connection pool exhausted)
10:25 UTC - Fix deployed (increased pool size)
10:35 UTC - Service fully restored

## Root Cause
[Detailed explanation of what caused the issue]

## Contributing Factors
[Why did this happen? What conditions led to it?]

## Resolution
[What was done to fix it immediately?]

## Permanent Fix
[What will prevent this in the future?]

## Lessons Learned
1. [Key lesson 1]
2. [Key lesson 2]
3. [Key lesson 3]

## Action Items
- [ ] Implement permanent fix
- [ ] Update monitoring/alerting
- [ ] Update runbooks
- [ ] Team training on prevention
```

**Step 3: Implement Preventive Measures**
- Update monitoring thresholds
- Improve alerting sensitivity
- Implement better pre-deployment tests
- Update runbooks and playbooks
- Conduct team training session

---

## Issue-Specific Playbooks

### Playbook 1: Database Connection Exhaustion

**Symptoms:**
- Increasing response times
- Error: "Too many connections"
- Error rate trending upward

**Immediate Response:**
```bash
# 1. Check current connections
psql -h ... -U admin -d bendcore \
  -c "SELECT count(*) FROM pg_stat_activity;"

# 2. Identify problematic connections
psql -h ... -U admin -d bendcore \
  -c "SELECT pid, user, state, query FROM pg_stat_activity WHERE state != 'idle';"

# 3. Kill long-running queries (if safe)
psql -h ... -U admin -d bendcore \
  -c "SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE (now() - query_start) > interval '5 minutes';"

# 4. Increase max connections (RDS parameter group)
aws rds modify-db-parameter-group \
  --db-parameter-group-name bendcore-prod-params \
  --parameters "ParameterName=max_connections,ParameterValue=500,ApplyMethod=immediate"

# 5. Bounce application to reset pool
# (Rolling restart via auto-scaling)
```

### Playbook 2: Out of Disk Space

**Symptoms:**
- Database write failures
- Application crashes
- Error: "No space left on device"

**Immediate Response:**
```bash
# 1. Check disk usage
df -h /var/lib/postgresql

# 2. Clean up old logs
rm -f /var/log/postgresql/*.log.*.gz

# 3. Purge old data (retention policy)
psql -h ... -U admin -d bendcore \
  -c "DELETE FROM audit_logs WHERE created_at < NOW() - INTERVAL '90 days';"

# 4. Increase EBS volume size
aws ec2 modify-volume \
  --volume-id vol-123456789 \
  --size 500

# 5. Extend filesystem
sudo growpart /dev/xvdf 1
sudo resize2fs /dev/xvdf1
```

### Playbook 3: Memory Leak in Application

**Symptoms:**
- Increasing memory usage over time
- Application slowness after running for days
- OOM killer activates

**Immediate Response:**
```bash
# 1. Identify memory leak
docker exec bendcore-container free -h

# 2. Get heap dump
docker exec bendcore-container curl http://localhost:9999/heapdump

# 3. Analyze dump
java -jar heapanalyzer.jar heapdump.hprof

# 4. Redeploy with memory limit
docker run \
  --memory 2g \
  --memory-swap 2g \
  bendcore:latest

# 5. Rolling restart (auto-scaling handles)
aws autoscaling set-desired-capacity \
  --auto-scaling-group-name bendcore-prod \
  --desired-capacity 6
```

### Playbook 4: Third-party Service Failure

**Symptoms:**
- Feature unavailable (not critical path)
- Graceful degradation in effect
- Error rates <0.1% (low impact)

**Immediate Response:**
```bash
# 1. Verify third-party status
curl -I https://api.example-service.com/health

# 2. Check retry logic
grep -A 5 "retry" src/services/thirdparty.ts

# 3. Enable fallback mode
export FEATURE_FALLBACK_MODE=true

# 4. Notify users of limited functionality
# (Status page update)

# 5. Escalate to third-party support
# (File ticket with their support team)

# 6. Update ETA estimate
# (Based on third-party communication)
```

---

## Communication Templates

### Initial Incident Notification
```
🚨 INCIDENT: [Service] - Severity: P[1-4]

Description: [Brief description of issue]
Impact: [Who is affected and how]
Status: Investigating

Next update in 10 minutes.
https://status.bendcore.io

#incidents-production
```

### Resolution Notification
```
✅ RESOLVED: [Service] is fully operational

Root Cause: [Brief explanation]
Fix Applied: [What was done]
Verification: All systems operational

Downtime: [X minutes]
Affected Users: [Approximate number]

Post-mortem report will be published within 24 hours.
```

### Post-Incident Summary (24 hours later)
```
📊 POST-MORTEM: [Incident Name]

Timeline: [Key events]
Root Cause: [Analysis]
Prevention: [What we're doing to prevent recurrence]

Full post-mortem: [Link to report]

We apologize for the disruption.
```

---

## Escalation Paths

### P1 Incident Escalation
```
On-Call Engineer (5 min)
    ↓ (if unresolved after 10 min)
Engineering Lead (10 min)
    ↓ (if unresolved after 30 min)
VP Engineering (30 min)
    ↓ (if unresolved after 1 hour)
CTO + CEO (1 hour)
```

### P2 Incident Escalation
```
On-Call Engineer (5 min)
    ↓ (if unresolved after 30 min)
Engineering Lead (30 min)
    ↓ (if unresolved after 2 hours)
VP Engineering (2 hours)
```

### P3 & P4 Escalation
```
Support Team (5 min)
    ↓ (if unresolved after 4 hours)
On-Call Engineer (4 hours)
    ↓ (if unresolved after 24 hours)
Engineering Lead (24 hours)
```

---

## Incident Metrics & SLAs

### Target Metrics
| Metric | P1 | P2 | P3 | P4 |
|--------|----|----|----|----|
| Incident Response | 15 min | 30 min | 1 hr | 4 hrs |
| Resolution | 1 hr | 4 hrs | 24 hrs | 72 hrs |
| Status Updates | Every 10 min | Every 30 min | Every 2 hrs | As needed |

### Success Criteria
- P1 incidents resolved within 1 hour: 95%+ success rate
- P2 incidents resolved within 4 hours: 98%+ success rate
- P3 incidents resolved within 24 hours: 99%+ success rate
- Customer communication within 15 minutes: 100%
- Post-mortem published within 24 hours: 100%

---

## Incident Response Training

### Quarterly Drills
- Simulate P1 outage (database failure)
- Simulate P2 partial outage (API endpoint down)
- Simulate P3 feature failure
- All team members participate
- Results reviewed and documented

### Annual Certification
- All on-call staff must complete incident response training
- Quiz on this playbook: 90% pass rate required
- Hands-on simulation: 2 hours
- Valid for 12 months

---

**Document Version:** 1.0.0  
**Last Review:** October 30, 2026  
**Next Review:** January 31, 2027  
**Owner:** Engineering Lead
