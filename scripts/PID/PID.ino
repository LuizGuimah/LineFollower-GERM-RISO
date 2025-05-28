<<<<<<< Updated upstream
#include <BluetoothSerial.h>

BluetoothSerial SerialBT;
=======
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#define SERVICE_UUID        "e5a220a3-ffd5-42a8-ac9c-4cdc31f68e6b"
#define CHARACTERISTIC_UUID_RX "beb5483e-36e1-4688-b7f5-ea07361b26a8"
>>>>>>> Stashed changes

#define PWMA 4 
#define AIN2 16
#define AIN1 17
#define STBY 5
#define BIN1 18
#define BIN2 19
#define PWMB 21

const unsigned int sensor[] = {35, 32, 33, 25, 26, 27};
<<<<<<< Updated upstream
const unsigned int sensorLat[] = {13, 39};
const int n_sensores = 6;
int pesos[] = {3, 2, 1, -1, -2, -3};

//speeds
int rspeed;
int lspeed;
const int base_speed = 80;

int pos = 0;
int sensor_read[n_sensores];
long sensor_average = 0;
int sensor_sum = 0;
int r_read=0;
int l_read=0;
int aux = 0, atual = 0;

float p;
float integral;
float integral_max = 5000;
float integral_min = -5000;
float d;
float lp;
float error;
float correction;
float sp;
=======
const int n_sensores = 6;
int pesos[] = {3, 2, 1, -1, -2, -3};

const unsigned int sensorLat[] = {13, 39};

int rspeed;
int lspeed;
int base_speed = 80;
const int V_max = 255;
>>>>>>> Stashed changes

float Kp = 2.4;
float Ki = 0.002;
float Kd = 7;
<<<<<<< Updated upstream
int state = 0;
int pid_calc();
void calc_turn();

void setup()
{
  SerialBT.begin("Hermes");
  //sensors
  for(int i=0; i<n_sensores; i++){
    pinMode(sensor[i], INPUT);
  }
  atual = 0;
  //motors
=======
float p, integral = 0, d, lp = 0;
float error;
float correction;
float sp = 0;

int state = 0;
int aux = 0;
int r_read=0;
int l_read=0;
unsigned long atual = 0;

int sensor_read[n_sensores];
long sensor_average = 0;
int sensor_sum = 0;
int pos = 0;

int r_lat_read = 0;
int l_lat_read = 0;

BLECharacteristic *pCharacteristicRX;
BLEServer *pServer_global;
bool deviceConnected = false;
bool oldDeviceConnected = false;

int pid_calc();
void calc_turn();
void updatePIDAndSpeedConstants(String input);

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pSrv) {
      deviceConnected = true;
      Serial.println("Dispositivo Conectado via BLE");
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
        updatePIDAndSpeedConstants(inputString);
      }
    }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando Hermes_BLE...");
  
  for(int i=0; i<n_sensores; i++){
    pinMode(sensor[i], INPUT);
  }
  pinMode(sensorLat[0], INPUT);
  pinMode(sensorLat[1], INPUT);

>>>>>>> Stashed changes
  pinMode(STBY, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(PWMB, OUTPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);

<<<<<<< Updated upstream
  analogWrite(PWMA, 0);
  analogWrite(PWMB, 0);
=======
>>>>>>> Stashed changes
  digitalWrite(STBY, HIGH);
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, HIGH);
<<<<<<< Updated upstream
  digitalWrite(BIN2, LOW); 
  
  Serial.begin(115200);
  
  sp = 0;
  while(!SerialBT.available()) {delay(10);}
  String input = SerialBT.readString();  // Read the incoming data as a string
  updatePIDConstants(input); 

  analogWrite(PWMA, base_speed);
  analogWrite(PWMB, base_speed);
=======
  digitalWrite(BIN2, LOW);
  
  analogWrite(PWMA, 0);
  analogWrite(PWMB, 0);
  
  BLEDevice::init("Hermes_BLE");
  pServer_global = BLEDevice::createServer();
  pServer_global->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer_global->createService(SERVICE_UUID);

  pCharacteristicRX = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_RX,
                      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
                    );
  pCharacteristicRX->setCallbacks(new MyCharacteristicCallbacks());

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
>>>>>>> Stashed changes
}


