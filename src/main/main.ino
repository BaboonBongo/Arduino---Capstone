#include "HX711.h"

const int LOADCELL_DOUT_PIN = 3;
const int LOADCELL_SCK_PIN = 2;

HX711 scale;

// VARIABEL KONTROL 
float calibration_factor = 414.20; 
float smoothedWeight = 0.0;
float unitWeight = 0.0; 

const float alpha = 0.1; 

unsigned long lastPrintTime = 0;

void setup() {
  Serial.begin(9600);

  Serial.println("\n=== SISTEM MULAI ===");
  Serial.println("Mencoba terhubung ke modul HX711...");
  
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor);
  
  Serial.println("Melakukan Tare (Nol) awal...");
  Serial.println("(Jika program berhenti di sini, cek kabel DT/SCK!)");

  scale.tare(); 

  Serial.println("\n=== TIMBANGAN DIGITAL READY ===");
  Serial.println("Ketik perintah lalu tekan ENTER:");
  Serial.println("[T] Tare | [K] Kalibrasi | [E] Set Berat Satuan");
  Serial.println("-----------------------------------------------");
  Serial.println("Memulai pengiriman data JSON...");
}

void loop() {
  // 1. SISTEM MENU SERIAL
  if (Serial.available() > 0) {
    String inputString = Serial.readStringUntil('\n');
    inputString.trim();
    
    if (inputString.length() > 0) {
      char cmd = toupper(inputString[0]);
      
      if (cmd == 'T') {
        scale.tare();
        smoothedWeight = 0; // Reset memori filter agar angka langsung ke 0
        Serial.println("\n[!] Timbangan di-nol-kan.");
      } 
      else if (cmd == 'K') {
        startCalibration();
      }
      else if (cmd == 'E') {
        startCountingSetup();
      }
    }
  }

  if (scale.is_ready()) {

    float rawWeight = scale.get_units(3); 
    //filter EMA
    smoothedWeight = (alpha * rawWeight) + ((1.0 - alpha) * smoothedWeight);

    if (abs(smoothedWeight) < 0.5) {
      smoothedWeight = 0.0;
    }

    if (millis() - lastPrintTime >= 500) {
      // Hitung jumlah barang
      int jumlahBarang = (unitWeight > 0 && smoothedWeight > (unitWeight * 0.5)) 
                          ? round(smoothedWeight / unitWeight) 
                          : 0;

      Serial.print("{\"berat\": ");
      Serial.print(abs(smoothedWeight), 2); 
      Serial.print(", \"satuan\": ");
      Serial.print(unitWeight, 2);
      Serial.print(", \"jumlah\": ");
      Serial.print(jumlahBarang);
      Serial.println("}");
      
      lastPrintTime = millis();
    }
  }
}

// FUNGSI KALIBRASI
void startCalibration() {
  Serial.println("\n--- MODE KALIBRASI ---");
  Serial.println("1. Kosongkan timbangan, lalu ketik sembarang huruf dan tekan ENTER...");
  
  // Menunggu Enter
  while (Serial.available() == 0); 
  Serial.readStringUntil('\n');
  
  scale.tare();
  smoothedWeight = 0;
  Serial.println("   [OK] Timbangan sudah nol.");

  Serial.println("2. Letakkan beban uji, masukkan beratnya (gram), lalu ENTER:");
 
  while (Serial.available() == 0);
  float realWeight = Serial.parseFloat();
  Serial.readStringUntil('\n');
  
  if (realWeight > 0) {
 
    long reading = scale.get_value(15); 
    calibration_factor = (float)reading / realWeight;
    scale.set_scale(calibration_factor);
    
    Serial.print("   [SUKSES] Faktor Kalibrasi Baru: ");
    Serial.println(calibration_factor);
  } else {
    Serial.println("   [GAGAL] Input berat tidak valid.");
  }
}

// FUNGSI HITUNG BARANG 
void startCountingSetup() {
  Serial.println("\n--- SETUP BERAT SATUAN ---");
  Serial.println("Masukkan berat 1 buah barang (gram), lalu tekan ENTER:");
  
  while (Serial.available() == 0);
  unitWeight = Serial.parseFloat();
  Serial.readStringUntil('\n');

  
  Serial.print("   [OK] Berat satuan diset ke: ");
  Serial.print(unitWeight);
  Serial.println(" g");
}
