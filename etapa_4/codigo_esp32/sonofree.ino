#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>

const char* SSID_WIFI = "SSID";
const char* SENHA_WIFI = "SENHA";

const char* SERVIDOR_MQTT = "IP_do_servidor_MQTT";
const uint16_t PORTA_MQTT = 1883;
const char* USUARIO_MQTT = "mqtt_user";
const char* SENHA_MQTT = "mqtt_password";

const char* TOPIC_CMD_MODO = "home/sensor/cmd";
const char* TOPIC_CMD_LUZ_PRES = "home/sensor/luz/cmd/presenca";
const char* TOPIC_CMD_LUZ_MAN = "home/sensor/luz/cmd/manual";
const char* TOPIC_CMD_LUZ_HORA = "home/sensor/luz/cmd/hora";
const char* TOPIC_PRESENCA_STATE = "home/sensor/presenca/state";
const char* TOPIC_TEMPO_SET = "home/sensor/presenca/tempo_s/set";
const char* TOPIC_TEMPO_STATE = "home/sensor/presenca/tempo_s/state";

const int PINO_PRESENCA = 4;
const int PINO_RELE = 1;
const bool RELE_ATIVO_ALTO = true;

enum Modo { MODO_PRESENCA, MODO_MANUAL, MODO_HORA };
volatile Modo modo_atual = MODO_PRESENCA;

String lamp_presenca = "OFF";
String lamp_manual = "OFF";
String lamp_hora = "OFF";
String estado_lampada = "OFF";

volatile bool estado_entrada_isr = false;
volatile bool flag_borda_subida = false;
volatile bool flag_borda_descida = false;

bool saida_virtual = false;
bool contando_tempo = false;
uint32_t instante_inicio = 0;

uint32_t tempo_ativo_seg = 10;
const uint32_t TEMPO_MIN = 1, TEMPO_MAX = 600;

volatile uint32_t ultimo_pulso_us = 0;
const uint32_t DEBOUNCE_US = 5000;
const uint32_t QUALIFICACAO_MS = 50;
bool aguardando_qualificacao = false;
uint32_t t_inicio_alta_ms = 0;

const unsigned long INTERVALO_PRINT_MS = 1000;
unsigned long t_ultimo_print = 0;

WiFiClient cliente_wifi;
PubSubClient mqtt(cliente_wifi);
Preferences preferencias;

const char* modo_para_str(Modo m) {
  switch (m) {
    case MODO_PRESENCA: return "Presença";
    case MODO_MANUAL: return "Manual";
    case MODO_HORA: return "Hora";
    default: return "Desconhecido";
  }
}

void aplicar_saida_lampada() {
  if (PINO_RELE < 0) return;
  bool ligar = (estado_lampada == "ON");
  if (!RELE_ATIVO_ALTO) ligar = !ligar;
  digitalWrite(PINO_RELE, ligar ? HIGH : LOW);
}

void publicar_tempo_state(bool retencao = true) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)tempo_ativo_seg);
  mqtt.publish(TOPIC_TEMPO_STATE, buf, retencao);
  Serial.printf("[CFG] tempoAtivoSeg=%lus publicado em %s, retain=%s\n",
                (unsigned long)tempo_ativo_seg, TOPIC_TEMPO_STATE, retencao ? "true" : "false");
}

void tratar_tempo_set(const String& payload) {
  long v = payload.toInt();
  if (v <= 0 && payload != "0") {
    Serial.printf("[CFG] Payload invalido em %s: %s\n", TOPIC_TEMPO_SET, payload.c_str());
    return;
  }
  uint32_t novo = constrain((uint32_t)v, TEMPO_MIN, TEMPO_MAX);
  tempo_ativo_seg = novo;
  preferencias.putUInt("tempo_s", tempo_ativo_seg);
  publicar_tempo_state(true);
}

void IRAM_ATTR isr_presenca() {
  uint32_t agora_us = micros();
  if ((agora_us - ultimo_pulso_us) < DEBOUNCE_US) return;
  ultimo_pulso_us = agora_us;

  bool estado_atual = digitalRead(PINO_PRESENCA);
  static bool ultimo_estado = estado_atual;

  if (estado_atual && !ultimo_estado) flag_borda_subida = true;
  else if (!estado_atual && ultimo_estado) flag_borda_descida = true;

  estado_entrada_isr = estado_atual;
  ultimo_estado = estado_atual;
}

