# BendCore Web - System Architecture

Complete architectural overview of BendCore Web system design.

## System Overview

BendCore Web is a modern, containerized, full-stack CAD application for tube bending design and manufacturing.

```
┌─────────────────────────────────────────────────────────────────┐
│                        Client Layer                              │
│                    (Web Browser / SPA)                           │
│                     React + TypeScript                           │
└────────────────────────────┬────────────────────────────────────┘
                             │
                      ┌──────▼──────┐
                      │  HTTP/HTTPS │
                      │    Nginx    │
                      │ (Reverse    │
                      │  Proxy)     │
                      └──────┬──────┘
                             │
        ┌────────────────────┼────────────────────┐
        │                    │                    │
┌───────▼────────┐  ┌────────▼────────┐  ┌──────▼───────┐
│   Backend API  │  │   Cache Layer   │  │ Static Files │
│  (Node.js +    │  │    (Redis)      │  │  (Nginx)     │
│  Express)      │  │                 │  │              │
└───────┬────────┘  └────────┬────────┘  └──────────────┘
        │                    │
        └────────────────────┼────────────────────┐
                             │                    │
                    ┌────────▼────────┐  ┌────────▼────────┐
                    │    Database     │  │   File Storage  │
                    │  (PostgreSQL)   │  │    (S3/Local)   │
                    │                 │  │                 │
                    └─────────────────┘  └─────────────────┘
```

## Technology Stack

### Frontend
- **Framework**: React 18.2
- **Language**: TypeScript 5.1
- **Build Tool**: Vite
- **Styling**: Tailwind CSS
- **State Management**: React Context / Redux (optional)
- **3D Graphics**: Three.js
- **HTTP Client**: Axios / Fetch API

### Backend
- **Runtime**: Node.js 20+
- **Framework**: Express.js 4.18
- **Language**: TypeScript 5.1
- **ORM**: Sequelize / TypeORM (optional)
- **Database**: PostgreSQL 15
- **Cache**: Redis 7
- **Authentication**: JWT / Session-based

### DevOps
- **Containerization**: Docker
- **Orchestration**: Docker Compose
- **CI/CD**: GitHub Actions
- **Deployment**: Docker Hub, Vercel, Railway, AWS
- **Monitoring**: Prometheus, Grafana (optional)
- **Logging**: ELK Stack (optional)

## Architecture Patterns

### Layered Architecture

```
┌─────────────────────────────────┐
│   Presentation Layer (React)    │  UI Components, Pages
├─────────────────────────────────┤
│   API Layer (Express Routes)    │  REST Endpoints
├─────────────────────────────────┤
│   Business Logic Layer          │  Services, Validators
├─────────────────────────────────┤
│   Data Access Layer (ORM)       │  Database Queries
├─────────────────────────────────┤
│   Database Layer (PostgreSQL)   │  Data Storage
└─────────────────────────────────┘
```

### Microservices-Ready (Future)

Can be extended to separate services:

```
bendcore-web/
├── api-gateway (Express)
├── auth-service (JWT validation)
├── design-service (CAD logic)
├── manufacturing-service (Production reports)
└── collaboration-service (Real-time sync)
```

## Data Flow

### Request Lifecycle

```
1. User Action (Browser)
   ↓
2. HTTP Request (Axios)
   ↓
3. Nginx Reverse Proxy
   ↓
4. Express Route Handler
   ↓
5. Authentication Middleware
   ↓
6. Business Logic Service
   ↓
7. Database Query (ORM)
   ↓
8. PostgreSQL
   ↓
9. Response (JSON)
   ↓
10. React State Update
   ↓
11. Component Re-render
```

### Caching Strategy

```
Browser Cache (HTTP Headers)
    ↓
CDN Cache (Cloudflare/CloudFront)
    ↓
Redis Cache (Application)
    ↓
Database (PostgreSQL)
```

Cache invalidation:
- Static assets: 1 year (immutable)
- API responses: 1-5 minutes (varies)
- User data: Real-time (no cache)

## API Design

### RESTful Endpoints

```
GET    /api/projects              # List projects
POST   /api/projects              # Create project
GET    /api/projects/:id          # Get project
PUT    /api/projects/:id          # Update project
DELETE /api/projects/:id          # Delete project

GET    /api/designs/:id/tubes     # List tubes
POST   /api/designs/:id/tubes     # Add tube
PUT    /api/tubes/:id             # Update tube
DELETE /api/tubes/:id             # Delete tube

GET    /api/reports               # List reports
POST   /api/reports               # Generate report
GET    /api/reports/:id/download  # Download report
```

