#include <WiFi.h>
#include <WebServer.h>

// =====================================================
// WIFI CONFIGURATION
// =====================================================

const char* WIFI_SSID = "wifi_name";
const char* WIFI_PASSWORD = "wifi_password";

// =====================================================
// ESP32-CAM STREAM URL (UPDATED IP)
// =====================================================

const char* CAMERA_STREAM = "http://10.106.176.84:81/stream";

// =====================================================
// TB6612 MOTOR DRIVER PINS
// =====================================================

#define PWMA 25
#define AIN1 26
#define AIN2 27

#define PWMB 14
#define BIN1 32
#define BIN2 33

#define STBY 23

// =====================================================
// ULTRASONIC SENSOR (HC-SR04)
// =====================================================

#define TRIG_PIN 4
#define ECHO_PIN 18
#define DETECTION_DISTANCE 100

// =====================================================
// BUZZER & SERVO PINS
// =====================================================

#define BUZZER_PIN 5
#define SERVO_PIN 13

// =====================================================
// GLOBAL VARIABLES & WEB SERVER
// =====================================================

WebServer server(80);

int motorSpeed = 200;
bool theftMode = false;
bool alarmTriggered = false;
unsigned long lastSensorCheck = 0;

// =====================================================
// MOTOR FUNCTIONS
// =====================================================

void stopRobot() {
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, LOW);
  analogWrite(PWMA, 0);
  analogWrite(PWMB, 0);
}

void forwardRobot() {
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);
  analogWrite(PWMA, motorSpeed);
  analogWrite(PWMB, motorSpeed);
}

void backwardRobot() {
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, HIGH);
  analogWrite(PWMA, motorSpeed);
  analogWrite(PWMB, motorSpeed);
}

void leftRobot() {
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);
  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);
  analogWrite(PWMA, motorSpeed);
  analogWrite(PWMB, motorSpeed);
}

void rightRobot() {
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, HIGH);
  analogWrite(PWMA, motorSpeed);
  analogWrite(PWMB, motorSpeed);
}

// =====================================================
// ULTRASONIC SENSOR & THEFT DETECTION
// =====================================================

float getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(3);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return 999.0;
  return (duration * 0.0343) / 2.0;
}

void checkTheftSensor() {
  if (!theftMode) {
    digitalWrite(BUZZER_PIN, LOW);
    return;
  }

  if (millis() - lastSensorCheck < 100) return;
  lastSensorCheck = millis();

  float distance = getDistance();
  if (distance <= DETECTION_DISTANCE) {
    alarmTriggered = true;
  }

  if (alarmTriggered) {
    digitalWrite(BUZZER_PIN, HIGH);
  }
}

// =====================================================
// SERVO CONTROL
// =====================================================

void servoWriteAngle(int angle) {
  angle = constrain(angle, 0, 180);
  int pulseWidth = map(angle, 0, 180, 500, 2400);
  uint32_t duty = ((uint32_t)pulseWidth * 1023UL) / 20000UL;
  ledcWrite(SERVO_PIN, duty);
}

void servoLeft() { servoWriteAngle(30); }
void servoCenter() { servoWriteAngle(90); }
void servoRight() { servoWriteAngle(150); }

// =====================================================
// WEBPAGE UI
// =====================================================

