export function cleanLabel(value, limit = 100) {
  if (typeof value !== 'string') return '';
  return value.replace(/\b(Bearer\s+)[^\s]+/gi, '$1[redacted]')
    .replace(/\b(?:sk-[A-Za-z0-9_-]{12,}|gh[pousr]_[A-Za-z0-9_]{12,}|github_pat_[A-Za-z0-9_]+|AKIA[A-Z0-9]{16})\b/g, '[redacted]')
    .replace(/\b(password|token|secret|api[-_]?key|authorization|cookie)\s*[:=]\s*[^\s,;]+/gi, '$1=[redacted]')
    .replace(/\s+/g, ' ').trim().slice(0, limit);
}
export function safeUrl(value) {
  if (typeof value !== 'string' || value.length > 2048) return null;
  try {
    const url = new URL(value);
    if (!['http:', 'https:'].includes(url.protocol) || url.username || url.password) return null;
    const sensitive = url.pathname.split('/').some(part => part.length > 48 || /token|password|secret|session|auth|apikey|api-key|cookie|oauth|login|account|user|profile|@|%/i.test(part));
    return url.origin + (sensitive ? '/' : url.pathname || '/');
  } catch { return null; }
}
export const shortcuts = new Set(['Ctrl+L', 'Ctrl+F', 'Ctrl+R', 'Ctrl+Enter']);
export function sanitizeStep(raw) {
  if (!raw || !['navigate', 'click', 'input_field', 'shortcut'].includes(raw.kind)) return null;
  const url = safeUrl(raw.url);
  if (!url) return null;
  const target = cleanLabel(raw.target);
  if (raw.kind === 'input_field' && /password|secret|token|card|credit|otp|\bpin\b|cookie|auth/i.test(target)) return null;
  if (raw.kind === 'shortcut' && !shortcuts.has(target)) return null;
  return {kind: raw.kind, url, target};
}
export function exported(state) {
  return {format: 'hotbox-demonstration-v1', steps: (state.steps || []).map(sanitizeStep).filter(Boolean)};
}
