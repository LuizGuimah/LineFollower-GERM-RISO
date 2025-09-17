#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

// UUIDs
#define SERVICE_UUID             "e5a220a3-ffd5-42a8-ac9c-4cdc31f68e6b"
#define CHARACTERISTIC_UUID_RX   "beb5483e-36e1-4688-b7f5-ea07361b26a8" // Para receber comandos do Python
#define CHARACTERISTIC_UUID_TX   "c33d5c6c-005a-45f5-8133-9142d7db0481" // Para enviar estado para o Python

#define PWMA 4 
#define AIN2 16
#define AIN1 17
#define STBY 5
#define BIN1 18
#define BIN2 19
#define PWMB 21

const unsigned int sensor[] = {35, 32, 33, 25, 26, 27}; // Sensores frontais para seguir linha
const int n_sensores = 6;
int pesos[] = {3, 2, 1, -1, -2, -3}; // Pesos para cálculo da posição

const unsigned int sensorLat[] = {13, 39}; // {Esquerdo, Direito}

int rspeed;
int lspeed;
int base_speed = 100; // Velocidade base dos motores
const int V_max = 255;

float Kp = 2.4;
float Ki = 0.002;
float Kd = 7;
float p, integral = 0, d, lp = 0; // Termos do PID (integral e lp precisam ser persistentes)
float error;                      // Erro atual (pos - sp)
float correction;                 // Correção calculada pelo PID
float sp = 0;                     // Setpoint (posição desejada, geralmente 0 para o centro da linha)

int sensor_read[n_sensores]; // Leituras brutas dos sensores de linha
long sensor_average = 0;     // Média ponderada das leituras (numerador)
int sensor_sum = 0;          // Soma das leituras (denominador)
int pos = 0;                 // Posição calculada da linha

int r_lat_read = 0;
int l_lat_read = 0;

int state = 4;                                    // Robô começa PARADO
int old_state_for_notification = -1;              // Para enviar notificação apenas quando o estado mudar
unsigned long last_state_send_time = 0;           // Para enviar estado periodicamente
const unsigned long state_send_interval = 1000;   // Intervalo para enviar estado (1 segundo)

unsigned long atual = 0;  // Timestamp para controle de tempo na máquina de estados
int aux = 0;              // Variável auxiliar para a lógica da máquina de estados

// Variáveis de Conexão BLE
BLECharacteristic *pCharacteristicRX;
BLECharacteristic *pCharacteristicTX;
BLEServer *pServer_global;
bool deviceConnected = false;
bool oldDeviceConnected = false;

int pid_calc();
void calc_turn();
void updatePIDAndSpeedConstants(String input);


// === CALLBACKS BLE ===
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pSrv) {
      deviceConnected = true;
      oldDeviceConnected = true; // Para a lógica do loop principal
      Serial.println("Dispositivo Conectado via BLE");
      // Enviar o estado inicial assim que conectar
      if (pCharacteristicTX != nullptr) {
        char stateStr[2]; // Suficiente para "0" a "9" + null terminator
        sprintf(stateStr, "%d", state);
        pCharacteristicTX->setValue(stateStr);
        pCharacteristicTX->notify();
        Serial.print("Estado inicial enviado via BLE: ");
        Serial.println(stateStr);
        old_state_for_notification = state; // Atualiza o estado antigo para evitar reenvio imediato
      }
    }

    void onDisconnect(BLEServer* pSrv) {
      deviceConnected = false;
      Serial.println("Dispositivo Desconectado via BLE");
      Serial.println("Reiniciando advertising...");
      BLEDevice::startAdvertising(); 
    }
};

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pChar) {
      String rxValue_str = pChar->getValue().c_str(); 
      if (rxValue_str.length() > 0) {
        String inputString = String(rxValue_str.c_str());
        Serial.print("BLE Recebido: ");
        Serial.println(inputString);
        updatePIDAndSpeedConstants(inputString); // Fun de parsing
      }
    }
};


