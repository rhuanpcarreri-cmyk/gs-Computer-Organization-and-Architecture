#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// ── Pinos ──────────────────────────────────────────────────────────
#define DHT_PIN       4        // Sensor DHT22 (temperatura e umidade)
#define DHT_TYPE      DHT22
#define LDR_PIN       34       // Sensor LDR (luminosidade) – entrada analógica
#define VIBR_PIN      35       // Sensor de vibração (SW-420) – entrada digital
#define LED_VERDE     25       // LED verde  → tudo normal
#define LED_AMARELO   26       // LED amarelo → atenção
#define LED_VERMELHO  27       // LED vermelho → alerta crítico
#define BUZZER_PIN    32       // Buzzer para alarme sonoro

// ── Limiares ───────────────────────────────────────────────────────
#define TEMP_CRITICA      35.0   // °C → acima = alerta
#define TEMP_ATENCAO      28.0   // °C → acima = atenção
#define LUZ_CRITICA       200    // valor ADC → abaixo = muito escuro (falha iluminação)
#define LUZ_ATENCAO       800    // valor ADC → abaixo = atenção

// ── Instâncias ──────────────────────────────────────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);   // Endereço I2C padrão do LCD 16x2
DHT dht(DHT_PIN, DHT_TYPE);

// ── Variáveis globais ───────────────────────────────────────────────
float temperatura    = 0.0;
float umidade        = 0.0;
int   luminosidade   = 0;
bool  vibracao       = false;
unsigned long ultimaLeitura = 0;
const unsigned long INTERVALO = 2000;   // 2 s entre leituras

// ── Enumeração de status ────────────────────────────────────────────
enum Status { NORMAL, ATENCAO, CRITICO };
Status statusAtual = NORMAL;

// ── Protótipos ──────────────────────────────────────────────────────
void lerSensores();
void avaliarStatus();
void atualizarLEDs();
void exibirDisplay(int tela);
void dispararAlarme();
void serialLog();

// ===================================================================
void setup() {
  Serial.begin(115200);
  Serial.println(F("=== CAPSULA ESPACIAL – BOOT ==="));

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(F("  CAPSULA FIAP  "));
  lcd.setCursor(0, 1);
  lcd.print(F("  Inicializando "));
  delay(2000);
  lcd.clear();

  // DHT
  dht.begin();

  // Pinos de saída
  pinMode(LED_VERDE,    OUTPUT);
  pinMode(LED_AMARELO,  OUTPUT);
  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(BUZZER_PIN,   OUTPUT);

  // Pinos de entrada
  pinMode(VIBR_PIN, INPUT);

  // Feedback visual de boot OK
  digitalWrite(LED_VERDE, HIGH);
  delay(500);
  digitalWrite(LED_VERDE, LOW);
  Serial.println(F("Sistema pronto."));
}

// ===================================================================
void loop() {
  unsigned long agora = millis();

  if (agora - ultimaLeitura >= INTERVALO) {
    ultimaLeitura = agora;

    lerSensores();
    avaliarStatus();
    atualizarLEDs();
    serialLog();
  }

  // Alterna entre 2 telas do LCD a cada 3 s
  static unsigned long trocaTela = 0;
  static int telAtual = 0;
  if (agora - trocaTela >= 3000) {
    trocaTela = agora;
    telAtual = (telAtual + 1) % 3;
    exibirDisplay(telAtual);
  }

  // Alarme sonoro apenas em estado CRITICO
  if (statusAtual == CRITICO) {
    dispararAlarme();
  }
}

// ===================================================================
// Lê todos os sensores
void lerSensores() {
  // DHT22 – temperatura e umidade
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) temperatura = t;
  if (!isnan(h)) umidade     = h;

  // LDR – valor analógico 0-4095 (ADC 12-bit do ESP32)
  luminosidade = analogRead(LDR_PIN);

  // SW-420 – vibração (HIGH = vibrando)
  vibracao = digitalRead(VIBR_PIN);
}

// ===================================================================
// Determina o status global com base nos limares
void avaliarStatus() {
  statusAtual = NORMAL;

  if (temperatura >= TEMP_CRITICA || luminosidade < LUZ_CRITICA || vibracao) {
    statusAtual = CRITICO;
  } else if (temperatura >= TEMP_ATENCAO || luminosidade < LUZ_ATENCAO) {
    statusAtual = ATENCAO;
  }
}

// ===================================================================
// Liga o LED correspondente ao status
void atualizarLEDs() {
  digitalWrite(LED_VERDE,    statusAtual == NORMAL   ? HIGH : LOW);
  digitalWrite(LED_AMARELO,  statusAtual == ATENCAO  ? HIGH : LOW);
  digitalWrite(LED_VERMELHO, statusAtual == CRITICO  ? HIGH : LOW);
}

// ===================================================================
// Exibe dados no LCD alternando entre 3 telas
void exibirDisplay(int tela) {
  lcd.clear();

  switch (tela) {
    case 0:  // Tela 1 – Temperatura e Umidade
      lcd.setCursor(0, 0);
      lcd.print(F("TEMP: "));
      lcd.print(temperatura, 1);
      lcd.print(F(" C"));

      lcd.setCursor(0, 1);
      lcd.print(F("UMID: "));
      lcd.print(umidade, 1);
      lcd.print(F(" %"));
      break;

    case 1:  // Tela 2 – Luminosidade e Vibração
      lcd.setCursor(0, 0);
      lcd.print(F("LUZ:  "));
      lcd.print(luminosidade);

      lcd.setCursor(0, 1);
      lcd.print(F("VIB: "));
      lcd.print(vibracao ? F("ALERTA!   ") : F("OK        "));
      break;

    case 2:  // Tela 3 – Status geral
      lcd.setCursor(0, 0);
      lcd.print(F("STATUS CAPSULA: "));
      lcd.setCursor(0, 1);
      switch (statusAtual) {
        case NORMAL:  lcd.print(F("[OK] NOMINAL    ")); break;
        case ATENCAO: lcd.print(F("[!] ATENCAO     ")); break;
        case CRITICO: lcd.print(F("[X] CRITICO!!!  ")); break;
      }
      break;
  }
}

// ===================================================================
// Dispara buzzer em pulsos curtos (estado crítico)
void dispararAlarme() {
  static unsigned long ultimoBeep = 0;
  unsigned long agora = millis();
  if (agora - ultimoBeep >= 800) {
    ultimoBeep = agora;
    tone(BUZZER_PIN, 1000, 200);
  }
}

// ===================================================================
// Envia leitura para o Monitor Serial em formato legível
void serialLog() {
  Serial.println(F("-------------------------------"));
  Serial.print(F("Temperatura : ")); Serial.print(temperatura); Serial.println(F(" °C"));
  Serial.print(F("Umidade     : ")); Serial.print(umidade);     Serial.println(F(" %"));
  Serial.print(F("Luminosidade: ")); Serial.println(luminosidade);
  Serial.print(F("Vibracao    : ")); Serial.println(vibracao ? F("DETECTADA") : F("NENHUMA"));

  Serial.print(F("Status      : "));
  switch (statusAtual) {
    case NORMAL:  Serial.println(F("NORMAL"));  break;
    case ATENCAO: Serial.println(F("ATENCAO")); break;
    case CRITICO: Serial.println(F("CRITICO")); break;
  }
}
