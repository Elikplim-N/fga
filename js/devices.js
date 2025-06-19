// js/devices.js
import { supabase } from './supabaseClient.js';
import { loadSidebar } from './utils.js';

document.addEventListener('DOMContentLoaded', async () => {
  // 1) Inject sidebar
  try {
    await loadSidebar();
  } catch (err) {
    console.error('Sidebar load failed:', err);
  }

  // 2) Auth guard
  const { data: { session } } = await supabase.auth.getSession();
  if (!session) {
    window.location.href = 'login.html';
    return;
  }

  // 3) Fetch devices
  const { data: devices, error } = await supabase
    .from('devices')
    .select('*')
    .order('last_ping', { ascending: false });

  const list = document.getElementById('device-list');
  if (error) {
    list.innerHTML = `<li class="text-red-500">Error loading devices: ${error.message}</li>`;
    return;
  }
  if (!devices.length) {
    list.innerHTML = `<li class="text-gray-500">No devices found.</li>`;
    return;
  }

  // 4) Render each device as a card
  devices.forEach(d => {
    const li = document.createElement('li');
    li.className = [
      'bg-white','rounded-xl','shadow','p-6',
      'flex','flex-col','md:flex-row',
      'justify-between','items-start',
      'space-y-4','md:space-y-0','md:items-center'
    ].join(' ');

    // status badge
    let statusClasses = 'bg-gray-200 text-gray-800';
    if (d.status === 'active')       statusClasses = 'bg-green-200 text-green-800';
    else if (d.status === 'inactive') statusClasses = 'bg-red-200 text-red-800';
    else if (d.status === 'maintenance') statusClasses = 'bg-yellow-200 text-yellow-800';

    li.innerHTML = `
      <div>
        <h3 class="text-lg font-semibold">${d.name}</h3>
        <p class="text-sm text-gray-600">ID: ${d.device_id}</p>
      </div>
      <div class="flex flex-col md:flex-row md:items-center space-y-2 md:space-y-0 md:space-x-4">
        <span class="px-3 py-1 rounded-full text-sm ${statusClasses}">
          ${d.status}
        </span>
        <span class="text-sm text-gray-500">
          Battery: ${d.battery_level ?? '—'}%
        </span>
        <span class="text-sm text-gray-500">
          Last Ping: ${d.last_ping 
            ? new Date(d.last_ping).toLocaleString() 
            : '—'}
        </span>
      </div>
    `;
    list.appendChild(li);
  });
});
