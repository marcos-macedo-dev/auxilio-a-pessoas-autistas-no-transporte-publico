#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>

/* --- DEFINIÇÕES DE HARDWARE --- */
#define PINO_BUZZER 12
#define PINO_TRIG 27
#define PINO_ECHO 14

/* --- CONFIGURAÇÃO DO SENSOR DE DISTÂNCIA --- */
#define DISTANCIA_GATILHO 200 // cm para detectar parada
#define DISTANCIA_RESET   210 // cm para considerar "saiu da parada"

/* --- PERIFÉRICOS --- */
LiquidCrystal_I2C lcd(0x27, 16, 2);

/* --- CREDENCIAIS DE REDE (WIFI & MQTT) --- */
const char *ssidWifi = "Wokwi-GUEST";
const char *senhaWifi = "";

const char *servidorMqtt = "37c8059a614a4b1caf32c484a8ad7f41.s1.eu.hivemq.cloud";
const uint16_t portaMqtt = 8883;
const char *usuarioMqtt = "marquin";
const char *senhaMqtt   = "Senha123";
const char *topicoMqtt  = "onibus/telemetria";
const char *idClienteMqtt = "esp32-onibus-demo";

/* --- DADOS DA ROTA --- */
const int idOnibus = 101;
const int idRota = 7;

struct Parada {
    int id;
    const char *nome;
    float km;
    int min;
};

Parada listaParadas[] = {
    {1, "Praca Central",  1.2, 4},
    {2, "Av. Araripe",    0.9, 3},
    {3, "Mercado Munic.", 2.3, 7},
    {4, "EEMTI Carlota",  0.0, 0}
};
const size_t totalParadas = sizeof(listaParadas) / sizeof(listaParadas[0]);

/* --- VARIÁVEIS GLOBAIS --- */
WiFiClientSecure clienteSeguro;
PubSubClient mqtt(clienteSeguro);

int indiceParadaAtual = 0;
bool paradaDetectada = false;

// Controle de interface LCD
unsigned long ultimaTrocaLCD = 0;
bool telaPrincipalAtiva = true;

/* --- FUNÇÕES AUXILIARES --- */

void emitirBeep() {
    tone(PINO_BUZZER, 1100, 120);
    delay(120);
    noTone(PINO_BUZZER);
}

long lerDistancia() {
    digitalWrite(PINO_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PINO_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PINO_TRIG, LOW);
    
    long duracao = pulseIn(PINO_ECHO, HIGH, 30000);
    if (duracao == 0) return 0;
    return duracao * 0.034 / 2;
}

void atualizarLCD(String linha1, String linha2) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(linha1.substring(0, 16));
    lcd.setCursor(0, 1);
    lcd.print(linha2.substring(0, 16));
}

/* --- CONECTIVIDADE --- */

void conectarWifi() {
    WiFi.begin(ssidWifi, senhaWifi);
    Serial.print("Conectando WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    Serial.println(" OK!");
}

void conectarMqtt() {
    clienteSeguro.setInsecure(); // Aceita certificado auto-assinado (simulação)
    mqtt.setServer(servidorMqtt, portaMqtt);
    
    if (!mqtt.connected()) {
        Serial.print("Conectando MQTT... ");
        if (mqtt.connect(idClienteMqtt, usuarioMqtt, senhaMqtt)) {
            Serial.println("OK!");
        } else {
            Serial.print("Falha. Estado: ");
            Serial.println(mqtt.state());
        }
    }
}

void enviarTelemetria(size_t indiceAtual) {
    if (!mqtt.connected()) return;

    // Calcula o índice da PRÓXIMA parada
    size_t indiceProx = (indiceAtual + 1) % totalParadas;
    
    // Cria documento JSON
    StaticJsonDocument<384> doc;
    
    // Dados do Veículo
    doc["id_onibus"] = idOnibus;
    doc["id_rota"] = idRota;

    // Localização Atual
    doc["id_parada"] = listaParadas[indiceAtual].id;
    doc["nome_parada"] = listaParadas[indiceAtual].nome;

    // Previsão (Próxima Parada)
    doc["id_proxima_parada"] = listaParadas[indiceProx].id;
    doc["nome_proxima_parada"] = listaParadas[indiceProx].nome;
    
    // Estimativas até o destino
    doc["distancia_km"] = listaParadas[indiceProx].km;
    doc["tempo_min"] = listaParadas[indiceProx].min;

    char buffer[384];
    serializeJson(doc, buffer);
    
    if (mqtt.publish(topicoMqtt, buffer)) {
        Serial.println("📡 Telemetria enviada com sucesso.");
    } else {
        Serial.println("❌ Falha no envio MQTT.");
    }
}

/* --- LÓGICA DE INTERFACE (LCD) --- */

void gerenciarTelasLCD() {
    unsigned long agora = millis();

    // Alterna a tela a cada 3 segundos
    if (agora - ultimaTrocaLCD > 3000) {
        ultimaTrocaLCD = agora;
        telaPrincipalAtiva = !telaPrincipalAtiva;

        if (telaPrincipalAtiva) {
            // Tela 1: Local Atual
            atualizarLCD("ESTAMOS EM:", listaParadas[indiceParadaAtual].nome);
        } else {
            // Tela 2: Próximo Destino
            int prox = (indiceParadaAtual + 1) % totalParadas;
            String info = String(listaParadas[prox].km, 1) + "km " + String(listaParadas[prox].min) + "min";
            atualizarLCD("PROX: " + String(listaParadas[prox].nome), info);
        }
    }
}

/* --- SETUP & LOOP --- */

void setup() {
    Serial.begin(115200);

    // Configura Pinos
    pinMode(PINO_TRIG, OUTPUT);
    pinMode(PINO_ECHO, INPUT);
    pinMode(PINO_BUZZER, OUTPUT);

    // Inicializa LCD
    Wire.begin(21, 22);
    lcd.init();
    lcd.backlight();

    atualizarLCD("SISTEMA ONIBUS", "INICIANDO...");
    
    conectarWifi();
    conectarMqtt();

    // Mostra a primeira parada imediatamente
    atualizarLCD("ESTAMOS EM:", listaParadas[indiceParadaAtual].nome);
}

void loop() {
    // 1. Manutenção da Conexão
    if (!mqtt.connected()) {
        static long ultimaTentativa = 0;
        if (millis() - ultimaTentativa > 5000) {
            ultimaTentativa = millis();
            conectarMqtt();
        }
    }
    mqtt.loop();

    // 2. Interface LCD
    gerenciarTelasLCD();

    // 3. Sensores e Lógica de Parada
    long distancia = lerDistancia();

    if (distancia > 2 && distancia < DISTANCIA_GATILHO) {
        if (!paradaDetectada) {
            Serial.println(">>> PARADA DETECTADA <<<");
            paradaDetectada = true;

            // Avança para a próxima parada na lista
            indiceParadaAtual = (indiceParadaAtual + 1) % totalParadas;

            emitirBeep();
            enviarTelemetria(indiceParadaAtual);

            // Atualiza LCD imediatamente para "Estamos em..."
            telaPrincipalAtiva = true;
            ultimaTrocaLCD = millis();
            atualizarLCD("CHEGAMOS EM:", listaParadas[indiceParadaAtual].nome);
        }
    } else if (distancia > DISTANCIA_RESET || distancia == 0) {
        if (paradaDetectada) {
            Serial.println(">>> Veículo em movimento <<<");
            paradaDetectada = false;
        }
    }

    delay(100);
}