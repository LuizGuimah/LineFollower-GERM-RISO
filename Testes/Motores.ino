#define PWMA 4 
#define AIN2 17
#define AIN1 16
#define STBY 5
#define BIN1 18
#define BIN2 19
#define PWMB 21

void setup() {
  //motors
  pinMode(STBY, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(PWMB, OUTPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);

  analogWrite(PWMA, 255);
  analogWrite(PWMB, 255);
  digitalWrite(STBY, HIGH);
  
  Serial.begin(115200);

}

void loop() {
  delay(1000);
  
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);
  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW); 
  analogWrite(PWMA, 255);
  analogWrite(PWMB, 255);

  delay(1000);

  for(int i = 5;i<20;i++){
    analogWrite(PWMA, i*12);
    analogWrite(PWMB, i*12);

    delay(1000);
  }
  delay(1000);
/*
  Serial.println("CHEGOU");
  delay(500);

  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, HIGH);

  delay(500);
*/
}