### Request/Response Format

**Request:**
```json
{
  "method": "POST",
  "url": "/api/designs",
  "headers": {
    "Authorization": "Bearer token",
    "Content-Type": "application/json"
  },
  "body": {
    "name": "Design 1",
    "description": "Tube bending project"
  }
}
```

**Response:**
```json
{
  "status": 201,
  "data": {
    "id": "uuid",
    "name": "Design 1",
    "created_at": "2024-01-01T00:00:00Z"
  },
  "error": null
}
```

### Error Handling

```
200 OK              # Success
201 Created         # Resource created
204 No Content      # Success, no body
400 Bad Request     # Invalid input
401 Unauthorized    # Auth required
403 Forbidden       # Access denied
404 Not Found       # Resource not found
500 Server Error    # Internal error
```

## Database Schema

### Core Tables

```sql
-- Users
users
├── id (UUID)
├── email (string)
├── password (hash)
├── profile (JSON)
└── created_at (timestamp)

-- Projects
projects
├── id (UUID)
├── user_id (FK)
├── name (string)
├── description (text)
├── settings (JSON)
└── created_at (timestamp)

-- Designs
designs
├── id (UUID)
├── project_id (FK)
├── name (string)
├── data (JSON)
└── version (integer)

-- Tubes
tubes
├── id (UUID)
├── design_id (FK)
├── profile (enum)
├── diameter (float)
├── thickness (float)
├── length (float)
└── bends (JSON)

-- Reports
reports
├── id (UUID)
├── design_id (FK)
├── report_type (enum)
├── data (JSON)
└── created_at (timestamp)
```

### Relationships

```
users
  1──────∞ projects
           │
           1──────∞ designs
                    │
                    1──────∞ tubes
                    │
                    1──────∞ reports
```

## Deployment Architecture

### Development Environment

```
docker-compose.yml
├── backend (Node.js) - port 3001
├── frontend (Nginx) - port 80
├── postgres (DB) - port 5432
├── redis (Cache) - port 6379
└── volumes (data persistence)
```

### Production Environment

```
docker-compose.prod.yml
├── backend (Node.js, 1 CPU, 512MB RAM)
│   ├── health checks every 30s
│   ├── auto-restart on failure
│   └── structured logging
├── frontend (Nginx, 2 CPU, 1GB RAM)
│   ├── SSL/TLS termination
│   ├── gzip compression
│   ├── security headers
│   └── rate limiting
├── postgres (DB, 2 CPU, 1GB RAM)
│   ├── persistent volume
│   ├── daily backups
│   └── connection pooling
└── redis (Cache, 1 CPU, 256MB RAM)
    ├── AOF persistence
    └── maxmemory policy
```

### Cloud Deployment (AWS Example)

```
Internet
    ↓
CloudFront (CDN)
    ↓
ELB (Load Balancer)
    ├─ ECS (Backend Cluster)
    │  ├─ Task 1 (Container)
    │  ├─ Task 2 (Container)
    │  └─ Task N (Container)
    │
    ├─ RDS (PostgreSQL)
    │
    └─ ElastiCache (Redis)

S3 + CloudFront (Frontend)
    ↓
Static assets
```

## Security Architecture

### Authentication Flow

```
1. User Login
   ├─ POST /api/auth/login
   ├─ Verify credentials
   └─ Issue JWT token
   
2. Authenticated Request
   ├─ Include token in Authorization header
   ├─ Verify token signature
   ├─ Check token expiration
   └─ Extract user ID from claims
   
3. Authorization
   ├─ Check user permissions
   ├─ Verify resource ownership
   └─ Deny/allow action
```

### Security Layers

```
Layer 1: Network
├─ HTTPS/TLS encryption
├─ SSL certificates
└─ Firewall rules

Layer 2: Application
├─ Input validation
├─ SQL injection prevention
├─ XSS protection
├─ CSRF tokens
└─ Rate limiting

Layer 3: Data
├─ Password hashing (bcrypt)
├─ Encryption at rest
├─ Encryption in transit
└─ Audit logging

Layer 4: Infrastructure
├─ Non-root containers
├─ Resource limits
├─ Network policies
└─ Secret management
```

## Scaling Strategy

