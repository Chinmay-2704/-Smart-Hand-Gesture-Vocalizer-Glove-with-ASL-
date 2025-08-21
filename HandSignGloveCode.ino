#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <SoftwareSerial.h>

SoftwareSerial BTSerial(0,1); // RX, TX

// MPU6050
Adafruit_MPU6050 mpu;

float maxGyro[3], minGyro[3]; // x, y, z
float maxAcc[3], minAcc[3];   // x, y, z
// I+a=J D+a=Z L+g=G L+g=Q 

// Flex thresholds (Thumb to Pinky)
const int thresholds[5] = {568, 640, 678, 688, 500};
const int flexPins[5] = {A6, A3, A2, A1, A0};  // Thumb to Pinky
const int touchPins[5] = {2, 3, 4, 5, 6};      // Thumb to Pinky
// C+2=O V+3=U V+2=R E+2=S E+4=T E+5=N E+6=M U+4=H

int getAverageFlexValue(int pin) {
  int total = 0;
  const int samples = 15;

  for (int i = 0; i < samples; i++) {
    total += analogRead(pin);
    delay(5); // Low delay for performance
  }
  return total / samples;
}


// SETUP MPU/TOUCH/SWITCH
void setup() {
  Serial.begin(115200);
  pinMode(12, INPUT_PULLUP);  // D12 input with pull-up resistor
  delay(1000);

  // MPU6050
  if (!mpu.begin()) {
    Serial.println("MPU6050 not detected!");
    while (1);
  }
  Serial.println("MPU6050 ready.");
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);

  // Touch sensor pins
  for (int i = 0; i < 5; i++) {
    pinMode(touchPins[i], INPUT_PULLUP); // HIGH = not touched, LOW = touched
  }
}


// SWITCH BETWEEN MODES
void loop() {
  if (digitalRead(12) == LOW) {
    Serial.println("Running ASL Mode...");
    BTSerial.println("A.S.L");
    while (digitalRead(12) == LOW) {
      asl_mode();  // Your ASL gesture processing loop
    }
  } else {
    Serial.println("Running INSTRUCTION Mode...");
     BTSerial.println("INSTRUCTION");
    instruction_mode();  // Instruction mode when D12 is not grounded
  }

  delay(100);  // Optional debounce delay
}



