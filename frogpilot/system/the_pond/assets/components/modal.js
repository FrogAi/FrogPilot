import { html } from "https://esm.sh/@arrow-js/core";

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
  return html`
    <div class="modal-overlay" tabindex="0"
      @click="${(e) => {
        if (e.target.classList.contains('modal-overlay')) {
          onCancel && onCancel();
        }
      }}"
      @keydown="${(e) => {
        if (e.key === 'Enter' && onConfirm) {
          e.preventDefault();
          onConfirm();
        } else if (e.key === 'Escape') {
          e.preventDefault();
          onCancel && onCancel();
        }
      }}"
    >
      <div class="modal ${customClass}">
        <div class="modal-header">${title}</div>
        <div class="modal-body">${message}</div>
        <div class="modal-actions">
          ${onCancel ? html`<button class="btn" @click="${onCancel}">${cancelText}</button>` : ''}
          ${onConfirm ? html`<button class="btn ${confirmClass}" @click="${onConfirm}">${confirmText}</button>` : ''}
        </div>
      </div>
    </div>
  `;
}
