#include <BluetoothSerial.h>

BluetoothSerial SerialBT;

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
float base_speed = 185; // Velocidade base dos motores
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

int state = 0;                                    // Robô começa PARADO
int old_state_for_notification = -1;              // Para enviar notificação apenas quando o estado mudar
unsigned long last_state_send_time = 0;           // Para enviar estado periodicamente
const unsigned long state_send_interval = 1000;   // Intervalo para enviar estado (1 segundo)

unsigned long atual = 0;  // Timestamp para controle de tempo na máquina de estados
int aux = 0;              // Variável auxiliar para a lógica da máquina de estados

bool deviceConnected = false;
bool oldDeviceConnected = false;

int pid_calc();
void calc_turn();
void updatePIDConstants(String input);


void setup() {
  SerialBT.begin("LineFollower");
  Serial.begin(115200);
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
  
  sp = 0;
  atual = 0;
  integral = 0;
  lp = 0;

  while(!SerialBT.available()) {delay(10);}
  String input = SerialBT.readString();  // Read the incoming data as a string
  updatePIDConstants(input); 
}


void loop() {
  if (SerialBT.available()) {  // Check if Bluetooth data is available
    String input = SerialBT.readString();  // Read the incoming data as a string
    updatePIDConstants(input);  // Function to parse and update Kp, Ki, Kd
  }
  
  unsigned long currentTime = millis(); // Para envio periódico

    // --- LÓGICA PRINCIPAL DO ROBÔ ---
    r_lat_read = analogRead(sensorLat[1]) <= 3200; 
    l_lat_read = analogRead(sensorLat[0]) <= 3200;
    bool all_white = true; // Resetar a cada loop

   // int previous_state_for_debug = state; // Para debug de mudança de estado

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
  //delay(10);
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
        // Lógica: o robo deve virar para um lado para reencontrar a linha
///////////////////////////////////////////
        Serial.println(lp);
        float spinSpeed = base_speed + 30;
        if (spinSpeed>255) spinSpeed = 255;
        if(lp>0){
          while (analogRead(sensor[2]) >= 2000 && analogRead(sensor[3]) >= 2000) { // Timeout de 1.5s
              analogWrite(PWMA, 0);   
              analogWrite(PWMB, spinSpeed); 
              delay(10);
              Serial.println(lp);
          }
        }else{
          while (analogRead(sensor[2]) >= 2000 && analogRead(sensor[3]) >= 2000) { // Timeout de 1.5s
              analogWrite(PWMA, spinSpeed);   
              analogWrite(PWMB, 0); 
              delay(10);
              Serial.println(lp);
          }
        }
//////////////////////////////////////////////tentar com lp ao inves de correction
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
  
  // Se correction > 0, linha à direita, precisa virar à DIREITA: rspeed DIMINUI, lspeed AUMENTA
  // Se correction < 0, linha à esquerda, precisa virar à ESQUERDA: rspeed AUMENTA, lspeed DIMINUI
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

  
  analogWrite(PWMA, rspeed);
  analogWrite(PWMB, lspeed); 
}

void updatePIDConstants(String input) {
  input.trim();
  int kpIndex = input.indexOf("Kp=");
  int kiIndex = input.indexOf("Ki=");
  int kdIndex = input.indexOf("Kd=");
  int speedIndex = input.indexOf("Speed=");
  
  if (kpIndex != -1 && kiIndex != -1 && kdIndex != -1 && speedIndex != -1) {
    String kpValue = input.substring(kpIndex + 3, input.indexOf(",", kpIndex));
    String kiValue = input.substring(kiIndex + 3, input.indexOf(",", kiIndex));
    String kdValue = input.substring(kdIndex + 3, input.indexOf(",", kdIndex));
    String speedValue = input.substring(speedIndex + 3);


    Kp = kpValue.toFloat();
    Ki = kiValue.toFloat();
    Kd = kdValue.toFloat();
    base_speed = speedValue.toFloat();

    SerialBT.print("Updated Kp: ");
    SerialBT.print(Kp);
    SerialBT.print(", Ki: ");
    SerialBT.print(Ki);
    SerialBT.print(", Kd: ");
    SerialBT.print(Kd);
    SerialBT.print(", Speed: ");
    SerialBT.println(base_speed);
  } else {
    SerialBT.println("Formato invalido! Esperado: Kp=0.5,Ki=0.0003,Kd=0.6,Speed=185");
  }
}