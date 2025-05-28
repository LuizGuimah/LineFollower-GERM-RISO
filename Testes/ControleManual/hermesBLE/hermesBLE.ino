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
<<<<<<< Updated upstream

  // Configuração dos Pinos dos Sensores de Linha
=======
  // state = 4; // Garante que o robô comece PARADO

>>>>>>> Stashed changes
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

<<<<<<< Updated upstream
// === LOOP ===
void loop() {
  if (deviceConnected) {
    // --- LÓGICA PRINCIPAL DO ROBÔ SEGuidor de LINHA ---
    
    // Leitura dos sensores laterais (1 se branco, 0 se preto)
    // sensorLat[1] é o direito (39), sensorLat[0] é o esquerdo (13)
    r_lat_read = analogRead(sensorLat[1]) <= 2000; 
    l_lat_read = analogRead(sensorLat[0]) <= 1000;
=======
>>>>>>> Stashed changes

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
    r_lat_read = analogRead(sensorLat[1]) <= 100; 
    l_lat_read = analogRead(sensorLat[0]) <= 100;
    bool all_white = true; // Resetar a cada loop

    int previous_state_for_debug = state; // Para debug de mudança de estado

     switch (state){
      case 0:
        calc_turn(); // Segue a linha
        if(r_lat_read){ 
          state++; 
        }
<<<<<<< Updated upstream
        break;
      case 1:
        calc_turn(); // Continua seguindo a linha
        if (!r_lat_read) // Se o sensor lateral direito voltar a ler preto (passou a marcação inicial)
          state++;
        break;
=======
        tratarPerdaLinha();
        break;
      
      case 1:
        calc_turn(); // Continua seguindo a linha
        if (!r_lat_read){ 
          state++; 
        }
        tratarPerdaLinha();
        break;
      
>>>>>>> Stashed changes
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
<<<<<<< Updated upstream
        break;
      case 3: // Saindo da curva/marcação
        calc_turn(); // Continua seguindo a linha
        if (!r_lat_read) // Se o sensor lateral direito ler preto novamente (fim da marcação/curva)
          state++;
        break;
=======
        tratarPerdaLinha();
        break;

      case 3: // Saindo da curva/marcação
        calc_turn(); // Continua seguindo a linha
        if (!r_lat_read){ 
          state++; 
        }
        tratarPerdaLinha();
        break;
      
>>>>>>> Stashed changes
      case 4: // Estado de parada ou espera
        analogWrite(PWMA, 0);
        analogWrite(PWMB, 0);
        break;
    }

<<<<<<< Updated upstream
    // Lógica para "sair da curva" se todos os sensores perderem a linha (lerem branco)
    // (Todos os sensores frontais leem branco)
    bool all_white = true;
    for(int i=0; i<n_sensores; i++){
        if(analogRead(sensor[i]) < 2000){ // Se algum sensor ler preto (valor baixo)
            all_white = false;
            break;
        }
    }

    if(all_white){
      unsigned long start_time_lost = millis();
      // Tenta virar para um lado para reencontrar a linha.
      // Aqui, está virando o motor B para frente e o A parado, o que faria o robô virar para a ESQUERDA.
      // Ajuste conforme a necessidade (ex: virar para o último lado que viu a linha).
      // Seu código original usava (sensor[2] (33) e sensor[3] (25) >= 2000)
      // Isso implica que se os sensores centrais perderem a linha, ele tenta essa manobra.
      // A condição `all_white` é mais genérica.
      // A lógica original era: while (analogRead(33) >= 2000 && analogRead(25) >= 2000)
      while (analogRead(sensor[2]) >= 2000 && analogRead(sensor[3]) >= 2000 && (millis() - start_time_lost < 1500) ){ // Timeout de 1.5s
        analogWrite(PWMA, 0);     // Motor A (direito) parado
        analogWrite(PWMB, 100);   // Motor B (esquerdo) para frente -> vira para a direita
                                  // SE PWMA É DIREITO E PWMB É ESQUERDO
                                  // Se PWMA é esquerdo e PWMB é direito, então vira para a ESQUERDA.
                                  // Verifique a sua montagem!
                                  // Assumindo: PWMA = motor direito, PWMB = motor esquerdo
                                  // Para virar para a direita: esquerdo para frente (PWMB), direito para trás ou parado.
                                  // Para virar para a esquerda: direito para frente (PWMA), esquerdo para trás ou parado.
                                  // Seu código original: PWMA=0, PWMB=100. Se PWMB é o esquerdo, ele vira para a DIREITA.
        delay(10);
      }
    }
    // --- FIM DA LÓGICA PRINCIPAL DO ROBÔ ---

=======
>>>>>>> Stashed changes
  } else { // Dispositivo não conectado
    analogWrite(PWMA, 0);
    analogWrite(PWMB, 0);
    if (oldDeviceConnected && !deviceConnected) { // Transição de conectado para desconectado
        delay(500); 
        Serial.println("Conexão perdida, advertising deveria ter sido reiniciado pelo callback.");
    }
  }

  oldDeviceConnected = deviceConnected;  // Atualiza o estado da conexão anterior para a próxima iteração
  
  delay(10);
}



