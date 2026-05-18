# Quick Start Guide

## 5 minuti per iniziare

### 1. Preparare il Database

```bash
# Navigare nella cartella del progetto
cd c:\Users\SIMONE\Documents\progetto-vessio

# Se hai sqlite3 installato
sqlite3 courses.db < schema.sql

# Altrimenti puoi usare l'app SQLite online o desktop SQLite browser
```

### 2. Compilare il Backend (opzionale per test)

Se hai un compilatore C installato:

```bash
mkdir build
cd build
cmake ..
cmake --build .

# Eseguire
.\bin\course_server.exe  # Windows
./bin/course_server      # Linux/Mac
```

### 3. Servire il Frontend

Il server C (`course_server`) può servire anche i file statici del frontend, quindi non è necessario avviare un server Python/Node separato.

Apri un terminale nella cartella del progetto e avvia il server C (dopo aver compilato):

```bash
mkdir build
cd build
cmake ..
cmake --build .

# Su Windows
.\bin\course_server.exe

# Su Linux/Mac
./bin/course_server
```

Il server sarà disponibile su `http://localhost:3000` e servirà sia le API che i file statici del frontend.

### 4. Accedere all'Applicazione

Apri il browser e vai a:
```
http://localhost:8000
```

---

## Struttura File Chiave

```
progetto-vessio/
├── 📄 schema.sql         ← Database structure
├── 🖥️  server.c          ← Backend API
├── 🌐 index.html         ← Frontend
├── 🎨 styles.css         ← Stili
├── 📱 app.js             ← Logica frontend
├── 🔌 api.js             ← Client API
├── 📖 README.md          ← Guida utente
├── 📚 API.md             ← Documentazione API
└── 💻 DEVELOPMENT.md     ← Guida sviluppo
```

---

## Cos'è già Implementato ✅

1. **Database Schema** - Tabelle complete per il sistema
2. **HTTP Server Base** - Server C con endpoints di base
3. **Frontend Structure** - Layout completo con tutte le pagine
4. **Utilities** - Helper per database, JSON, autenticazione
5. **Documentazione** - API docs, guida sviluppo, README

---

## Cosa Fare Dopo 🚀

### Per il Backend:
1. Implementare gli endpoint `/api/users/register` e `/api/users/login`
2. Aggiungere CRUD per corsi
3. Implementare sistema iscrizioni
4. Aggiungere feedback e valutazioni

### Per il Frontend:
1. Collegare i form alle API reali
2. Implementare gestione errori
3. Aggiungere loading states
4. Implementare caching locale

### Testing:
```bash
# Test API con curl
curl http://localhost:3000/api/health

# Test registrazione
curl -X POST http://localhost:3000/api/users/register \
  -H "Content-Type: application/json" \
  -d '{"username":"test","email":"test@test.com","password":"pass123","full_name":"Test User","role":"student"}'
```

---

## Ruoli Utente

### Studente
- ✅ Visualizzare catalogo corsi
- ✅ Cercare per categoria/difficoltà
- ✅ Iscriversi a corsi
- ✅ Visualizzare lezioni
- ✅ Tracciare progresso
- ✅ Lasciare valutazioni

### Docente
- ✅ Creare corsi
- ✅ Gestire lezioni
- ✅ Visualizzare studenti iscritti
- ✅ Leggere feedback

### Admin
- ✅ Visualizzare statistiche
- ✅ Gestire utenti
- ✅ Visualizzare log

---

## Problemi Comuni

### "Cannot connect to server"
- Assicurati che il backend sia in esecuzione su `localhost:3000`
- Verifica che il database sia stato creato

### "CORS error"
- Il backend deve essere in esecuzione
- Controlla che gli header CORS siano corretti

### "Database locked"
- Chiudi SQLite browser o altre connessioni al database
- Riavvia il backend

### "Port 8000 already in use"
```bash
# Usa una porta diversa
python -m http.server 8001
# Accedi a http://localhost:8001
```

---

## Risorse Utili

- **SQLite Documentation**: https://www.sqlite.org/docs.html
- **libmicrohttpd**: https://www.gnu.org/software/libmicrohttpd/
- **JWT**: https://jwt.io/
- **REST API Best Practices**: https://restfulapi.net/

---

## Prossimi Passi

1. Leggi `DEVELOPMENT.md` per dettagli di implementazione
2. Consulta `API.md` per la specifica degli endpoint
3. Inizia con gli endpoint di autenticazione
4. Testa con curl prima di collegare il frontend

---

## Supporto

Se hai dubbi sulla struttura o implementazione:
1. Consulta la documentazione fornita
2. Guarda gli esempi in `DEVELOPMENT.md`
3. Verifica gli endpoint in `API.md`

Buono sviluppo! 🚀