// ASL MODE FUNC
void asl_mode() {
  // --- Flex Sensor States ---
  int flexValues[5];
  for (int i = 0; i < 5; i++) {
    flexValues[i] = getAverageFlexValue(flexPins[i]);
  }

  

    // --- Touch Sensor States ---
  int touchStates[5];
  for (int i = 0; i < 5; i++) {
    if (digitalRead(touchPins[i]) == LOW) {
      touchStates[i] = 1;
    } else {
      touchStates[i] = 0;
    }
  }


  int thumbState   = (flexValues[0] < thresholds[0]) ? 1 : 0;
  int indexState   = (flexValues[1] < thresholds[1]) ? 1 : 0;
  int middleState  = (flexValues[2] < thresholds[2]) ? 1 : 0;
  int ringState    = (flexValues[3] < thresholds[3]) ? 1 : 0;
  int pinkyState   = (flexValues[4] < thresholds[4]) ? 1 : 0;

  // --- MPU6050 Reading Over 1 Second ---
  for (int i = 0; i < 3; i++) {
    maxGyro[i] = maxAcc[i] = -1000;
    minGyro[i] = minAcc[i] = 1000;
  }

  unsigned long tStart = millis();
  while (millis() - tStart < 1000) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    float gyro[3] = {g.gyro.x, g.gyro.y, g.gyro.z};
    float acc[3]  = {a.acceleration.x, a.acceleration.y, a.acceleration.z};

    for (int i = 0; i < 3; i++) {
      if (gyro[i] > maxGyro[i]) maxGyro[i] = gyro[i];
      if (gyro[i] < minGyro[i]) minGyro[i] = gyro[i];

      if (acc[i] > maxAcc[i]) maxAcc[i] = acc[i];
      if (acc[i] < minAcc[i]) minAcc[i] = acc[i];
    }
    delay(50);
  }

  float deltaGyro[3], deltaAcc[3];
  for (int i = 0; i < 3; i++) {
    deltaGyro[i] = maxGyro[i] - minGyro[i];
    deltaAcc[i]  = maxAcc[i] - minAcc[i];
  }
     // --- Print Flex Sensor Values ---
  Serial.print("Flex Values (Thumb to Pinky): ");
  for (int i = 0; i < 5; i++) {
    Serial.print(flexValues[i]);
    Serial.print(" ");
  }
  Serial.println();

    Serial.print("Bend States (Thumb to Pinky): ");
    Serial.print(thumbState);
    Serial.print(" ");
    Serial.print(indexState);
    Serial.print(" ");
    Serial.print(middleState);
    Serial.print(" ");
    Serial.print(ringState);
    Serial.print(" ");
    Serial.println(pinkyState);


  // --- Print Touch Sensor Values ---
  Serial.println("Touch Sensor States (Thumb to Pinky):");
  for (int i = 0; i < 5; i++) {
    Serial.print("D");
    Serial.print(touchPins[i]);
    Serial.print(": ");
    Serial.println(touchStates[i]);
  }


  // --- Print Gyroscope Deltas ---
  Serial.print("Gyroscope Δ (x y z): ");
  for (int i = 0; i < 3; i++) {
    Serial.print(deltaGyro[i], 4);  // 4 decimal places
    Serial.print(" ");
  }
  Serial.println();

  // --- Print Accelerometer Deltas ---
  Serial.print("Accelerometer Δ (x y z): ");
  for (int i = 0; i < 3; i++) {
    Serial.print(deltaAcc[i], 4);
    Serial.print(" ");
  }
  Serial.println();

  Serial.println("-----------------------------");


  // ==============================
  //   LETTER DETECTION LOGIC
  // ==============================
  
  // --- Handle Gesture-Based Commands ---
  static String currentText = "";  // Keep track of output text

  // SPACE: Significant movement along X-axis
  if (deltaAcc[0] > 10.0) {
    Serial.print("Current Word: ");
    Serial.println(currentText);
    BTSerial.println(currentText);
  }

  // BACKSPACE: Significant movement along Y-axis
  else if (deltaAcc[1] > 10.0 && currentText.length() > 0) {
    currentText.remove(currentText.length() - 1);
    Serial.println("Backspace");
  }


    // ---- Letter A ----
    // Thumb straight, others bent
    if (thumbState == 0 && indexState == 1 && middleState == 1 && ringState == 1 && pinkyState == 1) {
      if (touchStates[2] == 1) {
        Serial.println("T");  // A + 4
         currentText += "T";
      } else {
        Serial.println("A");
         currentText += "A";
      } 
    }
    

    // ---- Letter B ----
    // Thumb bent, all others straight
    if (thumbState == 1 && indexState == 0 && middleState == 0 && ringState == 0 && pinkyState == 0) {
      Serial.println("B");
       currentText += "B";
    }

    // ---- Letter D ----
    // Index straight, others bent
    if (thumbState == 1 && indexState == 0 && middleState == 1 && ringState == 1 && pinkyState == 1) {
      if (deltaAcc[0] > 2.0) {
        Serial.println("Z");  // D + a = Z
         currentText += "Z";
      } else {
        Serial.println("D");
         currentText += "D";
      }
    }

    // ---- Letter E ----
    // All bent
    if (thumbState == 1 && indexState == 1 && middleState == 1 && ringState == 1 && pinkyState == 1) {
      if (touchStates[0] == 1) {
        Serial.println("E");  // E + 2
         currentText += "E";
      } else if (touchStates[3] == 1) {
        Serial.println("N");  // E + 5
         currentText += "N";
      } else if (touchStates[4] == 1) {
        Serial.println("M");  // E + 5
         currentText += "M";
      } else {
        Serial.println("S");
         currentText += "S";
      }
    }
    

    // ---- Letter F ----
    // Thumb and index make a circle, middle-ring-pinky straight
    if (thumbState == 1 && indexState == 1 && middleState == 0 && ringState == 0 && pinkyState == 0) {
      Serial.println("F");
       currentText += "F";
    }

    // ---- Letter I ----
    // Pinky straight, rest bent
    if (thumbState == 1 && indexState == 1 && middleState == 1 && ringState == 1 && pinkyState == 0) {
      if (deltaAcc[0] > 2.0) {
        Serial.println("J");  // I + a = J
         currentText += "J";
      } else {
        Serial.println("I");
         currentText += "I";
      }
    }

    // ---- Letter K ----
    // Index and middle straight, thumb bent
    if (thumbState == 0 && indexState == 0 && middleState == 0 && ringState == 1 && pinkyState == 1) {
      Serial.println("K");
       currentText += "K";
    }

    // ---- Letter L ----
    // Thumb and index straight, rest bent
    if (thumbState == 0 && indexState == 0 && middleState == 1 && ringState == 1 && pinkyState == 1) {
      if (deltaAcc[1] > 3 ) {
        Serial.println("Q");  // L + g
         currentText += "Q";
      } else if (deltaAcc[1] > 1 && deltaAcc[1] < 3) {
        Serial.println("G");  // L + g (other axis)
         currentText += "G";
      } else {
        Serial.println("L");
         currentText += "L";
      }
    }
    /*

    // ---- Letter V ----
    // Index and middle straight, rest bent
    if (thumbState == 1 && indexState == 0 && middleState == 0 && ringState == 1 && pinkyState == 1) {
      if (touchStates[1]==1 && touchStates[2] == 1) {
        Serial.println("H");  // V + D2 touch = H
         currentText += "H";
      } else if (touchStates[1] == 1 && touchStates[2]==0) {
        Serial.println("U");  // V + 3
         currentText += "U";
      } else if (touchStates[0] == 1) {
        Serial.println("R");  // V + 2
         currentText += "R";
      } else {
        Serial.println("V");
         currentText += "V";
      }
    }


    // ---- Letter W ----
    // Index, middle, pinky straight
    if (thumbState == 1 && indexState == 0 && middleState == 0 && ringState == 0 && pinkyState == 1) {
      Serial.println("W");
       currentText += "W";
    }

    // ---- Letter Y ----
    // Thumb and pinky straight, rest bent
    if (thumbState == 0 && indexState == 1 && middleState == 1 && ringState == 1 && pinkyState == 0) {
      Serial.println("Y");
       currentText += "Y";
    }

    // ---- Letter C (based on flex curve) ----
  // All fingers slightly bent but not tightly curled
  if (flexValues[0] > 600 && flexValues[0] < 750 &&  // thumb
      flexValues[1] > 650 && flexValues[1] < 800 &&  // index
      flexValues[2] > 670 && flexValues[2] < 830 &&  // middle
      flexValues[3] > 680 && flexValues[3] < 850 &&  // ring
      flexValues[4] > 660 && flexValues[4] < 820) {  // pinky

    if (touchStates[0] == 1) {
      Serial.println("O");  // C + D2 touch
       currentText += "O";
    } else {
      Serial.println("C");
       currentText += "C";
    }
  }
  // ---- Letter X ----
  // Index slightly bent, rest tightly bent
  if (flexValues[1] > 600 && flexValues[1] < 680 &&  // index partially bent
      flexValues[0] > 700 &&  // thumb tight
      flexValues[2] > 700 &&  // middle tight
      flexValues[3] > 700 &&  // ring tight
      flexValues[4] > 700) {  // pinky tight
    Serial.println("X");
     currentText += "X";
  }
  // ---- Letter P ----
  // Thumb and middle straight, index slightly bent, others curled
  if (flexValues[0] < 600 &&       // thumb straight
      flexValues[1] > 600 && flexValues[1] < 680 &&  // index slightly bent
      flexValues[2] < 600 &&       // middle straight
      flexValues[3] > 700 &&       // ring bent
      flexValues[4] > 700) {       // pinky bent
    Serial.println("P");
     currentText += "P";
  }
  */

  Serial.println("---------------------------");
  delay(1000);
}

