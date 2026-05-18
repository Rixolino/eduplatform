# API Documentation - Piattaforma Corsi Online

## Base URL
```
http://localhost:3000
```

## Versione API
v1.0

## Content-Type
Tutte le richieste e risposte usano `application/json`

---

## Autenticazione

### POST /api/users/register

Registra un nuovo utente.

**Request:**
```json
{
  "username": "string (unique, 3-50 chars)",
  "email": "string (unique, valid email)",
  "password": "string (min 6 chars)",
  "full_name": "string",
  "role": "student | teacher | admin"
}
```

**Response (201 Created):**
```json
{
  "status": "success",
  "data": {
    "id": 1,
    "username": "newuser",
    "email": "user@example.com",
    "full_name": "John Doe",
    "role": "student"
  }
}
```

**Errori:**
- `400` - Dati invalidi
- `409` - Username o email già esistenti

---

### POST /api/users/login

Effettua il login e ritorna un token JWT.

**Request:**
```json
{
  "username": "string",
  "password": "string"
}
```

**Response (200 OK):**
```json
{
  "status": "success",
  "data": {
    "token": "eyJhbGciOiJIUzI1NiIs...",
    "user": {
      "id": 1,
      "username": "user",
      "email": "user@example.com",
      "full_name": "John Doe",
      "role": "student"
    }
  }
}
```

**Errori:**
- `401` - Credenziali invalide
- `400` - Dati mancanti

---

### GET /api/users/profile

Ottiene il profilo dell'utente corrente (autenticato).

**Headers:**
```
Authorization: Bearer <token>
```

**Response (200 OK):**
```json
{
  "status": "success",
  "data": {
    "id": 1,
    "username": "user",
    "email": "user@example.com",
    "full_name": "John Doe",
    "role": "student",
    "bio": "Student bio"
  }
}
```

**Errori:**
- `401` - Non autenticato
- `403` - Token invalido

---

## Corsi

### GET /api/courses

Ottiene la lista dei corsi disponibili.

**Query Parameters:**
- `category` (optional) - Filtro per categoria
- `difficulty` (optional) - Filtro difficoltà: `beginner | intermediate | advanced`
- `search` (optional) - Ricerca nel titolo
- `limit` (optional, default: 50) - Numero massimo risultati
- `offset` (optional, default: 0) - Offset per paginazione

**Response (200 OK):**
```json
{
  "status": "success",
  "data": [
    {
      "id": 1,
      "title": "Python Basics",
      "description": "Learn Python programming",
      "teacher_id": 5,
      "teacher_name": "Jane Smith",
      "category": "Programming",
      "difficulty_level": "beginner",
      "duration_hours": 20,
      "num_lessons": 10,
      "created_at": "2024-01-15T10:00:00Z"
    }
  ],
  "pagination": {
    "total": 100,
    "limit": 50,
    "offset": 0
  }
}
```

**Esempi:**
```bash
# Tutti i corsi
GET /api/courses

# Filtro per categoria
GET /api/courses?category=Programming

# Filtro per difficoltà
GET /api/courses?difficulty=beginner

# Ricerca
GET /api/courses?search=python

# Combinati
GET /api/courses?category=Programming&difficulty=beginner&limit=20
```

---

### GET /api/courses/:id

Ottiene i dettagli di uno specifico corso.

**Response (200 OK):**
```json
{
  "status": "success",
  "data": {
    "id": 1,
    "title": "Python Basics",
    "description": "Comprehensive Python course",
    "teacher_id": 5,
    "teacher_name": "Jane Smith",
    "category": "Programming",
    "difficulty_level": "beginner",
    "duration_hours": 20,
    "num_lessons": 10,
    "created_at": "2024-01-15T10:00:00Z",
    "lessons": [
      {
        "id": 1,
        "title": "Introduction to Python",
        "lesson_order": 1,
        "duration_minutes": 45
      }
    ]
  }
}
```

**Errori:**
- `404` - Corso non trovato

---

### POST /api/courses

Crea un nuovo corso (solo docenti).

**Headers:**
```
Authorization: Bearer <token>
```

