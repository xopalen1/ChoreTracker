function createChoresModule(context) {
  const { state, el, api, utils } = context;
  const RANDOM_ASSIGNEE_VALUE = "__random_d20__";
  let pendingDeleteId = null;
  let choresPollTimer = null;
  let lastChoresSnapshot = "";

  function clearFieldError(input) {
    if (!(input instanceof HTMLInputElement) && !(input instanceof HTMLSelectElement)) {
      return;
    }
    input.setCustomValidity("");
  }

  function showFieldError(input, message) {
    if (!(input instanceof HTMLInputElement) && !(input instanceof HTMLSelectElement)) {
      return;
    }
    input.setCustomValidity(message);
    input.reportValidity();
  }

  function bindEvents() {
    startLiveChoresPolling();

    el.filterStatus?.addEventListener("change", () => {
      state.filterStatus = el.filterStatus.value;
      render();
    });

    el.searchInput?.addEventListener("input", () => {
      state.searchTerm = el.searchInput.value.trim().toLowerCase();
      render();
    });

    el.newChoreBtn?.addEventListener("click", () => {
      renderRoommateOptions();
      if (!state.roommates.length) {
        el.roommateForm?.reset();
        el.roommateDialog?.showModal();
        return;
      }

      el.choreForm?.reset();
      if (el.assigneeSelect instanceof HTMLSelectElement) {
        el.assigneeSelect.value = RANDOM_ASSIGNEE_VALUE;
        el.assigneeSelect.selectedIndex = 0;
        clearFieldError(el.assigneeSelect);
      }
      if (el.autoReassignInput instanceof HTMLInputElement) {
        el.autoReassignInput.checked = false;
      }
      if (el.dueDateInput instanceof HTMLInputElement) {
        el.dueDateInput.value = utils.isoToDmy(utils.todayPlusDays(1));
        clearFieldError(el.dueDateInput);
      }
      clearFieldError(el.assigneeSelect);
      el.choreDialog?.showModal();
    });

    el.newRoommateBtn?.addEventListener("click", () => {
      el.roommateForm?.reset();
      clearFieldError(el.roommateNameInput);
      el.roommateDialog?.showModal();
    });

    el.cancelRoommateBtn?.addEventListener("click", () => el.roommateDialog?.close());

    el.roommateNameInput?.addEventListener("input", () => clearFieldError(el.roommateNameInput));

    el.dueDateInput?.addEventListener("input", () => clearFieldError(el.dueDateInput));

    el.cancelBtn?.addEventListener("click", () => el.choreDialog?.close());
    el.cancelEditDueDateBtn?.addEventListener("click", () => {
      state.editingDueDateId = null;
      el.editDueDateDialog?.close();
    });
    el.cancelDeleteBtn?.addEventListener("click", () => {
      pendingDeleteId = null;
      el.deleteConfirmDialog?.close();
    });

    el.choreForm?.addEventListener("submit", handleCreateChore);
    el.roommateForm?.addEventListener("submit", handleCreateRoommate);
    el.deleteConfirmForm?.addEventListener("submit", handleDeleteConfirmSubmit);

    el.editDueDateInput?.addEventListener("input", () => clearFieldError(el.editDueDateInput));

    el.editDueDateForm?.addEventListener("submit", handleEditDueDateSubmit);

    el.choreList?.addEventListener("click", async (event) => {
      const target = event.target;
      if (!(target instanceof HTMLElement)) return;

      const item = target.closest("[data-id]");
      if (!item) return;

      const id = item.getAttribute("data-id");
      if (!id) return;

      if (target.classList.contains("btn-delete")) {
        await handleDelete(id);
        return;
      }

      if (target.classList.contains("btn-edit-date")) {
        await openEditDueDateDialog(id);
        return;
      }

      if (target.classList.contains("btn-toggle")) {
        await handleToggle(id);
      }
    });
  }

  async function loadChores() {
    try {
      const fetched = await api.listChores();
      const normalized = Array.isArray(fetched) ? fetched.map(normalizeChore) : [];
      const snapshot = normalized
        .map((chore) => `${chore.id}|${chore.title}|${chore.assignee}|${chore.dueDate}|${chore.assignedDate}|${chore.isDone}`)
        .join("\n");

      if (snapshot === lastChoresSnapshot) {
        return;
      }

      lastChoresSnapshot = snapshot;
      state.chores = normalized;
    } catch (error) {
      console.error(error);
      state.chores = [];
      lastChoresSnapshot = "";
    }

    render();
  }

  function startLiveChoresPolling() {
    if (!context.config.useBackendChores) return;

    if (choresPollTimer) {
      clearInterval(choresPollTimer);
      choresPollTimer = null;
    }

    const intervalMs = Number(context.config.liveChoresPollMs) > 0 ? Number(context.config.liveChoresPollMs) : 1500;
    choresPollTimer = setInterval(() => {
      if (document.hidden) return;
      loadChores();
    }, intervalMs);

    document.addEventListener("visibilitychange", () => {
      if (!document.hidden) {
        loadChores();
      }
    });
  }

  async function loadRoommates() {
    try {
      const fetched = await api.listRoommates();
      state.roommates = Array.isArray(fetched) ? fetched.map(normalizeRoommate) : [];
    } catch (error) {
      console.error(error);
      state.roommates = [];
    }

    renderRoommateOptions();
  }

  async function handleCreateChore(event) {
    event.preventDefault();
    if (!el.choreForm) return;

    const formData = new FormData(el.choreForm);
    const dueDateRaw = String(formData.get("dueDate")).trim();
    const dueDateIso = utils.dmyToIso(dueDateRaw);

    if (!dueDateIso) {
      showFieldError(el.dueDateInput, "Použi DD-MM-YYYY, napr. 11-09-2001");
      return;
    }

    clearFieldError(el.dueDateInput);

    const title = String(formData.get("title")).trim();
    const assignee = String(formData.get("assignee")).trim();
    const autoReassign = formData.get("autoReassign") === "on";
    const randomAssign = assignee === RANDOM_ASSIGNEE_VALUE;

    const isKnownRoommate = state.roommates.some(
      (roommate) => roommate.name.toLocaleLowerCase() === assignee.toLocaleLowerCase(),
    );

    if (!randomAssign && !isKnownRoommate) {
      showFieldError(el.assigneeSelect, "Vyber existujuceho spolubyvajuceho.");
      return;
    }

    if (!title || !assignee || !dueDateIso) return;

    // Keep create payload minimal for easier backend handling.
    const createPayload = {
      title,
      assignee,
      dueDate: dueDateIso,
      autoReassign,
      randomAssign,
    };

    try {
      const created = await api.createChore(createPayload);
      state.chores.push(normalizeChore(created));
    } catch (error) {
      console.error(error);
      return;
    }

    el.choreDialog?.close();
    render();
  }

  async function handleCreateRoommate(event) {
    event.preventDefault();
    if (!el.roommateForm) return;

    const formData = new FormData(el.roommateForm);
    const name = String(formData.get("name")).trim();

    if (!name) {
      showFieldError(el.roommateNameInput, "Zadaj meno spolubyvajuceho.");
      return;
    }

    try {
      const created = await api.createRoommate(name);
      state.roommates.push(normalizeRoommate(created));
    } catch (error) {
      console.error(error);
      showFieldError(el.roommateNameInput, "Tento spolubyvajuci uz existuje alebo sa nepodarilo ulozit.");
      return;
    }

    state.roommates.sort((a, b) => a.name.localeCompare(b.name, undefined, { sensitivity: "base" }));
    renderRoommateOptions();
    el.roommateDialog?.close();
  }

  async function handleEditDueDateSubmit(event) {
    event.preventDefault();
    if (!state.editingDueDateId) return;

    const chore = state.chores.find((entry) => entry.id === state.editingDueDateId);
    if (!chore) return;

    const nextDueDate = utils.dmyToIso((el.editDueDateInput?.value || "").trim());
    if (!nextDueDate) {
      showFieldError(el.editDueDateInput, "Použi DD-MM-YYYY, napr. 11-09-2001");
      return;
    }

    clearFieldError(el.editDueDateInput);

    if (nextDueDate === chore.dueDate) {
      state.editingDueDateId = null;
      el.editDueDateDialog?.close();
      return;
    }

    const historyEntry = {
      previousDueDate: chore.dueDate,
      newDueDate: nextDueDate,
      editedAt: new Date().toISOString(),
      assignee: chore.assignee,
    };
    const scopedHistory = (chore.dueDateHistory || []).filter(
      (entry) => String(entry?.assignee || "") === String(chore.assignee || ""),
    );
    const nextHistory = [...scopedHistory, historyEntry];

    try {
      const updated = await api.updateChore(chore.id, {
        dueDate: nextDueDate,
        dueDateHistory: nextHistory,
      });
      state.chores = state.chores.map((entry) =>
        entry.id === chore.id ? normalizeChore(updated) : entry,
      );
    } catch (error) {
      console.error(error);
      return;
    }

    state.editingDueDateId = null;
    el.editDueDateDialog?.close();
    render();
  }

  async function handleToggle(id) {
    const chore = state.chores.find((entry) => entry.id === id);
    if (!chore) return;

    const nextIsDone = !chore.isDone;

    try {
      const updated = await api.updateChoreStatus(id, nextIsDone);
      state.chores = state.chores.map((entry) =>
        entry.id === id ? normalizeChore(updated) : entry,
      );

      if (nextIsDone && updated.autoReassign && !updated.isDone && typeof context.refreshChatMessages === "function") {
        await context.refreshChatMessages();
      }
    } catch (error) {
      console.error(error);
      return;
    }

    render();
  }

  async function handleDelete(id) {
    if (!state.chores.find((entry) => entry.id === id)) return;
    pendingDeleteId = id;
    const chore = state.chores.find((entry) => entry.id === id);
    if (el.deleteConfirmText) {
      const choreTitle = chore?.title ? `"${chore.title}"` : "túto úlohu";
      el.deleteConfirmText.textContent = `Naozaj chceš vymazať ${choreTitle}?`;
    }
    el.deleteConfirmDialog?.showModal();
  }

  async function handleDeleteConfirmSubmit(event) {
    event.preventDefault();
    if (!pendingDeleteId) {
      el.deleteConfirmDialog?.close();
      return;
    }

    try {
      await api.deleteChore(pendingDeleteId);
      state.chores = state.chores.filter((entry) => entry.id !== pendingDeleteId);
    } catch (error) {
      console.error(error);
      return;
    }

    pendingDeleteId = null;
    el.deleteConfirmDialog?.close();
    render();
  }

  async function openEditDueDateDialog(id) {
    const chore = state.chores.find((entry) => entry.id === id);
    if (!chore) return;

    state.editingDueDateId = id;
    if (el.editDueDateInput instanceof HTMLInputElement) {
      el.editDueDateInput.value = utils.isoToDmy(chore.dueDate);
      clearFieldError(el.editDueDateInput);
    }
    el.editDueDateDialog?.showModal();
  }

  function filteredChores() {
    return state.chores.filter((chore) => {
      const matchesStatus =
        state.filterStatus === "all" ||
        (state.filterStatus === "open" && !chore.isDone) ||
        (state.filterStatus === "done" && chore.isDone);
      const searchable = `${chore.title} ${chore.assignee}`.toLowerCase();
      const matchesSearch = searchable.includes(state.searchTerm);
      return matchesStatus && matchesSearch;
    });
  }

  function render() {
    renderStats();

    const chores = filteredChores().sort((a, b) => a.assignedDate.localeCompare(b.assignedDate));
    const activeChores = chores.filter((chore) => !chore.isDone);
    const doneChores = chores.filter((chore) => chore.isDone);
    if (!el.choreList) return;
    el.choreList.innerHTML = "";

    if (!chores.length) {
      const empty = document.createElement("li");
      empty.className = "card chore-item";
      empty.textContent = "Nič také zatiaľ neni :<";
      el.choreList.append(empty);
      return;
    }

    if (activeChores.length) {
      const grouped = groupActiveByAssignee(activeChores);
      Object.keys(grouped)
        .sort((a, b) => a.localeCompare(b, undefined, { sensitivity: "base" }))
        .forEach((assignee) => {
          const header = document.createElement("li");
          header.className = "group-header";
          header.textContent = assignee;
          el.choreList.append(header);

          grouped[assignee].forEach((chore) => {
            el.choreList.append(renderChoreItem(chore));
          });
        });
    }

    if (doneChores.length) {
      const doneHeader = document.createElement("li");
      doneHeader.className = "group-header";
      doneHeader.textContent = "Dokončené";
      el.choreList.append(doneHeader);

      doneChores.forEach((chore) => {
        el.choreList.append(renderChoreItem(chore));
      });
    }
  }

  function renderChoreItem(chore) {
    if (!el.choreTemplate) return document.createDocumentFragment();

    const fragment = el.choreTemplate.content.cloneNode(true);
    const item = fragment.querySelector(".chore-item");
    const title = fragment.querySelector(".chore-title");
    const meta = fragment.querySelector(".chore-meta");
    const toggleBtn = fragment.querySelector(".btn-toggle");

    item?.setAttribute("data-id", chore.id);
    item?.classList.toggle("done", chore.isDone);

    if (title) title.textContent = chore.title;
    if (meta) {
      const reassignNote = chore.autoReassign ? " • Auto prerozdelenie" : "";
      meta.textContent = `${chore.assignee} • Priradené ${utils.formatDate(chore.assignedDate)} • Deadline ${utils.formatDate(chore.dueDate)}${reassignNote}`;
    }

    if (toggleBtn) {
      toggleBtn.dataset.state = chore.isDone ? "done" : "open";
      toggleBtn.textContent = chore.isDone ? "Označiť nehotové" : "Označiť hotové";
    }

    const history = (chore.dueDateHistory || []).filter(
      (entry) => String(entry?.assignee || "") === String(chore.assignee || ""),
    );
    if (history.length) {
      const historyDetails = document.createElement("details");
      historyDetails.className = "due-history";

      const summary = document.createElement("summary");
      summary.textContent = `Zmeny (${history.length})`;
      historyDetails.append(summary);

      const list = document.createElement("ul");
      history.forEach((entry) => {
        const line = document.createElement("li");
        line.textContent = `${utils.formatDateTime(entry.editedAt)}: ${utils.formatDate(entry.previousDueDate)} -> ${utils.formatDate(entry.newDueDate)}`;
        list.append(line);
      });
      historyDetails.append(list);
      item?.querySelector(".chore-main")?.append(historyDetails);
    }

    return fragment;
  }

  function groupActiveByAssignee(chores) {
    const groupedMap = chores.reduce((acc, chore) => {
      const assignee = String(chore.assignee || "").trim();
      const normalizedKey = assignee ? assignee.toLocaleLowerCase() : "unassigned";

      if (!acc[normalizedKey]) {
        acc[normalizedKey] = {
          label: assignee || "Nepriradené",
          chores: [],
        };
      }

      acc[normalizedKey].chores.push(chore);
      return acc;
    }, {});

    return Object.values(groupedMap).reduce((result, entry) => {
      result[entry.label] = entry.chores;
      return result;
    }, {});
  }

  function renderStats() {
    if (!el.totalCount || !el.openCount || !el.doneCount) return;

    const total = state.chores.length;
    const done = state.chores.filter((chore) => chore.isDone).length;
    const open = total - done;

    el.totalCount.textContent = String(total);
    el.openCount.textContent = String(open);
    el.doneCount.textContent = String(done);
  }

  function renderRoommateOptions() {
    if (!(el.assigneeSelect instanceof HTMLSelectElement)) return;

    const previousValue = el.assigneeSelect.value;
    el.assigneeSelect.innerHTML = "";

    if (!state.roommates.length) {
      const placeholder = document.createElement("option");
      placeholder.value = "";
      placeholder.disabled = true;
      placeholder.selected = true;
      placeholder.textContent = "Najprv pridaj spolubývajúcich";
      el.assigneeSelect.append(placeholder);
      el.assigneeSelect.disabled = true;
      return;
    }

    const randomOption = document.createElement("option");
    randomOption.value = RANDOM_ASSIGNEE_VALUE;
    randomOption.textContent = "Náhodne (kocka d20, predvolené)";
    el.assigneeSelect.append(randomOption);

    state.roommates
      .slice()
      .sort((a, b) => a.name.localeCompare(b.name, undefined, { sensitivity: "base" }))
      .forEach((roommate) => {
        const option = document.createElement("option");
        option.value = roommate.name;
        option.textContent = roommate.name;
        el.assigneeSelect.append(option);
      });

    if (previousValue === RANDOM_ASSIGNEE_VALUE || state.roommates.some((roommate) => roommate.name === previousValue)) {
      el.assigneeSelect.value = previousValue;
    } else {
      el.assigneeSelect.value = RANDOM_ASSIGNEE_VALUE;
    }

    el.assigneeSelect.disabled = false;
  }

  function normalizeChore(chore) {
    const normalizedIsDone =
      typeof chore.isDone === "boolean" ? chore.isDone : String(chore.status || "").toLowerCase() === "done";

    return {
      ...chore,
      assignedDate: chore.assignedDate || utils.todayIso(),
      dueDateHistory: Array.isArray(chore.dueDateHistory) ? chore.dueDateHistory : [],
      autoReassign: Boolean(chore.autoReassign),
      isDone: normalizedIsDone,
      status: normalizedIsDone ? "done" : "open",
    };
  }

  function normalizeRoommate(roommate) {
    return {
      id: String(roommate?.id || ""),
      name: String(roommate?.name || "").trim(),
    };
  }

  return {
    bindEvents,
    loadChores,
    loadRoommates,
  };
}

window.createChoresModule = createChoresModule;