void ao_receber_mqtt(char* topico, byte* payload, unsigned int tamanho) {
  String msg; msg.reserve(tamanho);
  for (unsigned int i = 0; i < tamanho; i++) msg += (char)payload[i];
  msg.trim();

  if (strcmp(topico, TOPIC_CMD_MODO) == 0) {
    if (msg.equalsIgnoreCase("Presença") || msg.equalsIgnoreCase("Presenca")) modo_atual = MODO_PRESENCA;
    else if (msg.equalsIgnoreCase("Manual")) modo_atual = MODO_MANUAL;
    else if (msg.equalsIgnoreCase("Hora")) modo_atual = MODO_HORA;
    else { Serial.printf("[CMD] Payload invalido, modo: %s\n", msg.c_str()); return; }
    Serial.printf("[CMD] Modo alterado para %s\n", modo_para_str(modo_atual));
    if (modo_atual == MODO_PRESENCA) estado_lampada = lamp_presenca;
    if (modo_atual == MODO_MANUAL) estado_lampada = lamp_manual;
    if (modo_atual == MODO_HORA) estado_lampada = lamp_hora;
    aplicar_saida_lampada();
    return;
  }

  auto tratar_cmd_luz = [&](const char* topico_alvo, Modo modo, String& lamp_modo) -> bool {
    if (strcmp(topico, topico_alvo) != 0) return false;
    if (msg.equalsIgnoreCase("ON") || msg.equalsIgnoreCase("OFF")) {
      lamp_modo = msg;
      if (modo_atual == modo) {
        estado_lampada = msg;
        aplicar_saida_lampada();
        Serial.printf("[CMD] Luz %s aplicada no modo %s\n", estado_lampada.c_str(), modo_para_str(modo));
      } else {
        Serial.printf("[CMD] Luz %s registrada no modo %s (ignorada por enquanto)\n", lamp_modo.c_str(), modo_para_str(modo));
      }
    } else {
      Serial.printf("[CMD] Payload invalido, luz: %s\n", msg.c_str());
    }
    return true;
  };

  if (tratar_cmd_luz(TOPIC_CMD_LUZ_PRES, MODO_PRESENCA, lamp_presenca)) return;
  if (tratar_cmd_luz(TOPIC_CMD_LUZ_MAN, MODO_MANUAL, lamp_manual)) return;
  if (tratar_cmd_luz(TOPIC_CMD_LUZ_HORA, MODO_HORA, lamp_hora)) return;

  if (strcmp(topico, TOPIC_TEMPO_SET) == 0) {
    tratar_tempo_set(msg);
    return;
  }
}

void conectar_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID_WIFI, SENHA_WIFI);
  Serial.print("WiFi");
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
    if (++tentativas > 120) { Serial.println("\nReiniciando por falta de WiFi"); ESP.restart(); }
  }
  Serial.print("\nIP: "); Serial.println(WiFi.localIP());
}

