// Only high-level actions while explicitly enabled on this tab. Never read input values.
(() => {
  if (globalThis.__hotboxRecording) { globalThis.__hotboxRecording.enabled = true; return; }
  const state = globalThis.__hotboxRecording = {enabled: true};
  const sensitive = element => /password|secret|token|card|credit|otp|\bpin\b|cookie|auth|one-time-code/i.test(
    ['type', 'name', 'id', 'autocomplete'].map(key => element?.getAttribute?.(key) || '').join(' '));
  const label = element => (element?.getAttribute?.('aria-label') || element?.getAttribute?.('name') ||
    element?.getAttribute?.('placeholder') || (['BUTTON','A'].includes(element?.tagName) ? element.textContent : '') || element?.tagName || 'control').slice(0, 100);
  const url = () => {
    const path = location.pathname.split('/');
    return location.origin + (path.some(p => p.length > 48 || /token|password|secret|session|auth|apikey|cookie|oauth|login|account|user|profile|@|%/i.test(p)) ? '/' : location.pathname);
  };
  const send = (kind, target) => {
    if (state.enabled) chrome.runtime.sendMessage({type:'step', step:{kind,url:url(),target}}).catch(() => {state.enabled=false;});
  };
  document.addEventListener('click', event => {
    if (!state.enabled || !event.isTrusted) return;
    const element = event.target?.closest?.('button,a,input,select,[role="button"]');
    if (!element || sensitive(element)) return;
    send('click', label(element));
  }, true);
  document.addEventListener('change', event => {
    if (!state.enabled || !event.isTrusted || sensitive(event.target)) return;
    if (['INPUT','TEXTAREA','SELECT'].includes(event.target?.tagName)) send('input_field', label(event.target));
  }, true);
  document.addEventListener('keydown', event => {
    if (!state.enabled || !event.isTrusted || sensitive(event.target) || !event.ctrlKey || event.altKey || event.metaKey || event.shiftKey) return;
    const key = event.key === 'Enter' ? 'Ctrl+Enter' : 'Ctrl+' + event.key.toUpperCase();
    if (['Ctrl+L','Ctrl+F','Ctrl+R','Ctrl+Enter'].includes(key)) send('shortcut', key);
  }, true);
  chrome.runtime.onMessage.addListener(message => {if(message.type==='hotbox-stop')state.enabled=false;});
})();