**Request:**
```json
{
  "title": "string (non vuoto)",
  "description": "string",
  "category": "string",
  "difficulty_level": "beginner | intermediate | advanced",
  "duration_hours": "number (> 0)"
}
```

**Response (201 Created):**
```json
{
  "status": "success",
  "data": {
    "id": 2,
    "title": "Advanced Python",
    "description": "Deep dive into Python",
    "teacher_id": 5,
    "category": "Programming",
    "difficulty_level": "advanced",
    "duration_hours": 40,
    "num_lessons": 0,
    "created_at": "2024-01-20T10:00:00Z"
  }
}
```

**Errori:**
- `401` - Non autenticato
- `403` - Solo docenti possono creare corsi
- `400` - Dati invalidi

---

### PUT /api/courses/:id

Modifica un corso (solo creatore).

**Headers:**
```
Authorization: Bearer <token>
```

**Request:** (Come POST, tutti i campi opzionali)
```json
{
  "title": "Updated Title",
  "description": "Updated description"
}
```

**Response (200 OK):**
```json
{
  "status": "success",
  "data": { /* corso aggiornato */ }
}
```

**Errori:**
- `401` - Non autenticato
- `403` - Non sei il creatore
- `404` - Corso non trovato

---

### DELETE /api/courses/:id

Elimina un corso (solo creatore).

**Headers:**
```
Authorization: Bearer <token>
```

**Response (200 OK):**
```json
{
  "status": "success",
  "message": "Course deleted"
}
```

**Errori:**
- `401` - Non autenticato
- `403` - Non sei il creatore
- `404` - Corso non trovato

---

## Lezioni

### GET /api/courses/:courseId/lessons

Ottiene le lezioni di un corso.

**Response (200 OK):**
```json
{
  "status": "success",
  "data": [
    {
      "id": 1,
      "course_id": 1,
      "title": "Lesson 1",
      "description": "Introduction",
      "lesson_order": 1,
      "duration_minutes": 45,
      "created_at": "2024-01-15T10:00:00Z"
    }
  ]
}
```

---

### GET /api/courses/:courseId/lessons/:lessonId

Ottiene i dettagli di una lezione.

**Response (200 OK):**
```json
{
  "status": "success",
  "data": {
    "id": 1,
    "course_id": 1,
    "title": "Lesson 1",
    "description": "Introduction to the course",
    "content": "HTML content or markdown",
    "lesson_order": 1,
    "duration_minutes": 45
  }
}
```

---

### POST /api/courses/:courseId/lessons

Crea una lezione (solo creatore corso).

**Headers:**
```
Authorization: Bearer <token>
```

**Request:**
```json
{
  "title": "string",
  "description": "string",
  "content": "string (HTML or markdown)",
  "lesson_order": "number",
  "duration_minutes": "number"
}
```

**Response (201 Created):**
```json
{
  "status": "success",
  "data": { /* nuova lezione */ }
}
```

---

## Iscrizioni

### POST /api/enrollments

Iscriviti a un corso.

**Headers:**
```
Authorization: Bearer <token>
```

**Request:**
```json
{
  "course_id": "number"
}
```

**Response (201 Created):**
```json
{
  "status": "success",
  "data": {
    "id": 1,
    "student_id": 1,
    "course_id": 1,
    "enrollment_date": "2024-01-20T10:00:00Z",
    "status": "enrolled",
    "progress_percentage": 0
  }
}
```

**Errori:**
- `401` - Non autenticato
- `409` - Già iscritto a questo corso
- `404` - Corso non trovato

---

### GET /api/enrollments

Ottiene le iscrizioni dell'utente.

**Headers:**
```
Authorization: Bearer <token>
```

**Response (200 OK):**
```json
{
  "status": "success",
  "data": [
    {
      "id": 1,
      "student_id": 1,
      "course_id": 1,
      "course_title": "Python Basics",
      "enrollment_date": "2024-01-20T10:00:00Z",
      "status": "enrolled",
      "progress_percentage": 25
    }
  ]
}
```

---

### GET /api/enrollments/:id

Ottiene i dettagli di un'iscrizione.

**Headers:**
```
Authorization: Bearer <token>
```