const char MAIN_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>GARUD DOG ROVER</title>
<style>
* { box-sizing: border-box; }
body { margin: 0; background: #101010; color: white; font-family: Arial, sans-serif; text-align: center; }
.container { width: 100%; max-width: 480px; margin: auto; padding: 15px; }
h1 { font-size: 26px; margin: 10px 0 20px 0; }
h2 { margin-top: 25px; font-size: 20px; }
button { border: none; border-radius: 14px; font-weight: bold; color: white; background: #333; touch-action: manipulation; user-select: none; cursor: pointer; }
button:active { transform: scale(0.94); }

/* GRID ALIGNMENT FOR CONTROL BUTTONS */
.dpad {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 10px;
  max-width: 320px;
  margin: 15px auto;
}
.move {
  height: 65px;
  font-size: 26px;
  width: 100%;
}
.stop { background: #d62828; font-size: 16px; }

/* SPEED & STATUS */
.status { background: #242424; border-radius: 12px; padding: 15px; margin: 12px 0; font-size: 18px; }
input[type="range"] { width: 90%; }

/* CAMERA & AUDIO BUTTONS */
.cameraButton { width: 100%; max-width: 320px; height: 55px; background: #1976d2; font-size: 18px; }
.barkButton { width: 100%; max-width: 320px; height: 55px; background: #ff9800; font-size: 18px; margin-top: 5px; }
#cameraBox { display: none; margin-top: 15px; }
#cameraStream { width: 100%; border-radius: 12px; display: block; margin: auto; }

.theftGroup, .servoGroup {
  display: flex;
  justify-content: center;
  gap: 10px;
  max-width: 320px;
  margin: auto;
}
.theftOn { flex: 1; height: 50px; background: #b71c1c; font-size: 16px; }
.theftOff { flex: 1; height: 50px; background: #2e7d32; font-size: 16px; }
.servoButton { flex: 1; height: 50px; font-size: 16px; }
</style>
</head>
<body>
<div class="container">
<h1>GARUD DOG ROVER</h1>

<div class="status">Motor Speed: <span id="speedValue">200</span></div>
<input type="range" min="80" max="255" value="200" id="speedSlider" oninput="changeSpeed(this.value)">

<h2>ROVER CONTROL</h2>
<div class="dpad">
  <div></div>
  <button class="move" onpointerdown="move('forward')" onpointerup="stopRobot()" onpointercancel="stopRobot()" onpointerleave="stopRobot()">&#9650;</button>
  <div></div>
  <button class="move" onpointerdown="move('left')" onpointerup="stopRobot()" onpointercancel="stopRobot()" onpointerleave="stopRobot()">&#9664;</button>
  <button class="move stop" onclick="stopRobot()">STOP</button>
  <button class="move" onpointerdown="move('right')" onpointerup="stopRobot()" onpointercancel="stopRobot()" onpointerleave="stopRobot()">&#9654;</button>
  <div></div>
  <button class="move" onpointerdown="move('backward')" onpointerup="stopRobot()" onpointercancel="stopRobot()" onpointerleave="stopRobot()">&#9660;</button>
  <div></div>
</div>

<h2>AUDIO CONTROL</h2>
<button class="barkButton" onclick="playBarkSound()">&#128054; DOG BARK</button>

<h2>CAMERA</h2>
<button class="cameraButton" onclick="toggleCamera()">CAMERA</button>
<div id="cameraBox">
<h3>LIVE CAMERA</h3>
<img id="cameraStream" src="" alt="Camera Stream">
</div>

<h2>THEFT DETECTION</h2>
<div class="theftGroup">
  <button class="theftOn" onclick="theftOn()">THEFT ON</button>
  <button class="theftOff" onclick="theftOff()">THEFT OFF</button>
</div>
<div class="status">
Theft Mode: <span id="theftStatus">OFF</span><br><br>
Distance: <span id="distance">---</span> cm<br><br>
Alarm: <span id="alarmStatus">OFF</span>
</div>

<h2>DOG SERVO</h2>
<div class="servoGroup">
  <button class="servoButton" onclick="servo('left')">LEFT</button>
  <button class="servoButton" onclick="servo('center')">CENTER</button>
  <button class="servoButton" onclick="servo('right')">RIGHT</button>
</div>
</div>

<script>
function move(direction) { fetch("/move?dir=" + direction); }
function stopRobot() { fetch("/stop"); }
function changeSpeed(value) { document.getElementById("speedValue").innerText = value; fetch("/speed?value=" + value); }

function toggleCamera() {
  let box = document.getElementById("cameraBox");
  let stream = document.getElementById("cameraStream");
  if (box.style.display === "none" || box.style.display === "") {
    stream.src = "http://10.106.176.84:81/stream";
    box.style.display = "block";
  } else {
    stream.src = "";
    box.style.display = "none";
  }
}

// ACOUSTIC SYNTHESIZER FOR REALISTIC DOG BARK (WOOF WOOF!)
function playBarkSound() {
  const AudioContext = window.AudioContext || window.webkitAudioContext;
  if (!AudioContext) return;
  const ctx = new AudioContext();

  function triggerSingleBark(startTime) {
    // 1. Vocal Chord Tone (Rapid Pitch Drop)
    const osc1 = ctx.createOscillator();
    const osc1Gain = ctx.createGain();
    osc1.type = 'sawtooth';
    
    osc1.frequency.setValueAtTime(300, startTime);
    osc1.frequency.exponentialRampToValueAtTime(100, startTime + 0.15);

    osc1Gain.gain.setValueAtTime(0.8, startTime);
    osc1Gain.gain.exponentialRampToValueAtTime(0.01, startTime + 0.15);

    // 2. Chest Resonance Formant
    const osc2 = ctx.createOscillator();
    const osc2Gain = ctx.createGain();
    osc2.type = 'triangle';

    osc2.frequency.setValueAtTime(500, startTime);
    osc2.frequency.exponentialRampToValueAtTime(160, startTime + 0.13);

    osc2Gain.gain.setValueAtTime(0.6, startTime);
    osc2Gain.gain.exponentialRampToValueAtTime(0.01, startTime + 0.13);

    // 3. Throat Air / Breath Noise
    const bufferSize = ctx.sampleRate * 0.15;
    const noiseBuffer = ctx.createBuffer(1, bufferSize, ctx.sampleRate);
    const output = noiseBuffer.getChannelData(0);
    for (let i = 0; i < bufferSize; i++) {
      output[i] = Math.random() * 2 - 1;
    }

    const whiteNoise = ctx.createBufferSource();
    whiteNoise.buffer = noiseBuffer;

    const noiseFilter = ctx.createBiquadFilter();
    noiseFilter.type = 'bandpass';
    noiseFilter.frequency.setValueAtTime(850, startTime);
    noiseFilter.Q.setValueAtTime(1.8, startTime);

    const noiseGain = ctx.createGain();
    noiseGain.gain.setValueAtTime(0.5, startTime);
    noiseGain.gain.exponentialRampToValueAtTime(0.01, startTime + 0.14);

    // Routing
    osc1.connect(osc1Gain);
    osc2.connect(osc2Gain);
    whiteNoise.connect(noiseFilter);
    noiseFilter.connect(noiseGain);

    osc1Gain.connect(ctx.destination);
    osc2Gain.connect(ctx.destination);
    noiseGain.connect(ctx.destination);

    // Play
    osc1.start(startTime);
    osc2.start(startTime);
    whiteNoise.start(startTime);

    osc1.stop(startTime + 0.16);
    osc2.stop(startTime + 0.16);
    whiteNoise.stop(startTime + 0.16);
  }

  const now = ctx.currentTime;
  triggerSingleBark(now);
  triggerSingleBark(now + 0.22);
}

function theftOn() { fetch("/theft?state=on").then(() => { document.getElementById("theftStatus").innerText = "ON"; }); }
function theftOff() { fetch("/theft?state=off").then(() => { document.getElementById("theftStatus").innerText = "OFF"; document.getElementById("alarmStatus").innerText = "OFF"; }); }
function servo(direction) { fetch("/servo?dir=" + direction); }
function updateDistance() { fetch("/distance").then(response => response.text()).then(data => { document.getElementById("distance").innerText = data; }); }
function updateAlarm() { fetch("/alarm").then(response => response.text()).then(data => { document.getElementById("alarmStatus").innerText = data; }); }

setInterval(updateDistance, 500);
setInterval(updateAlarm, 500);
</script>
</body>
</html>
)rawliteral";

// =====================================================
// SERVER ROUTE HANDLERS
// =====================================================

void handleRoot() { server.send_P(200, "text/html; charset=UTF-8", MAIN_PAGE); }

void handleMove() {
  if (!server.hasArg("dir")) { server.send(400, "text/plain", "No direction"); return; }
  String direction = server.arg("dir");
  if (direction == "forward") forwardRobot();
  else if (direction == "backward") backwardRobot();
  else if (direction == "left") leftRobot();
  else if (direction == "right") rightRobot();
  else stopRobot();
  server.send(200, "text/plain", "OK");
}

void handleStop() { stopRobot(); server.send(200, "text/plain", "STOP"); }

void handleSpeed() {
  if (server.hasArg("value")) {
    motorSpeed = constrain(server.arg("value").toInt(), 0, 255);
  }
  server.send(200, "text/plain", String(motorSpeed));
}

void handleTheft() {
  if (!server.hasArg("state")) { server.send(400, "text/plain", "No state"); return; }
  String state = server.arg("state");
  if (state == "on") {
    theftMode = true;
    alarmTriggered = false;
    digitalWrite(BUZZER_PIN, LOW);
  } else {
    theftMode = false;
    alarmTriggered = false;
    digitalWrite(BUZZER_PIN, LOW);
  }
  server.send(200, "text/plain", "OK");
}

void handleDistance() {
  float distance = getDistance();
  if (distance >= 999) server.send(200, "text/plain", "---");
  else server.send(200, "text/plain", String(distance, 1));
}

void handleAlarm() {
  if (alarmTriggered) server.send(200, "text/plain", "TRIGGERED");
  else server.send(200, "text/plain", "OFF");
}

void handleServo() {
  if (!server.hasArg("dir")) { server.send(400, "text/plain", "No direction"); return; }
  String direction = server.arg("dir");
  if (direction == "left") servoLeft();
  else if (direction == "right") servoRight();
  else servoCenter();
  server.send(200, "text/plain", "SERVO OK");
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PWMA, OUTPUT); pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
  pinMode(PWMB, OUTPUT); pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, HIGH);
  stopRobot();

  pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  ledcAttach(SERVO_PIN, 50, 10);
  servoCenter();

  // Force clean reconnect to fetch new IP address
  WiFi.disconnect(true);
  delay(500);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  
  Serial.println("\nConnecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n================================");
  Serial.println("WiFi Connected Successfully!");
  Serial.print("ROBOT IP ADDRESS: ");
  Serial.println(WiFi.localIP());
  Serial.println("================================");

  server.on("/", handleRoot);
  server.on("/move", handleMove);
  server.on("/stop", handleStop);
  server.on("/speed", handleSpeed);
  server.on("/theft", handleTheft);
  server.on("/distance", handleDistance);
  server.on("/alarm", handleAlarm);
  server.on("/servo", handleServo);

  server.begin();
  Serial.println("GARUD DOG ROVER READY");
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop() {
  server.handleClient();
  checkTheftSensor();
}
