# ForestGuardAI

ForestGuardAI is an integrated forest monitoring system designed to detect and prevent **illegal logging in forest reserves**. It combines IoT sensor nodes, a LoRaWAN gateway, and a web dashboard powered by Supabase. The system enables real-time alerting, device management, and team coordination to protect forests from unauthorized activities.

---

## Features

- **IoT Sensor Nodes**: Edge ML for event detection (e.g., chainsaw sound, vibration, intrusion), LoRa uplink.
- **LoRa Gateway**: Receives node data, relays to Supabase, sends SMS alerts via SIM800L.
- **Web Dashboard**: Device/alert management, team deployments, map view, notifications, and reports.
- **User Authentication**: Supabase Auth for secure access.
- **Illegal Logging Detection**: Real-time alerts for suspicious activities such as chainsaw operation or unauthorized entry.

---

## Repository Structure

```
Firmware/
  FG_GATEWAY.ino         # ESP32 LoRa gateway firmware
  fg_node1.ino           # Example sensor node firmware
public/
  *.html                 # Web dashboard pages
  css/styles.css         # Custom styles
  js/                    # Frontend JS (Supabase, auth, dashboard, etc.)
  sidebar.html           # Sidebar navigation (injected)
  back2.jpeg, bck.jpeg   # Background images
```

---

## Getting Started

### 1. Firmware

- **Gateway**: Flash `Firmware/FG_GATEWAY.ino` to an ESP32 with LoRa and SIM800L modules.
  - Configure Wi-Fi, Supabase, and SMS credentials at the top of the file.
- **Node**: Flash `Firmware/fg_node1.ino` to your sensor node (e.g., Arduino Nano 33 BLE Sense).
  - Edge Impulse model integration for event detection.

### 2. Web Dashboard

- **Frontend**: All files in `public/` can be served statically (e.g., with [Live Server](https://marketplace.visualstudio.com/items?itemName=ritwickdey.LiveServer) or any HTTP server).
- **Supabase**: 
  - Create a [Supabase](https://supabase.com/) project.
  - Set up tables: `devices`, `alerts`, `teams`, `deployments`, `profiles`, `notifications`, `reports`.
  - Update API keys and URLs in `public/js/supabaseClient.js` and firmware as needed.

### 3. Usage

- **Login/Register**: Access via `index.html` → login/register.
- **Dashboard**: View device stats, alerts, team feed, and charts.
- **Devices**: Add, edit, or delete devices.
- **Alerts**: View recent alerts from sensor nodes.
- **Deployments**: Assign teams/devices to incidents.
- **Map**: Visualize device locations.
- **Notifications/Reports**: View and export system notifications and reports.

---

## Dependencies

- **Firmware**: Arduino libraries: `WiFi.h`, `HTTPClient.h`, `ArduinoJson.h`, `SPI.h`, `LoRa.h`
- **Web**: [Tailwind CSS](https://tailwindcss.com/), [Chart.js](https://www.chartjs.org/), [Supabase JS](https://supabase.com/docs/reference/javascript/installing)

---

## Customization

- Update Supabase credentials in:
  - [`public/js/supabaseClient.js`](public/js/supabaseClient.js)
  - [`Firmware/FG_GATEWAY.ino`](Firmware/FG_GATEWAY.ino)
- Change SMS recipient in `FG_GATEWAY.ino` (`SMS_RECIPIENT`).
- Adjust device/node firmware for your sensors and ML model.

---

## License

MIT License

---

## Authors

- [Elikplim Kofi Nyahe]
- [Sena Belinda Yekple]
- [Jurist Jenkins Smith]
- [Thomas Wewobane]

---

## Acknowledgements

- [Supabase](https://supabase.com/)
- [Edge Impulse](https://edgeimpulse.com/)
- [Chart.js](https://www.chartjs.org/)
- [Tailwind CSS](https://tailwindcss.com/)