void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando Hermes_BLE...");
  // state = 4; // Garante que o robô comece PARADO

  for(int i=0; i<n_sensores; i++){
    pinMode(sensor[i], INPUT);
  }
  pinMode(sensorLat[0], INPUT);
  pinMode(sensorLat[1], INPUT);
  pinMode(STBY, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(PWMB, OUTPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  digitalWrite(STBY, HIGH);
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW); 
  analogWrite(PWMA, 0);
  analogWrite(PWMB, 0);
  
  // Configuração do BLE
  BLEDevice::init("Hermes_BLE");
  pServer_global = BLEDevice::createServer();
  pServer_global->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer_global->createService(SERVICE_UUID);

  // Característica RX (para receber comandos do Python)
  pCharacteristicRX = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_RX,
                      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
                    );
  pCharacteristicRX->setCallbacks(new MyCharacteristicCallbacks());

  // Característica TX (para enviar estado para o Python)
  pCharacteristicTX = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_TX,
                      BLECharacteristic::PROPERTY_READ | // Permitir leitura (opcional, mas bom ter)
                      BLECharacteristic::PROPERTY_NOTIFY // ESSENCIAL para enviar dados sem o Python pedir
                    );
  pCharacteristicTX->addDescriptor(new BLE2902()); // ESSENCIAL para notificações/indicações

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();
  Serial.println("Aguardando conexão da App Desktop...");

  sp = 0;
  atual = 0;
  integral = 0;
  lp = 0;
  old_state_for_notification = state; // Inicializa o estado antigo
}


void loop() {
  unsigned long currentTime = millis(); // Para envio periódico

  if (deviceConnected) {
    if (state != old_state_for_notification || (currentTime - last_state_send_time >= state_send_interval)) { // Verificar se o estado mudou ou se é hora de enviar periodicamente
        if (pCharacteristicTX != nullptr) {
            char stateStr[3]; // Suficiente para "-1" a "99" + null terminator (embora state só vá de 0-4)
            sprintf(stateStr, "%d", state);
            pCharacteristicTX->setValue(stateStr);
            pCharacteristicTX->notify();
            old_state_for_notification = state;
            last_state_send_time = currentTime;
        }
    }

    // --- LÓGICA PRINCIPAL DO ROBÔ ---
    r_lat_read = analogRead(sensorLat[1]) <= 3200; 
    l_lat_read = analogRead(sensorLat[0]) <= 3200;
    int previous_state_for_debug = state; // Para debug de mudança de estado

     switch (state){
      case 0:
        calc_turn(); // Segue a linha
        if(r_lat_read){ 
          state++; 
        }
        tratarPerdaLinha();
        break;
      
      case 1:
        calc_turn(); // Continua seguindo a linha
        if (!r_lat_read){ 
          state++; 
        }
        tratarPerdaLinha();
        break;
      
      case 2:
        calc_turn(); // Continua seguindo a linha
        if (r_lat_read || l_lat_read){ 
          if(atual == 0){ 
            atual = millis(); 
          } 
        }
        if(atual != 0){ 
          if (millis() - atual >= 300){ 
            if(aux == 0){ 
              atual = 0; 
              state++; 
            } else { 
              aux = 0; 
              atual = 0; 
            } 
          } else { 
            if(l_lat_read){ 
              aux = 1; 
            } 
          } 
        }
        tratarPerdaLinha();
        break;

      case 3: // Saindo da curva/marcação
        calc_turn(); // Continua seguindo a linha
        if (!r_lat_read){ 
          state++; 
        }
        tratarPerdaLinha();
        break;
      
      case 4: // Estado de parada ou espera
        analogWrite(PWMA, 0);
        analogWrite(PWMB, 0);
        break;
    }

  } else { // Dispositivo não conectado
    analogWrite(PWMA, 0);
    analogWrite(PWMB, 0);
    if (oldDeviceConnected && !deviceConnected) { // Transição de conectado para desconectado
        delay(500); 
        Serial.println("Conexão perdida, advertising deveria ter sido reiniciado pelo callback.");
    }
  }

  oldDeviceConnected = deviceConnected;  // Atualiza o estado da conexão anterior para a próxima iteração
  
  //delay(10);
}



