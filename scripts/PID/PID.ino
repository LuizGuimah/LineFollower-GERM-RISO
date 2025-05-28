#include <BluetoothSerial.h>

BluetoothSerial SerialBT;

#define PWMA 4 
#define AIN2 16
#define AIN1 17
#define STBY 5
#define BIN1 18
#define BIN2 19
#define PWMB 21

const unsigned int sensor[] = {35, 32, 33, 25, 26, 27};
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

float Kp = 2.4;
float Ki = 0.002;
float Kd = 7;
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
  pinMode(STBY, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(PWMB, OUTPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);

  analogWrite(PWMA, 0);
  analogWrite(PWMB, 0);
  digitalWrite(STBY, HIGH);
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW); 
  
  Serial.begin(115200);
  
  sp = 0;
  while(!SerialBT.available()) {delay(10);}
  String input = SerialBT.readString();  // Read the incoming data as a string
  updatePIDConstants(input); 

  analogWrite(PWMA, base_speed);
  analogWrite(PWMB, base_speed);
}


void loop()
{
  
  if (SerialBT.available()) {  // Check if Bluetooth data is available
    String input = SerialBT.readString();  // Read the incoming data as a string
    updatePIDConstants(input);  // Function to parse and update Kp, Ki, Kd
  }
  
  /*
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
 */
  //boolean value
  //1 if reading white
  //0 if reading black
  r_read = analogRead(39) <= 2000;
  l_read = analogRead(13) <= 1000;
  switch (state){
    case 0:
      calc_turn();
      
      if(r_read){
        state++;
      }
      

      break;
    case 1:
      calc_turn();

      if (!r_read)
        state++;

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

          aux = 0;
          atual = 0;
          }
        }else{

          if(l_read){//se o da esquerda ler branco, salva isso
            aux = 1;
          }
        }
      }

      break;
    case 3:
      if (!r_read)
        state++;
      break;
    case 4:
      analogWrite(PWMA, 0);
      analogWrite(PWMB, 0);
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
}

int pid_calc()
{
  sensor_average = 0;
  sensor_sum = 0;

  

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
}

void calc_turn()
{
  correction = pid_calc();
  rspeed = base_speed - correction;
  lspeed = base_speed + correction;
  
  rspeed = constrain(rspeed, 0, 255);
  lspeed = constrain(lspeed, 0, 255);
  
  analogWrite(PWMA, rspeed);
  analogWrite(PWMB, lspeed); 
}

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
  }
}
