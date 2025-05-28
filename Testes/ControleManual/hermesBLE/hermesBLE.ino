#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h> // Para descritores, embora não explicitamente usado para notificação aqui, é bom ter

// === DEFINIÇÕES GLOBAIS ===

// UUIDs - DEVEM SER OS MESMOS DA SUA APP PYTHON
#define SERVICE_UUID        "e5a220a3-ffd5-42a8-ac9c-4cdc31f68e6b"
#define CHARACTERISTIC_UUID_RX "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Pinos dos Motores
#define PWMA 4 
#define AIN2 16
#define AIN1 17
#define STBY 5
#define BIN1 18
#define BIN2 19
#define PWMB 21

// Pinos dos Sensores de Linha
const unsigned int sensor[] = {35, 32, 33, 25, 26, 27}; // Sensores frontais para seguir linha
const int n_sensores = 6;
int pesos[] = {3, 2, 1, -1, -2, -3}; // Pesos para cálculo da posição

// Pinos dos Sensores Laterais (para a máquina de estados de curva/interseção)
const unsigned int sensorLat[] = {13, 39}; // {Esquerdo, Direito} - Confirme a ordem! No seu código original, 39 era r_read e 13 l_read.

// Variáveis de Controle e Estado do Robô
int rspeed;
int lspeed;
int base_speed = 80; // Velocidade base dos motores

// Variáveis do PID
float Kp = 2.4;
float Ki = 0.002;
float Kd = 7;
float p, integral = 0, d, lp = 0; // Termos do PID (integral e lp precisam ser persistentes)
float error;        // Erro atual (pos - sp)
float correction;   // Correção calculada pelo PID
float sp = 0;       // Setpoint (posição desejada, geralmente 0 para o centro da linha)

// Variáveis da Máquina de Estados para Curvas/Interseções
int state = 0;
int aux = 0;        // Variável auxiliar para a lógica da máquina de estados
unsigned long atual = 0; // Timestamp para controle de tempo na máquina de estados

// Variáveis de Leitura dos Sensores
int sensor_read[n_sensores]; // Leituras brutas dos sensores de linha
long sensor_average = 0;     // Média ponderada das leituras (numerador)
int sensor_sum = 0;          // Soma das leituras (denominador)
int pos = 0;                 // Posição calculada da linha

// Leituras dos sensores laterais (0 para preto, 1 para branco, conforme seu código original)
int r_lat_read = 0; // Leitura do sensor lateral direito
int l_lat_read = 0; // Leitura do sensor lateral esquerdo

// Variáveis de Conexão BLE
BLECharacteristic *pCharacteristicRX;
BLEServer *pServer_global;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// === DECLARAÇÃO DAS FUNÇÕES ===
int pid_calc();
void calc_turn();
void updatePIDAndSpeedConstants(String input);

// === CALLBACKS BLE ===
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pSrv) {
      deviceConnected = true;
      Serial.println("Dispositivo Conectado via BLE");
    }

    void onDisconnect(BLEServer* pSrv) {
      deviceConnected = false;
      Serial.println("Dispositivo Desconectado via BLE");
      Serial.println("Reiniciando advertising...");
      // A biblioteca ESP32 BLE geralmente reinicia o advertising automaticamente ao desconectar.
      // Mas para garantir:
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
        updatePIDAndSpeedConstants(inputString); // Chama sua função de parsing
      }
    }
};

