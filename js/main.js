const config = {
  apiBaseUrl: `${window.location.protocol}//${window.location.hostname}:8080`,
  locale: "sk-SK",
  useBackendChores: true,
  useBackendMessages: true,
  liveMessagesPollMs: 1500,
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
  autoReassignInput: document.getElementById("autoReassign"),
  roommateDialog: document.getElementById("roommateDialog"),
  roommateForm: document.getElementById("roommateForm"),
  roommateNameInput: document.getElementById("roommateName"),
  cancelRoommateBtn: document.getElementById("cancelRoommateBtn"),
  editDueDateDialog: document.getElementById("editDueDateDialog"),
  editDueDateForm: document.getElementById("editDueDateForm"),
  editDueDateInput: document.getElementById("editDueDateInput"),
  cancelEditDueDateBtn: document.getElementById("cancelEditDueDateBtn"),
  deleteConfirmDialog: document.getElementById("deleteConfirmDialog"),
  deleteConfirmForm: document.getElementById("deleteConfirmForm"),
  deleteConfirmText: document.getElementById("deleteConfirmText"),
  cancelDeleteBtn: document.getElementById("cancelDeleteBtn"),
  chatList: document.getElementById("chatList"),
  chatHeader: document.querySelector(".chat-header"),
  chatForm: document.getElementById("chatForm"),
  chatInput: document.getElementById("chatInput"),
  chatPanel: document.getElementById("chatPanel"),
  chatResizer: document.getElementById("chatResizer"),
  chatToggleBtn: document.getElementById("chatToggleBtn"),
  chatCloseBtn: document.getElementById("chatCloseBtn"),
};

const api = {
  async listChores() {
    return requestApiJson("/api/chores", { errorMessage: "Nepodarilo sa nacitat ulohy" });
  },

  async createChore(chore) {
    return requestApiJson("/api/chores", {
      method: "POST",
      body: chore,
      errorMessage: "Nepodarilo sa vytvorit ulohu",
    });
  },

  async updateChoreStatus(id, isDone) {
    return requestApiJson(`/api/chores/${id}`, {
      method: "PATCH",
      body: { isDone },
      errorMessage: "Nepodarilo sa upravit ulohu",
    });
  },

  async updateChore(id, patch) {
    return requestApiJson(`/api/chores/${id}`, {
      method: "PATCH",
      body: patch,
      errorMessage: "Nepodarilo sa upravit ulohu",
    });
  },

  async deleteChore(id) {
    await requestApi(`/api/chores/${id}`, {
      method: "DELETE",
      errorMessage: "Nepodarilo sa vymazat ulohu",
      expectJson: false,
    });
  },

  async listMessages() {
    return requestApiJson("/api/messages", { errorMessage: "Nepodarilo sa nacitat spravy" });
  },

  async createMessage(text) {
    return requestApiJson("/api/messages", {
      method: "POST",
      body: { text },
      errorMessage: "Nepodarilo sa odoslat spravu",
    });
  },

  async listRoommates() {
    return requestApiJson("/api/roommates", { errorMessage: "Nepodarilo sa nacitat spolubyvajucich" });
  },

  async createRoommate(name) {
    return requestApiJson("/api/roommates", {
      method: "POST",
      body: { name },
      errorMessage: "Nepodarilo sa pridat spolubyvajuceho",
    });
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

context.refreshChatMessages = () => messagesModule.loadChatMessages();

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

async function requestApi(path, options = {}) {
  const {
    method = "GET",
    body,
    errorMessage = "Poziadavka zlyhala",
    expectJson = true,
  } = options;

  const requestOptions = { method };
  if (body !== undefined) {
    requestOptions.headers = { "Content-Type": "application/json" };
    requestOptions.body = JSON.stringify(body);
  }

  const res = await fetch(apiUrl(path), requestOptions);
  if (!res.ok) throw new Error(errorMessage);
  if (!expectJson) return null;
  return res.json();
}

function requestApiJson(path, options = {}) {
  return requestApi(path, { ...options, expectJson: true });
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
