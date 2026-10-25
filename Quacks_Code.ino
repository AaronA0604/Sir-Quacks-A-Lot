#include <Audio.h>
#include <audiolib_structs.hpp>
#include <psram_unique_ptr.hpp>

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

#include <ESP_I2S.h>
#include <WiFi.h>

#include "secrets.h"
#include "certs.h"

extern "C" {
#include "fvad.h"
}

I2SClass i2s;
Fvad *vad;
Audio audio;

const char *SYSTEM_PROMPT = "You are a voice assistant. Reply in one or two short sentences, plain text only, no markdown, no lists, no emojis. Reply with one question that helps the user figure out their problem. Only give answers if the user says Pineapple.";

const int MAX_SECS = 10;
const int MAX_SAMPLES = MAX_SECS * 16000;
const int SAMPLES_NEEDED = 320;                             // 20 ms at 16 kHz
const size_t RAW_BYTES = SAMPLES_NEEDED * sizeof(int32_t);  // 1280 bytes
int32_t raw[SAMPLES_NEEDED];                                // what the mic delivers (32-bit)
int16_t frame[SAMPLES_NEEDED];                              // what the VAD needs (16-bit)
int16_t *recording = nullptr;
size_t recSamples = 0;

const int HANGOVER_FRAMES = 15;  // 15 x 20 ms = 300 ms
int silenceCount = 0;
bool speaking = false;

// Return true when a full frame is ready, false if not yet.
bool readMicFrame(int16_t *buf, int n) {
  if (i2s.available() < (int)RAW_BYTES) return false;

  i2s.readBytes((char *)raw, RAW_BYTES);

  for (int i = 0; i < n; i++) {
    buf[i] = (int16_t)(raw[i] >> 16);  // keep the top 16 bits
  }
  return true;
}

void flushMic() {
  while (i2s.available() > 0) {
    size_t n = i2s.available();
    if (n > RAW_BYTES) n = RAW_BYTES;  // never read more than raw[] can hold
    i2s.readBytes((char *)raw, n);     // read it and ignore it
  }
}

void appendToRecording(const int16_t *f, int n) {
  if (recSamples + n > MAX_SAMPLES) return;
  memcpy(recording + recSamples, f, n * sizeof(int16_t));
  recSamples += n;
}

void writeWavHeader(uint8_t *h, uint32_t dataBytes) {
  memcpy(h, "RIFF", 4);
  uint32_t x1 = dataBytes + 36;
  memcpy(h + 4, &x1, 4);
  memcpy(h + 8, "WAVE", 4);
  memcpy(h + 12, "fmt ", 4);
  uint32_t x2 = 16;
  memcpy(h + 16, &x2, 4);
  uint16_t x3 = 1;
  memcpy(h + 20, &x3, 2);
  memcpy(h + 22, &x3, 2);
  uint32_t x4 = 16000;
  memcpy(h + 24, &x4, 4);
  uint32_t x5 = x4 * 2;
  memcpy(h + 28, &x5, 4);
  uint16_t x6 = 2;
  memcpy(h + 32, &x6, 2);
  uint16_t x7 = 16;
  memcpy(h + 34, &x7, 2);
  memcpy(h + 36, "data", 4);
  memcpy(h + 40, &dataBytes, 4);
}

uint8_t *buildWav(const int16_t *samples, size_t count) {
  uint32_t dataBytes = count * sizeof(int16_t);
  uint8_t *wav = (uint8_t *)ps_malloc(44 + dataBytes);

  if (wav == NULL) {
    Serial.println("ps_malloc failed");
    return nullptr;
  }

  writeWavHeader(wav, dataBytes);

  memcpy(wav + 44, samples, dataBytes);

  return wav;
}

const char *BOUNDARY = "B123";

String buildHead() {
  String b = BOUNDARY;
  return "--" + b + "\r\n"
                    "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
                    "whisper-large-v3-turbo\r\n"
                    "--"
         + b + "\r\n"
               "Content-Disposition: form-data; name=\"file\"; filename=\"speech.wav\"\r\n"
               "Content-Type: audio/wav\r\n\r\n";
}

String buildTail() {
  String b = BOUNDARY;
  return "\r\n--" + b + "--\r\n";
}

