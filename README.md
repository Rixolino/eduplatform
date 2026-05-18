# Piattaforma Corsi Online

Una piattaforma digitale completa per la gestione e la fruizione di corsi online.

## Architettura

- **Backend**: C con HTTP server (libmicrohttpd)
- **Frontend**: HTML5, CSS3, JavaScript vanilla
- **Database**: SQLite

## Requisiti

### Backend
- Compilatore C (gcc, clang, MSVC)
- CMake (versione 3.10+)
- libmicrohttpd
- SQLite3

### Frontend
- Browser moderno (Chrome, Firefox, Safari, Edge)
- Server HTTP locale

## Installazione

### 1. Setup Database

```bash
# Inizializzare il database SQLite
sqlite3 courses.db < schema.sql
```

### 2. Compilare il Backend

#### Su Linux/macOS:
```bash
mkdir build
cd build
cmake ..
make
```

#### Su Windows (con MSVC):
```bash
mkdir build
cd build
cmake -G "Visual Studio 17 2022" ..
cmake --build . --config Release
```

### 3. Avviare il Backend

```bash
# Su Linux/macOS
./bin/course_server

# Su Windows
.\bin\course_server.exe
```

Il server sarà disponibile su `http://localhost:3000`

### 4. Servire il Frontend

```bash
# Con Python 3
python -m http.server 8000

# Con Python 2
python -m SimpleHTTPServer 8000

# Con Node.js (http-server)
npm install -g http-server
http-server
```

Accedi a `http://localhost:8000` nel browser

## Utilizzo

### Registrazione

1. Clicca su "Registrati" nella landing page
2. Completa il modulo con:
   - Nome completo
   - Username (unico)
   - Email
   - Password
   - Ruolo (Studente o Docente)

### Accesso

1. Clicca su "Accedi"
2. Inserisci username e password
3. Verrai reindirizzato alla tua dashboard

### Funzionalità Studente

- **Catalogo Corsi**: Visualizza e ricerca corsi disponibili
- **Iscrizione**: Iscriviti ai corsi di interesse
- **Visualizza Lezioni**: Accedi ai contenuti del corso
- **Traccia Progresso**: Monitora il tuo avanzamento
- **Lascia Feedback**: Valuta i corsi completati

### Funzionalità Docente

- **Crea Corso**: Crea nuovi corsi
- **Gestisci Lezioni**: Aggiungi e modifica lezioni
- **Monitora Studenti**: Visualizza studenti iscritti e il loro progresso
- **Leggi Feedback**: Visualizza valutazioni e commenti degli studenti

## API Endpoints

### Autenticazione
- `POST /api/users/register` - Registrazione utente
- `POST /api/users/login` - Login utente
- `GET /api/users/profile` - Profilo utente

### Corsi
- `GET /api/courses` - Lista corsi
- `GET /api/courses/:id` - Dettagli corso
- `POST /api/courses` - Crea corso (docente)
- `PUT /api/courses/:id` - Modifica corso (docente)
- `DELETE /api/courses/:id` - Elimina corso (docente)

### Lezioni
- `GET /api/courses/:courseId/lessons` - Lista lezioni
- `GET /api/courses/:courseId/lessons/:lessonId` - Dettagli lezione
- `POST /api/courses/:courseId/lessons` - Crea lezione
- `PUT /api/courses/:courseId/lessons/:lessonId` - Modifica lezione
- `DELETE /api/courses/:courseId/lessons/:lessonId` - Elimina lezione

### Iscrizioni
- `POST /api/enrollments` - Iscriviti a corso
- `GET /api/enrollments` - Lista iscrizioni utente
- `GET /api/enrollments/:id` - Dettagli iscrizione
- `PUT /api/enrollments/:id/progress` - Aggiorna progresso
- `PUT /api/enrollments/:id/complete` - Completa corso

### Feedback
- `POST /api/enrollments/:id/feedback` - Lascia feedback
- `GET /api/courses/:courseId/feedback` - Leggi feedback corso

### Statistiche
- `GET /api/statistics/popular-courses` - Corsi più frequentati
- `GET /api/statistics/completion-rate` - Tasso completamento
- `GET /api/statistics/enrollments` - Statistiche iscrizioni

## Schema Database

### Tabelle

- **users** - Utenti registrati
- **courses** - Corsi disponibili
- **lessons** - Lezioni di un corso
- **enrollments** - Iscrizioni studenti
- **lesson_progress** - Progresso lezioni
- **feedback** - Valutazioni e feedback
- **activity_log** - Log attività utenti
- **sessions** - Sessioni JWT

## Struttura Progetto

```
progetto-vessio/
├── server.c              # Server HTTP principale
├── db_utils.h            # Utilities per database
├── CMakeLists.txt        # Build configuration
├── schema.sql            # Schema database SQLite
├── index.html            # Frontend principale
├── styles.css            # Stili CSS
├── api.js                # Client API
├── app.js                # Logica applicazione
└── courses.db            # Database SQLite (generato)
```

## Sviluppo

### Aggiungere Nuovi Endpoint

1. Aggiungere la funzione handler in `server.c`
2. Registrare l'endpoint nel router
3. Implementare la logica di database
4. Testare con curl/Postman

### Aggiungere Nuove Pagine Frontend

1. Aggiungere l'HTML in `index.html`
2. Aggiungere gli stili in `styles.css`
3. Aggiungere la logica in `app.js`
4. Aggiungere i metodi API necessari in `api.js`

## Testing

### Test API

```bash
# Health check
curl http://localhost:3000/api/health

# Registrazione
curl -X POST http://localhost:3000/api/users/register \
  -H "Content-Type: application/json" \
  -d '{"username":"test","email":"test@example.com","password":"pass","full_name":"Test","role":"student"}'

# Login
curl -X POST http://localhost:3000/api/users/login \
  -H "Content-Type: application/json" \
  -d '{"username":"test","password":"pass"}'

# Lista corsi
curl http://localhost:3000/api/courses
```

## Troubleshooting

### Backend non si avvia
- Verificare che la porta 3000 sia libera
- Controllare i permessi del database file
- Verificare che libmicrohttpd sia installato

### Frontend non comunica con backend
- Verificare che il backend sia in esecuzione
- Controllare la console del browser per errori CORS
- Verificare l'indirizzo IP/hostname del backend

### Errori di compilazione
- Verificare che CMake sia correttamente installato
- Reinstallare libmicrohttpd e SQLite3
- Verificare il compilatore C installato

## Roadmap Futuro

- [ ] Autenticazione OAuth2
- [ ] Upload file per lezioni
- [ ] Video streaming
- [ ] Sistema di chat live
- [ ] Certificati digitali
- [ ] Pagamenti (integrazione Stripe)
- [ ] Mobile app (React Native)
- [ ] Docker containerization
- [ ] Load balancing
- [ ] Cache distribuito (Redis)

## Licenza

MIT License

## Contatti

Per domande o suggerimenti, contattare il team di sviluppo.