void conectar_mqtt() {
  mqtt.setServer(SERVIDOR_MQTT, PORTA_MQTT);
  mqtt.setBufferSize(256);
  mqtt.setCallback(ao_receber_mqtt);

  while (!mqtt.connected()) {
    String cid = "esp32_" + String((uint32_t)esp_random(), HEX);
    Serial.print("MQTT, tentando: "); Serial.println(cid);
    if (mqtt.connect(cid.c_str(), USUARIO_MQTT, SENHA_MQTT)) {
      mqtt.subscribe(TOPIC_CMD_MODO);
      mqtt.subscribe(TOPIC_CMD_LUZ_PRES);
      mqtt.subscribe(TOPIC_CMD_LUZ_MAN);
      mqtt.subscribe(TOPIC_CMD_LUZ_HORA);
      mqtt.subscribe(TOPIC_TEMPO_SET);
      Serial.println("MQTT conectado e inscrito");
      mqtt.publish(TOPIC_PRESENCA_STATE, saida_virtual ? "ON" : "OFF", true);
      publicar_tempo_state(true);
    } else {
      Serial.print("Falha MQTT rc="); Serial.println(mqtt.state());
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  preferencias.begin("cfg", false);
  tempo_ativo_seg = preferencias.getUInt("tempo_s", 10);

  pinMode(PINO_PRESENCA, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(PINO_PRESENCA), isr_presenca, CHANGE);
  estado_entrada_isr = digitalRead(PINO_PRESENCA);

  if (PINO_RELE >= 0) {
    pinMode(PINO_RELE, OUTPUT);
    if (RELE_ATIVO_ALTO) digitalWrite(PINO_RELE, LOW);
    else digitalWrite(PINO_RELE, HIGH);
  }

  if (estado_entrada_isr) {
    aguardando_qualificacao = true;
    t_inicio_alta_ms = millis();
  }

  conectar_wifi();
  conectar_mqtt();

  Serial.println("Pronto.");
  Serial.printf("Inicial, Modo=%s, Lamp=%s, Presenca=%s, tempoAtivo=%lus\n",
                modo_para_str(modo_atual), estado_lampada.c_str(),
                saida_virtual ? "ON" : "OFF", (unsigned long)tempo_ativo_seg);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) conectar_wifi();
  if (!mqtt.connected()) conectar_mqtt();
  mqtt.loop();

  if (modo_atual == MODO_PRESENCA) {
    if (flag_borda_subida) {
      noInterrupts(); flag_borda_subida = false; interrupts();
      aguardando_qualificacao = true;
      t_inicio_alta_ms = millis();
      Serial.println("[PRESENCA] Borda subida, iniciando qualificacao de 50 ms");
    }

    if (flag_borda_descida) {
      noInterrupts(); flag_borda_descida = false; interrupts();
      if (aguardando_qualificacao) {
        aguardando_qualificacao = false;
        Serial.println("[PRESENCA] Borda descida durante qualificacao, cancelado");
      }
      contando_tempo = true;
      instante_inicio = millis();
      Serial.println("[PRESENCA] Borda descida, iniciando contagem para OFF");
    }

    if (aguardando_qualificacao) {
      if (!estado_entrada_isr) {
        aguardando_qualificacao = false;
      } else if ((millis() - t_inicio_alta_ms) >= QUALIFICACAO_MS && !saida_virtual) {
        saida_virtual = true;
        aguardando_qualificacao = false;
        contando_tempo = false;
        bool ok = mqtt.publish(TOPIC_PRESENCA_STATE, "ON", true);
        Serial.printf("[PRESENCA] Qualificacao OK, %lums -> ON, pub=%s\n",
                      (unsigned long)QUALIFICACAO_MS, ok ? "ok" : "falha");
        lamp_presenca = "ON";
        estado_lampada = "ON";
        aplicar_saida_lampada();
      }
    }

    if (contando_tempo && !estado_entrada_isr) {
      uint32_t decorrido = (millis() - instante_inicio) / 1000;
      if (decorrido >= tempo_ativo_seg) {
        saida_virtual = false;
        contando_tempo = false;
        bool ok = mqtt.publish(TOPIC_PRESENCA_STATE, "OFF", true);
        Serial.printf("[PRESENCA] Tempo expirou, %lus -> OFF, pub=%s\n",
                      (unsigned long)tempo_ativo_seg, ok ? "ok" : "falha");
        lamp_presenca = "OFF";
        estado_lampada = "OFF";
        aplicar_saida_lampada();
      }
    }

    if (contando_tempo && estado_entrada_isr) {
      contando_tempo = false;
      aguardando_qualificacao = true;
      t_inicio_alta_ms = millis();
      Serial.println("[PRESENCA] Retorno a HIGH durante contagem, recomecando qualificacao");
    }
  }

  unsigned long agora = millis();
  if (agora - t_ultimo_print >= INTERVALO_PRINT_MS) {
    t_ultimo_print = agora;
    Serial.printf("[STATUS] Modo=%s, Lamp=%s, Presenca=%s, tempoAtivo=%lus | CmdPres=%s, CmdMan=%s, CmdHora=%s\n",
                  modo_para_str(modo_atual), estado_lampada.c_str(),
                  saida_virtual ? "ON" : "OFF", (unsigned long)tempo_ativo_seg,
                  lamp_presenca.c_str(), lamp_manual.c_str(), lamp_hora.c_str());
  }
}
