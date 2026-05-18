# Authentication Endpoints - Testing Guide

## Prerequisites

1. Database initialized:
```bash
sqlite3 courses.db < schema.sql
```

2. Server running:
```bash
cd build
./bin/course_server
```

3. Server should output:
```
[2024-XX-XX XX:XX:XX] Starting Piattaforma Corsi Online Backend...
[2024-XX-XX XX:XX:XX] Database opened successfully: courses.db
[2024-XX-XX XX:XX:XX] HTTP Server started on http://localhost:3000
```

---

## Test Sequence

### Step 1: Health Check
Verify the API is running:
```bash
curl http://localhost:3000/api/health
```

Expected Response:
```json
{"status": "healthy", "message": "API is running"}
```

---

### Step 2: Register a New User

Test with valid data:
```bash
curl -X POST http://localhost:3000/api/users/register \
  -H "Content-Type: application/json" \
  -d '{
    "username": "john_doe",
    "email": "john@example.com",
    "password": "securepass123",
    "full_name": "John Doe",
    "role": "student"
  }'
```

Expected Response (201 Created):
```json
{
  "status": "success",
  "message": "User registered successfully",
  "user_id": 1,
  "username": "john_doe",
  "email": "john@example.com",
  "role": "student"
}
```

### Test 2a: Duplicate Username
```bash
curl -X POST http://localhost:3000/api/users/register \
  -H "Content-Type: application/json" \
  -d '{
    "username": "john_doe",
    "email": "different@example.com",
    "password": "securepass123",
    "full_name": "Another John",
    "role": "student"
  }'
```

Expected Response (409 Conflict):
```json
{"error": "Username already exists"}
```

### Test 2b: Duplicate Email
```bash
curl -X POST http://localhost:3000/api/users/register \
  -H "Content-Type: application/json" \
  -d '{
    "username": "jane_doe",
    "email": "john@example.com",
    "password": "securepass123",
    "full_name": "Jane Doe",
    "role": "student"
  }'
```

Expected Response (409 Conflict):
```json
{"error": "Email already exists"}
```

### Test 2c: Invalid Email
```bash
curl -X POST http://localhost:3000/api/users/register \
  -H "Content-Type: application/json" \
  -d '{
    "username": "bob_smith",
    "email": "invalid-email",
    "password": "securepass123",
    "full_name": "Bob Smith",
    "role": "student"
  }'
```

Expected Response (400 Bad Request):
```json
{"error": "Invalid email format"}
```

### Test 2d: Short Username
```bash
curl -X POST http://localhost:3000/api/users/register \
  -H "Content-Type: application/json" \
  -d '{
    "username": "ab",
    "email": "ab@example.com",
    "password": "securepass123",
    "full_name": "AB User",
    "role": "student"
  }'
```

Expected Response (400 Bad Request):
```json
{"error": "Username must be at least 3 characters"}
```

### Test 2e: Weak Password
```bash
curl -X POST http://localhost:3000/api/users/register \
  -H "Content-Type: application/json" \
  -d '{
    "username": "weak_pass",
    "email": "weak@example.com",
    "password": "short",
    "full_name": "Weak Pass User",
    "role": "student"
  }'
```

Expected Response (400 Bad Request):
```json
{"error": "Password must be at least 6 characters"}
```

---

### Step 3: Login with Valid Credentials

```bash
curl -X POST http://localhost:3000/api/users/login \
  -H "Content-Type: application/json" \
  -d '{
    "username": "john_doe",
    "password": "securepass123"
  }'
```

Expected Response (200 OK):
```json
{
  "status": "success",
  "message": "Login successful",
  "token": "eyJ1c2VyX2lkIjoxLCJ1c2VybmFtZSI6ImpvaG5fZG9lIiwicm9sZSI6InN0dWRlbnQiLCJleHAiOjE3MDQ0NDA0OTF9",
  "user_id": 1,
  "username": "john_doe",
  "email": "john@example.com",
  "role": "student"
}
```

**Save the token for next tests:**
```bash
TOKEN="<token_from_response>"
```

### Test 3a: Login with Wrong Password
```bash
curl -X POST http://localhost:3000/api/users/login \
  -H "Content-Type: application/json" \
  -d '{
    "username": "john_doe",
    "password": "wrongpassword"
  }'
```

Expected Response (401 Unauthorized):
```json
{"error": "Invalid username or password"}
```

### Test 3b: Login with Non-existent User
```bash
curl -X POST http://localhost:3000/api/users/login \
  -H "Content-Type: application/json" \
  -d '{
    "username": "nonexistent_user",
    "password": "anypassword"
  }'
```

Expected Response (401 Unauthorized):
```json
{"error": "Invalid username or password"}
```

### Test 3c: Missing Password
```bash
curl -X POST http://localhost:3000/api/users/login \
  -H "Content-Type: application/json" \
  -d '{
    "username": "john_doe"
  }'
```

Expected Response (400 Bad Request):
```json
{"error": "Username and password are required"}
```

---

### Step 4: Get User Profile with Valid Token

