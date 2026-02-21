// ==========================================
// DNA-Style Error Correction Communication
// Architecture: Fully Recursive 8-Bit Frames
// ==========================================
// Pin Definitions:
// Pin 13: Clock (S1)
// Pin 12: Status (S2)
// Pin 6:  Data Bit (X)
// Pin 5:  Inverse Data Bit (X-Bar)
// ==========================================

// Forward Declarations
void sendfun(String str);
void send_8bits(int* bits);
int recieve_8bits(int* read_byte);
void errorfun(int* read_byte, int* errorbits, int count);
int errorcorr(int* bitVal);
void sendtrans(char str, int* bitval); 
char translate(int* read_byte);
String reading();

// Global Variables
int read_byte[9]; 

// Helper: Convert bits to char
char translate(int* read_byte) {
  char asciiChar = 0; 
  for (int i = 0; i < 8; i++) {
    asciiChar = (asciiChar << 1) | read_byte[i];
  }
  return asciiChar;
}

// Helper: Convert char to bits
void sendtrans(char str, int* bitval) {
  for (int j = 7; j >= 0; j--) {
    bitval[7 - j] = (str >> j) & 1;
  }
}

// ==========================================
// CORE TRANSMISSION ENGINES (Recursive)
// ==========================================

// [RECURSION UPDATE] Unified Send Engine
void send_8bits(int* bits) {
  pinMode(13, OUTPUT);
  pinMode(12, INPUT); 
  pinMode(6, OUTPUT);
  pinMode(5, OUTPUT);
  digitalWrite(13, HIGH);

  for (int j = 7; j >= 0; j--) {
    digitalWrite(13, LOW);
    digitalWrite(6, bits[7 - j]);
    delayMicroseconds(10);  // this small delay in sending is to prevent errors by the processor when he reads.
    digitalWrite(5, !bits[7 - j]);
    delayMicroseconds(50); 
    digitalWrite(13, HIGH);
    delayMicroseconds(50);
  }
  
  // Wait for Receiver to process and set ACK/NACK
  delay(5); 

  if (digitalRead(12) == LOW) {
    // Error flagged by receiver! Enter correction mode (can recurse)
    errorcorr(bits);
  }
  
  // Ensure we revert to sender pins when exiting deep recursion
  pinMode(13, OUTPUT);
  pinMode(12, INPUT); 
  pinMode(6, OUTPUT);
  pinMode(5, OUTPUT);
  digitalWrite(13, HIGH);
}

// [RECURSION UPDATE] Unified Receive Engine
int recieve_8bits(int* read_byte) {
  int error_bits[8];
  int k = 0; 
  
  pinMode(13, INPUT_PULLUP);
  pinMode(12, OUTPUT);
  pinMode(6, INPUT); 
  pinMode(5, INPUT);
  
  for (int i = 0; i < 8; i++) {
    long timeout = millis();
    while (digitalRead(13) == HIGH) {
      if (millis() - timeout > 1000) return 2; // Timeout
    }
    
    delayMicroseconds(20); // Stabilize

    if (digitalRead(13) == LOW) {
      int x = digitalRead(5); // Inverse
      delayMicroseconds(10);  // this small delay in sending is to prevent errors by the processor when he reads.
      int y = digitalRead(6); // Data
      
      if (x != y) {
        read_byte[i] = y; // Valid Bit
      } else {
        error_bits[k] = i; // Corrupted Bit
        k++;
      }
    }
    while (digitalRead(13) == LOW) {
      delayMicroseconds(5);
    }
  }
  
  if (k == 0) {
    digitalWrite(12, HIGH); // Success
    return 1; 
  } else {
    digitalWrite(12, LOW); // Error
    delay(10); // Hold error signal slightly longer to ensure Sender sees it
    
    // Call error handler (which uses send_8bits, triggering recursion if needed)
    errorfun(read_byte, error_bits, k);
    return 0; // Returns 0 but `read_byte` is now fixed by errorfun!
  }
}

// ==========================================
// ERROR CORRECTION PROTOCOL
// ==========================================

// RECEIVER SIDE: Builds mask, sends it, gets corrections
void errorfun(int* read_byte, int* errorbits, int count) {
  // 1. Build the 8-bit mask (1 = corrupt, 0 = skip)
  int mask[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  for (int x = 0; x < count; x++) {
    mask[errorbits[x]] = 1;
  }

  // 2. Send the mask. 
  // [RECURSION MAGIC]: If this mask gets corrupted on the wire, 
  // send_8bits automatically handles the error check and recursion!
  send_8bits(mask);

  // 3. Receive the corrected data frame
  int corrections[8];
  int status = recieve_8bits(corrections);
  
  // 4. Patch the corrupted bits
  if (status != 2) { // As long as it didn't time out
    for (int j = 0; j < 8; j++) {
      if (mask[j] == 1) { 
         read_byte[j] = corrections[j]; 
      }
    }
  }
}

// SENDER SIDE: Gets mask, sends true values
int errorcorr(int *original_bits) {
  int mask[8];

  // 1. Receive the mask
  // [RECURSION MAGIC]: If the mask arrives corrupted, recieve_8bits 
  // will pull D12 LOW and trigger errorfun, swapping roles seamlessly!
  int status = recieve_8bits(mask);

  if (status != 2) {
    // 2. Send the original bits back so receiver can extract the corrections
    // We send the whole byte; the receiver only uses the bits defined in its mask.
    send_8bits(original_bits);
  }

  return 0;
} 

// ==========================================
// USER & LOOP LOGIC
// ==========================================

void sendfun(String str) {
  for (int i = 0; i < str.length(); i++) {
    int bitVal[8]; 
    sendtrans(str[i], bitVal); 
    send_8bits(bitVal);
  }
}

String reading() {
  if (Serial.available() > 0) {
    return Serial.readString();
  }
  return ""; 
}

void setup() {
  Serial.begin(9600); 
  pinMode(13, INPUT_PULLUP);
  pinMode(12, OUTPUT);
  pinMode(6, INPUT); 
  pinMode(5, INPUT);
}

void loop() {
  String mode = reading();
  
  // === SENDER MODE ===
  if(mode == "@"){ 
    pinMode(12, INPUT); 
    delay(5);
    
    String strToSend = "";
    bool active = true;

    Serial.println(F("\n--- Sender Mode Active ---"));

    while(active){
      strToSend = reading();
      if(strToSend.length() > 0){
          if (strToSend.indexOf('!') != -1) {
             sendfun("!"); 
             active = false;
          } else {
             sendfun(strToSend); 
          }
      }
    } 
    
    Serial.println(F("\n--- Sender Mode Ended ---"));
    pinMode(13, INPUT_PULLUP);
    pinMode(12, OUTPUT);
  }

  // === RECEIVER MODE ===
  if(digitalRead(13) == LOW){
    do {
      int status = recieve_8bits(read_byte);
      if (status == 2) break; // Timeout break
      
      // Whether status is 1 (Success) or 0 (Corrected by recursion), 
      // read_byte now contains valid data!
      char c = translate(read_byte);
      if (c == '!') break; 
      
      Serial.print(c);
    } while (true);
  }
}