void tratarPerdaLinha() {
  bool all_black = true;
  for (int i = 0; i < n_sensores; i++) {
    if (analogRead(sensor[i]) < 3000) {  // Se algum sensor estiver vendo branco (linha), não é perda total
      all_black = false;
      break;
    }
  }

  if (all_black) {
    Serial.println("Perda de linha detectada!");

    if (lp > 0) {
      while (analogRead(sensor[2]) < 3000 && analogRead(sensor[3]) < 3000) {  // Espera até reencontrar a linha no centro
        analogWrite(PWMA, 0);
        analogWrite(PWMB, base_speed);
        delay(10);
      }
    } else {
      while (analogRead(sensor[2]) < 3000 && analogRead(sensor[3]) < 3000) {
        analogWrite(PWMA, base_speed);
        analogWrite(PWMB, 0);
        delay(10);
      }
    }
  }
}


int pid_calc() {
  static unsigned long lastTime = micros();
  unsigned long now = micros();
  float dt = (now - lastTime) / 1000.0;  // dt em ms, convertendo para segundos
  if (dt <= 0) dt = 0.1;                 // Proteção contra divisão por zero
  lastTime = now;

  sensor_average = 0; 
  sensor_sum = 0;
  bool all_white = true;

  for (int i = 0; i < n_sensores; i++) {
    sensor_read[i] = analogRead(sensor[i]);
    if (sensor_read[i] > 1000) all_white = false;  // Se algum sensor vê preto, não é tudo branco
    sensor_average += (long)sensor_read[i] * pesos[i] * 100;
    sensor_sum += sensor_read[i];
  }

  // ======== ENCRUZILHADA (TUDO BRANCO) ========
  if (all_white) {
    pos = 0;  // Assume centralizado (ou outro valor fixo, a seu critério)
  } else if (sensor_sum != 0) {
    pos = sensor_average / sensor_sum;
  } else {
    // ======== PERDA COMPLETA DA LINHA (TUDO PRETO)[TUDO BRANCO ACHO!!!] ========
    if (lp > 0) return Kp * 1000;
    if (lp < 0) return Kp * -1000;
    return 0;
  }

  error = pos - sp;
  p = error;

  integral += p * dt;

  // Anti-windup
  float integral_max = 5000;
  float integral_min = -5000;
  integral = constrain(integral, integral_min, integral_max);

  d = (p - lp) / dt;
  lp = p;

  float output = Kp * p + Ki * integral + Kd * d;

  // Anti-windup por saturação da saída
  float output_max = 255;
  float output_min = -255;

  if (output > output_max) {
    output = output_max;
    if (p > 0) integral -= p;
  }
  else if (output < output_min) {
    output = output_min;
    if (p < 0) integral -= p;
  }

  Serial.println(p);
  
  return int(output);
}



void calc_turn() {
  if (!deviceConnected && state != 4) { // Não calcula nem move se não estiver conectado, a menos que esteja no estado de parada intencional
      analogWrite(PWMA, 0);
      analogWrite(PWMB, 0);
      return;
  }

/////////////////////////////////////////////////
  /*static unsigned long lastTime = 0;
  static int count = 0;
  
  count++;
  
  if (millis() - lastTime >= 1000) {  // Contagem por segundo
    Serial.print("Frequência PID: ");
    Serial.print(count);
    Serial.println(" Hz");
    count = 0;
    lastTime = millis();
  }
  */
/////////////////////////////////////////////////
  
  correction = pid_calc();

  // Ajuste dinâmico da base_speed com base no erro
  int base_dynamic = base_speed;
  if (abs(p) > 800) base_dynamic = base_speed * 0.6;
  if (abs(p) > 1500) base_dynamic = base_speed * 0.4;
  
  rspeed = base_dynamic - correction; 
  lspeed = base_dynamic + correction;

  
  //BLOCO NOVO VOLKMAN-----------------------------------------------------------------------
  
  if (lspeed <= 0)                          //Inverte a rotação do motor para reduzir a velocidade mais rápido
  {
    lspeed = -lspeed;
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, HIGH);
  }
  else
  {
    digitalWrite(BIN1, HIGH);
    digitalWrite(BIN2, LOW);
  }

  if (rspeed <= 0)                        //Inverte a rotação do motor para reduzir a velocidade mais rápido
  {
    rspeed = -rspeed;
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);
  }
  else
  {
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);
  }

  if (lspeed >= V_max)
  {
    lspeed = V_max;
    //    erro_I = erro_I - ((erro + erro_anterior) / 2) * t_loop;  //anti windup
  }
  if (rspeed >= V_max)
  {
    rspeed = V_max;
    //    erro_I = erro_I - ((erro + erro_anterior) / 2) * t_loop;  //anti windup
  }

  
  analogWrite(PWMA, rspeed);
  analogWrite(PWMB, lspeed); 
}


