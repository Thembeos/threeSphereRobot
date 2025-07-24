#include <SPI.h>
#include <WiFi101.h>
#include <Wire.h>
#include <MotorDriver.h>

#define MAX_PWM 2000
#define DEFAULT_PWM 500

MotorDriver motor(NO_R_REMOVED);

char ssid[] = "flushFactory 2.4";
char pass[] = "Q2K5@r)Z6p0rP2!";

int pwmVal = DEFAULT_PWM;
String statusMsg = "Idle";

WiFiServer server(80);

// Custom command structure
struct MotorCmd {
  const char* path;
  int motorID;
  int pwm;
  const char* msg;
};

const char *HTML_PAGE = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <title>Motor Control</title>
    <style>
        button {
            width: 100px;
            height: 50px;
            margin: 5px;
            font-size: 16px;
        }
        .row {
            display: flex;
            justify-content: center;
            gap: 10px;
            margin-bottom: 10px;
        }
        .motor {
            margin: 20px;
            padding: 10px;
            border: 1px solid #ccc;
            border-radius: 5px;
            background-color: #f9f9f9;
        }
    </style>
</head>
<body>
    <h2>Motor Control</h2>
    <div class="motor">
        <button onclick="cmd('/m1/cw', 'm1')">M1 CW</button>
        <button onclick="cmd('/m1/ccw', 'm1')">M1 CCW</button>
        <button onclick="cmd('/m1/stop', 'm1')">M1 Stop</button>
        <form>
            <label for="speed1">Speed (0 to 100):</label>
            <input type="range" id="speed1" min="0" max="100" value="25" oninput="updateSpeed('1')">
        </form>
    </div>
    <div class="motor">
        <button onclick="cmd('/m2/cw', 'm2')">M2 CW</button>
        <button onclick="cmd('/m2/ccw', 'm2')">M2 CCW</button>
        <button onclick="cmd('/m2/stop', 'm2')">M2 Stop</button>
        <form>
            <label for="speed2">Speed (0 to 100):</label>
            <input type="range" id="speed2" min="0" max="100" value="25" oninput="updateSpeed('2')">
        </form>
    </div>
    <p>
        M1 Status: <span id="m1status">Idle</span><br>
        M2 Status: <span id="m2status">Idle</span>
    </p>
    <script>
        function cmd(path, motor) {
            fetch(path)
                .then(r => r.text())
                .then(t => {
                    document.getElementById(`${motor}status`).innerText = t;
                })
                .catch(e => alert('Error: ' + e));
        }

        function updateSpeed(motor) {
            const speed = document.getElementById(`speed${motor}`).value;
            fetch(`/m${motor}/speed/${speed}`)
                .then(r => r.text())
                .then(t => {
                    document.getElementById(`m${motor}status`).innerText = t;
                })
                .catch(e => alert('Error: ' + e));
        }
    </script>
</body>
</html>
)rawliteral";

void setup() {
  SerialUSB.begin(9600);
  WiFi.setPins(8, 2, A3, -1); // TinyZero pin mapping
  Wire.begin();

  if (motor.begin(MAX_PWM)) {
    SerialUSB.println("Motor driver not detected!");
    while (1);
  }

  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    SerialUSB.print("Connecting to ");
    SerialUSB.println(ssid);
    delay(3000);
  }

  SerialUSB.print("WiFi up, IP: ");
  SerialUSB.println(WiFi.localIP());

  server.begin();
  motor.setFailsafe(0);
}

void sendOK(WiFiClient &client, const char *msg) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.println("Connection: close");
  client.println();
  client.println(msg);
}

void sendHTML(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.println(HTML_PAGE);
}

bool handleMotorCommand(String &line, WiFiClient &client) {
  MotorCmd commands[] = {
    { "/m1/cw",   1,  pwmVal,  "M1 CW" },
    { "/m1/ccw",  1, -pwmVal,  "M1 CCW" },
    { "/m1/stop", 1,  0,       "M1 Stopped" },
    { "/m2/cw",   2,  pwmVal,  "M2 CW" },
    { "/m2/ccw",  2, -pwmVal,  "M2 CCW" },
    { "/m2/stop", 2,  0,       "M2 Stopped" }
  };

  for (auto &cmd : commands) {
    if (line.indexOf(cmd.path) >= 0) {
      motor.setMotor(cmd.motorID, cmd.pwm);
      statusMsg = cmd.msg;
      sendOK(client, statusMsg.c_str());
      return true;
    }
  }
  return false;
}

bool handleSpeedCommand(String &line, WiFiClient &client) {
  if (line.indexOf("/m1/speed/") >= 0) {
    int speed = line.substring(line.indexOf("/m1/speed/") + 11).toInt();
    pwmVal = map(speed, 0, 100, 0, MAX_PWM);
    statusMsg = "M1 Speed set to " + String(speed) + "%";
    sendOK(client, statusMsg.c_str());
    return true;
  }

  if (line.indexOf("/m2/speed/") >= 0) {
    int speed = line.substring(line.indexOf("/m2/speed/") + 11).toInt();
    pwmVal = map(speed, 0, 100, 0, MAX_PWM);
    statusMsg = "M2 Speed set to " + String(speed) + "%";
    sendOK(client, statusMsg.c_str());
    return true;
  }

  return false;
}

void loop() {
  WiFiClient client = server.available();
  if (client) {
    String line = "";
    while (client.connected()) {
      if (!client.available()) continue;
      char c = client.read();
      line += c;

      if (c == '\n') {
        if (line == "\r\n") {
          sendHTML(client);
          break;
        }

        if (handleMotorCommand(line, client)) break;
        if (handleSpeedCommand(line, client)) break;

        line = "";
      }
    }
    client.stop();
  }
}
