const config = {
  apiBaseUrl: `${window.location.protocol}//${window.location.hostname}:8080`,
  locale: "sk-SK",
  useBackendChores: true,
  useBackendMessages: true,
  storageKeys: {
    chatWidth: "roommate-chat-width-v1",
    chatHeight: "roommate-chat-height-v1",
  },
};

const state = {
  chores: [],
  roommates: [],
  chatMessages: [],
  chatWidth: 360,
  chatMobileHeight: 420,
  filterStatus: "all",
  searchTerm: "",
  editingDueDateId: null,
};

const el = {
  choreList: document.getElementById("choreList"),
  choreTemplate: document.getElementById("choreTemplate"),
  totalCount: document.getElementById("totalCount"),
  openCount: document.getElementById("openCount"),
  doneCount: document.getElementById("doneCount"),
  filterStatus: document.getElementById("filterStatus"),
  searchInput: document.getElementById("searchInput"),
  newChoreBtn: document.getElementById("newChoreBtn"),
  newRoommateBtn: document.getElementById("newRoommateBtn"),
  choreDialog: document.getElementById("choreDialog"),
  choreForm: document.getElementById("choreForm"),
  cancelBtn: document.getElementById("cancelBtn"),
  assigneeSelect: document.getElementById("assignee"),
  dueDateInput: document.getElementById("dueDate"),
  roommateDialog: document.getElementById("roommateDialog"),
  roommateForm: document.getElementById("roommateForm"),
  roommateNameInput: document.getElementById("roommateName"),
  cancelRoommateBtn: document.getElementById("cancelRoommateBtn"),
  editDueDateDialog: document.getElementById("editDueDateDialog"),
  editDueDateForm: document.getElementById("editDueDateForm"),
  editDueDateInput: document.getElementById("editDueDateInput"),
  cancelEditDueDateBtn: document.getElementById("cancelEditDueDateBtn"),
  chatList: document.getElementById("chatList"),
  chatForm: document.getElementById("chatForm"),
  chatInput: document.getElementById("chatInput"),
  chatPanel: document.getElementById("chatPanel"),
  chatResizer: document.getElementById("chatResizer"),
  chatToggleBtn: document.getElementById("chatToggleBtn"),
  chatCloseBtn: document.getElementById("chatCloseBtn"),
};

const api = {
  async listChores() {
    const res = await fetch(apiUrl("/api/chores"));
    if (!res.ok) throw new Error("Nepodarilo sa nacitat ulohy");
    return res.json();
  },

  async createChore(chore) {
    const res = await fetch(apiUrl("/api/chores"), {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(chore),
    });
    if (!res.ok) throw new Error("Nepodarilo sa vytvorit ulohu");
    return res.json();
  },

  async updateChoreStatus(id, isDone) {
    const res = await fetch(apiUrl(`/api/chores/${id}`), {
      method: "PATCH",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ isDone }),
    });
    if (!res.ok) throw new Error("Nepodarilo sa upravit ulohu");
    return res.json();
  },

  async updateChore(id, patch) {
    const res = await fetch(apiUrl(`/api/chores/${id}`), {
      method: "PATCH",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(patch),
    });
    if (!res.ok) throw new Error("Nepodarilo sa upravit ulohu");
    return res.json();
  },

  async deleteChore(id) {
    const res = await fetch(apiUrl(`/api/chores/${id}`), { method: "DELETE" });
    if (!res.ok) throw new Error("Nepodarilo sa vymazat ulohu");
    return true;
  },

  async listMessages() {
    const res = await fetch(apiUrl("/api/messages"));
    if (!res.ok) throw new Error("Nepodarilo sa nacitat spravy");
    return res.json();
  },

  async createMessage(text) {
    const res = await fetch(apiUrl("/api/messages"), {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ text }),
    });
    if (!res.ok) throw new Error("Nepodarilo sa odoslat spravu");
    return res.json();
  },

  async listRoommates() {
    const res = await fetch(apiUrl("/api/roommates"));
    if (!res.ok) throw new Error("Nepodarilo sa nacitat spolubyvajucich");
    return res.json();
  },

  async createRoommate(name) {
    const res = await fetch(apiUrl("/api/roommates"), {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ name }),
    });
    if (!res.ok) throw new Error("Nepodarilo sa pridat spolubyvajuceho");
    return res.json();
  },
};

const utils = {
  todayIso,
  todayPlusDays,
  isoToDmy,
  dmyToIso,
  formatDate,
  formatDateTime,
  formatTimeShort,
};

const context = { config, state, el, api, utils };

const choresModule = window.createChoresModule(context);
const messagesModule = window.createMessagesModule(context);

bootstrap();

async function bootstrap() {
  messagesModule.loadChatWidth();
  choresModule.bindEvents();
  messagesModule.bindEvents();
  await choresModule.loadRoommates();
  choresModule.loadChores();
  messagesModule.loadChatMessages();
}

function apiUrl(path) {
  return `${config.apiBaseUrl}${path}`;
}

function todayIso() {
  return new Date().toISOString().slice(0, 10);
}

function todayPlusDays(days) {
  const now = new Date();
  now.setDate(now.getDate() + days);
  return now.toISOString().slice(0, 10);
}

function isoToDmy(isoDate) {
  const [year, month, day] = isoDate.split("-");
  return `${day}-${month}-${year}`;
}

function dmyToIso(dmyDate) {
  const match = dmyDate.match(/^(\d{2})-(\d{2})-(\d{4})$/);
  if (!match) return null;

  const [, day, month, year] = match;
  const date = new Date(Number(year), Number(month) - 1, Number(day));
  if (
    date.getFullYear() !== Number(year) ||
    date.getMonth() !== Number(month) - 1 ||
    date.getDate() !== Number(day)
  ) {
    return null;
  }

  return `${year}-${month}-${day}`;
}

function formatDate(isoDate) {
  const date = new Date(isoDate + "T00:00:00");
  return date.toLocaleDateString(config.locale, {
    month: "short",
    day: "numeric",
    year: "numeric",
  });
}

function formatDateTime(isoDateTime) {
  const date = new Date(isoDateTime);
  return date.toLocaleString(config.locale, {
    day: "2-digit",
    month: "short",
    year: "numeric",
    hour: "2-digit",
    minute: "2-digit",
    hour12: false,
  });
}

function formatTimeShort(isoDateTime) {
  const date = new Date(isoDateTime);
  return date.toLocaleTimeString(config.locale, {
    hour: "2-digit",
    minute: "2-digit",
    hour12: false,
  });
}
