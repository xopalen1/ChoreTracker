function createMessagesModule(context) {
  const { config, state, el, api, utils } = context;
  const chatOpenStorageKey = "roommate-chat-open-v1";

  function bindEvents() {
    syncResponsiveUi();
    bindChatVisibility();
    setupChatResize();

    el.chatForm?.addEventListener("submit", async (event) => {
      event.preventDefault();
      const raw = String(el.chatInput?.value || "").trim();
      if (!raw) return;

      if (config.useBackendMessages) {
        try {
          const created = await api.createMessage(raw);
          state.chatMessages.push(normalizeChatMessage(created));
          state.chatMessages = state.chatMessages.slice(-80);
          renderChat();
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
        state.chatMessages = Array.isArray(messages) ? messages.map(normalizeChatMessage) : [];
      } catch (error) {
        console.error(error);
        state.chatMessages = [];
      }
      renderChat();
      return;
    }

    state.chatMessages = [];
    renderChat();
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

      isDragging = true;
      mode = isMobileViewport() ? "mobile" : "desktop";
      startX = event.clientX;
      startY = event.clientY;
      startWidth = state.chatWidth;
      startHeight = state.chatMobileHeight;
      event.preventDefault();
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