void updatePIDAndSpeedConstants(String input) {
  input.trim(); 
  Serial.print("Parsing BLE input: "); Serial.println(input);
  int currentPos = 0;
  bool updatedSomething = false;
  int previous_state_for_update_func = state; // Para verificar se o comando mudou o estado

  while(currentPos < input.length()){
    int equalSignIdx = input.indexOf('=', currentPos);
    if(equalSignIdx == -1) break; 
    int commaIdx = input.indexOf(',', equalSignIdx);
    if(commaIdx == -1) commaIdx = input.length(); 
    String paramName = input.substring(currentPos, equalSignIdx);
    String paramValueStr = input.substring(equalSignIdx + 1, commaIdx);
    paramName.trim();
    paramValueStr.trim();

    if (paramName.equalsIgnoreCase("Kp")) {
      Kp = paramValueStr.toFloat();
      Serial.print("Kp atualizado para: "); Serial.println(Kp, 4);
      updatedSomething = true;
    } else if (paramName.equalsIgnoreCase("Ki")) {
      Ki = paramValueStr.toFloat();
      Serial.print("Ki atualizado para: "); Serial.println(Ki, 6);
      updatedSomething = true;
    } else if (paramName.equalsIgnoreCase("Kd")) {
      Kd = paramValueStr.toFloat();
      Serial.print("Kd atualizado para: "); Serial.println(Kd, 4);
      updatedSomething = true;
    } else if (paramName.equalsIgnoreCase("Bs")) {
      base_speed = paramValueStr.toInt();
      base_speed = constrain(base_speed, 0, 255); 
      Serial.print("Base Speed (Bs) atualizada para: "); Serial.println(base_speed);
      updatedSomething = true;
    } else if (paramName.equalsIgnoreCase("STATE")) { 
        int newState = paramValueStr.toInt();
        if (newState >= 0 && newState <= 4) { 
            state = newState; // ATUALIZA O ESTADO GLOBAL
            Serial.print("Estado alterado para: "); Serial.println(state);
            if (state == 0) { 
                integral = 0; 
                lp = 0;       
                atual = 0;    
            }
            updatedSomething = true;
        } else {
            Serial.print("Valor de estado invalido: "); Serial.println(newState);
        }
    }
    currentPos = commaIdx + 1;
  }

  if (!updatedSomething) {
    Serial.println("Formato invalido ou nenhum parametro reconhecido! Ex: Kp=0.5,Ki=0.0003,Kd=0.6,Bs=100,STATE=0");
  } else {
    Serial.println("Constantes atualizadas.");
    // Se o estado foi alterado por um comando, força o envio da notificação imediatamente
    if (state != previous_state_for_update_func && deviceConnected && pCharacteristicTX != nullptr) {
        char stateStr[3];
        sprintf(stateStr, "%d", state);
        pCharacteristicTX->setValue(stateStr);
        pCharacteristicTX->notify();
        Serial.print("Estado (após comando) enviado via BLE: "); Serial.println(stateStr);
        old_state_for_notification = state; // Atualiza para evitar reenvio no loop principal
        last_state_send_time = millis();    // Reseta timer de envio periódico
    }
  }
}
