# C23 Backend (CSV Database)

This backend is a decomposed C23 HTTP server that stores data in two CSV files:

- `backend/data/chores.csv`
- `backend/data/messages.csv`

It serves the frontend API contract from `docs/BACKEND_INTEGRATION.md`.

## Features

- REST endpoints for chores and messages
- CSV persistence for chores and messages
- Chore state with `isDone` and `isDeleted`
- New chores are assigned on **Sunday** by backend rule
- Readable module split: server, router, handlers, db, json, date

## Folder Structure

- `backend/src/main.c`: backend entry point
- `backend/src/server.c`: TCP server accept loop and connection handling
- `backend/src/router.c`: route dispatch
- `backend/src/handlers.c`: endpoint handlers
- `backend/src/db.c`: CSV read/write and ID generation
- `backend/src/json_utils.c`: lightweight JSON parsing helpers
- `backend/src/date_utils.c`: date functions (including Sunday assignment)
- `backend/src/http.c`: HTTP parsing and response writing
- `backend/src/net.c`: socket init/cleanup abstraction

## Build (PowerShell)

From project root:

```powershell
.\windows\build-backend.ps1
```

## Build (Linux/macOS)

From project root:

```bash
./linux/build-backend.sh
```

This produces:

- Windows: `backend/bin/roommate_backend.exe`
- Linux/macOS: `backend/bin/roommate_backend`

## Run

```powershell
.\backend\bin\roommate_backend.exe 8080
```

```bash
./backend/bin/roommate_backend 8080
```

The backend binds to all interfaces (`0.0.0.0`) so devices on the same Wi-Fi can connect using your PC IP.

Then open frontend and ensure backend mode is enabled in `js/main.js`.

## API Endpoints

- `GET /api/chores`
- `GET /api/chores/:id`
- `POST /api/chores`
- `PATCH /api/chores/:id`
- `GET /api/messages`
- `POST /api/messages`

## Sunday Assignment Rule

On `POST /api/chores`, the backend sets `assignedDate` to Sunday via `date_next_sunday_iso(...)` in `backend/src/date_utils.c`.
