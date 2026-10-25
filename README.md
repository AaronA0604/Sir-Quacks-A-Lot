# Sir-Quacks-A-Lot
I made Sir Quacks-A-Lot because I know that the method of rubber duck debugging is really helpful for me, and I wanted to take it to the next level. Sir Quacks-A-Lot is a device that listens to the user talking through their problem, and then replies to them in short and prompting sentences to help the user figure out their problem.

This device works in a few simple steps. First it has an mic that 1. detects if you are speaking using a VAD model, and 2. if you are, records what you until you stop talking. It stores your recording on a microSD card. The next step is to translate the recording to text. It does this using an API key to access groq_whisper-large-v3-turbo. Then groq outputs the text and the device sends the text through an API key to anthropic/claude-haiku-5.5. This LLM then reads the text and replies in a short and prompting message. The final step is a text to speech program that turns the reply into audio. This audio is then played through the amp and speaker, so the user can hear it.

This is the schematic:
<img width="955" height="676" alt="Screenshot 2026-10-25 at 5 13 49 PM" src="https://github.com/user-attachments/assets/e44396e5-3024-4782-9cfd-b81c8cf67e5a" />

This is the pcb. I shaped it like a duck to keep with the theme, but you can shape it however you like:
<img width="734" height="701" alt="Screenshot 2026-10-25 at 5 14 18 PM" src="https://github.com/user-attachments/assets/ed57453d-4464-4415-bad3-ba378495c524" />

Here are the parts you need:
# Bill of Materials (BOM)

| Item | Qty | Supplier | Cost/item | Total Cost | Link |
|---|---:|---|---:|---:|---|
| ESP32-S3-DevKitC-1 (N8R8 version) | 1 | wottzentic | $18.99 | $18.99 | [Amazon](https://www.amazon.com/ESP32-S3-DevKitC-1-N8R8-Dual-Core-ESP32-S3-Arduino-ESP-IDF/dp/B0H6MHKJXN/) |
| MAX98357A I2S amplifier breakout module | 1 | GODIYMODULES | $6.88 | $6.88 | [Amazon](https://www.amazon.com/MAX98357-MAX98357A-Amplifier-Interface-Raspberry/dp/B0DPJRLMDJ/) |
| 4 Ohm 40mm Diameter 3W Full Range Audio Speaker | 1 | Gikfun | $8.99 | $8.99 | [Amazon](https://www.amazon.com/Gikfun-Speaker-Stereo-Loudspeaker-Arduino/dp/B01LN8ONG4/) |
| Micro SD Flash Memory Card 16GB | 1 | Patriot Memory | $9.99 | $9.99 | [Amazon](https://www.amazon.com/Patriot-Micro-Flash-Memory-Card/dp/B08KSTKGVX/) |
| Micro SD card reader module | 1 | WWZMDiB | $5.99 | $5.99 | [Amazon](https://www.amazon.com/WWZMDiB-Adater-Module-Support-Arduino/dp/B0B779R5TZ/) |
| INMP441 I2S microphone module | 1 | JESSINIE | $6.95 | $6.95 | [Amazon](https://www.amazon.com/JESSINIE-INMP441-Omnidirectional-Microphone-Module/dp/B0F9451N2L/) |
| Estimated total shipping | 1 | — | $0.00 | $0.00 | — |

## Total Cost

**$57.79** (excluding any additional taxes or fees)
