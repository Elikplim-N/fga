// js/utils.js
export async function loadSidebar() {
  const resp = await fetch('sidebar.html');
  if (!resp.ok) throw new Error('Failed to load sidebar');
  document.getElementById('sidebar-placeholder').innerHTML = await resp.text();
}
loadSidebar().catch(console.error);
