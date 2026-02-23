// ==========================================
//  Bidirectional Serial Comms (Fixed)
//  s1 (Clock) = D13
//  s2 (Error) = D12
//  x  (Data)  = D6
//  x_bar      = D5
// ==========================================

// --- Globals ---
int k = 0;          // Error index counter
int read_byte[8];   // Buffer for current byte being read
int error_bits[8];  // Buffer for error indices

// --- Forward Declarations ---
void sendfun(String str);
int recievefun(int* read_byte, int* error_bits);
void errorfun(int* read_byte, int* errorbits, int count);
void sendtrans(char str, int* bitval);
char translate(int* read_byte);
int errorcorr(int* bitVal);
String reading();

void setup() {
  Serial.begin(9600);
  Serial.setTimeout(100);  // Faster serial reading

  // Default State: Receiver Mode
  pinMode(13, INPUT_PULLUP);  // Listen for Clock
  pinMode(12, OUTPUT);        // Error Signal Driver
  pinMode(6, INPUT);          // Data Input
  pinMode(5, INPUT);          // Data Bar Input

  Serial.println(F("System Ready. Type '@' to enter Sender Mode."));
}

void loop() {
  // -------------------------------------------------
  // 1. CHECK SERIAL FOR COMMANDS (Sender Mode)
  // -------------------------------------------------
  if (Serial.available() > 0) {
    String input = reading();
    String cmd = input;
    cmd.trim();  // Remove \n or spaces

    if (cmd == "@") {
// --- ENTER SENDER MODE ---
start_loop:
      Serial.println(F("--- SENDER MODE ACTIVE ---"));
      Serial.println(F("Type text to send. Type '!' to exit."));

      // Switch Pins to Output
      pinMode(13, OUTPUT);
      digitalWrite(13, HIGH);  // Idle High
      pinMode(12, INPUT);      // Listen for Error
      pinMode(6, OUTPUT);
      pinMode(5, OUTPUT);

      bool active = true;
      while (active) {
        String strToSend = reading();
        if (strToSend.length() > 0) {
          // Check for exit command inside the loop
          if (strToSend.indexOf('!') != -1) {
            sendfun("!");
            active = false;
          } else {
            sendfun(strToSend);
          }
        }
      }

      // --- EXIT SENDER MODE ---
      // Restore Receiver Pins
      pinMode(13, INPUT_PULLUP);
      pinMode(12, OUTPUT);
      pinMode(6, INPUT);
      pinMode(5, INPUT);
      Serial.println(F("--- SENDER MODE ENDED ---"));
    }
  }
  if (digitalRead(13) == LOW) {
    goto end_loop;
  }
  if (Serial.available() > 0) {  // comment by het fix
    String input = reading();
    String cmd = input;
    cmd.trim();  // Remove \n or spaces
    if (cmd == "@") {
      // Jump back to the label
      goto start_loop;
    }
  }
  // -------------------------------------------------
  // 2. CHECK PINS FOR INCOMING DATA (Receiver Mode)
  // -------------------------------------------------
  // If Pin 13 goes LOW, data is coming
  if (digitalRead(13) == LOW) {
end_loop:
    
    while (true) {
      int status = recievefun(read_byte, error_bits);

      // Status 2 = Timeout (Sender stopped or disconnected)
      if (status == 2) break;

      char c = translate(read_byte);

      // Check for End of Transmission character
      if (c == '!') {
        Serial.println();  // Newline at end of message
        break;
      }

      Serial.print(c);
    }
  }
}
// ==========================================
//  HELPER FUNCTIONS
// ==========================================

String reading() {
  if (Serial.available() > 0) {
    return Serial.readString();
  }
  return "";
}

// Convert Array of Bits -> Char
char translate(int* read_byte) {
  char asciiChar = 0;
  for (int i = 0; i < 8; i++) {
    asciiChar = (asciiChar << 1) | read_byte[i];
  }
  return asciiChar;
}

// Convert Char -> Array of Bits
void sendtrans(char str, int* bitval) {
  for (int j = 7; j >= 0; j--) {
    bitval[7 - j] = (str >> j) & 1;
  }
}

// -------------------------------------------------
//  RECEIVE FUNCTION
//  Returns: 1 = Success, 0 = Error Detected, 2 = Timeout
// -------------------------------------------------
int recievefun(int* read_byte, int* error_bits) {
  // Ensure pins are set correctly (redundant safety)
  pinMode(13, INPUT_PULLUP);
  pinMode(12, OUTPUT);
  pinMode(6, INPUT);
  pinMode(5, INPUT);

  k = 0;  // Reset error count for this byte

  for (int i = 0; i < 8; i++) {
    unsigned long timeout = millis();

    // Wait for Clock LOW (Data Start)
    while (digitalRead(13) == HIGH) {
      if (millis() - timeout > 500) return 2;  // Return Timeout if waiting too long
    }

    // Slight delay to let data stabilize
    delayMicroseconds(20);

    // Read Data
    if (digitalRead(13) == LOW) {
      int x = digitalRead(5);
      int y = digitalRead(6);

      // Logic: x should be inverse of y. x is the data bit.
      if (x != y) {
        read_byte[i] = x;
      } else {
        read_byte[i] = 0;   // Default to 0 on error
        error_bits[k] = i;  // Record error index
        k++;
      }
    }

    // Wait for Clock HIGH (Data End)
    while (digitalRead(13) == LOW) {
      delayMicroseconds(5);
    }
  }

  // Check Error Status
  if (k == 0) {
    digitalWrite(12, HIGH);  // No Error
    return 1;
  } else {
    digitalWrite(12, LOW);  // Error Detected
    delay(5);               // Hold signal for Sender
    errorfun(read_byte, error_bits, k);
    return 0;
  }
}

