# MeshRoof HomeChat Protocol Extensions

`meshroof` extends the base `HomeChat` protocol on ESP32/ESP32-S3 rooftop nodes, adding commands for WiFi station management, network routing/ping diagnostics, RF power amplifier (PA/LNA) control, acoustic signaling, and internal chip temperature monitoring.

---

## 1. WiFi & Networking (`wifi`, `net`)

| Command Syntax | Description | Example Reply |
| :--- | :--- | :--- |
| `wifi` | Query WiFi link state, SSID, RSSI, and IP address. | `wifi: status=connected ssid=HomeMesh rssi=-62 ip=192.168.1.150` |
| `net` | Query network interface status, gateway, and DNS. | `net: ip=192.168.1.150 gw=192.168.1.1 dns=1.1.1.1` |
| `net ping <host>` | Send ICMP ping echo to host or IP. | `net: ping=1.1.1.1 rtt=18ms` |

---

## 2. RF Power Amplifier Control (`amplify`)

Controls external Power Amplifier (PA) / Low Noise Amplifier (LNA) front-ends for high-power rooftop LoRa transceivers:

| Command Syntax | Description | Example Reply |
| :--- | :--- | :--- |
| `amplify [on\|off]` | Enable or bypass external RF power amplification. | `amplify: state=on` |
| `amplify gain <level>` | Adjust amplifier gain preset. | `amplify: gain=high` |
| `amplify` | Query current amplifier state and parameters. | `amplify: state=on gain=high pa=27dBm` |

---

## 3. Audio & Morse Signaling

| Command Syntax | Description | Example Reply |
| :--- | :--- | :--- |
| `buzz <freq> <duration_ms>` | Play buzzer tone at specified frequency and duration. | `buzz: freq=2000 dur=300` |
| `morse <text>` | Sound Morse code transmission on buzzer. | `morse: text='HI'` |

---

## 4. System & Diagnostics Extensions

- **`reset`**:
  - Displays ESP32 reset reason and uptime since reboot:
    ```text
    reset: count=3 reason=poweron secs_ago=7200
    ```
- **`env`**:
  - Appends ESP32 internal silicon junction temperature to environmental metrics:
    ```text
    env: temp=28.1 hum=60.4 press=1009.8 temp_chip=43.2
    ```
- **`status`**:
  - Reports operational status including rooftop solar/battery power supply conditions.

---

## 5. HomeMesh Auto-Discovery (`identify`)

`meshroof` reports machine-readable capabilities for `meshmon` / Home Assistant registration.

* **MeshMon probe**: `identify` (typically `!nodeid identify` or `all identify` on the robot channel).
  * HomeChat addressing (node hex, short name, long name, `meshroof`, or `all`) selects who replies.
* **Structured Response**:
  ```text
  identify: app=meshroof ver=2.1.4 hw=esp32s3 caps=amplify,wifi,net,cpu_temp,buzzer
  ```
* **HomeChat `rollcall [target]`**: still supported for human rollcall and still returns `rollcall: app=meshroof …`. MeshMon does not use `rollcall` for fleet probing.
