// VERSAO ARDUINO UNO - STANDALONE (SEM SERIAL)
// 4 TEMAS FAMOSOS + SENSOR

/* PINOS */
#define PINO_BUZZER 3
#define PINO_TRIG    5
#define PINO_ECHO    7

/* --- LISTA COMPLETA DE NOTAS (C3 a C7) --- */
#define NOTE_B0  31
#define NOTE_C1  33
#define NOTE_CS1 35
#define NOTE_D1  37
#define NOTE_DS1 39
#define NOTE_E1  41
#define NOTE_F1  44
#define NOTE_FS1 46
#define NOTE_G1  49
#define NOTE_GS1 52
#define NOTE_A1  55
#define NOTE_AS1 58
#define NOTE_B1  62
#define NOTE_C2  65
#define NOTE_CS2 69
#define NOTE_D2  73
#define NOTE_DS2 78
#define NOTE_E2  82
#define NOTE_F2  87
#define NOTE_FS2 93
#define NOTE_G2  98
#define NOTE_GS2 104
#define NOTE_A2  110
#define NOTE_AS2 117
#define NOTE_B2  123
#define NOTE_C3  131
#define NOTE_CS3 139
#define NOTE_D3  147
#define NOTE_DS3 156
#define NOTE_E3  165
#define NOTE_F3  175
#define NOTE_FS3 185
#define NOTE_G3  196
#define NOTE_GS3 208
#define NOTE_A3  220
#define NOTE_AS3 233
#define NOTE_B3  247
#define NOTE_C4  262
#define NOTE_CS4 277
#define NOTE_D4  294
#define NOTE_DS4 311
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_CS5 554
#define NOTE_D5  587
#define NOTE_DS5 622
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_FS5 740
#define NOTE_G5  784
#define NOTE_GS5 831
#define NOTE_A5  880
#define NOTE_AS5 932
#define NOTE_B5  988
#define NOTE_C6  1047
#define NOTE_CS6 1109
#define NOTE_D6  1175
#define NOTE_DS6 1245
#define NOTE_E6  1319
#define NOTE_F6  1397
#define NOTE_FS6 1480
#define NOTE_G6  1568
#define NOTE_GS6 1661
#define NOTE_A6  1760
#define NOTE_AS6 1865
#define NOTE_B6  1976
#define NOTE_C7  2093

/* CONFIGURACAO DO SENSOR */
#define DISTANCIA_TRIGGER 20  
#define DISTANCIA_RESET   25  

/* DADOS DA ROTA */
struct Parada { 
  int id; 
  const char* nome; 
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

int indiceParada = 0;
bool paradaDetectada = false; 
int contadorMusica = 0; 

/* FUNCOES AUXILIARES DE SOM */

// Funcao ajustada para tocar a nota fluida
void toneDelay(int nota, int duracao) {
  tone(PINO_BUZZER, nota, duracao);
  // O delay deve ser um pouco maior que a nota para separar os sons
  delay(duracao * 1.30); 
  noTone(PINO_BUZZER);
}

void tocarProximaMusica() {
  switch (contadorMusica) {
    case 0: // === SUPER MARIO BROS (INTRO) ===
      toneDelay(NOTE_E5, 100);
      toneDelay(NOTE_E5, 100);
      delay(50); 
      toneDelay(NOTE_E5, 100);
      delay(50);
      toneDelay(NOTE_C5, 100);
      toneDelay(NOTE_E5, 100);
      delay(50);
      toneDelay(NOTE_G5, 200);
      delay(100);
      toneDelay(NOTE_G4, 200);
      break;

    case 1: // === IMPERIAL MARCH (STAR WARS) ===
      toneDelay(NOTE_G4, 300);
      toneDelay(NOTE_G4, 300);
      toneDelay(NOTE_G4, 300);
      toneDelay(NOTE_DS4, 250);
      toneDelay(NOTE_AS4, 100);
      toneDelay(NOTE_G4, 300);
      toneDelay(NOTE_DS4, 250);
      toneDelay(NOTE_AS4, 100);
      toneDelay(NOTE_G4, 400);
      break;

    case 2: // === MISSAO IMPOSSIVEL (RITMO) ===
      toneDelay(NOTE_G4, 150);
      toneDelay(NOTE_G4, 150);
      delay(100);
      toneDelay(NOTE_AS4, 150);
      toneDelay(NOTE_C5, 150);
      toneDelay(NOTE_G4, 150);
      toneDelay(NOTE_G4, 150);
      delay(100);
      toneDelay(NOTE_F4, 150);
      toneDelay(NOTE_FS4, 150);
      break;

    case 3: // === FANFARRA VITORIA (FINAL FANTASY STYLE) ===
      toneDelay(NOTE_C5, 80);
      toneDelay(NOTE_C5, 80);
      toneDelay(NOTE_C5, 80);
      toneDelay(NOTE_C5, 200);
      toneDelay(NOTE_GS4, 200);
      toneDelay(NOTE_AS4, 200);
      toneDelay(NOTE_C5, 150);
      delay(50);
      toneDelay(NOTE_AS4, 80);
      toneDelay(NOTE_C5, 400);
      break;
  }

  // Prepara para a proxima musica
  contadorMusica++;
  if (contadorMusica > 3) {
    contadorMusica = 0; // Reseta para a primeira
  }
}

long lerDistanciaCM() {
  digitalWrite(PINO_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PINO_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PINO_TRIG, LOW);

  long duration = pulseIn(PINO_ECHO, HIGH, 30000); 
  if (duration == 0) return 0;
  return duration * 0.034 / 2;
}

/* SETUP */

void setup() {
  pinMode(PINO_TRIG, OUTPUT);
  pinMode(PINO_ECHO, INPUT);
  pinMode(PINO_BUZZER, OUTPUT);

  digitalWrite(PINO_TRIG, LOW);
  
  // Som curto para avisar que ligou
  toneDelay(NOTE_C6, 150);
  toneDelay(NOTE_G5, 150);
}

/* LOOP PRINCIPAL */

void loop() {
  long distancia = lerDistanciaCM();

  // Se estiver dentro da faixa de detecao
  if (distancia > 2 && distancia < DISTANCIA_TRIGGER) {
    if (!paradaDetectada) {
      paradaDetectada = true;

      // Avanca rota interna
      indiceParada = (indiceParada + 1) % totalParadas;

      // Toca a musica
      tocarProximaMusica();
    }
  } 
  // Se estiver fora, rearmar sensor
  else if (distancia > DISTANCIA_RESET || distancia == 0) {
    if (paradaDetectada) {
      paradaDetectada = false;
    }
  }

  delay(200);
}