# Vault-Deck MK-I

**An IoT-based mechatronic telemetry station and handheld cyberdeck, built around an ESP32.**

I built Vault-Deck MK-I to answer one question: can I take a project from CAD to firmware to signal processing and keep all of it inside a single holdable, rigid enclosure? The result is a battery-powered handheld that reads environmental sensors in real time, streams cleaned-up telemetry over Wi-Fi to a MATLAB dashboard, talks to cloud APIs (Spotify, OpenWeather), and has a small game built in.

It is a deliberate mix of the three areas I care most about: mechanics, electronics, and software.

---

## Features

- **Vault-OS**: a 7-tab interface written in C++ for the ESP32 and a 2.4" SPI TFT (ILI9341). Tabs: `STAT`, `ENV`, `WAVE`, `RDO`, `DIAG`, `LGHT`, `GAME`.
- **Tab-based state machine**: each tab is a state with its own draw and update routine, and the D-pad moves between them. Timing for telemetry, API polling, weather refresh and game frames is handled with `millis()` timers.
- **Spotify Web API (OAuth 2.0)**: the deck acts as a remote media controller. It authenticates with a refresh token, shows the current track, artist and duration, and lets me skip or pause/resume. Between API polls, the track progress is predicted locally from the ESP32's clock, so the progress bar stays smooth without spamming Spotify's servers.
- **OpenWeather API**: fetches outdoor temperature and humidity for Istanbul every 10 minutes, parses the JSON with ArduinoJson, and shows it next to the onboard DHT22 indoor readings.
- **10 Hz UDP telemetry to MATLAB**: while the `DIAG` tab is active, the deck broadcasts microphone, potentiometer, temperature and light data to a custom oscilloscope-style MATLAB dashboard.
- **On-device signal processing**: see below.
- **Tactical flashlight**: the 10K potentiometer controls the PWM brightness of a yellow LED, with an automotive-style gauge drawn on the display.
- **Orbit Defense**: a small three-lane arcade game coded into the firmware, with a high score and a boost button.
- **Offline mode**: at boot I choose between a fast offline start and a Wi-Fi/API start. If the connection fails, the deck falls back to offline mode, and I can retry from the `ENV` tab.

## Signal processing

Raw analog sensor data is noisy, so the telemetry pipeline cleans it up before sending it:

| Problem | Fix |
|---|---|
| **ADC ghosting** when switching between analog pins on the ESP32 | A dummy read on the LDR pin, a short wait for the voltage to settle, then the real read |
| **Fast audio vs. a 100 ms telemetry cycle** | The KY-037 is sampled 50 times in a tight loop and the amplitude is taken as `micMax - micMin` (peak detection), then multiplied by a 15x software gain so spikes are clearly visible on the plot |
| **50 Hz AC flicker** from room lighting picked up by the LDR | An exponential moving average: `smoothedLDR = smoothedLDR * 0.85 + rawLDR * 0.15` |

Each packet is a simple comma-separated line: `mic,pot,temp,ldr`.

## Hardware

| Part | Role |
|---|---|
| ESP32 | Main microcontroller |
| 2.4" SPI TFT (ILI9341) | Display |
| DHT22 / AM2302 | Temperature and humidity |
| KY-037 | Sound sensor |
| LDR | Light sensor |
| 10K potentiometer | Analog dial / flashlight control |
| 5x tactile buttons | D-pad (up, down, left, right, OK) using the ESP32's internal `INPUT_PULLUP` |
| Yellow LED | Flashlight |
| LM2596 buck converter | 8.4 V to 5.0 V, manually trimmed |
| 2S LiPo + KCD11 rocker switch | Power source and on/off |

**Power:** the 8.4 V DC jack or the internal 2S LiPo goes through the rocker switch into the LM2596, which feeds the ESP32 with a stable 5.0 V. On the perfboard I built a "power-bus": two continuous horizontal solder bridges act as the main 3.3 V and GND rails, and the sensors and buttons share them. That kept the wiring clean and avoided voltage drops during Wi-Fi bursts.

## Mechanical design

The enclosure is designed entirely in SolidWorks as a 3-layer "sandwich" instead of a single hollow shell:

1. **Front faceplate**: the user interface layer. It has a chamfered window for the display, slotted screw holes for 0.5 mm positional calibration, and custom "volcanic" protrusions that absorb the depth of the screen and potentiometer without thickening the whole body.
2. **Mid-frame chassis ("the spine")**: the load-bearing layer. The 7x9 cm perfboard and the ESP32 are mounted here, so the wiring stays undisturbed when I replace the battery.
3. **Backplate**: holds the 2S LiPo and the buck converter. Vents at the top and smaller intakes at the bottom let cool air enter from below and push the hot air from the buck converter out through the top.

Design-for-3D-printing rules I followed:

- All wall thicknesses are multiples of the 0.4 mm nozzle diameter.
- Circular cutouts have a +0.2 to +0.3 mm offset (for example, the 7 mm potentiometer shaft) to compensate for plastic shrinkage.
- M2 screw standoffs have a minimum 5 mm outer diameter so they don't split when the screws are tightened.


## Repository structure

```
Vault-Deck/
├── src/                  # Vault-OS firmware
├── include/              # credentials template
├── docs/                 # project document and images
├── platformio.ini        # dependencies and display configuration
└── README.md
```

## Getting started

**Requirements:** [PlatformIO](https://platformio.org/) (VS Code extension or CLI), an ESP32 board, and the hardware listed above.

```bash
git clone https://github.com/anakinthewalking/vault-deck.git
cd vault-deck
```

**1. Credentials.** Copy `include/secrets.example.h` to `include/secrets.h` and fill in your Wi-Fi name and password, your PC's local IP and the UDP port, your OpenWeather API key, and your Spotify client ID, client secret and refresh token. `secrets.h` is listed in `.gitignore`, so it is never committed.

**2. Display configuration.** The TFT_eSPI setup (SPI pins, `ILI9341_DRIVER`, clock speed) is injected through `build_flags` in `platformio.ini`. I did it this way so I never have to modify the library files, and the project stays portable.

**3. Build and upload.**

```bash
pio run -t upload
pio device monitor
```

**4. MATLAB dashboard.** On the PC, run a MATLAB script that listens for UDP packets on the same port, then open the `DIAG` tab on the deck.

## Results

- The 3-layer layout kept the ESP32 and sensor wiring thermally isolated from the battery and buck converter.
- The power-bus perfboard removed the wiring clutter, and I saw no voltage drops during heavy Wi-Fi and display use.
- Telemetry ran at a steady 10 Hz over the local network with near-zero packet loss in my tests.
- Peak detection and the EMA filter gave clean, readable waveforms on the MATLAB side.

## What I learned

This project taught me that the hard part of mechatronics is the interfaces between disciplines: a screw standoff has to survive the torque of assembly, a buck converter's heat has to be vented by the CAD, and an ADC has to be given time to settle before any filtering can do something useful.

## Roadmap

- [ ] Custom PCB to replace the perfboard
- [ ] Data logging to the SD card
- [ ] Replace the remaining blocking `delay()` calls with fully `millis()`-based scheduling

## Author

**Emre Göral**, Mechatronics Engineering student at Yıldız Technical University, Istanbul.
GitHub: [@anakinthewalking](https://github.com/anakinthewalking)