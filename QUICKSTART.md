# BendCore Quick Start

## 🚀 One-Minute Setup

### Option 1: Docker (Simplest)
```bash
docker build -t bendcore:latest .
docker-compose up
# Open http://localhost:3001
```

### Option 2: Manual Build
```bash
# 1. Build C++ server
cmake --preset host -DBENDCORE_WITH_OCCT=ON
cmake --build --preset host

# 2. Start server
./build/bendcore_server 8080 &

# 3. Start Node backend
cd backend && npm install && npm start &

# 4. Start React frontend
cd ../ui && npm install && npm start
# Open http://localhost:3000
```

## 📋 Complete Workflow

1. **Import** STEP/IGES file via UI
2. **View** 3D mesh in Viewport3D
3. **Export** as SVG drawing
4. **Run** FEA stress analysis
5. **Optimize** geometry for weight reduction

## ✅ Test Everything

```bash
./tests/integration/test_complete_workflow.sh
```

Should show: "All Tests Passed!"

## 📚 Full Documentation

- **USER_GUIDE.md** - Complete user manual
- **IMPLEMENTATION_STATUS.md** - Development roadmap
- **Dockerfile** - Production deployment
- **docker-compose.yml** - Multi-service orchestration

## 🔗 Endpoints

| Service | URL | Purpose |
|---------|-----|---------|
| bendcore_server | http://localhost:8080 | C++ analysis engine |
| Node backend | http://localhost:3001 | API proxy layer |
| React UI | http://localhost:3000 | Web interface |

## 🎯 What You Can Do

✅ Import STEP/IGES CAD files  
✅ Analyze tube geometry automatically  
✅ View 3D meshes in browser  
✅ Export technical drawings  
✅ Calculate FEA stresses  
✅ Optimize for weight  

## 🐛 Troubleshooting

**Port already in use?**
```bash
./build/bendcore_server 9090
# Update PORT in docker-compose.yml if needed
```

**Missing OCCT?**
```bash
sudo apt install libocct-data-exchange-dev libocct-modeling-algorithms-dev
```

**Node issues?**
```bash
cd backend && rm -rf node_modules package-lock.json && npm install
```

## 📞 Support

See USER_GUIDE.md for detailed troubleshooting and advanced features.

---

**Ready to use!** 🎉