// -------------------------------------------------
//  SEND FUNCTION
// -------------------------------------------------
void sendfun(String str) {
  // Ensure Sender Pin Configuration
  pinMode(13, OUTPUT);
  pinMode(12, INPUT);
  pinMode(6, OUTPUT);
  pinMode(5, OUTPUT);
  digitalWrite(13, HIGH);

  for (int i = 0; i < str.length(); i++) {
    int bitVal[8];
    sendtrans(str[i], bitVal);

    for (int j = 7; j >= 0; j--) {
      // 1. Clock LOW (Start Bit)
      digitalWrite(13, LOW);

      // 2. Write Data
      digitalWrite(5, bitVal[7 - j]);
      digitalWrite(6, !bitVal[7 - j]);  // Inverse logic

      delayMicroseconds(50);

      // 3. Clock HIGH (End Bit)
      digitalWrite(13, HIGH);
      delayMicroseconds(50);
    }

    // Wait for Receiver to process (Error Check)
    delay(5);

    if (digitalRead(12) == HIGH) {
      // Receiver signaled Success (High)
      continue;
    } else {
      // Receiver signaled Error (Low)
      errorcorr(bitVal);
    }
  }
}

// -------------------------------------------------
//  ERROR HANDLING (Receive Side - BITMASK UPDATED)
// -------------------------------------------------
void errorfun(int* read_byte, int* errorbits, int count) {
  // 1. Build the 8-bit mask (1 = corrupt, 0 = skip)
  int mask[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  for (int x = 0; x < count; x++) {
    if (errorbits[x] >= 0 && errorbits[x] < 8) {
      mask[errorbits[x]] = 1;
    }
  }

  // 2. Temporarily act as Sender to transmit the mask back
  pinMode(13, OUTPUT);
  pinMode(12, INPUT);
  pinMode(6, OUTPUT);
  pinMode(5, OUTPUT);
  digitalWrite(13, HIGH);

  // Send the 8 bits of the mask manually to avoid overhead
  for (int j = 0; j < 8; j++) {
    digitalWrite(13, LOW);
    digitalWrite(6, mask[j]);     // Data
    digitalWrite(5, !mask[j]);    // Inverse
    delayMicroseconds(50);
    digitalWrite(13, HIGH);
    delayMicroseconds(50);
  }
  delay(5); // Give sender time to process

  // 3. Revert to Receiver mode to get the corrected bits
  pinMode(13, INPUT_PULLUP);
  pinMode(12, OUTPUT);
  pinMode(6, INPUT);
  pinMode(5, INPUT);

  // 4. Listen for the incoming corrected bits
  int read_corrected[8];
  int error_corrected[8];

  for (int i = 0; i < 8; i++) {
    if (mask[i] == 1) { // Only listen if we asked for a correction for this index
      recievefun(read_corrected, error_corrected);
      
      char correction = translate(read_corrected);
      // Apply fix
      if (correction == '0') {
        read_byte[i] = 0;
      } else if (correction == '1') {
        read_byte[i] = 1;
      }
    }
  }
}

// -------------------------------------------------
//  ERROR CORRECTION (Sender Side - BITMASK UPDATED)
// -------------------------------------------------
int errorcorr(int* bitVal) {
  int mask[8] = {0, 0, 0, 0, 0, 0, 0, 0};

  // 1. Switch to Receiver Mode temporarily to hear the mask
  pinMode(13, INPUT_PULLUP);
  pinMode(12, OUTPUT);
  pinMode(6, INPUT);
  pinMode(5, INPUT);

  // Safe read loop for the 8-bit mask
  for (int i = 0; i < 8; i++) {
    unsigned long timeout = millis();
    while (digitalRead(13) == HIGH) {
      if (millis() - timeout > 1000) break; // Timeout safety
    }
    
    delayMicroseconds(20);
    
    if (digitalRead(13) == LOW) {
      mask[i] = digitalRead(6); // Read the mask bit
    }
    
    while (digitalRead(13) == LOW) {
      delayMicroseconds(5);
    }
  }

  // 2. Switch back to Sender Mode to send the specific bit corrections
  pinMode(13, OUTPUT);
  pinMode(12, INPUT);
  pinMode(6, OUTPUT);
  pinMode(5, OUTPUT);
  digitalWrite(13, HIGH);
  delay(5);

  // 3. Re-send corrected data for every '1' found in the mask
  for (int i = 0; i < 8; i++) {
    if (mask[i] == 1) {
      if (bitVal[i] == 0) sendfun("0");
      else sendfun("1");
    }
  }

  // Restore Sender Mode fully before returning to main send loop
  pinMode(13, OUTPUT);
  pinMode(12, INPUT);
  pinMode(6, OUTPUT);
  pinMode(5, OUTPUT);
  digitalWrite(13, HIGH);
  
  Serial.println(F("Error in bits sent was encountered \n Successfully sent the corrected bits"));
  return 0;
}