**Response (200 OK):**
```json
{
  "status": "success",
  "data": {
    "id": 1,
    "student_id": 1,
    "course_id": 1,
    "enrollment_date": "2024-01-20T10:00:00Z",
    "completion_date": null,
    "status": "enrolled",
    "progress_percentage": 25,
    "lessons_completed": 3,
    "total_lessons": 10
  }
}
```

---

### PUT /api/enrollments/:id/progress

Aggiorna il progresso in una lezione.

**Headers:**
```
Authorization: Bearer <token>
```

**Request:**
```json
{
  "lesson_id": "number"
}
```

**Response (200 OK):**
```json
{
  "status": "success",
  "data": {
    "enrollment_id": 1,
    "lesson_id": 1,
    "completed": true,
    "progress_percentage": 30
  }
}
```

---

### PUT /api/enrollments/:id/complete

Completa un corso.

**Headers:**
```
Authorization: Bearer <token>
```

**Response (200 OK):**
```json
{
  "status": "success",
  "data": {
    "id": 1,
    "status": "completed",
    "completion_date": "2024-01-25T10:00:00Z",
    "progress_percentage": 100
  }
}
```

---

## Feedback

### POST /api/enrollments/:enrollmentId/feedback

Lascia feedback su un corso.

**Headers:**
```
Authorization: Bearer <token>
```

**Request:**
```json
{
  "rating": "number (1-5)",
  "comment": "string"
}
```

**Response (201 Created):**
```json
{
  "status": "success",
  "data": {
    "id": 1,
    "enrollment_id": 1,
    "student_id": 1,
    "rating": 5,
    "comment": "Great course!",
    "created_at": "2024-01-25T10:00:00Z"
  }
}
```

---

### GET /api/courses/:courseId/feedback

Ottiene il feedback di un corso (solo docente/admin).

**Headers:**
```
Authorization: Bearer <token>
```

**Response (200 OK):**
```json
{
  "status": "success",
  "data": [
    {
      "id": 1,
      "student_name": "John Doe",
      "rating": 5,
      "comment": "Great course!",
      "created_at": "2024-01-25T10:00:00Z"
    }
  ],
  "stats": {
    "average_rating": 4.5,
    "total_reviews": 10
  }
}
```

---

## Statistiche

### GET /api/statistics/popular-courses

Ottiene i corsi più frequentati.

**Response (200 OK):**
```json
{
  "status": "success",
  "data": [
    {
      "course_id": 1,
      "title": "Python Basics",
      "enrollments": 150,
      "completions": 45,
      "average_rating": 4.5
    }
  ]
}
```

---

### GET /api/statistics/completion-rate

Ottiene il tasso di completamento dei corsi.

**Response (200 OK):**
```json
{
  "status": "success",
  "data": {
    "overall_completion_rate": 35,
    "by_difficulty": {
      "beginner": 45,
      "intermediate": 35,
      "advanced": 20
    }
  }
}
```

---

### GET /api/statistics/enrollments

Ottiene le statistiche di iscrizione.

**Response (200 OK):**
```json
{
  "status": "success",
  "data": {
    "total_enrollments": 1000,
    "active_students": 500,
    "by_category": {
      "Programming": 300,
      "Design": 200,
      "Business": 500
    }
  }
}
```

---

## Health Check

### GET /api/health

Verifica lo stato dell'API.

**Response (200 OK):**
```json
{
  "status": "healthy",
  "message": "API is running"
}
```

---

## Codici di Stato HTTP

| Codice | Significato |
|--------|-------------|
| 200 | OK - Richiesta completata |
| 201 | Created - Risorsa creata |
| 400 | Bad Request - Dati invalidi |
| 401 | Unauthorized - Token mancante/invalido |
| 403 | Forbidden - Permessi insufficienti |
| 404 | Not Found - Risorsa non trovata |
| 409 | Conflict - Risorsa già esiste |
| 500 | Internal Server Error - Errore server |

---

## CORS

Tutte le API supportano CORS con:
```
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS
Access-Control-Allow-Headers: Content-Type, Authorization
```

---

## Rate Limiting

Attualmente non implementato, da considerare per la produzione.

---

## Changelog

### v1.0 (Attuale)
- Endpoints base autenticazione
- CRUD corsi
- Gestione iscrizioni
- Sistema feedback