Using token from login response:
```bash
curl -X GET http://localhost:3000/api/users/profile \
  -H "Authorization: Bearer $TOKEN"
```

Expected Response (200 OK):
```json
{
  "status": "success",
  "user_id": 1,
  "username": "john_doe",
  "email": "john@example.com",
  "role": "student"
}
```

### Test 4a: Profile without Authorization Header
```bash
curl -X GET http://localhost:3000/api/users/profile
```

Expected Response (401 Unauthorized):
```json
{"error": "Authorization header missing"}
```

### Test 4b: Profile with Invalid Token Format
```bash
curl -X GET http://localhost:3000/api/users/profile \
  -H "Authorization: InvalidToken"
```

Expected Response (401 Unauthorized):
```json
{"error": "Invalid authorization header format"}
```

### Test 4c: Profile with Invalid Token
```bash
curl -X GET http://localhost:3000/api/users/profile \
  -H "Authorization: Bearer invalid_token_value"
```

Expected Response (401 Unauthorized):
```json
{"error": "Invalid or expired token"}
```

---

## Activity Log Verification

After running tests, verify activity logging:
```bash
sqlite3 courses.db "SELECT * FROM activity_log ORDER BY created_at DESC LIMIT 10;"
```

Expected to see entries like:
```
user_id | action      | resource_type | resource_id | details | ip_address
--------|-------------|---------------|-------------|---------|----------
1       | register    | user          | 1           | NULL    | 127.0.0.1
1       | login       | user          | 1           | NULL    | 127.0.0.1
1       | get_profile | user          | 1           | NULL    | 127.0.0.1
```

---

## Session Table Verification

Check stored sessions:
```bash
sqlite3 courses.db "SELECT * FROM sessions WHERE user_id = 1;"
```

Expected to see:
```
id | user_id | token                                              | expires_at            | created_at
---|---------|----------------------------------------------------|-----------------------|----------
1  | 1       | eyJ1c2VyX2lkIjoxLCJ1c2VybmFtZSI6ImpvaG5fZG9lIi... | 2024-XX-XX XX:XX:XX  | 2024-XX-XX XX:XX:XX
```

---

## Stress Testing

### Test Multiple Registrations
```bash
for i in {1..5}; do
  curl -X POST http://localhost:3000/api/users/register \
    -H "Content-Type: application/json" \
    -d "{
      \"username\": \"user_$i\",
      \"email\": \"user$i@example.com\",
      \"password\": \"password$i\",
      \"full_name\": \"User $i\",
      \"role\": \"student\"
    }"
done
```

### Test Concurrent Logins
```bash
curl -X POST http://localhost:3000/api/users/login \
  -H "Content-Type: application/json" \
  -d '{"username": "user_1", "password": "password1"}' &

curl -X POST http://localhost:3000/api/users/login \
  -H "Content-Type: application/json" \
  -d '{"username": "user_2", "password": "password2"}' &

curl -X POST http://localhost:3000/api/users/login \
  -H "Content-Type: application/json" \
  -d '{"username": "user_3", "password": "password3"}' &

wait
```

---

## Database State Verification

### Check All Users Created
```bash
sqlite3 courses.db "SELECT id, username, email, role FROM users;"
```

### Check Session Count
```bash
sqlite3 courses.db "SELECT COUNT(*) as session_count FROM sessions;"
```

### Check Activity Log Count
```bash
sqlite3 courses.db "SELECT COUNT(*) as activity_count FROM activity_log;"
```

### Check User with Most Activity
```bash
sqlite3 courses.db "SELECT user_id, COUNT(*) as activity_count FROM activity_log GROUP BY user_id ORDER BY activity_count DESC LIMIT 1;"
```

---

## Common Issues & Solutions

### Issue: 400 Bad Request on valid JSON
**Solution:** Ensure Content-Type header is set to `application/json`

### Issue: Field not found errors
**Solution:** Check field names match exactly: username, email, password, full_name, role

### Issue: Token expired immediately
**Solution:** Tokens are valid for 24 hours. If test token is very old, re-login to get new token.

### Issue: Database locked
**Solution:** Close all other connections to courses.db and retry

### Issue: Port 3000 already in use
**Solution:** Change PORT in server.c or kill process using port: `lsof -ti:3000 | xargs kill -9`

---

## Performance Baseline

Expected response times:
- Registration (valid): 50-100ms
- Registration (duplicate check): 10-20ms
- Login (valid): 50-100ms
- Login (invalid): 20-30ms
- Profile (valid token): 30-50ms
- Profile (invalid token): 10-20ms

---

## Notes

1. **Passwords are NOT transmitted securely** - Use HTTPS in production
2. **Tokens are NOT cryptographically signed** - Add HMAC signing for production
3. **Email verification is NOT implemented** - Add verification workflow for production
4. **Rate limiting is NOT implemented** - Add protection against brute force for production
5. **Test data persists** - Clear courses.db between test runs with: `rm courses.db && sqlite3 courses.db < schema.sql`

---

## Cleanup

To reset the database for fresh testing:
```bash
rm courses.db
sqlite3 courses.db < schema.sql
```

The endpoints will now start fresh with empty database.
