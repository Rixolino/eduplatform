# Configuration Guide

## Environment Setup

### Windows Setup

#### 1. SQLite3 Installation
```bash
# Option 1: Download from https://www.sqlite.org/download.html
# Extract sqlite3.exe and add to PATH

# Option 2: With Chocolatey
choco install sqlite
```

#### 2. Python Installation (for serving files)
```bash
# Already comes with many Windows versions
python --version

# If not installed, download from https://www.python.org
```

#### 3. C Compiler Setup
```bash
# Option 1: Visual Studio Community (free)
# Download from https://visualstudio.microsoft.com/community/

# Option 2: MinGW-w64
# Download from https://www.mingw-w64.org/

# Option 3: LLVM/Clang
# Download from https://releases.llvm.org/download.html
```

#### 4. libmicrohttpd Installation
```bash
# From source: https://www.gnu.org/software/libmicrohttpd/download.html
# Or use vcpkg
vcpkg install libmicrohttpd:x64-windows
```

### Linux Setup (Ubuntu/Debian)

```bash
# Update packages
sudo apt-get update
sudo apt-get upgrade

# Install dependencies
sudo apt-get install -y \
  build-essential \
  cmake \
  libmicrohttpd-dev \
  sqlite3 \
  libsqlite3-dev \
  git

# Install Python (usually pre-installed)
python3 --version
```

### macOS Setup

```bash
# Install Homebrew if not present
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake libmicrohttpd sqlite3 gcc

# Verify installations
cmake --version
gcc --version
sqlite3 --version
```

---

## Database Configuration

### Initial Setup

```bash
# Navigate to project directory
cd progetto-vessio

# Create database from schema
sqlite3 courses.db < schema.sql

# Verify database creation
sqlite3 courses.db ".tables"
# Should show: activity_log, courses, enrollments, feedback, lesson_progress, lessons, sessions, users
```

### Database Backup

```bash
# Backup database
cp courses.db courses.db.backup

# Or with timestamp
cp courses.db "courses.db.backup.$(date +%Y%m%d_%H%M%S)"
```

### Database Restore

```bash
# From backup
cp courses.db.backup courses.db

# Or rebuild from schema
rm courses.db
sqlite3 courses.db < schema.sql
```

### Database Cleanup

```bash
# Remove all data (keep schema)
sqlite3 courses.db < cleanup.sql

# Or manually
sqlite3 courses.db "DELETE FROM activity_log;"
sqlite3 courses.db "DELETE FROM feedback;"
sqlite3 courses.db "DELETE FROM lesson_progress;"
sqlite3 courses.db "DELETE FROM enrollments;"
sqlite3 courses.db "DELETE FROM lessons;"
sqlite3 courses.db "DELETE FROM courses;"
sqlite3 courses.db "DELETE FROM sessions;"
sqlite3 courses.db "DELETE FROM users;"
```

---

## Backend Configuration

### CMake Build Configuration

```bash
# Navigate to project
cd progetto-vessio

# Create build directory
mkdir build
cd build

# Configure (Linux/Mac)
cmake .. -DCMAKE_BUILD_TYPE=Release

# Configure (Windows MSVC)
cmake -G "Visual Studio 17 2022" ..

# Configure (Windows MinGW)
cmake -G "MinGW Makefiles" ..

# Build
cmake --build . --config Release

# On Windows, specify output directory
cmake --install . --prefix ../bin
```

### Runtime Configuration

#### server.c Configuration Constants

In `server.c`, modifica questi valori:

```c
#define PORT 3000              // Server port
#define MAX_RESPONSE 16384     // Max response size
```

#### auth.h Configuration

In `auth.h`, modifica:

```c
#define JWT_SECRET "your-secret-key-change-in-production"
#define TOKEN_EXPIRY 86400  // 24 hours in seconds
```

#### Database Path

In `server.c` main():

```c
open_database("courses.db");  // Change path if needed
```

---

## Frontend Configuration

### api.js Configuration

In `api.js`, modifica l'URL base:

```javascript
constructor(baseURL = 'http://localhost:3000') {
    // Change to your backend URL
    this.baseURL = baseURL;
}
```

### Environment-specific Configuration

**Development:**
```javascript
const api = new APIClient('http://localhost:3000');
```

**Staging:**
```javascript
const api = new APIClient('https://staging.example.com:3000');
```

**Production:**
```javascript
const api = new APIClient('https://api.example.com');
```

### CORS Configuration

Il backend è configurato con CORS aperto. Per produzione, configurare:

In `server.c`:
```c
MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
// Cambia "*" con dominio specifico in produzione
```

---

## Environment Variables (Futuri)

Per implementazione futura, crea `.env` file:

```
# .env (non committare in git)
DB_PATH=courses.db
API_PORT=3000
JWT_SECRET=your-secret-key
ENVIRONMENT=development
LOG_LEVEL=debug
MAX_UPLOAD_SIZE=104857600  # 100MB
CORS_ORIGIN=http://localhost:8000
```

---

## Server Deployment

### Production Checklist

- [ ] Database backup creato
- [ ] JWT secret cambiato da default
- [ ] CORS configurato per dominio specifico
- [ ] Logging attivato
- [ ] Frontend minificato
- [ ] Database permissions verificati
- [ ] Backup strategy implementata
- [ ] Monitoring setup

### Using Systemd (Linux)

Crea `/etc/systemd/system/course-api.service`:

```ini
[Unit]
Description=Course Platform API
After=network.target

[Service]
Type=simple
User=www-data
WorkingDirectory=/var/www/course-platform
ExecStart=/var/www/course-platform/bin/course_server
Restart=on-failure
RestartSec=10

[Install]
WantedBy=multi-user.target
```