void instruction_mode() {
    int flexValues[5];
    bool bent[5] = {false, false, false, false, false}; // Track which fingers are bent

    // Read flex sensor values
    for (int i = 0; i < 5; i++) {
        flexValues[i] = analogRead(flexPins[i]);
        bent[i] = (flexValues[i] <= thresholds[i]); // Mark as bent if <= threshold
    }

    // Identify gestures based on explicit finger bending conditions
    String gesture = "";
    
    if (bent[1] && !bent[0] && !bent[2] && !bent[3] && !bent[4]) gesture = "Food";
    else if (bent[2] && !bent[0] && !bent[1] && !bent[3] && !bent[4]) gesture = "Water";
    else if (bent[1] && bent[2] && bent[3] && !bent[4] && bent[0]) gesture = "Washroom";
    else if (!bent[0] && !bent[1] && !bent[2] && bent[3] && bent[4]) gesture = "Pain";
    else if (bent[0] && !bent[1] && !bent[2] && bent[3] && bent[4]) gesture = "Medicine";
    else if (bent[0] && bent[1] && bent[2] && bent[3] && bent[4]) gesture = "Emergency";
    else if (!bent[0] && !bent[1] && !bent[2] && bent[3] && !bent[4]) gesture = "Hot";
    else if (!bent[0] && !bent[1] && !bent[2] && !bent[3] && bent[4]) gesture = "Cold";
    else if (!bent[0] && bent[1] && bent[2] && bent[3] && bent[4]) gesture = "Yes";
    else if (bent[0] && !bent[1] && bent[2] && bent[3] && bent[4]) gesture = "No";
    else if (bent[0] && !bent[1] && !bent[2] && !bent[3] && bent[4]) gesture = "Need Help";
    else if (bent[0] && !bent[1] && !bent[2] && !bent[3] && !bent[4]) gesture = "Call the Doctor";
    else if (bent[0] && bent[1] && !bent[2] && !bent[3] && !bent[4]) gesture = "Feeling Good";
    else if (bent[0] && bent[1] && bent[2] && !bent[3] && !bent[4]) gesture = "Thank You";
    else if (bent[0] && !bent[1] && bent[2] && bent[3] && !bent[4]) gesture = "Turn On the Lights";
    else if (!bent[0] && !bent[1] && bent[2] && bent[3] && !bent[4]) gesture = "Turn Off the Lights";
    
    // Print flex sensor values
    Serial.print("Flex Values: ");
    for (int i = 0; i < 5; i++) {
        Serial.print(flexValues[i]);
        Serial.print(" ");
    }
    Serial.println();

    // Send gesture to Bluetooth and Serial only if a valid gesture is detected
    if (gesture != "") {
        Serial.println(gesture);
        BTSerial.println(gesture);
    }

    delay(1000); // 1-second delay

}