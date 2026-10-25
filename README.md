# Sir-Quacks-A-Lot
I made Sir Quacks-A-Lot because I know that the method of rubber duck debugging is really helpful for me, and I wanted to take it to the next level. Sir Quacks-A-Lot is a device that listens to the user talking through their problem, and then replies to them in short and prompting sentences to help the user figure out their problem.

This device works in a few simple steps. First it has an mic that 1. detects if you are speaking using a VAD model, and 2. if you are, records what you until you stop talking. It stores your recording on a microSD card. The next step is to translate the recording to text. It does this using an API key to access groq_whisper-large-v3-turbo. Then groq outputs the text and the device sends the text through an API key to anthropic/claude-haiku-5.5. This LLM then reads the text and replies in a short and prompting message. The final step is a text to speech program that turns the reply into audio. This audio is then played through the amp and speaker, so the user can hear it.

This is the schematic:
<img width="955" height="676" alt="Screenshot 2026-10-25 at 5 13 49 PM" src="https://github.com/user-attachments/assets/e44396e5-3024-4782-9cfd-b81c8cf67e5a" />

This is the pcb. I shaped it like a duck to keep with the theme, but you can shape it however you like:
<img width="734" height="701" alt="Screenshot 2026-10-25 at 5 14 18 PM" src="https://github.com/user-attachments/assets/ed57453d-4464-4415-bad3-ba378495c524" />

Here are the parts you need:
