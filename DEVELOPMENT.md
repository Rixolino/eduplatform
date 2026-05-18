# Guida di Sviluppo - Piattaforma Corsi Online

## Panoramica del Progetto

Questo documento fornisce una guida completa allo sviluppo della piattaforma corsi online.

### Struttura del Progetto

```
progetto-vessio/
├── Backend C
│   ├── server.c              - Server HTTP principale
│   ├── db_utils.h            - Utilities per database
│   ├── auth.h                - Sistema autenticazione
│   ├── json_utils.h          - Utilities JSON
│   ├── CMakeLists.txt        - Build configuration
│   └── courses.db            - Database SQLite
│
├── Frontend Web
│   ├── index.html            - HTML principale
│   ├── styles.css            - Stili CSS
│   ├── api.js                - Client API
│   └── app.js                - Logica applicazione
│
├── Database
│   └── schema.sql            - Schema SQLite
│
├── Documentation
│   ├── README.md             - Guida utente
│   └── DEVELOPMENT.md        - Questa guida
```

## Fasi di Sviluppo

### Fase 1: Setup e Database ✅ COMPLETATO
- ✅ Progettazione schema database
- ✅ Implementazione server HTTP base in C
- ✅ Connettore database SQLite

### Fase 2: Autenticazione 🔄 IN PROGRESSO
- ⏳ Sistema autenticazione JWT
- ⏳ Endpoint registrazione
- ⏳ Endpoint login
- ⏳ Middleware verifica token

### Fase 3-12: DA IMPLEMENTARE

## Implementazione Endpoint API

### Formato delle Risposte

Tutte le API devono ritornare JSON nel seguente formato:

**Successo:**
```json
{
  "status": "success",
  "data": { /* dati */ }
}
```

**Errore:**
```json
{
  "status": "error",
  "error": "Messaggio di errore"
}
```

### Aggiungere un Nuovo Endpoint

1. **Definire il route in server.c:**

```c
// In request_handler()
if (strcmp(url, "/api/courses") == 0 && strcmp(method, "GET") == 0) {
    return handle_get_courses(connection);
}
```

2. **Implementare il handler:**

```c
static int handle_get_courses(struct MHD_Connection *connection) {
    SQLite3_stmt *stmt;
    const char *query = "SELECT * FROM courses LIMIT 50";
    
    // Preparare la query
    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    
    // Costruire il JSON
    JSONBuilder *jb = json_create();
    json_start_object(jb);
    json_add_string(jb, "status", "success");
    json_start_array(jb, "data");
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        // Aggiungere ogni riga
    }
    
    json_end_array(jb);
    json_end_object(jb);
    
    char *response = json_get_string(jb);
    send_json_response(connection, response, MHD_HTTP_OK);
    
    free(response);
    json_free(jb);
    sqlite3_finalize(stmt);
    
    return MHD_YES;
}
```

3. **Testare con curl:**

```bash
curl http://localhost:3000/api/courses
```

## Implementazione Funzionalità Frontend

### Aggiungere una Nuova Pagina

1. **Aggiungere HTML in index.html:**

```html
<div id="newPage" class="page hidden">
    <div class="page-content">
        <h1>Titolo Pagina</h1>
        <!-- Contenuto -->
    </div>
</div>
```

2. **Aggiungere Stili in styles.css:**

```css
#newPage {
    padding: 2rem;
}

#newPage h1 {
    color: var(--primary-color);
}
```

3. **Aggiungere Logica in app.js:**

```javascript
showPage('new-page') {
    // Nascondere le altre pagine
    document.querySelectorAll('.page').forEach(page => {
        page.classList.add('hidden');
    });
    
    // Mostrare questa pagina
    document.getElementById('newPage').classList.remove('hidden');
}
```

4. **Aggiungere Metodo API in api.js:**

```javascript
async getNewData() {
    return this.request('GET', '/api/new-endpoint');
}
```

## Database

### Aggiungere una Nuova Tabella

1. **Modificare schema.sql:**

```sql
CREATE TABLE new_table (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_new_table_name ON new_table(name);
```

2. **Ricreare il database:**

```bash
rm courses.db
sqlite3 courses.db < schema.sql
```

### Query Utili

```sql
-- Trovare un utente
SELECT * FROM users WHERE username = 'test';

-- Contare corsi per categoria
SELECT category, COUNT(*) as count FROM courses GROUP BY category;

-- Trovare studenti iscritti a un corso
SELECT u.* FROM users u
INNER JOIN enrollments e ON u.id = e.student_id
WHERE e.course_id = 1;
```

## Autenticazione e Sicurezza

### Implementare JWT

