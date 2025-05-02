const unsigned int sensor[] = {34, 35, 32, 33, 25, 26, 27, 14};
const int n_sensores = 8;

void setup() {
  for(int i=0; i<n_sensores; i++){
    pinMode(sensor[i], INPUT);
  }
  Serial.begin(115200);
}

void loop() {
  
  Serial.print(analogRead(34));
  Serial.print("\t");
  Serial.print(analogRead(35));
  Serial.print("\t");
  Serial.print(analogRead(32));
  Serial.print("\t");
  Serial.print(analogRead(33));
  Serial.print("\t");
  Serial.print(analogRead(25));
  Serial.print("\t");
  Serial.print(analogRead(26));
  Serial.print("\t");
  Serial.println(analogRead(27));
  Serial.print("\t");
  Serial.println(analogRead(14));

}
