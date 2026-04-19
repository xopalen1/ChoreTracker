# Roommate Chore Tracker (Frontend)

A lightweight web app UI for tracking roommate chores.

This project intentionally ships frontend-only. You can connect your own backend API when ready.

## Features

- Add chores with title, assignee, assigned date, and due date.
- Edit due dates after creation.
- Due date edit history appears only after the first due-date change.
- Active chores are grouped by assignee.
- Mark chores as open or done.
- Delete chores.
- Filter by status and search by text.
- Summary cards for total/open/done chores.
- Right-side house chat with backend-ready message APIs.
- Responsive layout for desktop and mobile.

## Tech Stack

- HTML
- CSS
- Vanilla JavaScript

## Run Locally

Use any static server.

### Option 1: Python

```bash
python -m http.server 5500 --bind 0.0.0.0
```

Then open:

- On your computer: `http://localhost:5500`
- On phone/tablet (same Wi-Fi): `http://<YOUR_PC_IP>:5500`

### Option 2: Node serve package

```bash
npx serve .
```

## Backend Integration

The app has built-in API hooks in `js/main.js` (with logic split into `js/chores.js` and `js/messages.js`).

1. Open `js/main.js`.
2. `config.apiBaseUrl` auto-uses the current host with port `8080`.
  Example: if frontend is `http://192.168.1.44:5500`, API calls go to `http://192.168.1.44:8080`.
3. Keep backend flags enabled (`useBackendChores` and `useBackendMessages`).
4. Implement backend routes matching the contract in `docs/BACKEND_INTEGRATION.md`.

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
  "isDeleted": false,
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

- Chores and chat are fully backend-managed.
- Backend create payload is intentionally minimal: `title`, `assignee`, `dueDate`.
- Chore state checks should use booleans: `isDone` and `isDeleted`.
- Your backend can store chores in a CSV database; frontend only depends on the API contract.
- No build step is required.

## Backend Source

The decomposed C23 backend implementation is in `backend/`.
Build and run instructions are in `backend/README.md`.

## Same-WiFi Access Checklist

- Start backend on your PC at port `8080`.
- Serve frontend with `--bind 0.0.0.0`.
- Open Windows firewall for your backend/frontend ports (`8080` and `5500`) if needed.
- Use your PC local IP from your phone browser: `http://<YOUR_PC_IP>:5500`.

