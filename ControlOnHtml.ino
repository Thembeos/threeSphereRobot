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

        .motor1, .motor2 {
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

    <div class="motor1">
        <button onclick="cmd('/m1/cw', 'm1')">M1 CW</button>
        <button onclick="cmd('/m1/ccw', 'm1')">M1 CCW</button>
        <button onclick="cmd('/m1/stop', 'm1')">M1 Stop</button>
        <form>
            <label for="speed1">Speed (between 0 and 100):</label>
            <input type="range" id="speed1" name="speed1" min="0" max="100">
        </form>
    </div>
    <div class="motor2">
        <button onclick="cmd('/m2/cw', 'm2')">M2 CW</button>
        <button onclick="cmd('/m2/ccw', 'm2')">M2 CCW</button>
        <button onclick="cmd('/m2/stop', 'm2')">M2 Stop</button>
        <form>
            <label for="speed2">Speed (between 0 and 100):</label>
            <input type="range" id="speed2" name="speed2" min="0" max="100">
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
                    if (motor === 'm1') {
                        document.getElementById('m1status').innerText = t;
                    } else if (motor === 'm2') {
                        document.getElementById('m2status').innerText = t;
                    }
                })
                .catch(e => alert('Error: ' + e));
        }
    </script>
</body>
</html>
)rawliteral";

void setup()
{
  SerialUSB.begin(9600);
  WiFi.setPins(8, 2, A3, -1); // TinyZero
  Wire.begin();

  if (motor.begin(MAX_PWM))
  {
    SerialUSB.println("Motor driver not detected!");
    while (1)
      ;
  }

  // connect to WiFi
  while (WiFi.begin(ssid, pass) != WL_CONNECTED)
  {
    SerialUSB.print("Connecting to ");
    SerialUSB.println(ssid);
    delay(3000);
  }
  SerialUSB.print("WiFi up, IP: ");
  SerialUSB.println(WiFi.localIP());

  server.begin();
  motor.setFailsafe(0);
}

void sendOK(WiFiClient &client, const char *msg)
{
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.println("Connection: close");
  client.println();
  client.println(msg);
}

void loop()
{
  WiFiClient client = server.available();
  if (client)
  {
    String line = "";
    while (client.connected())
    {
      if (!client.available())
        continue;
      char c = client.read();
      line += c;
      if (c == '\n')
      {
        // blank line means end of headers → serve page
        if (line.equals("\r\n"))
        {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");
          client.println();
          client.println(HTML_PAGE);
          break;
        }
        // check our four endpoints
        if (line.indexOf("GET /m1/cw") >= 0)
        {
          motor.setMotor(1, pwmVal);
          statusMsg = "CW";
          sendOK(client, statusMsg.c_str());
          break;
        }
        if (line.indexOf("GET /m1/ccw") >= 0)
        {
          motor.setMotor(1, -pwmVal);
          statusMsg = "CCW";
          sendOK(client, statusMsg.c_str());
          break;
        }
        if (line.indexOf("GET /m1/stop") >= 0)
        {
          motor.setMotor(1, 0);
          statusMsg = "stop";
          sendOK(client, statusMsg.c_str());
          break;
        }

        if (line.indexOf("GET /m2/cw") >= 0)
        {
          motor.setMotor(2, pwmVal);
          statusMsg = "CCW";
          sendOK(client, statusMsg.c_str());
          break;
        }
        if (line.indexOf("GET /m2/ccw") >= 0)
        {
          motor.setMotor(2, -pwmVal);
          statusMsg = "CCW";
          sendOK(client, statusMsg.c_str());
          break;
        }
        if (line.indexOf("GET /m2/stop") >= 0)
        {
          motor.setMotor(2, 0);
          statusMsg = "stop";
          sendOK(client, statusMsg.c_str());
          break;
        }
        // otherwise keep reading headers
        line = "";
      }
    }
    client.stop();
  }
}
