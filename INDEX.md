# 📚 Documentation Index

Welcome to the **Piattaforma Corsi Online** project! This index helps you navigate all available documentation.

---

## 🚀 Start Here

### For First-Time Users
1. **[PROJECT_STATUS.md](PROJECT_STATUS.md)** ⭐ **START HERE**
   - Executive summary
   - What's been completed
   - Quick overview

2. **[QUICKSTART.md](QUICKSTART.md)** ⚡ **5 Minutes to Running**
   - Setup in 5 easy steps
   - Common issues
   - What to do next

---

## 📖 Complete Guides

### User Documentation
- **[README.md](README.md)** - Complete user guide
  - Installation instructions
  - How to use the platform
  - Features overview
  - Troubleshooting

### Developer Documentation
- **[DEVELOPMENT.md](DEVELOPMENT.md)** - Developer guide
  - How to add endpoints
  - How to add pages
  - Database modifications
  - Testing procedures

- **[API.md](API.md)** - API Reference
  - All 35+ endpoints documented
  - Request/response examples
  - Error codes
  - CORS configuration

### Setup Documentation
- **[CONFIGURATION.md](CONFIGURATION.md)** - Advanced setup
  - Environment setup (Windows/Linux/Mac)
  - Database configuration
  - Backend/frontend configuration
  - Security setup
  - Deployment guide

### Technical Documentation
- **[IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md)** - Technical summary
  - What's been implemented
  - Architecture diagrams
  - File statistics
  - Database schema details

---

## 🗂️ Project Files

### Backend (C)
```
server.c           - HTTP server with routing
auth.h            - Authentication system (JWT, password hashing)
db_utils.h        - Database abstraction layer
json_utils.h      - JSON response formatting
CMakeLists.txt    - Build configuration
```

### Frontend (Web)
```
index.html        - Complete HTML markup (400 lines)
styles.css        - Professional styling (400 lines)
api.js            - API client (250 lines)
app.js            - Application logic (500 lines)
```

### Database
```
schema.sql        - SQLite3 database schema (8 tables)
courses.db        - Database file (generated on first run)
```

---

## 🎯 By Use Case

### "I want to understand the project"
1. Read [PROJECT_STATUS.md](PROJECT_STATUS.md)
2. Read [README.md](README.md)
3. Read [IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md)

### "I want to get it running"
1. Follow [QUICKSTART.md](QUICKSTART.md)
2. Refer to [CONFIGURATION.md](CONFIGURATION.md) for setup issues

### "I want to add features"
1. Read [DEVELOPMENT.md](DEVELOPMENT.md)
2. Reference [API.md](API.md) for endpoint specs
3. Consult [IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md) for architecture

### "I want to understand the API"
1. Read [API.md](API.md)
2. Test endpoints with curl examples
3. Refer to [README.md](README.md) for usage

### "I want to deploy"
1. Read [CONFIGURATION.md](CONFIGURATION.md)
2. Consult [DEVELOPMENT.md](DEVELOPMENT.md) for best practices
3. Check [README.md](README.md) for troubleshooting

---

## 📊 Documentation Statistics

| Document | Size | Lines | Focus |
|----------|------|-------|-------|
| PROJECT_STATUS.md | 12.8 KB | 400 | Executive summary |
| QUICKSTART.md | 4.1 KB | 130 | Getting started |
| README.md | 6.3 KB | 200 | User guide |
| API.md | 11.9 KB | 380 | API reference |
| DEVELOPMENT.md | 9.0 KB | 280 | Developer guide |
| CONFIGURATION.md | 10.8 KB | 340 | Setup guide |
| IMPLEMENTATION_SUMMARY.md | 12.7 KB | 400 | Technical summary |
| **TOTAL** | **67.6 KB** | **2,130** | **Complete docs** |

---

## 🔍 Search by Topic

