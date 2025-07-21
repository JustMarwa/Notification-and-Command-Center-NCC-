#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <SD.h>
#include <SPI.h>
#include <ESP_Mail_Client.h>
#include <HTTPClient.h>  // Added for HTTP client functionality
#include <ESP32Time.h>



// WiFi credentials
const char* ssid = "Wifi17";
const char* password = "12345678";


// Email settings
#define SMTP_HOST "smtp.gmail.com"
// Choose ONE port configuration:
#define SMTP_PORT 587  // TLS port (STARTTLS)
//#define SMTP_PORT 465  // SSL port
#define AUTHOR_EMAIL "eee4113group17@gmail.com"
#define AUTHOR_PASSWORD "cact jsfh fsip olti"
#define RECIPIENT_EMAIL "christinawardenemail@gmail.com"
//#define SMTP_HOST "smtp.gmail.com"
//#define SMTP_PORT 587
//#define SMTP_PORT (SSL) 465
//Predator Text
#define PREDATOR_TEXT "Predator attack"


// Image server settings
#define IMAGE_SERVER_URL "http://predator-camera-server/capture"  // Replace with actual image server URL
#define IMAGE_FILENAME "/predator.jpg"
#define IMAGE_FETCH_DELAY 3000  // Delay in ms before fetching the image after detection (3 seconds)


// GPIO Pins
const int predatorDetectedPin = 35;  // Interrupt pin for predator detection
const int predatorGonePin = 14;      // Interrupt pin for predator gone
const int deterrentControlPin = 5;   // GPIO to control deterrent system


// Variables
volatile bool predatorDetected = false;
volatile unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50; // Debounce time in milliseconds
bool fetchingImage = false;
unsigned long imageRequestTime = 0;


// Email client
SMTPSession smtp;
Session_Config config;
SMTP_Message message;

//File details
File predator_attack_history;
String dataframe;//data to be saved to file i.e date_time + description
String fileheader = "Timestamp,Description"; //csv file column headers



// Create AsyncWebServer object on port 80
AsyncWebServer server(80);


// ESP32 RTC settings
ESP32Time rtc(7200);  // offset in seconds GMT+2


// Interrupt Service Routine for predator detection
void IRAM_ATTR predatorDetectedISR() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    predatorDetected = true;
    lastDebounceTime = millis();
  }
}


// Interrupt Service Routine for predator gone
void IRAM_ATTR predatorGoneISR() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    predatorDetected = false;
    lastDebounceTime = millis();
  }
}


// Function to fetch predator image from server
bool fetchPredatorImage() {
  HTTPClient http;
  Serial.println("Fetching predator image from server...");
  
  // Begin HTTP connection to the image server
  //http.begin(http::\x2F\x2F192.168.184.199);
  http.begin("http://192.168.184.199");

  // Send GET requestzz
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    
    // Get image data
    if (httpResponseCode == HTTP_CODE_OK) {
      // Get the image data
      WiFiClient *stream = http.getStreamPtr();
      
      // Open file on SD card for writing
      File file = SD.open(IMAGE_FILENAME, FILE_WRITE);
      if (!file) {
        Serial.println("Failed to open file for writing");
        http.end();
        return false;
      }
      
      // Write the image data to the file
      size_t totalBytes = http.getSize();
      size_t writtenBytes = 0;
      const size_t bufferSize = 512;
      uint8_t buffer[bufferSize];
      
      while (http.connected() && (writtenBytes < totalBytes)) {
        size_t availableBytes = stream->available();
        if (availableBytes > 0) {
          size_t readBytes = stream->readBytes(buffer, min(availableBytes, bufferSize));
          file.write(buffer, readBytes);
          writtenBytes += readBytes;
        }
        yield();
      }
      
      file.close();
      Serial.println("Image saved to SD card");
      http.end();
      return true;
    }
  } else {
    Serial.print("Error on HTTP request. Error code: ");
    Serial.println(httpResponseCode);
  }
  
  http.end();
  return false;
}