void tratarPerdaLinha() {  // FUNÇÃO PARA TRATAR PERDA DE LINHA
    bool all_white = true;
    for(int i = 0; i < n_sensores; i++) {
        if(analogRead(sensor[i]) < 2000) { // Se algum sensor ler preto (valor baixo)
            all_white = false;
            break;
        }
    }

    if(all_white) {
        unsigned long startTimeLost = millis();
        // Lógica: o robo deve virar para um lado para reencontrar a linha
        while (analogRead(sensor[2]) >= 2000 && analogRead(sensor[3]) >= 2000 && (millis() - startTimeLost < 1500)) { // Timeout de 1.5s
            analogWrite(PWMA, 0);   
            analogWrite(PWMB, 120); 
            delay(10);
        }
        // Após a tentativa, pode ser útil parar brevemente ou reavaliar antes de continuar o PID
        // analogWrite(PWMA, 0); // Opcional: Parar os motores após a tentativa
        // analogWrite(PWMB, 0);
        // delay(50);
    }
}


int pid_calc() {
  sensor_average = 0; 
  sensor_sum = 0;

  for(int i = 0; i < n_sensores; i++) {
    sensor_read[i] = analogRead(sensor[i]); 
    sensor_average += (long)sensor_read[i] * pesos[i] * 100; 
    sensor_sum += sensor_read[i];
  }

  if (sensor_sum == 0) {            // Todos os sensores leram algo que somou zero (ou todos leram valor mínimo)
    if (lp > 0) return Kp * 1000;   // Se o último erro 'p' (lp) era para a direita, continua virando forte para direita
    if (lp < 0) return Kp * -1000;  // Se o último erro 'p' (lp) era para a esquerda, continua virando forte para esquerda
    return 0;
  }

  pos = sensor_average / sensor_sum;

  error = pos - sp;
  p = error; 
  integral += p;
  
  float integral_max = 5000;
  float integral_min = -5000;
  integral = constrain(integral, integral_min, integral_max);
  
  d = p - lp;       
  lp = p;           

  return int(Kp * p + Ki * integral + Kd * d);
}

void calc_turn() {
  if (!deviceConnected && state != 4) { // Não calcula nem move se não estiver conectado, a menos que esteja no estado de parada intencional
      analogWrite(PWMA, 0);
      analogWrite(PWMB, 0);
      return;
  }
  correction = pid_calc();
  
  // Se correction > 0, linha à direita, precisa virar à DIREITA: rspeed DIMINUI, lspeed AUMENTA
  // Se correction < 0, linha à esquerda, precisa virar à ESQUERDA: rspeed AUMENTA, lspeed DIMINUI
  rspeed = base_speed - correction; 
  lspeed = base_speed + correction;
  
  rspeed = constrain(rspeed, 0, 255); // Limita a velocidade entre 0 e 255
  lspeed = constrain(lspeed, 0, 255);
  
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
