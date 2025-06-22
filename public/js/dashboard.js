// js/dashboard.js
import { supabase } from './supabaseClient.js';

document.addEventListener('DOMContentLoaded', async () => {
  document.body.style.height = '200vh';
  // 1) Redirect if not logged in
  const { data: { session } } = await supabase.auth.getSession();
  if (!session) return window.location.href = 'login.html';

  // 2) Fetch summary counts in parallel
  const [
    { count: devicesCount },
    { count: alertsCount },
    { count: teamsCount }
  ] = await Promise.all([
    supabase.from('devices').select('*', { count: 'exact' }),
    supabase.from('alerts').select('*', { count: 'exact' }),
    supabase.from('teams').select('*', { count: 'exact' })
  ]);

  document.getElementById('devices-count').textContent = devicesCount;
  document.getElementById('alerts-count').textContent = alertsCount;
  document.getElementById('teams-count').textContent = teamsCount;

  // 3) Build Device Status Pie Chart
  const { data: devices } = await supabase.from('devices').select('status');
  const statusCounts = devices.reduce((acc, { status }) => {
    acc[status] = (acc[status] || 0) + 1;
    return acc;
  }, {});
  new Chart(document.getElementById('status-chart'), {
    type: 'pie',
    data: {
      labels: Object.keys(statusCounts),
      datasets: [{ data: Object.values(statusCounts) }]
    }
  });

  // 4) Build Alerts Over Time Line Chart
  const { data: alerts } = await supabase
    .from('alerts')
    .select('created_at');
  const dates = alerts.map(a => a.created_at.split('T')[0]);
  const countsByDate = dates.reduce((acc, date) => {
    acc[date] = (acc[date] || 0) + 1;
    return acc;
  }, {});
  new Chart(document.getElementById('alerts-chart'), {
    type: 'line',
    data: {
      labels: Object.keys(countsByDate),
      datasets: [{
        label: 'Alerts',
        data: Object.values(countsByDate),
        fill: false,
        tension: 0.1
      }]
    }
  });

  // 5) Populate Recent Activity (last 5 alerts)
  const { data: recent } = await supabase
    .from('alerts')
    .select('alert_type, created_at')
    .order('created_at', { ascending: false })
    .limit(5);
  const recentEl = document.getElementById('recent-activity');
  recent.forEach(a => {
    const li = document.createElement('li');
    li.className = 'flex items-center gap-2';
    li.innerHTML = `
      <span class="text-xs text-gray-500">${new Date(a.created_at).toLocaleTimeString()}</span>
      <span class="font-medium">${a.alert_type}</span>
    `;
    recentEl.appendChild(li);
  });

  // 6) Populate Team Feed (last 5 deployments)
  const { data: feed } = await supabase
    .from('deployments')
    .select('status, created_at')
    .order('created_at', { ascending: false })
    .limit(5);
  const feedEl = document.getElementById('team-feed');
  feed.forEach(d => {
    const li = document.createElement('li');
    li.className = 'flex items-center gap-2';
    li.innerHTML = `
      <span class="text-xs text-gray-500">${new Date(d.created_at).toLocaleDateString()}</span>
      <span>Status:</span>
      <span class="font-medium">${d.status}</span>
    `;
    feedEl.appendChild(li);
  });

  // 7) Sign-out is already handled in utils.js/sidebar.html
});