void loop()
{
<<<<<<< Updated upstream
  
  if (SerialBT.available()) {  // Check if Bluetooth data is available
    String input = SerialBT.readString();  // Read the incoming data as a string
    updatePIDConstants(input);  // Function to parse and update Kp, Ki, Kd
  }
  
  /*
=======
  if (deviceConnected) {
  
>>>>>>> Stashed changes
  Serial.print("state: ");
  Serial.print(state);
  Serial.print("\taux: ");
  Serial.print(aux);
  Serial.print("\tesquerdo: ");
  Serial.print(analogRead(13));
  Serial.print("\tdireito: ");
  Serial.print(analogRead(39));
  Serial.print("\tatual: ");
  Serial.println(atual);
<<<<<<< Updated upstream
 */
  //boolean value
  //1 if reading white
  //0 if reading black
  r_read = analogRead(39) <= 2000;
  l_read = analogRead(13) <= 1000;
=======
  //boolean value
  //1 if reading white
  //0 if reading black
  r_read = analogRead(39) <= 100;
  l_read = analogRead(13) <= 100;
>>>>>>> Stashed changes
  switch (state){
    case 0:
      calc_turn();
      
      if(r_read){
        state++;
      }
<<<<<<< Updated upstream
      

=======

      if(analogRead(35) >= 2000 && analogRead(32) >= 2000 && analogRead(33) >= 2000 && analogRead(26) >= 2000 && analogRead(25) >= 2000 && analogRead(27) >= 2000){
        while (analogRead(33) >= 2000 && analogRead(25) >= 2000){
          analogWrite(PWMA, 0);
          analogWrite(PWMB, 100);
        }
      }
      
>>>>>>> Stashed changes
      break;
    case 1:
      calc_turn();

      if (!r_read)
        state++;

<<<<<<< Updated upstream
      break;
    case 2:
      calc_turn();
      
      
        if (r_read || l_read){//se o da direita ler branco
    
          if(atual == 0){//se for a primeira vez que o da direita leu branco, salva como referencia para subtrair depois
            atual = millis();
          }
        }

      if(atual != 0){
        if (millis() - atual >= 300){
          if(aux == 0){

          atual = 0;
          state ++;
          }else{

=======
      if(analogRead(35) >= 2000 && analogRead(32) >= 2000 && analogRead(33) >= 2000 && analogRead(26) >= 2000 && analogRead(25) >= 2000 && analogRead(27) >= 2000){
        while (analogRead(33) >= 2000 && analogRead(25) >= 2000){
          analogWrite(PWMA, 0);
          analogWrite(PWMB, 100);
        }
      }
      break;
    case 2:
      calc_turn();
   
      if (r_read || l_read){//se o da direita ler branco
        if(atual == 0){//se for a primeira vez que o da direita leu branco, salva como referencia para subtrair depois
          atual = millis();
        }
      }
      if(atual != 0){
        if (millis() - atual >= 100){
          if(aux == 0){
          atual = 0;
          state ++;
          }else{
>>>>>>> Stashed changes
          aux = 0;
          atual = 0;
          }
        }else{
<<<<<<< Updated upstream

=======
>>>>>>> Stashed changes
          if(l_read){//se o da esquerda ler branco, salva isso
            aux = 1;
          }
        }
      }

<<<<<<< Updated upstream
=======
      if(analogRead(35) >= 2000 && analogRead(32) >= 2000 && analogRead(33) >= 2000 && analogRead(26) >= 2000 && analogRead(25) >= 2000 && analogRead(27) >= 2000){
        while (analogRead(33) >= 2000 && analogRead(25) >= 2000){
          analogWrite(PWMA, 0);
          analogWrite(PWMB, 100);
        }
      }
>>>>>>> Stashed changes
      break;
    case 3:
      if (!r_read)
        state++;
<<<<<<< Updated upstream
=======
      if(analogRead(35) >= 2000 && analogRead(32) >= 2000 && analogRead(33) >= 2000 && analogRead(26) >= 2000 && analogRead(25) >= 2000 && analogRead(27) >= 2000){
        while (analogRead(33) >= 2000 && analogRead(25) >= 2000){
          analogWrite(PWMA, 0);
          analogWrite(PWMB, 100);
        }
      }
>>>>>>> Stashed changes
      break;
    case 4:
      analogWrite(PWMA, 0);
      analogWrite(PWMB, 0);
<<<<<<< Updated upstream
      while(!SerialBT.available()) {delay(10);}
      String input = SerialBT.readString();  // Read the incoming data as a string
      updatePIDConstants(input);
      state=0;
  }

//leave the curve
  if(analogRead(35) >= 2000 && analogRead(32) >= 2000 && analogRead(33) >= 2000 && analogRead(26) >= 2000 && analogRead(25) >= 2000 && analogRead(27) >= 2000){
    while (analogRead(33) >= 2000 && analogRead(25) >= 2000){
      analogWrite(PWMA, 0);
      analogWrite(PWMB, 100);
    }
  }
  
  delay(10);
=======
      break;
  }
  }else{
    analogWrite(PWMA, 0);
    analogWrite(PWMB, 0);
    if (oldDeviceConnected) {
        delay(500);
        Serial.println("Conexão perdida, advertising deveria ter sido reiniciado pelo callback.");
        oldDeviceConnected = deviceConnected;
    }
  }

  if (deviceConnected && !oldDeviceConnected) {
      oldDeviceConnected = deviceConnected;
  }
  if (!deviceConnected && oldDeviceConnected) {
      oldDeviceConnected = deviceConnected;
  }

  
>>>>>>> Stashed changes
}

int pid_calc()
{
  sensor_average = 0;
  sensor_sum = 0;

<<<<<<< Updated upstream
  

  for(int i = 0; i < n_sensores; i++)
  {
    sensor_read[i]=analogRead(sensor[i]);
    sensor_average += sensor_read[i]*pesos[i]*100;
    sensor_sum += sensor_read[i];
  }

  pos = int(sensor_average / sensor_sum);

  error = pos-sp;
  p = error;
  integral += p;
  integral = constrain(integral, integral_min, integral_max);
  d = p - lp;
  lp = p;
  
  return  int(Kp*p + Ki*integral + Kd*d);
=======
  for(int i = 0; i < n_sensores; i++) {
    sensor_read[i] = analogRead(sensor[i]);
    sensor_average += (long)sensor_read[i] * pesos[i] * 100;
    sensor_sum += sensor_read[i];
  }

  if (sensor_sum == 0) {
    if (lp > 0) return Kp * 1000;
    if (lp < 0) return Kp * -1000;
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
>>>>>>> Stashed changes
}

void calc_turn()
{
<<<<<<< Updated upstream
  correction = pid_calc();
  rspeed = base_speed - correction;
  lspeed = base_speed + correction;
  
  rspeed = constrain(rspeed, 0, 255);
  lspeed = constrain(lspeed, 0, 255);
=======
  if (!deviceConnected && state != 4) {
      analogWrite(PWMA, 0);
      analogWrite(PWMB, 0);
      return;
  }
  
  correction = pid_calc();
  rspeed = base_speed - correction;
  lspeed = base_speed + correction;

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

  //-----------------------------------------------------------------------------------------------------------
>>>>>>> Stashed changes
  
  analogWrite(PWMA, rspeed);
  analogWrite(PWMB, lspeed); 
}

<<<<<<< Updated upstream
void updatePIDConstants(String input) {
  input.trim();
  int kpIndex = input.indexOf("Kp=");
  int kiIndex = input.indexOf("Ki=");
  int kdIndex = input.indexOf("Kd=");
  
  if (kpIndex != -1 && kiIndex != -1 && kdIndex != -1) {
    String kpValue = input.substring(kpIndex + 3, input.indexOf(",", kpIndex));
    String kiValue = input.substring(kiIndex + 3, input.indexOf(",", kiIndex));
    String kdValue = input.substring(kdIndex + 3);

    Kp = kpValue.toFloat();
    Ki = kiValue.toFloat();
    Kd = kdValue.toFloat();

    SerialBT.print("Updated Kp: ");
    SerialBT.print(Kp);
    SerialBT.print(", Ki: ");
    SerialBT.print(Ki);
    SerialBT.print(", Kd: ");
    SerialBT.println(Kd);
  } else {
    SerialBT.println("Formato invalido! Esperado: Kp=0.5,Ki=0.0003,Kd=0.6");
=======
void updatePIDAndSpeedConstants(String input) {
  input.trim();
  Serial.print("Parsing BLE input: "); Serial.println(input);

  int currentPos = 0;
  bool updatedSomething = false;

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
            state = newState;
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
    if (state == 0 && deviceConnected) {
    }
>>>>>>> Stashed changes
  }
}