// === SETUP ===
void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando Hermes_BLE...");

  // Configuração dos Pinos dos Sensores de Linha
  for(int i=0; i<n_sensores; i++){
    pinMode(sensor[i], INPUT);
  }
  // Configuração dos Pinos dos Sensores Laterais
  pinMode(sensorLat[0], INPUT); // Sensor Lateral Esquerdo
  pinMode(sensorLat[1], INPUT); // Sensor Lateral Direito

  // Configuração dos Pinos dos Motores
  pinMode(STBY, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(PWMB, OUTPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);

  // Inicialização dos Motores (parados e direção para frente)
  digitalWrite(STBY, HIGH); // Habilita o driver
  digitalWrite(AIN1, HIGH); // Motor A para frente
  digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, HIGH); // Motor B para frente
  digitalWrite(BIN2, LOW); 
  
  analogWrite(PWMA, 0); // Motor A parado
  analogWrite(PWMB, 0); // Motor B parado
  
  // Configuração do BLE
  BLEDevice::init("Hermes_BLE"); // Nome do dispositivo BLE (MESMO DA APP PYTHON)
  pServer_global = BLEDevice::createServer();
  pServer_global->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer_global->createService(SERVICE_UUID);

  pCharacteristicRX = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_RX,
                      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR // WRITE para com resposta, WRITE_NR para sem resposta
                    );
  pCharacteristicRX->setCallbacks(new MyCharacteristicCallbacks());
  // Você pode adicionar um descritor se sua app Python precisar, como o 2902 para notificações/indicações
  // pCharacteristicRX->addDescriptor(new BLE2902()); // Não estritamente necessário para WRITE

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true); // Permite que mais dados sejam enviados na resposta do scan
  // Configurações opcionais para compatibilidade (especialmente iOS)
  // pAdvertising->setMinPreferred(0x06); 
  // pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Aguardando conexão da App Desktop...");

  sp = 0; // Setpoint inicial do PID
  atual = 0; // Reseta timestamp da máquina de estados
  integral = 0; // Reseta integral do PID
  lp = 0;       // Reseta erro anterior do PID
}

// === LOOP ===
void loop() {
  if (deviceConnected) {
    // --- LÓGICA PRINCIPAL DO ROBÔ SEGuidor de LINHA ---
    
    // Leitura dos sensores laterais (1 se branco, 0 se preto)
    // sensorLat[1] é o direito (39), sensorLat[0] é o esquerdo (13)
    r_lat_read = analogRead(sensorLat[1]) <= 2000; 
    l_lat_read = analogRead(sensorLat[0]) <= 1000;

    // Máquina de Estados (lógica original)
    switch (state){
      case 0:
        calc_turn(); // Segue a linha
        if(r_lat_read){ // Se o sensor lateral direito detectar branco (início de uma marcação/curva)
          state++;
        }
        break;
      case 1:
        calc_turn(); // Continua seguindo a linha
        if (!r_lat_read) // Se o sensor lateral direito voltar a ler preto (passou a marcação inicial)
          state++;
        break;
      case 2:
        calc_turn(); // Continua seguindo a linha
        
        // Lógica para detectar se é uma curva ou uma intersecção (baseado no tempo e no sensor esquerdo)
        if (r_lat_read || l_lat_read){ // Se algum sensor lateral ler branco
          if(atual == 0){ // Se for a primeira vez que um sensor lateral leu branco neste estado
            atual = millis(); // Salva o tempo atual
          }
        }

        if(atual != 0){ // Se já detectamos uma marcação lateral e estamos cronometrando
          if (millis() - atual >= 300){ // Se passou 300ms
            if(aux == 0){ // Se 'aux' é 0 (significa que o sensor esquerdo não detectou branco durante esses 300ms - provavelmente uma curva simples para a direita)
              atual = 0;
              state++; // Avança para o próximo estado
            } else { // Se 'aux' é 1 (sensor esquerdo detectou branco - pode ser uma intersecção ou outra marcação)
              aux = 0;   // Reseta aux
              atual = 0; // Reseta o tempo
              // O robô continua no estado 2, seguindo a linha, até a próxima detecção.
              // Esta parte pode precisar de ajuste dependendo do tipo de pista.
            }
          } else { // Se ainda não passaram 300ms
            if(l_lat_read){ // Se o sensor esquerdo ler branco durante este intervalo
              aux = 1;    // Marca que o sensor esquerdo detectou
            }
          }
        }
        break;
      case 3: // Saindo da curva/marcação
        calc_turn(); // Continua seguindo a linha
        if (!r_lat_read) // Se o sensor lateral direito ler preto novamente (fim da marcação/curva)
          state++;
        break;
      case 4: // Estado de parada ou espera
        analogWrite(PWMA, 0);
        analogWrite(PWMB, 0);
        // O robô para. Novos comandos (PID/velocidade) podem ser recebidos via BLE.
        // Para reiniciar o robô, a app desktop poderia enviar um comando específico
        // ou você pode decidir que ao receber novos PIDs/velocidade, o estado volta para 0.
        // Por segurança, vamos mantê-lo parado até que a app envie um "start" implícito
        // (por exemplo, se a app enviar novos PIDs, você pode decidir resetar state=0 lá)
        // ou explícito (um comando "CMD=START").
        // state = 0; // DESCOMENTE SE QUISER QUE ELE RECOMECE AUTOMATICAMENTE APÓS ESTE ESTADO
                     // AO RECEBER NOVOS PARÂMETROS, MAS PODE SER INESPERADO.
        break;
    }

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

  } else { // Dispositivo não conectado
    // Garante que os motores estejam parados se não houver conexão
    analogWrite(PWMA, 0);
    analogWrite(PWMB, 0);
    
    // Gerenciamento da reconexão/advertising (o callback onDisconnect já tenta reiniciar)
    if (oldDeviceConnected) {
        delay(500); // Dá um tempo para o stack BLE processar a desconexão
        // BLEDevice::startAdvertising(); // Já é chamado no onDisconnect
        Serial.println("Conexão perdida, advertising deveria ter sido reiniciado pelo callback.");
        oldDeviceConnected = deviceConnected; // Atualiza o estado anterior
    }
  }

  // Atualiza o estado da conexão anterior para a próxima iteração
  if (deviceConnected && !oldDeviceConnected) {
      oldDeviceConnected = deviceConnected;
  }
  if (!deviceConnected && oldDeviceConnected) { // Esta condição já está no `else` acima.
      oldDeviceConnected = deviceConnected;     // Pode ser redundante aqui, mas não prejudica.
  }
  
  delay(10); // Pequeno delay no loop principal
}

