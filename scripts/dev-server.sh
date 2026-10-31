#!/usr/bin/env bash
# Development Server - Start all services for BendCore UI testing
# Usage: ./scripts/dev-server.sh
# Will start:
# - Mock BendCore Server (http://localhost:8080)
# - Frontend dev server (http://localhost:3000)
# - Backend (optional, if available)

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SERVICES=()

echo "🚀 BendCore Development Server"
echo ""

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[0;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

cleanup() {
  echo ""
  echo "🛑 Stopping services..."
  for pid in "${SERVICES[@]}"; do
    kill "$pid" 2>/dev/null || true
  done
  wait "${SERVICES[@]}" 2>/dev/null || true
  echo "✓ All services stopped"
  exit 0
}

trap cleanup SIGINT SIGTERM EXIT

# 1. Start mock server
echo "📡 Starting Mock Server..."
node "$ROOT/scripts/mock-server.js" > /tmp/bendcore-mock-server.log 2>&1 &
MOCK_PID=$!
SERVICES+=("$MOCK_PID")
echo "   PID: $MOCK_PID (http://localhost:8080)"

# Wait for mock server
sleep 1
if ! curl -sf http://localhost:8080/v1/health >/dev/null 2>&1; then
  echo -e "${RED}✗ Mock server failed to start${NC}"
  tail -n 20 /tmp/bendcore-mock-server.log
  exit 1
fi
echo -e "${GREEN}✓ Mock server ready${NC}"

# 2. Check for real backend
if [[ -f "$ROOT/backend/package.json" ]]; then
  if pgrep -f "node.*backend.*main" >/dev/null; then
    echo -e "${YELLOW}⚠ Backend already running (detected)${NC}"
  else
    echo "🔨 Backend available (not started - use: cd backend && npm start)"
  fi
fi

# 3. Start frontend dev server
echo ""
echo "🎨 Starting Frontend Dev Server..."
cd "$ROOT"

# Check if frontend is already started
if lsof -Pi :3000 -sTCP:LISTEN -t >/dev/null 2>&1; then
  echo -e "${YELLOW}⚠ Port 3000 already in use${NC}"
  echo "   Frontend might already be running"
else
  # Start npm dev server if available
  if grep -q '"start"' package.json 2>/dev/null; then
    npm start > /tmp/bendcore-frontend.log 2>&1 &
    FRONTEND_PID=$!
    SERVICES+=("$FRONTEND_PID")
    echo "   PID: $FRONTEND_PID (http://localhost:3000)"

    sleep 3
    if ! curl -sf http://localhost:3000 >/dev/null 2>&1; then
      echo -e "${YELLOW}⚠ Frontend might still be starting...${NC}"
    else
      echo -e "${GREEN}✓ Frontend ready${NC}"
    fi
  fi
fi

echo ""
echo -e "${GREEN}═══════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}✓ Development environment ready!${NC}"
echo -e "${GREEN}═══════════════════════════════════════════════════════════${NC}"
echo ""
echo "📍 Available Services:"
echo "   Mock Server:  http://localhost:8080"
echo "   Frontend:     http://localhost:3000"
echo "   Backend:      http://localhost:8080 (if running)"
echo ""
echo "🔧 API Endpoints (for testing):"
echo "   GET  /v1/health"
echo "   POST /v1/tube?format=csv|json&od=16&wall=1.5"
echo "   POST /v1/analyze?format=step&deflection=0.1"
echo ""
echo "💡 Testing ImportDialog:"
echo "   1. Open http://localhost:3000"
echo "   2. Try importing a STEP file"
echo "   3. Check console for API calls"
echo ""
echo "📊 Logs:"
echo "   Mock Server:  /tmp/bendcore-mock-server.log"
echo "   Frontend:     /tmp/bendcore-frontend.log (if started)"
echo ""
echo "Press Ctrl+C to stop all services"
echo ""

# Keep running
wait
