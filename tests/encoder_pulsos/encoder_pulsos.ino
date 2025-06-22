const int encoderPinA = 14;
const int encoderPinB = 27; // Opcional para direção
volatile long encoderCount = 0;
volatile long lastEncoderCount = 0;
unsigned long lastTime = 0;

// Definindo a constante PPR (pulsos por revolução)
// Número de pulsos por revolução do encoder
const int PPR = 6; //*medir cada motor (fiz testes um que tinha 6 por revolucao)

// Interrupção para contar os pulsos do encoder
void IRAM_ATTR encoderISR() {
  encoderCount++;
}

void setup() {
  Serial.begin(115200);
  
  // Configuração dos pinos do encoder
  pinMode(encoderPinA, INPUT_PULLUP);
  pinMode(encoderPinB, INPUT_PULLUP);

  // Anexando a interrupção no pino do encoder
  attachInterrupt(digitalPinToInterrupt(encoderPinA), encoderISR, RISING);
  
  lastTime = millis();
}

void loop() {
  // Medir a velocidade a cada 1 segundo (1000 ms)
  if (millis() - lastTime >= 1000) {
    noInterrupts(); // Desativar interrupções para leitura segura do encoderCount
    long currentCount = encoderCount;
    interrupts(); // Reativar interrupções

    // Calculando a diferença de pulsos e o tempo passado
    long countDifference = currentCount - lastEncoderCount;
    lastEncoderCount = currentCount;
    
    unsigned long currentTime = millis();
    unsigned long timeDifference = currentTime - lastTime;
    lastTime = currentTime;

    // Calculando a velocidade em RPM (rotações por minuto)
    float revolutions = (float)countDifference / PPR;
    float rpm = (revolutions / timeDifference) * 60000.0; // 60000 ms em um minuto

    // Imprimindo a velocidade medida
    Serial.print("Velocidade: ");
    Serial.print(rpm);
    Serial.println(" RPM");
  }
}
