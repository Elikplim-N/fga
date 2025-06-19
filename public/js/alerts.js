// js/alerts.js
import { supabase } from './supabaseClient.js';
import { loadSidebar } from './utils.js';

document.addEventListener('DOMContentLoaded', async () => {
  // 1) sidebar
  try {
    await loadSidebar();
  } catch (err) {
    console.error('Sidebar load failed:', err);
  }

  // 2) auth check
  const { data: { session } } = await supabase.auth.getSession();
  if (!session) {
    window.location.href = 'login.html';
    return;
  }

  // 3) fetch alerts
  const { data: alerts, error } = await supabase
    .from('alerts')
    .select('alert_type, confidence, created_at')
    .order('created_at', { ascending: false });

  if (error) {
    console.error('Error loading alerts:', error);
    document.getElementById('alerts-list').innerHTML = `
      <li class="text-red-500">Failed to load alerts.</li>
    `;
    return;
  }

  // 4) render
  const list = document.getElementById('alerts-list');
  if (!alerts.length) {
    list.innerHTML = `<li class="text-gray-500">No alerts found.</li>`;
    return;
  }

  alerts.forEach(a => {
    const li = document.createElement('li');
    li.className = 'bg-white rounded-xl shadow p-4';
    li.innerHTML = `
      <div class="flex justify-between mb-1">
        <span class="font-semibold">${a.alert_type}</span>
        <span class="text-sm text-gray-600">${a.confidence}%</span>
      </div>
      <div class="text-xs text-gray-500">
        ${new Date(a.created_at).toLocaleString()}
      </div>
    `;
    list.appendChild(li);
  });
});