String sendToGroq() {
  if (recSamples == 0) return "";

  uint8_t *wav = buildWav(recording, recSamples);
  if (!wav) return "";

  String head = buildHead();
  String tail = buildTail();
  size_t wavSize = 44 + recSamples * sizeof(int16_t);
  size_t bodySize = head.length() + wavSize + tail.length();

  uint8_t *body = (uint8_t *)ps_malloc(bodySize);
  if (!body) {
    free(wav);
    return "";
  }

  memcpy(body, head.c_str(), head.length());
  memcpy(body + head.length(), wav, wavSize);
  memcpy(body + head.length() + wavSize,
         tail.c_str(), tail.length());

  String transcription = "";

  WiFiClientSecure client;
  client.setCACert(GTS_ROOT_R4);

  HTTPClient http;
  http.setTimeout(15000);
  if (http.begin(client,
                 "https://api.groq.com/openai/v1/audio/transcriptions")) {
    http.addHeader("Authorization", String("Bearer ") + GROQ_API_KEY);
    http.addHeader("Content-Type",
                   "multipart/form-data; boundary=" + String(BOUNDARY));

    int status = http.POST(body, bodySize);

    if (status == 200) {
      String response = http.getString();

      JsonDocument doc;
      if (deserializeJson(doc, response) == DeserializationError::Ok) {
        transcription = doc["text"].as<String>();
      }
    } else if (status > 0) {
      Serial.println(http.getString());
    } else {
      Serial.println(http.errorToString(status));
    }

    http.end();
  }

  free(body);
  free(wav);

  return transcription;
}

String askLLM(const String &userMessage) {
  if (userMessage.length() == 0) return "";

  // Build the request body
  JsonDocument req;
  req["model"] = "anthropic/claude-haiku-5.5";
  req["max_tokens"] = 60;  // hard cap on length
  req["temperature"] = 0.6;

  JsonArray messages = req["messages"].to<JsonArray>();

  JsonObject sys = messages.add<JsonObject>();
  sys["role"] = "system";
  sys["content"] = SYSTEM_PROMPT;

  JsonObject usr = messages.add<JsonObject>();
  usr["role"] = "user";
  usr["content"] = userMessage;  // ArduinoJson escapes it for you

  String body;
  serializeJson(req, body);

  String reply = "";

  WiFiClientSecure client;
  client.setCACert(GTS_ROOT_R4);

  HTTPClient http;
  http.setTimeout(30000);  // LLM calls can be slow

  if (http.begin(client, "https://ai.hackclub.com/proxy/v1/chat/completions")) {
    http.addHeader("Authorization", String("Bearer ") + HC_API_KEY);
    http.addHeader("Content-Type", "application/json");

    int status = http.POST(body);

    if (status == 200) {
      String response = http.getString();

      JsonDocument doc;
      if (deserializeJson(doc, response) == DeserializationError::Ok) {
        reply = doc["choices"][0]["message"]["content"].as<String>();
      }
    } else if (status > 0) {
      Serial.println(http.getString());
    } else {
      Serial.println(http.errorToString(status));
    }

    http.end();
  }

  // Model may include <think>...</think> reasoning; strip it
  int end = reply.indexOf("</think>");
  if (end >= 0) reply = reply.substring(end + 8);
  reply.trim();

  return reply;
}

void speak(const String &text) {
  if (text.length() == 0) return;
  audio.connecttospeech(text.c_str(), "en");
  unsigned long start = millis();
  while (audio.isRunning() && millis() - start < 20000) {
    audio.loop();
    vTaskDelay(1);
  }
}

void onSpeechStart() {
  recSamples = 0;
}

void onSpeechEnd() {
  if (recSamples < 8000) return;

  String userMessage = sendToGroq();
  Serial.println("You: " + userMessage);

  String reply = askLLM(userMessage);
  Serial.println("AI: " + reply);

  speak(reply);

  flushMic();
  fvad_reset(vad);
  silenceCount = 0;
}

void setup() {
  recording = (int16_t *)ps_malloc(MAX_SAMPLES * sizeof(int16_t));

  Serial.begin(115200);

  WiFi.begin(WIFI_NAME, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    now = time(nullptr);
  }

  vad = fvad_new();

  if (vad == NULL) {
    Serial.println("fvad_new failed");
    while (1)
      ;  // halt
  }

  if (recording == NULL) {
    Serial.println("ps_malloc failed");
    while (1)
      ;  // halt
  }

  fvad_set_mode(vad, 2);
  fvad_set_sample_rate(vad, 16000);

  i2s.setPins(14, 17, -1, 18);

  if (!i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
    while (1)
      ;  // Halt if initialization fails
  }

  audio.setPinout(5, 6, 7);
  audio.setVolume(8);
}

void loop() {
  if (!readMicFrame(frame, SAMPLES_NEEDED)) return;

  int r = fvad_process(vad, frame, SAMPLES_NEEDED);

  if (r == 1) {
    silenceCount = 0;
    if (!speaking) {
      speaking = true;
      onSpeechStart();
    }
  } else if (r == 0) {
    if (speaking && ++silenceCount >= HANGOVER_FRAMES) {
      speaking = false;
      onSpeechEnd();
    }
  }

  if (speaking == true) {
    appendToRecording(frame, SAMPLES_NEEDED);
  }
}