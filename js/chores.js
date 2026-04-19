function createChoresModule(context) {
  const { state, el, api, utils } = context;

  function bindEvents() {
    el.filterStatus?.addEventListener("change", () => {
      state.filterStatus = el.filterStatus.value;
      render();
    });

    el.searchInput?.addEventListener("input", () => {
      state.searchTerm = el.searchInput.value.trim().toLowerCase();
      render();
    });

    el.newChoreBtn?.addEventListener("click", () => {
      el.choreForm?.reset();
      if (el.dueDateInput instanceof HTMLInputElement) {
        el.dueDateInput.value = utils.isoToDmy(utils.todayPlusDays(1));
        el.dueDateInput.setCustomValidity("");
      }
      el.choreDialog?.showModal();
    });

    if (el.dueDateInput instanceof HTMLInputElement) {
      el.dueDateInput.addEventListener("input", () => el.dueDateInput.setCustomValidity(""));
    }

    el.cancelBtn?.addEventListener("click", () => el.choreDialog?.close());
    el.cancelEditDueDateBtn?.addEventListener("click", () => {
      state.editingDueDateId = null;
      el.editDueDateDialog?.close();
    });

    el.choreForm?.addEventListener("submit", handleCreateChore);

    if (el.editDueDateInput instanceof HTMLInputElement) {
      el.editDueDateInput.addEventListener("input", () => {
        el.editDueDateInput.setCustomValidity("");
      });
    }

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
      state.chores = Array.isArray(fetched) ? fetched.map(normalizeChore) : [];
    } catch (error) {
      console.error(error);
      state.chores = [];
    }

    render();
  }

  async function handleCreateChore(event) {
    event.preventDefault();
    if (!el.choreForm) return;

    const formData = new FormData(el.choreForm);
    const dueDateRaw = String(formData.get("dueDate")).trim();
    const dueDateIso = utils.dmyToIso(dueDateRaw);

    if (!dueDateIso) {
      if (el.dueDateInput instanceof HTMLInputElement) {
        el.dueDateInput.setCustomValidity("Použi DD-MM-YYYY, napr. 11-09-2001");
        el.dueDateInput.reportValidity();
      }
      return;
    }

    if (el.dueDateInput instanceof HTMLInputElement) {
      el.dueDateInput.setCustomValidity("");
    }

    const title = String(formData.get("title")).trim();
    const assignee = String(formData.get("assignee")).trim();

    if (!title || !assignee || !dueDateIso) return;

    // Keep create payload minimal for easier backend handling.
    const createPayload = {
      title,
      assignee,
      dueDate: dueDateIso,
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

  async function handleEditDueDateSubmit(event) {
    event.preventDefault();
    if (!state.editingDueDateId) return;

    const chore = state.chores.find((entry) => entry.id === state.editingDueDateId);
    if (!chore) return;

    const nextDueDate = utils.dmyToIso((el.editDueDateInput?.value || "").trim());
    if (!nextDueDate) {
      if (el.editDueDateInput instanceof HTMLInputElement) {
        el.editDueDateInput.setCustomValidity("Použi DD-MM-YYYY, napr. 11-09-2001");
        el.editDueDateInput.reportValidity();
      }
      return;
    }

    if (el.editDueDateInput instanceof HTMLInputElement) {
      el.editDueDateInput.setCustomValidity("");
    }

    if (nextDueDate === chore.dueDate) {
      state.editingDueDateId = null;
      el.editDueDateDialog?.close();
      return;
    }

    const historyEntry = {
      previousDueDate: chore.dueDate,
      newDueDate: nextDueDate,
      editedAt: new Date().toISOString(),
    };
    const nextHistory = [...(chore.dueDateHistory || []), historyEntry];

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
    } catch (error) {
      console.error(error);
      return;
    }

    render();
  }

  async function handleDelete(id) {
    if (!state.chores.find((entry) => entry.id === id)) return;

    try {
      const updated = await api.updateChore(id, { isDeleted: true });
      state.chores = state.chores.map((entry) =>
        entry.id === id ? normalizeChore(updated) : entry,
      );
    } catch (error) {
      console.error(error);
      return;
    }

    render();
  }

  async function openEditDueDateDialog(id) {
    const chore = state.chores.find((entry) => entry.id === id);
    if (!chore) return;

    state.editingDueDateId = id;
    if (el.editDueDateInput instanceof HTMLInputElement) {
      el.editDueDateInput.value = utils.isoToDmy(chore.dueDate);
      el.editDueDateInput.setCustomValidity("");
    }
    el.editDueDateDialog?.showModal();
  }

  function filteredChores() {
    return state.chores.filter((chore) => {
      const matchesStatus =
        (state.filterStatus === "all" && !chore.isDeleted) ||
        (state.filterStatus === "open" && !chore.isDone && !chore.isDeleted) ||
        (state.filterStatus === "done" && chore.isDone && !chore.isDeleted);
      const searchable = `${chore.title} ${chore.assignee}`.toLowerCase();
      const matchesSearch = searchable.includes(state.searchTerm);
      return matchesStatus && matchesSearch;
    });
  }

  function render() {
    renderStats();

    const chores = filteredChores().sort((a, b) => a.assignedDate.localeCompare(b.assignedDate));
    const activeChores = chores.filter((chore) => !chore.isDone && !chore.isDeleted);
    const doneChores = chores.filter((chore) => chore.isDone && !chore.isDeleted);
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
      meta.textContent = `${chore.assignee} • Priradené ${utils.formatDate(chore.assignedDate)} • Deadline ${utils.formatDate(chore.dueDate)}`;
    }

    if (toggleBtn) {
      toggleBtn.dataset.state = chore.isDone ? "done" : "open";
      toggleBtn.textContent = chore.isDone ? "Označiť nehotové" : "Označiť hotové";
    }

    const history = chore.dueDateHistory || [];
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
    const visible = state.chores.filter((chore) => !chore.isDeleted);
    const totalVisible = visible.length;
    const done = visible.filter((chore) => chore.isDone).length;
    const open = totalVisible - done;

    el.totalCount.textContent = String(totalVisible);
    el.openCount.textContent = String(open);
    el.doneCount.textContent = String(done);
  }

  function normalizeChore(chore) {
    const normalizedIsDone =
      typeof chore.isDone === "boolean" ? chore.isDone : String(chore.status || "").toLowerCase() === "done";
    const normalizedIsDeleted =
      typeof chore.isDeleted === "boolean" ? chore.isDeleted : String(chore.status || "").toLowerCase() === "deleted";

    return {
      ...chore,
      assignedDate: chore.assignedDate || utils.todayIso(),
      dueDateHistory: Array.isArray(chore.dueDateHistory) ? chore.dueDateHistory : [],
      isDone: normalizedIsDone,
      isDeleted: normalizedIsDeleted,
      status: normalizedIsDeleted ? "deleted" : normalizedIsDone ? "done" : "open",
    };
  }

  return {
    bindEvents,
    loadChores,
  };
}

window.createChoresModule = createChoresModule;
