// ==========================================
//  DNA-LINK PROTOCOL (Final Recursive Version)
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
void send_8bits(int* bits);
int recievefun(int* read_byte, int* error_bits);
void errorfun(int* read_byte, int* errorbits, int count);
void sendtrans(char str, int* bitval);
char translate(int* read_byte);
int errorcorr(int* bitVal);
String reading();

void setup() {
  Serial.begin(9600);
  Serial.setTimeout(50);  // Fast timeout for responsive typing

  // Default State: Receiver Mode
  pinMode(13, INPUT_PULLUP);  // Listen for Clock
  pinMode(12, OUTPUT);        // Error Signal Driver
  pinMode(6, INPUT);          // Data Input
  pinMode(5, INPUT);          // Data Bar Input

  Serial.println(F("System Ready. Type '@' to enter Sender Mode."));
}

void loop() {
  // 1. CHECK SERIAL FOR COMMANDS
  if (Serial.available() > 0) {
    String cmd = reading();
    cmd.trim(); 

    if (cmd.indexOf('@') != -1) {
      Serial.println(F("\n--- SENDER MODE ACTIVE ---"));
      Serial.println(F("Type text to send. Type '!' to exit."));

      pinMode(13, OUTPUT);
      digitalWrite(13, HIGH);  
      pinMode(12, INPUT);      
      pinMode(6, OUTPUT);
      pinMode(5, OUTPUT);

      bool active = true;
      while (active) {
        if (Serial.available() > 0) {
          String strToSend = reading();
          
          if (strToSend.length() > 0) {
            
            if (strToSend.indexOf('!') != -1) {
              // Extract any text typed BEFORE the '!' and send it first
              String msg = strToSend.substring(0, strToSend.indexOf('!'));
              if (msg.length() > 0) {
                // Force a newline if it doesn't have one
                if (msg.indexOf('\n') == -1) msg += '\n';
                sendfun(msg);
              }
              
              sendfun("!"); // Send the exit command to receiver
              active = false; // Exit sender mode
              
            } else {
              // Normal text sending: ensure it always has a newline attached
              if (strToSend.indexOf('\n') == -1) {
                strToSend += '\n';
              }
              sendfun(strToSend);
            }
            
          }
        }
      }

      pinMode(13, INPUT_PULLUP);
      pinMode(12, OUTPUT);
      pinMode(6, INPUT);
      pinMode(5, INPUT);
      Serial.println(F("--- SENDER MODE ENDED ---"));
    }
  }

  // 2. CHECK PINS FOR INCOMING DATA
  if (digitalRead(13) == LOW) {
    delayMicroseconds(5); 
    
    if (digitalRead(13) == LOW) {
      while (true) {
        int status = recievefun(read_byte, error_bits);

        if (status == 2) break;

        char c = translate(read_byte);

        if (c == '!') {
          Serial.println();  
          break;
        }

        Serial.print(c);
      }
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

// ==========================================
//  CORE TRANSMISSION ENGINES (Recursive)
// ==========================================

// -------------------------------------------------
//  UNIFIED SEND FUNCTION (Sends exactly 8 bits)
// -------------------------------------------------
void send_8bits(int* bits) {
  // Ensure Sender Pin Configuration
  pinMode(13, OUTPUT);
  pinMode(12, INPUT);
  pinMode(6, OUTPUT);
  pinMode(5, OUTPUT);
  digitalWrite(13, HIGH);

  for (int j = 0; j < 8; j++) {
    // 1. Clock LOW (Start Bit)
    digitalWrite(13, LOW);

    // 2. Write Data
    digitalWrite(5, bits[j]);
    // Error prevention: Send Inverse on Pin 6
    digitalWrite(6, !bits[j]);  

    delayMicroseconds(50);

    // 3. Clock HIGH (End Bit)
    digitalWrite(13, HIGH);
    delayMicroseconds(50);
  }

  // Wait for Receiver to process (Error Check)
  delay(5);

  if (digitalRead(12) == LOW) {
    // Receiver signaled Error (Low). Trigger Correction!
    errorcorr(bits);
  }
}

// -------------------------------------------------
//  STRING WRAPPER FOR SENDING
// -------------------------------------------------
void sendfun(String str) {
  for (int i = 0; i < str.length(); i++) {
    int bitVal[8];
    sendtrans(str[i], bitVal);
    send_8bits(bitVal);
  }
}

// -------------------------------------------------
//  RECEIVE FUNCTION
//  Returns: 1 = Success, 0 = Error Detected, 2 = Timeout
// -------------------------------------------------
int recievefun(int* read_byte, int* error_bits) {
  pinMode(13, INPUT_PULLUP);
  pinMode(12, OUTPUT);
  pinMode(6, INPUT);
  pinMode(5, INPUT);

  k = 0; 

  for (int i = 0; i < 8; i++) {
    unsigned long timeout = millis();
    while (digitalRead(13) == HIGH) {
      // INCREASED TIMEOUT to 1500ms to allow for the 500ms hardware restoration gap
      if (millis() - timeout > 1500) return 2;  
    }

    // Wait 20us for the electrical signal to stabilize
    delayMicroseconds(20);

    if (digitalRead(13) == LOW) {
      int x = digitalRead(5);
      int y = digitalRead(6);

      if (x != y) {
        read_byte[i] = x;
      } else {
        read_byte[i] = 0;
        error_bits[k] = i;
        k++;
      }
    }
    
    // Safely wait for the clock to go HIGH again with a timeout
    unsigned long low_timeout = millis();
    while (digitalRead(13) == LOW) {
      if (millis() - low_timeout > 1500) return 2; // Increased to 1500ms safety limit
      delayMicroseconds(5);
    }
  }

  if (k == 0) {
    digitalWrite(12, HIGH);  
    return 1;
  } else {
    digitalWrite(12, LOW);  
    delay(500);             // INCREASED: 0.5 second gap to let physical connection restore
    errorfun(read_byte, error_bits, k);
    return 0; 
  }
}

// ==========================================
//  ERROR CORRECTION PROTOCOL
// ==========================================

// -------------------------------------------------
//  RECEIVER SIDE: Builds Mask, Sends Mask, Receives Fix
// -------------------------------------------------
void errorfun(int* read_byte, int* errorbits, int count) {
  // 1. Build the 8-bit mask (1 = corrupt, 0 = skip)
  int mask[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  for (int x = 0; x < count; x++) {
    if (errorbits[x] >= 0 && errorbits[x] < 8) {
      mask[errorbits[x]] = 1;
    }
  }

  // 2. Send the mask using the unified recursive engine!
  send_8bits(mask);

  // 3. Listen for the entire corrected byte from sender
  int read_corrected[8];
  int error_corrected[8];

  int status = recievefun(read_corrected, error_corrected);

  // 4. Apply fix (only updating the corrupted indices)
  if (status != 2) { 
    for (int i = 0; i < 8; i++) {
      if (mask[i] == 1) {
        read_byte[i] = read_corrected[i];
      }
    }
  }
}

// -------------------------------------------------
//  SENDER SIDE: Receives Mask, Re-Sends Original Bits
// -------------------------------------------------
int errorcorr(int* bitVal) {
  int mask[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  int dummy_err[8];

  // 1. Receive the mask using unified recursive engine!
  int status = recievefun(mask, dummy_err);

  if (status != 2) {
    // 2. Sender safely re-sends the entire original byte. 
    // The receiver will use its own mask to pluck out only the bits it needs!
    send_8bits(bitVal);
    //Serial.println(F("\n[Network healed] Error corrected dynamically."));
  }

  return 0;
}
