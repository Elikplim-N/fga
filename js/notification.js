// js/notifications.js
import { supabase } from './supabaseClient.js';
import { loadSidebar } from './utils.js';

document.addEventListener('DOMContentLoaded', async () => {
  // 1) Sidebar
  try { await loadSidebar(); } catch (e) { console.error(e); }

  // 2) Auth guard
  const { data: { session } } = await supabase.auth.getSession();
  if (!session) return window.location.href = 'login.html';

  // 3) Fetch notifications
  const { data: notifications, error } = await supabase
    .from('notifications')
    .select('id, title, message, created_at, read')
    .order('created_at', { ascending: false })
    .limit(20);

  const list = document.getElementById('notifications-list');
  if (error) {
    list.innerHTML = `<li class="text-red-500">Error: ${error.message}</li>`;
    return;
  }
  if (!notifications.length) {
    list.innerHTML = `<li class="text-gray-500">No notifications.</li>`;
    return;
  }

  notifications.forEach(n => {
    const li = document.createElement('li');
    li.className = 'bg-white rounded-xl shadow p-4 flex justify-between items-start';
    li.innerHTML = `
      <div class="flex-1">
        <div class="font-semibold">${n.title}</div>
        <div class="text-sm text-gray-700">${n.message}</div>
        <div class="text-xs text-gray-500">${new Date(n.created_at).toLocaleString()}</div>
      </div>
      <div class="ml-4">
        ${
          n.read
            ? '<span class="text-green-600">✓</span>'
            : `<button data-id="${n.id}" class="mark-read px-2 py-1 bg-blue-600 text-white rounded">Mark read</button>`
        }
      </div>
    `;
    list.appendChild(li);
  });

  // 4) Mark-as-read handlers
  document.querySelectorAll('.mark-read').forEach(btn => {
    btn.addEventListener('click', async e => {
      const id = e.target.dataset.id;
      const { error: updErr } = await supabase
        .from('notifications')
        .update({ read: true })
        .eq('id', id);
      if (updErr) return alert('Update failed: ' + updErr.message);
      e.target.outerHTML = '<span class="text-green-600">✓</span>';
    });
  });
});