// === FUNÇÕES AUXILIARES DO ROBÔ ===

int pid_calc() {
  sensor_average = 0; // Use as variáveis globais declaradas
  sensor_sum = 0;

  for(int i = 0; i < n_sensores; i++) {
    // No seu código original, valores altos de analogRead significam BRANCO
    // e valores baixos significam PRETO.
    // A lógica de pesos {3,2,1,-1,-2,-3} assume que um valor positivo de 'pos'
    // significa que a linha está à DIREITA, e o robô precisa virar à DIREITA.
    // Se 'pos' for negativo, linha à ESQUERDA, robô vira à ESQUERDA.
    sensor_read[i] = analogRead(sensor[i]); 
    // Se a linha é PRETA, o sensor_read[i] será BAIXO.
    // Se a linha é BRANCA, o sensor_read[i] será ALTO.
    // Para que o PID funcione corretamente, precisamos que os sensores sobre a LINHA PRETA
    // contribuam mais para a soma e média.
    // Uma forma comum é inverter a leitura ou subtrair de um máximo:
    // int leitura_invertida = 4095 - sensor_read[i]; // Se o máximo for 4095
    // sensor_average += (long)leitura_invertida * pesos[i] * 100; // *100 para aumentar a resolução antes da divisão
    // sensor_sum += leitura_invertida;
    // OU, se os sensores já estão calibrados para dar valores altos no preto e baixos no branco, sua lógica original está OK.
    // Assumindo que sua lógica original de `analogRead` e `pesos` está correta para o seu hardware:
    sensor_average += (long)sensor_read[i] * pesos[i] * 100; 
    sensor_sum += sensor_read[i];
  }

  if (sensor_sum == 0) { // Todos os sensores leram algo que somou zero (ou todos leram valor mínimo)
    // Linha completamente perdida. O que fazer?
    // 1. Usar o último erro conhecido: `error` global já tem isso.
    // 2. Virar bruscamente para um lado.
    // Se `error` for positivo, a linha estava à direita, então a correção deve ser para a direita (diminuir velocidade esquerda, aumentar direita).
    // Se `error` for negativo, a linha estava à esquerda.
    // Retornar um valor grande de correção baseado no último erro.
    if (lp > 0) return Kp * 1000; // Se o último erro 'p' (lp) era para a direita, continua virando forte para direita
    if (lp < 0) return Kp * -1000; // Se o último erro 'p' (lp) era para a esquerda, continua virando forte para esquerda
    return 0; // Sem informação, sem correção (pode não ser o ideal)
  }

  pos = sensor_average / sensor_sum;

  error = pos - sp; // 'error' é global
  p = error;        // 'p' é global
  integral += p;    // 'integral' é global
  
  // Limita a integral para evitar wind-up
  float integral_max = 5000; // Você tinha isso como global, mantenha ou defina aqui
  float integral_min = -5000;
  integral = constrain(integral, integral_min, integral_max);
  
  d = p - lp;       // 'd' é global
  lp = p;           // 'lp' é global (atualiza o último erro 'p')
  
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

// Função para atualizar constantes PID e Velocidade Base via BLE
void updatePIDAndSpeedConstants(String input) {
  input.trim(); // Remove espaços em branco extras
  Serial.print("Parsing BLE input: "); Serial.println(input);

  // Exemplo de formato esperado: "Kp=2.4,Ki=0.002,Kd=7.0,Bs=90"
  // Ou individualmente: "Kp=2.5" ou "Bs=100"
  
  int currentPos = 0;
  bool updatedSomething = false;

  while(currentPos < input.length()){
    int equalSignIdx = input.indexOf('=', currentPos);
    if(equalSignIdx == -1) break; // Não há mais '='

    int commaIdx = input.indexOf(',', equalSignIdx);
    if(commaIdx == -1) commaIdx = input.length(); // Se não houver vírgula, é o último parâmetro

    String paramName = input.substring(currentPos, equalSignIdx);
    String paramValueStr = input.substring(equalSignIdx + 1, commaIdx);
    paramName.trim();
    paramValueStr.trim();

    if (paramName.equalsIgnoreCase("Kp")) {
      Kp = paramValueStr.toFloat();
      Serial.print("Kp atualizado para: "); Serial.println(Kp, 4); // 4 casas decimais
      updatedSomething = true;
    } else if (paramName.equalsIgnoreCase("Ki")) {
      Ki = paramValueStr.toFloat();
      Serial.print("Ki atualizado para: "); Serial.println(Ki, 6); // 6 casas decimais
      updatedSomething = true;
    } else if (paramName.equalsIgnoreCase("Kd")) {
      Kd = paramValueStr.toFloat();
      Serial.print("Kd atualizado para: "); Serial.println(Kd, 4);
      updatedSomething = true;
    } else if (paramName.equalsIgnoreCase("Bs")) {
      base_speed = paramValueStr.toInt();
      base_speed = constrain(base_speed, 0, 255); // Garante que está no range válido
      Serial.print("Base Speed (Bs) atualizada para: "); Serial.println(base_speed);
      updatedSomething = true;
    } else if (paramName.equalsIgnoreCase("STATE")) { // Comando para mudar o estado
        int newState = paramValueStr.toInt();
        if (newState >= 0 && newState <= 4) { // Adicione mais estados se necessário
            state = newState;
            Serial.print("Estado alterado para: "); Serial.println(state);
            if (state == 0) { // Se o comando for para iniciar (estado 0)
                integral = 0; // Reseta a integral do PID
                lp = 0;       // Reseta o erro anterior do PID
                atual = 0;    // Reseta o temporizador da máquina de estados
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
    // Se você quiser que o robô reaja imediatamente a uma mudança de base_speed
    // enquanto estiver parado no estado 4, você pode adicionar lógica aqui.
    // Por exemplo, se state == 4 e base_speed foi alterada, poderia chamar calc_turn()
    // mas isso faria ele andar. Melhor deixar a app controlar o "START" (mudando state para 0).
    Serial.println("Constantes atualizadas.");
    // Se estava parado e recebeu novos parâmetros e um comando para iniciar
    if (state == 0 && deviceConnected) {
        // calc_turn(); // A próxima iteração do loop já fará isso.
    }
  }
}