### Horizontal Scaling

```
Load Balancer
    ├─ Backend Instance 1
    ├─ Backend Instance 2
    ├─ Backend Instance 3
    └─ Backend Instance N
    
Shared Resources:
├─ PostgreSQL (RDS)
├─ Redis (Cluster)
└─ S3 Storage
```

Scale-out triggers:
- CPU > 70%
- Memory > 80%
- Request latency > 500ms
- Database connections > 80

### Database Optimization

```
Indexes:
├─ Primary keys
├─ Foreign keys
├─ Frequently queried columns
└─ Composite indexes

Query optimization:
├─ Connection pooling
├─ Prepared statements
├─ Lazy loading
└─ Caching

Partitioning (future):
├─ By user (multi-tenancy)
├─ By date (time-series)
└─ By region (geo-distribution)
```

### Caching Optimization

```
Level 1: Browser Cache
├─ Static assets: 1 year
├─ API responses: 1-5 minutes
└─ User data: No cache

Level 2: CDN Cache (CloudFront)
├─ Geographically distributed
├─ Edge caching
└─ Cache invalidation

Level 3: Application Cache (Redis)
├─ Session data
├─ API responses
├─ User preferences
└─ TTL-based expiration
```

## Monitoring & Observability

### Key Metrics

```
Application:
├─ Request latency (p50, p95, p99)
├─ Error rate
├─ Requests per second
├─ Cache hit ratio
└─ Database query time

Infrastructure:
├─ CPU usage
├─ Memory usage
├─ Disk space
├─ Network bandwidth
└─ Container health

Business:
├─ Active users
├─ Design creation rate
├─ Report generation rate
└─ User retention
```

### Logging Strategy

```
Log Levels:
├─ ERROR: Stack traces, critical failures
├─ WARN: Deprecations, unusual conditions
├─ INFO: Request/response logs, user actions
└─ DEBUG: Variable values, code flow

Log Format:
├─ Timestamp
├─ Level
├─ Service
├─ Request ID (correlation)
├─ User ID
├─ Message
└─ Stack trace (if error)

Retention:
├─ ERROR: 30 days
├─ WARN: 7 days
├─ INFO: 3 days
└─ DEBUG: 1 day
```

## Disaster Recovery

### Backup Strategy

```
Database:
├─ Automated daily backups
├─ Point-in-time recovery
├─ Cross-region replication
└─ 30-day retention

Application:
├─ Infrastructure as Code (Terraform)
├─ Configuration versioning
└─ Deployment automation

Data:
├─ S3 versioning
├─ Backup encryption
└─ Off-site storage
```

### High Availability

```
Multi-region:
├─ Primary region (active)
├─ Secondary region (standby)
└─ Automatic failover

Within region:
├─ Load balancer distribution
├─ Multiple availability zones
├─ Database replication
└─ Redis clustering
```

## Performance Optimization

### Frontend

```
Code Splitting:
├─ Route-based code splitting
├─ Component lazy loading
└─ Vendor bundle separation

Asset Optimization:
├─ Image compression
├─ SVG optimization
├─ Minification
├─ Gzip compression
└─ WebP format

Network:
├─ HTTP/2
├─ CDN distribution
├─ Service workers
└─ Prefetching
```

### Backend

```
Code:
├─ Async/await patterns
├─ Connection pooling
├─ Query optimization
└─ Caching

Infrastructure:
├─ Load balancing
├─ Horizontal scaling
├─ Database indexing
└─ Redis caching
```

## Future Enhancements

### Phase 2: Real-time Collaboration
```
WebSocket support
├─ Live updates
├─ Conflict resolution
└─ Presence awareness
```

### Phase 3: Advanced Features
```
AI/ML Integration
├─ Design optimization
├─ Manufacturing prediction
└─ Anomaly detection
```

### Phase 4: Enterprise
```
Advanced Security:
├─ SAML/OAuth2
├─ Role-based access
└─ Audit logging

Multi-tenancy:
├─ User organizations
├─ Data isolation
└─ Custom branding
```

## References

- [Express.js Best Practices](https://expressjs.com/en/advanced/best-practice-performance.html)
- [React Best Practices](https://react.dev/learn)
- [Docker Best Practices](https://docs.docker.com/develop/dev-best-practices/)
- [PostgreSQL Documentation](https://www.postgresql.org/docs/)
- [System Design Principles](https://www.system-design-primer.com/)