// Function to send email notification
void sendEmailNotification(bool isPredatorDetected) {
  message.clear();
 
  // Set the message headers
  message.sender.name = "ESP32 Predator Alert System";
  message.sender.email = AUTHOR_EMAIL;
 
  message.subject = isPredatorDetected ? "ALERT: Predator Detected!" : "INFO: Predator Gone";
  message.addRecipient("Warden", RECIPIENT_EMAIL);
 
  // Set the message content
  String textMsg = isPredatorDetected ?
    "A predator has been detected in your area. The deterrent system has been activated." :
    "The predator is no longer detected. The deterrent system has been deactivated.";
 
  message.text.content = textMsg.c_str();
  message.text.charSet = "us-ascii";
 
  // Connect to server with the session config
  if (!smtp.connect(&config)) {
    Serial.println("Connection failed");
    return;
  }
 
  // Send email
  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.println("Error sending Email, " + smtp.errorReason());
  } else {
    Serial.println("Email sent successfully");
  }
}


// Function to handle predator detection state changes
void handlePredatorState() {
  static bool lastPredatorState = false;
 
  if (predatorDetected != lastPredatorState) {
    lastPredatorState = predatorDetected;
   
    if (predatorDetected) {
      Serial.println("Predator detected!");
      digitalWrite(deterrentControlPin, HIGH); // Activate deterrent
      sendEmailNotification(true);

      //Save the attack on SD Card
      save_predator_attack();
      
      // Set flag to fetch image and record the time
      fetchingImage = true;
      imageRequestTime = millis();
      
    } else {
      Serial.println("Predator gone");
      digitalWrite(deterrentControlPin, LOW);  // Deactivate deterrent
      sendEmailNotification(false);
    }
  }
  
  // Check if it's time to fetch the image
  if (fetchingImage && (millis() - imageRequestTime >= IMAGE_FETCH_DELAY)) {
    fetchingImage = false;
    bool imageSuccess = fetchPredatorImage();
    
    if (imageSuccess) {
      Serial.println("Successfully fetched and saved predator image");
    } else {
      Serial.println("Failed to fetch predator image");
    }
  }
}


//Function to read time and date from RTC
String get_date_time(){

  return rtc.getDateTime();

}

//Function to set up the history file
void setup_files(){
  //Set up the predator history file
  predator_attack_history = SD.open("/Predator_attack_history.txt", FILE_WRITE);
  if (predator_attack_history) {
    predator_attack_history.println(fileheader);
    predator_attack_history.close();
    Serial.println("Success, data written to Predator_attack_history.txt");
  } else {
    Serial.println("Error, couldn't not open Predator_attack_history.txt");
  }
}




//Function to save the data to the file
void save_predator_attack(){
  // Write data to predator history file
  predator_attack_history = SD.open("/Predator_attack_history.txt", FILE_APPEND);
  if (predator_attack_history) {
    dataframe = get_date_time() + "," + PREDATOR_TEXT;
    predator_attack_history.println(dataframe);
    predator_attack_history.close();
    Serial.println("Success, data written to Predator_attack_history.txt");
  } else {
    Serial.println("Error, couldn't not open Predator_attack_history.txt");
  }


}

