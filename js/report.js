// js/report.js
import { supabase } from './supabaseClient.js';
import { loadSidebar } from './utils.js';

document.addEventListener('DOMContentLoaded', async () => {
  // 1) Sidebar
  try { await loadSidebar(); } catch (e) { console.error(e); }

  // 2) Auth guard
  const { data: { session } } = await supabase.auth.getSession();
  if (!session) return window.location.href = 'login.html';

  // 3) Export CSV
  document.getElementById('export-csv').addEventListener('click', async () => {
    const { data, error } = await supabase.from('reports').select('*');
    if (error) return alert('Export failed: ' + error.message);

    const header = Object.keys(data[0]).join(',');
    const rows = data.map(r =>
      Object.values(r).map(v => `"${v}"`).join(',')
    );
    const csv  = [header, ...rows].join('\n');

    const blob = new Blob([csv], { type: 'text/csv' });
    const url  = URL.createObjectURL(blob);
    const a    = document.createElement('a');
    a.href     = url;
    a.download = 'reports.csv';
    a.click();
  });

  // 4) Fetch & render recent reports
  const { data: reports, error: err2 } = await supabase
    .from('reports')
    .select('report_type, description, created_at')
    .order('created_at', { ascending: false })
    .limit(20);

  const list = document.getElementById('report-list');
  if (err2) {
    list.innerHTML = `<li class="text-red-500">Error: ${err2.message}</li>`;
    return;
  }
  if (!reports.length) {
    list.innerHTML = `<li class="text-gray-500">No reports found.</li>`;
    return;
  }

  reports.forEach(r => {
    const li = document.createElement('li');
    li.className = 'bg-white rounded-xl shadow p-4';
    li.innerHTML = `
      <div class="font-semibold">${r.report_type}</div>
      <div class="text-sm text-gray-700">${r.description}</div>
      <div class="text-xs text-gray-500">${new Date(r.created_at).toLocaleString()}</div>
    `;
    list.appendChild(li);
  });
});
