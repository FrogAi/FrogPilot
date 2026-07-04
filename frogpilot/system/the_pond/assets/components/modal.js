import { html } from "/assets/vendor/arrow.mjs";

let modalIdCounter = 0;

export function Modal({
  title,
  message,
  onConfirm,
  onCancel,
  confirmText = "Confirm",
  cancelText = "Cancel",
  confirmClass = "btn-danger",
  customClass = ""
}) {
  const active = document.activeElement;
  const previouslyFocused = (active && active.closest && active.closest(".modal-overlay")) ? null : active;
  const restoreFocus = () => {
    setTimeout(() => {
      if (previouslyFocused && typeof previouslyFocused.focus === "function") previouslyFocused.focus();
    }, 0);
  };
  const wrappedConfirm = onConfirm ? () => { onConfirm(); restoreFocus(); } : null;
  const wrappedCancel = onCancel ? () => { onCancel(); restoreFocus(); } : null;

  setTimeout(() => {
    const overlays = document.querySelectorAll(".modal-overlay");
    const overlay = overlays[overlays.length - 1];
    if (overlay && !overlay.contains(document.activeElement)) overlay.focus();
  }, 0);

  const trapTab = (e) => {
    const overlay = e.currentTarget;
    const focusable = overlay.querySelectorAll(
      'button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])'
    );
    if (!focusable.length) {
      e.preventDefault();
      overlay.focus();
      return;
    }
    const first = focusable[0];
    const last = focusable[focusable.length - 1];
    const active = document.activeElement;
    if (e.shiftKey && (active === first || active === overlay)) {
      e.preventDefault();
      last.focus();
    } else if (!e.shiftKey && active === last) {
      e.preventDefault();
      first.focus();
    }
  };

  const titleId = `modal-title-${++modalIdCounter}`;

  return html`
    <div class="modal-overlay" tabindex="0"
      @click="${(e) => {
        if (e.target.classList.contains("modal-overlay")) {
          wrappedCancel && wrappedCancel();
        }
      }}"
      @keydown="${(e) => {
        if (e.key === "Enter" && wrappedConfirm && e.target.tagName !== "BUTTON") {
          e.preventDefault();
          wrappedConfirm();
        } else if (e.key === "Escape") {
          e.preventDefault();
          wrappedCancel && wrappedCancel();
        } else if (e.key === "Tab") {
          trapTab(e);
        }
      }}"
    >
      <div class="modal ${customClass}" role="dialog" aria-modal="true" aria-labelledby="${titleId}">
        <div class="modal-header" id="${titleId}">${() => title}</div>
        <div class="modal-body">${() => message}</div>
        <div class="modal-actions">
          ${onCancel ? html`<button class="btn" @click="${wrappedCancel}">${cancelText}</button>` : ""}
          ${onConfirm ? html`<button class="btn ${confirmClass}" @click="${wrappedConfirm}">${confirmText}</button>` : ""}
        </div>
      </div>
    </div>
  `;
}
