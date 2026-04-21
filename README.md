# Roommate Chore Tracker

A full-stack roommate chore tracker with a browser frontend and a C23 backend API.

The frontend and backend in this repository are designed to work together out of the box.
This is a fun, non-serious project.

## Features

- Add chores with title, assignee, assigned date, and due date.
- Edit due dates after creation.
- Due date edit history appears only after the first due-date change.
- Active chores are grouped by assignee.
- Mark chores as open or done.
- Delete chores.
- Filter by status and search by text.
- Right-side house chat with backend-ready message APIs.
- Responsive layout for desktop and mobile.
- C23 backend with CSV persistence for chores and chat messages.
- UI text is currently in Slovak.

## Tech Stack

- Frontend: HTML, CSS, Vanilla JavaScript
- Backend: C23 (modular HTTP server)
- Storage: CSV files in `backend/data/`

## Run Locally

Recommended: start frontend and backend together from the project root.

```powershell
.\windows\start-app.ps1
```

```bash
./linux/start-app.sh
```

Then open:

- On your computer: `http://localhost:5500`
- On phone/tablet (same Wi-Fi): `http://<YOUR_PC_IP>:5500`

Manual setup options:

### Option 1: Build and run backend, then serve frontend

```powershell
.\windows\build-backend.ps1
.\backend\bin\roommate_backend.exe 8080
python -m http.server 5500 --bind 0.0.0.0
```

```bash
./linux/build-backend.sh
./backend/bin/roommate_backend 8080
nginx
```

### Option 2: Node serve package for frontend

```bash
npx serve .
```

Use this only after the backend is already running on port `8080`.

To stop both services:

```powershell
.\windows\stop-app.ps1
```

```bash
./linux/stop-app.sh
```

## API Configuration

The app is preconfigured for the included backend via `js/main.js` (logic split into `js/chores.js` and `js/messages.js`).

1. Open `js/main.js`.
2. `config.apiBaseUrl` auto-uses the current host with port `8080`.
  Example: if frontend is `http://192.168.1.44:5500`, API calls go to `http://192.168.1.44:8080`.
3. Keep backend flags enabled (`useBackendChores` and `useBackendMessages`).
4. API routes follow the contract in `docs/BACKEND_INTEGRATION.md`.

## Data Model

Each chore object uses this shape:

```json
{
  "id": "string",
  "title": "Take out trash",
  "assignee": "Alex",
  "assignedDate": "2026-04-09",
  "dueDate": "2026-04-10",
  "isDone": false,
  "dueDateHistory": [
    {
      "previousDueDate": "2026-04-09",
      "newDueDate": "2026-04-10",
      "editedAt": "2026-04-09T18:03:10.000Z"
    }
  ],
  "status": "open"
}
```

## Notes

- Chores and chat are backend-managed.
- Backend create payload is intentionally minimal: `title`, `assignee`, `dueDate`.
- Chore state checks should use boolean: `isDone`.
- The included backend stores chores and messages in CSV files.
- No frontend build step is required.
- Frontend API calls are centralized via shared request helpers in `js/main.js` for readability and consistent error handling.
- `windows/start-app.ps1` uses Python `http.server` on port `5500`.
- `linux/start-app.sh` uses Nginx on port `5500`.

## Backend Source

The decomposed C23 backend implementation is in `backend/`.
Build and run instructions are in `backend/README.md`.

## Same-WiFi Access Checklist

- Start backend on your PC at port `8080`.
- Serve frontend on port `5500` (Windows uses Python `http.server`, Linux uses Nginx).
- Open Windows firewall for your backend/frontend ports (`8080` and `5500`) if needed.
- Use your PC local IP from your phone browser: `http://<YOUR_PC_IP>:5500`.

