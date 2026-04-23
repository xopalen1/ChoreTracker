function createMessagesModule(context) {
  const { config, state, el, api, utils } = context;
  const chatOpenStorageKey = "roommate-chat-open-v1";
  let messagesPollTimer = null;
  let lastMessagesSnapshot = "";

  function bindEvents() {
    syncResponsiveUi();
    bindChatVisibility();
    setupChatResize();
    startLiveMessagesPolling();

    el.chatForm?.addEventListener("submit", async (event) => {
      event.preventDefault();
      const raw = String(el.chatInput?.value || "").trim();
      if (!raw) return;

      if (config.useBackendMessages) {
        try {
          await api.createMessage(raw);
          await loadChatMessages();
        } catch (error) {
          console.error(error);
          return;
        }
      }

      if (el.chatInput instanceof HTMLInputElement) {
        el.chatInput.value = "";
        el.chatInput.focus();
      }
    });
  }

  async function loadChatMessages() {
    if (config.useBackendMessages) {
      try {
        const messages = await api.listMessages();
        const normalized = Array.isArray(messages) ? messages.map(normalizeChatMessage) : [];
        const snapshot = normalized
          .map((message) => `${message.id}|${message.sentAt}|${message.text}`)
          .join("\n");

        if (snapshot === lastMessagesSnapshot) {
          return;
        }

        lastMessagesSnapshot = snapshot;
        state.chatMessages = normalized;
      } catch (error) {
        console.error(error);
        state.chatMessages = [];
        lastMessagesSnapshot = "";
      }
      renderChat();
      return;
    }

    state.chatMessages = [];
    lastMessagesSnapshot = "";
    renderChat();
  }

  function startLiveMessagesPolling() {
    if (!config.useBackendMessages) return;

    if (messagesPollTimer) {
      clearInterval(messagesPollTimer);
      messagesPollTimer = null;
    }

    const intervalMs = Number(config.liveMessagesPollMs) > 0 ? Number(config.liveMessagesPollMs) : 1500;
    messagesPollTimer = setInterval(() => {
      if (document.hidden) return;
      loadChatMessages();
    }, intervalMs);

    document.addEventListener("visibilitychange", () => {
      if (!document.hidden) {
        loadChatMessages();
      }
    });
  }

  function loadChatWidth() {
    const savedWidth = Number(localStorage.getItem(config.storageKeys.chatWidth));
    if (!Number.isNaN(savedWidth) && savedWidth > 0) {
      state.chatWidth = savedWidth;
    }

    const savedHeight = Number(localStorage.getItem(config.storageKeys.chatHeight));
    if (!Number.isNaN(savedHeight) && savedHeight > 0) {
      state.chatMobileHeight = savedHeight;
    }

    applyChatWidth(state.chatWidth);
    applyChatHeight(state.chatMobileHeight);

    const wasOpen = localStorage.getItem(chatOpenStorageKey) === "1";
    setChatOpen(wasOpen);
  }

  function bindChatVisibility() {
    el.chatToggleBtn?.addEventListener("click", () => {
      setChatOpen(true);
      el.chatInput?.focus();
    });

    el.chatCloseBtn?.addEventListener("click", () => {
      setChatOpen(false);
    });
  }

  function setChatOpen(isOpen) {
    if (!el.chatPanel || !el.chatToggleBtn) return;

    const effectiveOpen = isMobileViewport() ? isOpen : true;

    el.chatPanel.classList.toggle("open", effectiveOpen);
    el.chatPanel.setAttribute("aria-hidden", effectiveOpen ? "false" : "true");
    el.chatToggleBtn.setAttribute("aria-expanded", effectiveOpen ? "true" : "false");
    document.body.classList.toggle("chat-open", effectiveOpen);

    if (isMobileViewport()) {
      localStorage.setItem(chatOpenStorageKey, effectiveOpen ? "1" : "0");
    }

    syncMobileFullscreenClass();
  }

  function syncResponsiveUi() {
    const mobile = isMobileViewport();

    if (el.chatToggleBtn) {
      el.chatToggleBtn.hidden = !mobile;
      el.chatToggleBtn.style.display = mobile ? "inline-block" : "none";
    }

    if (el.chatCloseBtn) {
      el.chatCloseBtn.hidden = !mobile;
      el.chatCloseBtn.style.display = mobile ? "inline-flex" : "none";
    }
  }

  function applyChatWidth(widthPx) {
    const min = 300;
    const max = Math.max(360, Math.min(620, window.innerWidth - 220));
    const clamped = Math.max(min, Math.min(max, Math.round(widthPx)));
    state.chatWidth = clamped;
    document.documentElement.style.setProperty("--chat-width", `${clamped}px`);
  }

  function applyChatHeight(heightPx) {
    const min = 220;
    const max = Math.max(320, window.innerHeight);
    const clamped = Math.max(min, Math.min(max, Math.round(heightPx)));
    state.chatMobileHeight = clamped;
    document.documentElement.style.setProperty("--chat-mobile-height", `${clamped}px`);
    syncMobileFullscreenClass();
  }

  function syncMobileFullscreenClass() {
    if (!el.chatPanel) return;

    const mobile = isMobileViewport();
    const fullHeightThreshold = Math.max(320, window.innerHeight) - 2;
    const isFullscreen = mobile && state.chatMobileHeight >= fullHeightThreshold;

    el.chatPanel.classList.toggle("mobile-fullscreen", isFullscreen);
  }

  function isMobileViewport() {
    return window.innerWidth <= 700;
  }

  function setupChatResize() {
    if (!el.chatResizer || !el.chatPanel) return;

    let isDragging = false;
    let mode = "desktop";
    let startX = 0;
    let startY = 0;
    let startWidth = state.chatWidth;
    let startHeight = state.chatMobileHeight;

    const onMove = (event) => {
      if (!isDragging) return;
      if (!el.chatPanel?.classList.contains("open")) return;

      if (mode === "mobile") {
        const delta = startY - event.clientY;
        const next = startHeight + delta;
        applyChatHeight(next);
      } else {
        const next = window.innerWidth - event.clientX;
        applyChatWidth(next);
      }
    };

    const onUp = () => {
      if (!isDragging) return;
      isDragging = false;
      mode = "desktop";
      el.chatPanel?.classList.remove("resizing");
      localStorage.setItem(config.storageKeys.chatWidth, String(state.chatWidth));
      localStorage.setItem(config.storageKeys.chatHeight, String(state.chatMobileHeight));
      window.removeEventListener("pointermove", onMove);
      window.removeEventListener("pointerup", onUp);
      window.removeEventListener("pointercancel", onUp);
    };

    const startResize = (event) => {
      if (!el.chatPanel?.classList.contains("open")) return;
      if (event.target instanceof Element && event.target.closest("button, input, textarea, select, a, [role='button']")) {
        return;
      }

      const nextMode = isMobileViewport() ? "mobile" : "desktop";

      if (nextMode === "mobile" && event.pointerType === "touch") {
        event.preventDefault();
      }

      isDragging = true;
      mode = nextMode;
      startX = event.clientX;
      startY = event.clientY;
      startWidth = state.chatWidth;
      startHeight = state.chatMobileHeight;
      event.preventDefault();
      if (event.currentTarget instanceof Element && event.currentTarget.setPointerCapture) {
        event.currentTarget.setPointerCapture(event.pointerId);
      }
      el.chatPanel?.classList.add("resizing");
      window.addEventListener("pointermove", onMove);
      window.addEventListener("pointerup", onUp);
      window.addEventListener("pointercancel", onUp);
    };

    el.chatResizer.addEventListener("pointerdown", startResize);
    el.chatHeader?.addEventListener("pointerdown", startResize);

    window.addEventListener("resize", () => {
      syncResponsiveUi();
      const wasOpen = localStorage.getItem(chatOpenStorageKey) === "1";
      setChatOpen(wasOpen);

      if (isMobileViewport()) {
        applyChatHeight(state.chatMobileHeight);
      } else {
        applyChatWidth(state.chatWidth);
      }

      syncMobileFullscreenClass();
    });
  }

  function renderChat() {
    if (!el.chatList) return;
    el.chatList.innerHTML = "";

    if (!state.chatMessages.length) {
      const empty = document.createElement("li");
      empty.className = "chat-empty";
      empty.textContent = "Nikto nič zatiaľ nenapísal :<";
      el.chatList.append(empty);
      return;
    }

    state.chatMessages.forEach((message) => {
      const item = document.createElement("li");
      item.className = "chat-item";

      const bubble = document.createElement("p");
      bubble.className = "chat-bubble";
      bubble.textContent = message.text;

      const time = document.createElement("p");
      time.className = "chat-time";
      time.textContent = utils.formatTimeShort(message.sentAt);

      item.append(bubble, time);
      el.chatList.append(item);
    });

    el.chatList.scrollTop = el.chatList.scrollHeight;
  }

  function normalizeChatMessage(message) {
    return {
      id: message.id || crypto.randomUUID(),
      text: String(message.text || ""),
      sentAt: message.sentAt || new Date().toISOString(),
    };
  }

  return {
    bindEvents,
    loadChatMessages,
    loadChatWidth,
  };
}

window.createMessagesModule = createMessagesModule;