Abilita il servizio:

```bash
sudo systemctl daemon-reload
sudo systemctl enable course-api
sudo systemctl start course-api
sudo systemctl status course-api
```

### Using Supervisor (Linux)

Crea `/etc/supervisor/conf.d/course-api.conf`:

```ini
[program:course-api]
command=/var/www/course-platform/bin/course_server
directory=/var/www/course-platform
user=www-data
autostart=true
autorestart=true
redirect_stderr=true
stdout_logfile=/var/log/course-api.log
stdout_logfile_maxbytes=1MB
stdout_logfile_backups=10
```

---

## Logging Configuration

### Backend Logging

Il backend logga a stdout. Per file logging:

```bash
# Redirect to file
./bin/course_server > server.log 2>&1 &

# Or with rotation
./bin/course_server 2>&1 | tee -a >(rotatelogs server.log 1M)
```

### Frontend Logging

Nel browser, controllare console (F12):

```javascript
// Enable debug logging in app.js
const DEBUG = true;

if (DEBUG) {
    console.log('API Response:', response);
}
```

---

## Security Configuration

### HTTPS/TLS

Per produzione, aggiungere SSL certificate:

```c
// In server.c, quando si crea il daemon
MHD_start_daemon(
    MHD_USE_SELECT_INTERNALLY | MHD_USE_TLS,
    PORT,
    NULL, NULL,
    &request_handler, NULL,
    MHD_OPTION_HTTPS_MEM_KEY, private_key_pem,
    MHD_OPTION_HTTPS_MEM_CERT, certificate_pem,
    MHD_OPTION_END
);
```

### Password Security

Sostituire il semplice hashing in `auth.h` con bcrypt:

```c
// Installare libbcrypt
#include <bcrypt.h>

char *hash_password(const char *password) {
    char hash[61];
    bcrypt_hashpw(password, bcrypt_gensalt(12), hash);
    return strdup(hash);
}

int verify_password(const char *password, const char *hash) {
    return bcrypt_checkpw(password, hash) == 0;
}
```

### Rate Limiting

Aggiungere rate limiting in futuro:

```c
// Tracks per IP
struct RateLimit {
    const char *ip;
    int requests;
    time_t window_start;
} rate_limits[MAX_IPS];
```

---

## Performance Tuning

### Database Optimization

```sql
-- Analyze query performance
EXPLAIN QUERY PLAN
SELECT * FROM courses WHERE category = 'Programming';

-- Add indexes if needed
CREATE INDEX IF NOT EXISTS idx_courses_category ON courses(category);

-- Vacuum database
VACUUM;
```

### Memory Optimization

In `server.c`:

```c
// Limit concurrent connections
#define MAX_CONNECTIONS 1000

// Limit request size
#define MAX_REQUEST_SIZE 10485760  // 10MB
```

### Connection Pooling

Per futuri load balancer:

```c
#define DB_POOL_SIZE 10
sqlite3 *db_pool[DB_POOL_SIZE];
```

---

## Testing Configuration

### Test Database

Crea database separato per test:

```bash
sqlite3 courses_test.db < schema.sql
```

### Test API Endpoints

Crea file `test.sh`:

```bash
#!/bin/bash

BASE_URL="http://localhost:3000"

echo "Testing health endpoint..."
curl -s $BASE_URL/api/health | jq .

echo "Testing registration..."
curl -s -X POST $BASE_URL/api/users/register \
  -H "Content-Type: application/json" \
  -d '{"username":"test","email":"test@test.com","password":"pass","full_name":"Test","role":"student"}' | jq .
```

---

## Troubleshooting Configuration

### Port Already in Use

```bash
# Linux/Mac: Find process using port
lsof -i :3000

# Windows: Find process using port
netstat -ano | findstr :3000

# Kill process
kill -9 <PID>  # Linux/Mac
taskkill /PID <PID> /F  # Windows
```

### Database Locked

```bash
# Verify no other connections
lsof | grep courses.db

# Restart server
killall course_server
# or
taskkill /IM course_server.exe /F
```

### Memory Leaks

```bash
# Use valgrind on Linux
valgrind --leak-check=full ./bin/course_server

# Use heaptrack
heaptrack ./bin/course_server
```

---

## Backup Strategy

### Automatic Backups

Crea script `backup.sh`:

```bash
#!/bin/bash
BACKUP_DIR="/backups/course-platform"
DATE=$(date +%Y%m%d_%H%M%S)

mkdir -p $BACKUP_DIR
cp courses.db $BACKUP_DIR/courses_$DATE.db

# Keep only last 7 days
find $BACKUP_DIR -name "courses_*.db" -mtime +7 -delete
```

Aggiungi a cron:

```bash
# Backup ogni ora
0 * * * * /path/to/backup.sh
```

---

## Maintenance Tasks

### Regular Maintenance

```bash
# Daily
- Check server logs
- Monitor database size
- Verify backups

# Weekly
- Vacuum database (VACUUM;)
- Analyze queries (ANALYZE;)
- Test restore procedure

# Monthly
- Review performance metrics
- Update dependencies
- Security audit
```

### Database Maintenance

```sql
-- Analyze database
ANALYZE;

-- Optimize storage
VACUUM;

-- Check integrity
PRAGMA integrity_check;

-- Get statistics
PRAGMA page_size;
PRAGMA page_count;
```

---

## Next Steps

1. Complete Phase 2 autenticazione endpoint
2. Implementare API endpoints per corsi
3. Aggiungere unit testing
4. Setup CI/CD pipeline
5. Implementare monitoring
6. Pianificare scale-out

---

**Nota**: Questa configurazione è per ambienti di sviluppo e staging. Per produzione, consultare security best practices e compliance requirements.
