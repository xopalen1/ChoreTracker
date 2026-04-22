# C23 Backend (CSV Database)

This backend is a decomposed C23 HTTP server that stores data in two CSV files:

- `backend/data/chores.csv`
- `backend/data/messages.csv`

It serves the frontend API contract from `docs/BACKEND_INTEGRATION.md`.

## Features

- REST endpoints for chores and messages
- CSV persistence for chores and messages
- Chore state with `isDone`
- New chores are assigned for **today** by backend rule
- Weekly balancing: chores are distributed across roommates by due-date week (Monday-Sunday)
- Readable module split: server, router, handlers, db, json, date

## Folder Structure

- `backend/src/main.c`: backend entry point
- `backend/src/server.c`: TCP server accept loop and connection handling
- `backend/src/router.c`: route dispatch
- `backend/src/handlers_chores.c`: chore endpoint handlers
- `backend/src/handlers_common.c`: shared handler response helpers
- `backend/src/handlers_messages.c`: message endpoint handlers
- `backend/src/handlers_roommates.c`: roommate endpoint handlers
- `backend/src/db.c`: CSV read/write and ID generation
- `backend/src/json_utils.c`: lightweight JSON parsing helpers
- `backend/src/string_builder.c`: shared response string builder
- `backend/src/text_utils.c`: shared text utilities (trim/cleanup)
- `backend/src/date_utils.c`: date functions (including relative-date assignment)
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
- `DELETE /api/chores/:id`
- `GET /api/messages`
- `POST /api/messages`

## Decomposition Notes

- Handler JSON response assembly is centralized in `backend/src/handlers_common.c` to avoid repeated StringBuilder boilerplate.
- Endpoint-specific handlers now focus on validation, persistence operations, and domain rules.
- `backend/src/handlers_chores.c` uses shared internal helpers for repeated ID lookup and reassignment logic blocks.

## Assignment Date Rule

On `POST /api/chores`, the backend sets `assignedDate` to today via `date_today_plus_days_iso(0, ...)` in `backend/src/date_utils.c`.
