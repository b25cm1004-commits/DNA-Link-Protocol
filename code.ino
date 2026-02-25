// ==========================================
//  DNA-LINK PROTOCOL (Ultimate Stability)
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
int errorfun(int* read_byte, int* errorbits, int count); 
void sendtrans(char str, int* bitval);
char translate(int* read_byte);
int errorcorr(int* bitVal);
String reading();

void setup() {
  Serial.begin(9600);
  Serial.setTimeout(50);  

  // Default State: Receiver Mode
  pinMode(13, INPUT_PULLUP);  // Listen for Clock
  pinMode(12, OUTPUT);        // Error Signal Driver
  pinMode(6, INPUT_PULLUP);   
  pinMode(5, INPUT_PULLUP);   

  Serial.println(F("System Ready. Type '@' to enter Sender Mode."));
}

void loop() {
  // 1. CHECK SERIAL FOR COMMANDS
  if (Serial.available() > 0) {
    String cmd = reading();
    cmd.trim(); 

    if (cmd.indexOf('@') != -1) {
      Serial.println(F("\n--- SENDER MODE ACTIVE ---"));
      Serial.println(F("Type text to send. Type a solo '!' and press Enter to exit."));

      pinMode(13, OUTPUT);
      digitalWrite(13, HIGH);  
      // FIX: PULLUP prevents the Sender from freezing if the error wire is pulled!
      pinMode(12, INPUT_PULLUP);      
      pinMode(6, OUTPUT);
      pinMode(5, OUTPUT);

      bool active = true;
      while (active) {
        if (Serial.available() > 0) {
          String strToSend = reading();
          
          if (strToSend.length() > 0) {
            String checkStr = strToSend;
            checkStr.trim(); 
            
            if (checkStr == "!") {
              sendfun(String((char)4)); 
              active = false; 
            } else {
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
      pinMode(6, INPUT_PULLUP);
      pinMode(5, INPUT_PULLUP);
      Serial.println(F("--- SENDER MODE ENDED ---"));
    }
  }

  // 2. CHECK PINS FOR INCOMING DATA
  if (digitalRead(13) == LOW) {
    delayMicroseconds(50); 
    
    if (digitalRead(13) == LOW) {
      while (true) {
        int status = recievefun(read_byte, error_bits);

        // Abort completely if healing failed/timed out
        if (status == 2) break;

        char c = translate(read_byte);

        if (c == 4) {
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

char translate(int* read_byte) {
  char asciiChar = 0;
  for (int i = 0; i < 8; i++) {
    asciiChar = (asciiChar << 1) | read_byte[i];
  }
  return asciiChar;
}

void sendtrans(char str, int* bitval) {
  for (int j = 7; j >= 0; j--) {
    bitval[7 - j] = (str >> j) & 1;
  }
}

// ==========================================
//  CORE TRANSMISSION ENGINES (Recursive)
// ==========================================

void send_8bits(int* bits) {
  pinMode(13, OUTPUT);
  // FIX: PULLUP prevents phantom errors
  pinMode(12, INPUT_PULLUP); 
  pinMode(6, OUTPUT);
  pinMode(5, OUTPUT);
  digitalWrite(13, HIGH);

  for (int j = 0; j < 8; j++) {
    digitalWrite(13, LOW);
    digitalWrite(5, bits[j]);
    digitalWrite(6, !bits[j]);  
    delayMicroseconds(1000);
    digitalWrite(13, HIGH);
    delayMicroseconds(1000);
  }

  delay(15);
  if (digitalRead(12) == LOW) {
    errorcorr(bits);
  }
}

void sendfun(String str) {
  for (int i = 0; i < str.length(); i++) {
    int bitVal[8];
    sendtrans(str[i], bitVal);
    send_8bits(bitVal);
  }
}

int recievefun(int* read_byte, int* error_bits) {
  pinMode(13, INPUT_PULLUP);
  pinMode(12, OUTPUT);
  pinMode(6, INPUT_PULLUP); 
  pinMode(5, INPUT_PULLUP); 

  k = 0; 

  for (int i = 0; i < 8; i++) {
    unsigned long timeout = millis();
    while (digitalRead(13) == HIGH) {
      if (millis() - timeout > 3000) return 2;  
    }

    delayMicroseconds(500);

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
    
    unsigned long low_timeout = millis();
    while (digitalRead(13) == LOW) {
      if (millis() - low_timeout > 3000) return 2; 
      delayMicroseconds(50); 
    }
  }

  if (k == 0) {
    digitalWrite(12, HIGH);  
    return 1; 
  } else {
    digitalWrite(12, LOW);  
    delay(1000);            
    int heal_status = errorfun(read_byte, error_bits, k);
    return heal_status; 
  }
}

// ==========================================
//  ERROR CORRECTION PROTOCOL
// ==========================================

int errorfun(int* read_byte, int* errorbits, int count) {
  int mask[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  for (int x = 0; x < count; x++) {
    if (errorbits[x] >= 0 && errorbits[x] < 8) {
      mask[errorbits[x]] = 1;
    }
  }

  send_8bits(mask);

  int read_corrected[8];
  int error_corrected[8];

  int status = recievefun(read_corrected, error_corrected);

  if (status != 2) { 
    for (int i = 0; i < 8; i++) {
      if (mask[i] == 1) {
        read_byte[i] = read_corrected[i];
      }
    }
    return 1; 
  }
  
  return 2; 
}

int errorcorr(int* bitVal) {
  int mask[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  int dummy_err[8];

  int status = recievefun(mask, dummy_err);

  if (status != 2) {
    delay(100); 
    send_8bits(bitVal);
  }

  return 0;
}