### Authentication
- API: [AUTH endpoints](API.md#autenticazione)
- Implementation: [auth.h](DEVELOPMENT.md#implementare-jwt)
- Setup: [CONFIGURATION.md - Security](CONFIGURATION.md#security-configuration)

### Courses
- API: [Course endpoints](API.md#corsi)
- Implementation: [Add course endpoint](DEVELOPMENT.md#aggiungere-un-nuovo-endpoint)
- Database: [courses table](IMPLEMENTATION_SUMMARY.md#database)

### Frontend
- UI Components: [index.html structure](README.md)
- Styling: [CSS classes](styles.css)
- Client: [api.js methods](DEVELOPMENT.md#aggiungere-un-nuovo-endpoint)

### Database
- Schema: [schema.sql](schema.sql)
- Details: [Table definitions](IMPLEMENTATION_SUMMARY.md#database)
- Optimization: [CONFIGURATION.md - Performance](CONFIGURATION.md#performance-tuning)

### Deployment
- Production: [CONFIGURATION.md - Deployment](CONFIGURATION.md#server-deployment)
- Systemd: [CONFIGURATION.md - Systemd](CONFIGURATION.md#using-systemd-linux)
- Docker: [Roadmap](README.md#roadmap-futuro)

---

## 🚦 Quick Command Reference

### Database
```bash
# Create database
sqlite3 courses.db < schema.sql

# Backup database
cp courses.db courses.db.backup

# Access database
sqlite3 courses.db
```

### Backend
```bash
# Compile
mkdir build && cd build
cmake .. && cmake --build .

# Run
./bin/course_server

# Test
curl http://localhost:3000/api/health
```

### Frontend
```bash
# Serve
python -m http.server 8000

# Access
http://localhost:8000
```

---

## 📞 Getting Help

### Problem Resolution Steps
1. Check [QUICKSTART.md - Common Problems](QUICKSTART.md#problemi-comuni)
2. Read [README.md - Troubleshooting](README.md#troubleshooting)
3. Consult [CONFIGURATION.md - Troubleshooting](CONFIGURATION.md#troubleshooting-configuration)
4. Review [DEVELOPMENT.md - Troubleshooting](DEVELOPMENT.md#troubleshooting)

### Common Issues & Solutions

**"Cannot connect to server"**
- See [QUICKSTART.md](QUICKSTART.md#cannot-connect-to-server)

**"Port already in use"**
- See [CONFIGURATION.md](CONFIGURATION.md#port-already-in-use)

**"Database locked"**
- See [CONFIGURATION.md](CONFIGURATION.md#database-locked)

**"CORS error"**
- See [API.md - CORS](API.md#cors)

**"Compilation error"**
- See [CONFIGURATION.md](CONFIGURATION.md#environment-setup)

---

## 📚 Learning Path

### Beginner
1. PROJECT_STATUS.md (overview)
2. QUICKSTART.md (setup)
3. README.md (usage)
4. Try the frontend

### Intermediate
1. API.md (understand endpoints)
2. DEVELOPMENT.md (how it works)
3. schema.sql (database design)
4. Try curl commands

### Advanced
1. IMPLEMENTATION_SUMMARY.md (architecture)
2. CONFIGURATION.md (production setup)
3. server.c (backend code)
4. app.js (frontend code)

---

## ✅ Checklist for New Developers

- [ ] Read PROJECT_STATUS.md
- [ ] Follow QUICKSTART.md
- [ ] Verify database created
- [ ] Test frontend loads
- [ ] Read API.md
- [ ] Try curl command on /api/health
- [ ] Read DEVELOPMENT.md
- [ ] Understand database schema
- [ ] Review authentication system
- [ ] Plan first feature implementation

---

## 🔗 Document Relationships

```
PROJECT_STATUS.md
    ├── QUICKSTART.md (how to start)
    └── README.md (overview)
         ├── API.md (what endpoints exist)
         ├── DEVELOPMENT.md (how to extend)
         ├── CONFIGURATION.md (how to setup)
         └── IMPLEMENTATION_SUMMARY.md (technical details)
```

---

## 📋 File Tree

```
progetto-vessio/
├── Documentation/
│   ├── INDEX.md ......................... This file
│   ├── PROJECT_STATUS.md ............... Executive summary ⭐
│   ├── QUICKSTART.md ................... Quick start ⚡
│   ├── README.md ....................... User guide
│   ├── API.md .......................... API reference
│   ├── DEVELOPMENT.md .................. Developer guide
│   ├── CONFIGURATION.md ................ Setup guide
│   └── IMPLEMENTATION_SUMMARY.md ....... Technical summary
│
├── Backend/
│   ├── server.c ........................ HTTP Server
│   ├── auth.h .......................... Authentication
│   ├── db_utils.h ...................... Database Layer
│   └── json_utils.h .................... JSON Formatter
│
├── Frontend/
│   ├── index.html ...................... UI Markup
│   ├── styles.css ...................... Styling
│   ├── api.js .......................... API Client
│   └── app.js .......................... App Logic
│
├── Database/
│   ├── schema.sql ...................... Database Schema
│   └── courses.db ...................... Database File
│
└── Build/
    └── CMakeLists.txt .................. Build Config
```

---

## 🎓 Learning Resources

### For Backend Development
- Study [auth.h](DEVELOPMENT.md#implementare-jwt) for authentication
- Study [db_utils.h](DEVELOPMENT.md#aggiungere-un-nuovo-endpoint) for database
- Study [json_utils.h](DEVELOPMENT.md#aggiungere-un-nuovo-endpoint) for responses

### For Frontend Development
- Study [index.html](README.md) for structure
- Study [styles.css](README.md) for styling
- Study [api.js](DEVELOPMENT.md#aggiungere-un-metodo-api) for API integration
- Study [app.js](DEVELOPMENT.md#aggiungere-una-nuova-pagina) for logic

### For Database Development
- Study [schema.sql](IMPLEMENTATION_SUMMARY.md#database) for design
- Refer to [CONFIGURATION.md - Database Optimization](CONFIGURATION.md#database-optimization)
- Review [DEVELOPMENT.md - Database](DEVELOPMENT.md#database) section

---

## 🚀 Next Steps

1. **New to the project?** → Start with [PROJECT_STATUS.md](PROJECT_STATUS.md)
2. **Want to run it?** → Follow [QUICKSTART.md](QUICKSTART.md)
3. **Want to extend it?** → Read [DEVELOPMENT.md](DEVELOPMENT.md)
4. **Want to deploy it?** → Check [CONFIGURATION.md](CONFIGURATION.md)
5. **Want API details?** → Consult [API.md](API.md)

---

## 📞 Support

All documentation is self-contained and cross-referenced. Each document includes:
- Table of contents
- Code examples
- Troubleshooting sections
- Links to related documents

Start with any document that matches your current need!

---

**Last Updated**: 2024  
**Version**: 1.0  
**Status**: Production Ready (Beta)

**Total Documentation**: 67.6 KB, 2,130 lines, fully indexed and cross-referenced
