// js/manage-devices.js
import { supabase } from './supabaseClient.js';
import { loadSidebar } from './utils.js';

document.addEventListener('DOMContentLoaded', async () => {
  await loadSidebar();

  // Auth guard
  const { data: { session } } = await supabase.auth.getSession();
  if (!session) return window.location.href = 'login.html';

  const listEl = document.getElementById('device-list');
  const form   = document.getElementById('add-device-form');

  // Fetch & render devices
  async function loadDevices() {
    const { data: devices, error } = await supabase
      .from('devices')
      .select('*')
      .order('created_at', { ascending: false });
    if (error) {
      listEl.innerHTML = `<li class="text-red-500">Error: ${error.message}</li>`;
      return;
    }
    if (!devices.length) {
      listEl.innerHTML = `<li class="text-gray-500">No devices found.</li>`;
      return;
    }
    listEl.innerHTML = '';
    devices.forEach(d => {
      const li = document.createElement('li');
      li.className = 'bg-gray-50 rounded-xl shadow p-4 flex justify-between items-center';
      li.innerHTML = `
        <div>
          <div class="font-semibold">${d.name} (${d.device_id})</div>
          <div class="text-sm text-gray-600">${d.latitude.toFixed(4)}, ${d.longitude.toFixed(4)}</div>
        </div>
        <div class="flex space-x-2">
          <button data-id="${d.id}" class="edit btn btn-secondary">Edit</button>
          <button data-id="${d.id}" class="delete bg-red-600 text-white px-2 py-1 rounded">Delete</button>
        </div>
      `;
      listEl.appendChild(li);
    });
    attachListHandlers();
  }

  // Add device
  form.addEventListener('submit', async e => {
    e.preventDefault();
    const vals = Object.fromEntries(new FormData(form).entries());
    const { error } = await supabase.from('devices').insert({
      device_id: vals.device_id,
      name: vals.name,
      latitude: parseFloat(vals.latitude),
      longitude: parseFloat(vals.longitude),
      status: vals.status,
      communication_type: vals.communication_type
    });
    if (error) return alert('Insert failed: ' + error.message);
    form.reset();
    loadDevices();
  });

  // Edit/Delete handlers
  function attachListHandlers() {
    document.querySelectorAll('.delete').forEach(btn => {
      btn.onclick = async () => {
        if (!confirm('Delete this device?')) return;
        const { error } = await supabase
          .from('devices')
          .delete()
          .eq('id', btn.dataset.id);
        if (error) return alert('Delete failed: ' + error.message);
        loadDevices();
      };
    });
    document.querySelectorAll('.edit').forEach(btn => {
      btn.onclick = async () => {
        const newName = prompt('New device name:');
        if (!newName) return;
        const { error } = await supabase
          .from('devices')
          .update({ name: newName })
          .eq('id', btn.dataset.id);
        if (error) return alert('Update failed: ' + error.message);
        loadDevices();
      };
    });
  }

  // initial load
  loadDevices();
});
