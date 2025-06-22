#define ENCODER_A 23 // Pin for Encoder A
#define ENCODER_B 22 // Pin for Encoder B

#define ENCODER_C 15  // Pin for Encoder C
#define ENCODER_D 2 // Pin for Encoder D

volatile int encoder_value = 0; // Global variable for storing the encoder position
volatile int encoder_value2 = 0; // Global variable for storing the encoder position

void encoder_isr() {
  // Reading the current state of encoder A and B
  int A = digitalRead(ENCODER_A);
  int B = digitalRead(ENCODER_B);

  int C = digitalRead(ENCODER_C);
  int D = digitalRead(ENCODER_D);

  /*
  // If the state of A changed, it means the encoder has been rotated
  if ((A == HIGH) != (B == LOW)) {
    encoder_value--;
  } else {
    encoder_value++;
  }
  */

  // If the state of A changed, it means the encoder has been rotated
  if ((C == HIGH) != (D == LOW)) {
    encoder_value2--;
  } else {
    encoder_value2++;
  }

}

void setup() {
  Serial.begin(115200);
  pinMode(ENCODER_A, INPUT_PULLUP);
  pinMode(ENCODER_B, INPUT_PULLUP);
  pinMode(ENCODER_C, INPUT_PULLUP);
  pinMode(ENCODER_D, INPUT_PULLUP);

  // Attaching the ISR to encoder A
  //attachInterrupt(digitalPinToInterrupt(ENCODER_A), encoder_isr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_C), encoder_isr, CHANGE);
}

void loop() {
  // Serial.println("Encoder1 value: " + String(encoder_value) + "  |  Encoder2 value: " + String(encoder_value2));
  Serial.println("Encoder2 value: " + String(encoder_value2));
}