// Function to generate HTML content based on predator status
String generateHTML() {
  String html = "<!DOCTYPE html>\n";
  html += "<html>\n";
  html += "<head>\n";
  html += "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
  html += "  <style>\n";
  html += "    body { font-family: Arial; text-align: center; margin: 0; padding: 20px; }\n";
  html += "    .normal { background-color: #f0f8ff; color: #000; }\n";
  html += "    .alert { background-color: #ff4500; color: #fff; animation: blinker 1s linear infinite; }\n";
  html += "    @keyframes blinker { 50% { opacity: 0.8; } }\n";
  html += "    h1 { margin-bottom: 20px; }\n";
  html += "    .status-box { border-radius: 10px; padding: 20px; margin-bottom: 20px; }\n";
  html += "    img { max-width: 100%; height: auto; margin-top: 20px; border-radius: 10px; }\n";
  html += "  </style>\n";
  html += "  <script>\n";
  html += "    function refreshStatus() {\n";
  html += "      fetch('/status')\n";
  html += "        .then(response => response.json())\n";
  html += "        .then(data => {\n";
  html += "          document.body.className = data.predatorDetected ? 'alert' : 'normal';\n";
  html += "          document.getElementById('status-box').className = 'status-box ' + (data.predatorDetected ? 'alert' : 'normal');\n";
  html += "          document.getElementById('status-text').innerText = data.predatorDetected ? 'DANGER: PREDATOR DETECTED!' : 'Status: Normal';\n";
  html += "          document.getElementById('predator-image').style.display = data.predatorDetected ? 'block' : 'none';\n";
  html += "          if (data.predatorDetected) {\n";
  html += "            document.getElementById('predator-image').src = '/predator.jpg?t=' + new Date().getTime();\n";
  html += "          }\n";
  html += "        });\n";
  html += "    }\n";
  html += "    setInterval(refreshStatus, 1000);\n";
  html += "    window.onload = refreshStatus;\n";
  html += "  </script>\n";
  html += "</head>\n";
  html += "<body class=\"" + String(predatorDetected ? "alert" : "normal") + "\">\n";
  html += "  <h1>Predator Detection System</h1>\n";
  html += "  <div id=\"status-box\" class=\"status-box " + String(predatorDetected ? "alert" : "normal") + "\">\n";
  html += "    <h2 id=\"status-text\">" + String(predatorDetected ? "DANGER: PREDATOR DETECTED!" : "Status: Normal") + "</h2>\n";
  html += "  </div>\n";
  html += "  <img id=\"predator-image\" src=\"" + String(predatorDetected ? "/predator.jpg" : "") + "\" " + String(predatorDetected ? "" : "style=\"display:none;\"") + ">\n";
  html += "</body>\n";
  html += "</html>\n";
 
  return html;
}


void setup() {
  // Initialize serial communication
  Serial.begin(115200);
 
  // Configure GPIO pins
  pinMode(predatorDetectedPin, INPUT);
  pinMode(predatorGonePin, INPUT);
  pinMode(deterrentControlPin, OUTPUT);
 
  // Set initial state of deterrent system
  digitalWrite(deterrentControlPin, LOW);
 
  // Attach interrupts
  attachInterrupt(digitalPinToInterrupt(predatorDetectedPin), predatorDetectedISR, RISING);
  attachInterrupt(digitalPinToInterrupt(predatorGonePin), predatorGoneISR, RISING);
 
  // Initialize SD card
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }

  //Set the current time
  // rtc.setTime(second, minute, hour, date, month, year)
  rtc.setTime(30, 37, 21, 17, 5, 2025); //17th May 2025(Set the current date whenever you run the script)

  // Set up files
  setup_files();
 
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
 
  Serial.println("SD Card Initialized");
 
  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
 
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
 
  // Print local IP address
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
 
  // Set up email configuration
  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;
  config.login.email = AUTHOR_EMAIL;
  config.login.password = AUTHOR_PASSWORD;
  config.login.user_domain = "";
 
  // Configure web server routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", generateHTML());
  });
 
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{\"predatorDetected\":" + String(predatorDetected ? "true" : "false") + "}";
    request->send(200, "application/json", json);
  });
 
  server.on("/predator.jpg", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SD, "/predator.jpg", "image/jpeg");
  });
 
  // Start server
  server.begin();
}


void loop() {
  // Handle predator detection state changes
  handlePredatorState();
 
  // Add any additional periodic tasks here
  delay(100);
}
