// js/deployments.js
import { supabase } from './supabaseClient.js';
import { loadSidebar } from './utils.js';

document.addEventListener('DOMContentLoaded', async () => {
  await loadSidebar();

  // Auth check
  const { data: { session } } = await supabase.auth.getSession();
  if (!session) return window.location.href = 'login.html';

  const alertSelect    = document.querySelector('#deploy-team-form select[name="alert_id"]');
  const teamSelect     = document.querySelector('#deploy-team-form select[name="team_id"]');
  const deviceSelect   = document.querySelector('#deploy-device-form select[name="device_id"]');
  const deployListEl   = document.getElementById('deployment-list');

  // Load options
  const [{ data: alerts }, { data: teams }, { data: devices }] =
    await Promise.all([
      supabase.from('alerts').select('id,alert_type,created_at').eq('resolved', false),
      supabase.from('teams').select('id,name,status'),
      supabase.from('devices').select('id,name,device_id')
    ]);

  alerts.forEach(a =>
    alertSelect.innerHTML += `<option value="${a.id}">
      [${new Date(a.created_at).toLocaleDateString()}] ${a.alert_type}
    </option>`
  );
  teams.forEach(t =>
    teamSelect.innerHTML += `<option value="${t.id}">
      ${t.name} (${t.status})
    </option>`
  );
  devices.forEach(d =>
    deviceSelect.innerHTML += `<option value="${d.id}">
      ${d.name} (${d.device_id})
    </option>`
  );

  // Handle team deployment
  document.getElementById('deploy-team-form').addEventListener('submit', async e => {
    e.preventDefault();
    const { alert_id, team_id } = Object.fromEntries(new FormData(e.target).entries());
    const { error } = await supabase.from('deployments').insert({
      alert_id, team_id
    });
    if (error) return alert('Deploy failed: ' + error.message);
    loadDeployments();
  });

  // Handle device deployment
  document.getElementById('deploy-device-form').addEventListener('submit', async e => {
    e.preventDefault();
    const { device_id, location } = Object.fromEntries(new FormData(e.target).entries());
    const { error } = await supabase.from('deployments').insert({
      device_id, notes: location
    });
    if (error) return alert('Record failed: ' + error.message);
    loadDeployments();
  });

  // Load existing deployments
  async function loadDeployments() {
    const { data: deps, error } = await supabase
      .from('deployments')
      .select(`
        id, status, created_at,
        alerts(alert_type), teams(name), devices(name)
      `)
      .order('created_at', { ascending: false });

    if (error) {
      deployListEl.innerHTML = `<li class="text-red-500">Error: ${error.message}</li>`;
      return;
    }
    if (!deps.length) {
      deployListEl.innerHTML = `<li class="text-gray-500">No deployments yet.</li>`;
      return;
    }

    deployListEl.innerHTML = '';
    deps.forEach(d => {
      const li = document.createElement('li');
      li.className = 'bg-white rounded-xl shadow p-4 flex justify-between items-center';
      li.innerHTML = `
        <div>
          <div class="font-semibold">
            ${d.teams.name} → ${d.alerts?.alert_type || d.devices?.name}
          </div>
          <div class="text-xs text-gray-500">
            ${new Date(d.created_at).toLocaleString()}
          </div>
        </div>
        <div class="flex space-x-2">
          <button data-id="${d.id}" data-status="on-site"
                  class="px-2 py-1 bg-blue-600 text-white rounded change-status">
            On-site
          </button>
          <button data-id="${d.id}" data-status="completed"
                  class="px-2 py-1 bg-green-600 text-white rounded change-status">
            Complete
          </button>
          <button data-id="${d.id}" data-status="cancelled"
                  class="px-2 py-1 bg-red-600 text-white rounded change-status">
            Cancel
          </button>
        </div>
      `;
      deployListEl.appendChild(li);
    });

    // Attach status-change handlers
    document.querySelectorAll('.change-status').forEach(btn => {
      btn.onclick = async () => {
        const { error } = await supabase
          .from('deployments')
          .update({ status: btn.dataset.status })
          .eq('id', btn.dataset.id);
        if (error) return alert('Update failed: ' + error.message);
        loadDeployments();
      };
    });
  }

  // Initial load
  loadDeployments();
});
