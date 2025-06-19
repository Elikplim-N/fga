// js/map.js
import { supabase } from './supabaseClient.js';

// redirect if not logged in…
supabase.auth.getSession().then(({ data: { session } }) => {
  if (!session) window.location.href = 'login.html';
});

// initialize map with global L
const map = L.map('map').setView([0, 0], 2);
L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
  attribution: '&copy; OpenStreetMap contributors'
}).addTo(map);

(async () => {
  const { data: devices, error } = await supabase
    .from('devices')
    .select('name, latitude, longitude');
  if (error) return alert(error.message);

  devices.forEach((d) => {
    if (d.latitude && d.longitude) {
      L.marker([d.latitude, d.longitude])
        .addTo(map)
        .bindPopup(`<strong>${d.name}</strong>`);
    }
  });
})();