Il file `auth.h` fornisce funzioni per:
- Generare token JWT: `generate_jwt_token()`
- Verificare token: `verify_jwt_token()`
- Hashing password: `hash_password()`
- Verificare password: `verify_password()`

**Nota:** L'implementazione attuale è semplificata. In produzione usare bcrypt e proper JWT signing.

### Verificare Token nei Handler

```c
static int handle_protected_endpoint(struct MHD_Connection *connection) {
    const char *auth_header = MHD_lookup_connection_value(
        connection, MHD_HEADER_KIND, "Authorization"
    );
    
    if (!auth_header || strncmp(auth_header, "Bearer ", 7) != 0) {
        char *error = json_error_response("Unauthorized");
        send_json_response(connection, error, MHD_HTTP_UNAUTHORIZED);
        free(error);
        return MHD_YES;
    }
    
    const char *token = auth_header + 7;
    
    // Verificare il token
    if (!verify_jwt_token(token, &user)) {
        char *error = json_error_response("Invalid token");
        send_json_response(connection, error, MHD_HTTP_FORBIDDEN);
        free(error);
        return MHD_YES;
    }
    
    // Continuare con l'elaborazione
    // ...
}
```

## Testing

### Test Manuali con curl

```bash
# Health check
curl http://localhost:3000/api/health

# Test GET
curl -i http://localhost:3000/api/courses

# Test POST
curl -X POST http://localhost:3000/api/users/register \
  -H "Content-Type: application/json" \
  -d '{"username":"test","email":"test@example.com","password":"pass"}'

# Test con Authorization
curl -H "Authorization: Bearer TOKEN" http://localhost:3000/api/protected

# Salvare risposta in file
curl -o response.json http://localhost:3000/api/courses

# Visualizzare headers
curl -i http://localhost:3000/api/health
```

### Test Frontend

1. Aprire browser console (F12)
2. Verificare che api.js sia caricato
3. Testare manualmente:

```javascript
// Test API client
api.healthCheck().then(data => console.log(data));

// Test registrazione
api.register({
    username: 'test',
    email: 'test@example.com',
    password: 'password',
    full_name: 'Test User',
    role: 'student'
});
```

## Troubleshooting

### Il backend non si avvia

```bash
# Verificare che la porta 3000 sia libera
lsof -i :3000  # Linux/Mac
netstat -ano | findstr :3000  # Windows

# Verificare i log
# Controllare che libmicrohttpd sia installato
pkg-config --cflags --libs libmicrohttpd

# Verificare il database
sqlite3 courses.db ".tables"
```

### Frontend non comunica con backend

```javascript
// Controllare la console del browser
// Verificare CORS headers
// Testare manualmente:
fetch('http://localhost:3000/api/health')
    .then(r => r.json())
    .then(d => console.log(d))
    .catch(e => console.error(e));
```

### Errori di compilazione

```bash
# Pulire la build
rm -rf build
mkdir build
cd build

# Riconfigurare
cmake ..

# Se ancora errori, controllare dipendenze
apt-get install libmicrohttpd-dev sqlite3 libsqlite3-dev  # Ubuntu
brew install libmicrohttpd sqlite3  # Mac
```

## Performance

### Ottimizzazioni Database

1. **Indici:** Assicurarsi che le colonne frequentemente cercate abbiano indici
2. **Query Optimization:** Usare EXPLAIN QUERY PLAN
3. **Connection Pooling:** Considerare per futuri load balancers

### Ottimizzazioni Backend

1. **Caching:** Implementare caching HTTP
2. **Compression:** Gzip per grandi risposte JSON
3. **Async I/O:** Considerare per operazioni lunghe

### Ottimizzazioni Frontend

1. **Lazy Loading:** Caricare dati solo quando necessario
2. **Minification:** Minimizzare CSS/JS in produzione
3. **Service Workers:** Per offline support

## Deployment

### Pre-Deployment Checklist

- [ ] Database backup
- [ ] Frontend minified
- [ ] JWT secret cambiato
- [ ] CORS configurato correttamente
- [ ] Logs rotati
- [ ] Database indici verificati
- [ ] Testi d'integrazione passati

### Produzione

```bash
# Compilare in Release
cmake -DCMAKE_BUILD_TYPE=Release ..
make

# Eseguire con supervisore (systemd/supervisor)
systemctl start course-backend
```

## Roadmap Futuro

- [ ] WebSocket per real-time updates
- [ ] File upload per contenuti lezioni
- [ ] Video streaming con HLS
- [ ] Sistema di pagamenti
- [ ] Mobile app
- [ ] Certificati digitali
- [ ] Analytics avanzato
- [ ] Sistema di cache (Redis)

## Contatti e Supporto

Per domande sullo sviluppo, contattare il team tecnico.
