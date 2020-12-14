#include <Arduino.h>
#include <network.h>
#include <ESP8266Wifi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>

// Wifi login info
const char* ssid = network::ssid;
const char* password = network::password;

// servers
ESP8266WebServer server(80);
WebSocketsServer webSocket(81);

// globals
unsigned long total_time{0};
unsigned long high_time{0};
unsigned long low_time{0};
int sensor_pin{3};

// webpage
const char* webpage {R"(
<!DOCTYPE html>
<style>
  * {
    padding: 0;
    margin: 0;
    flex-shrink: 0;
  }
  html, body {
    height: 100%;
    display: flex;
    flex-direction: column;
  }
  #hud {
    color: white;
    background-color:black;
    display: flex;
    flex-direction: row;
    justify-content: center;
  }
  #plot {
    background-color: gray;
    flex-grow: 1;
  }
  .play_btn {
    background-color: transparent;
    border-color: transparent;
    margin-right: 20px;
    fill: white;
  }
  .pause_btn {
    background-color: transparent;
    border-color: transparent;
    margin-right: 40px;
    fill: white;
  }
  .connect_icon {
    background-color: transparent;
    border-color: transparent;
    margin-right: 20px;
    fill: white;
  }
</style>

<head>
  <script src='https://cdn.plot.ly/plotly-latest.min.js'></script>
  <script>
    var COUNT = 0;             // starting value
    var SAMPLING_RATE = 1000;  // ms
    var IS_PAUSED = true;      // paused flag
    var IS_CONNECTED = false;  // connected flag
    function update_connection() {
      // collect the objects
      svg = document.getElementById('plug_svg');
      poly = document.getElementById('plug_poly');
      points = poly.points;
      // check the state
      let delta = IS_CONNECTED ? 10 : -10;
      // modify the poly
      for (let p of points) {p.x += delta};
      // modify the svg
      svg.width += delta;
      // toggle the state
      IS_CONNECTED = !IS_CONNECTED;
    }
  </script>
</head>
<body>
  <div id='hud'>
    <button class='connect_icon'>
      <svg id='plug_svg' width='36', height='20'>
        <polygon points='0, 17, 9, 17, 9, 20, 13, 20, 13, 15, 20, 15, 20, 12,
        13, 12, 13, 8, 20, 8, 20, 5, 13, 5, 13, 0, 9, 0, 9, 3, 0, 3'></polygon>
        <polygon id='plug_poly' points='23, 20, 27, 20, 27, 17, 36, 17, 36, 3, 27, 3, 27, 0, 23, 0'></polygon>
      </svg>
    </button>
    <button class='play_btn' onclick='(()=>{IS_PAUSED = false;})()'>
      <svg width='20', height='20'>
        <polygon points='0, 20, 0, 0, 20, 10'></polygon>
      </svg>
    </button>
    <button class='pause_btn' onclick='(()=>{IS_PAUSED = true;})()'>
      <svg width='20', height='20'>
        <polygon points='2, 0, 2, 20, 7, 20, 7, 0'></polygon>
        <polygon points='13, 0, 13, 20, 18, 20, 18, 0'></polygon>
      </svg>
    </button>
    <h1 id='watt_label'>WATTS:</h1>
    <h1 id='watts' style='margin-left: 10px;'>0</h1>
    <h1 id='timer_label', style='margin-left: 70px;'>TIME:</h1>
    <h1 id='timer', style='margin-left: 10px;'>0</h1>
  </div>
  <div id='plot'>
  </div>
  <script>
    // =================== create plot the layout ===================
    let layout = {
      xaxis: {title: {text: 'time [min]'}},
      yaxis: {title: {text: 'power [watts]'}}
    }
    let config = {responsive: true}

    // ==================== instantiate the plot =====================
    Plotly.newPlot(
      'plot',                                  // div
      [{x:[COUNT/60], y:[COUNT],               // data
        mode: 'lines',}],                      // type
      layout,
      config,
    );

    // ================= collect the nonlocal objects =================
    let watts = document.getElementById('watts');  // text object
    let timer = document.getElementById('timer');  // text object
    let power = 0;  // y value on the plot
    let time = 0;   // x value on the plot
    const a = 199_844_880_469.07413 - 48_000_000_000.0;  // coeffs for power calcs
    const b = -7_111_362.674411774 + 2_100_500.0;
    const c = 68.65055109403046;

    // ================ file reader and event handlers ================
    let binary_reader = new FileReader();
    binary_reader.addEventListener('load', function(){
      // reads the binary and sets the nonlocal power
      // to the value read in from the reader, and increments the timer
      let data = new Uint32Array(binary_reader.result);
      if (data[0] !== 0){
        let omega = 1/data[0];  // 1/micros
        power = (a*(omega**2)) + (b*omega) + c;
      }
      else {power = 0;}
      time += SAMPLING_RATE/1000;
    });

    // ================= websocket and event handlers =================
    let socket = new WebSocket('ws://' + location.hostname + ':81/', ['arduino']);
    socket.onopen = function(event) {update_connection();}
    socket.onerror = function(event) {update_connection();}
    socket.onclose = function(event) {update_connection();}
    // the on message function handles the responses
    // from the arduino, and updates the plot accordingly
    socket.onmessage = function(event) {
      // read the blob
      binary_reader.readAsArrayBuffer(event.data);
      // update the text
      watts.innerHTML = power;
      timer.innerHTML = new Date(time * 1000).toISOString().substr(11, 8);
      // update the plot
      Plotly.extendTraces(      // update the plot
        'plot',
        {x: [[time/60]], y: [[power]],},
        [0]
      );
    }

    function loop() {
      // sends a single byte of data, the arduino returns the
      // time in micros from the infrared sensor
      if (IS_CONNECTED && !IS_PAUSED) {socket.send('c')};
    }

    // run the loop
    setInterval(loop, SAMPLING_RATE);
  </script>
</body>


)"};

// websocket function
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:{
            break;
        }
        case WStype_TEXT:{
              String _payload = String((char *) &payload[0]);
              Serial.println(_payload);
              // send data back to server
              Serial.println(total_time);
              webSocket.sendBIN(num, (uint8_t *) &total_time, sizeof(total_time));
              break;
        }
        case WStype_BIN:{
            hexdump(payload, length);
            // echo data back to browser
            webSocket.sendBIN(num, payload, length);
            break;
        }
    }
}


void setup(){
    // start the coms
    Serial.begin(115200);

    // set the pinmode
    pinMode(sensor_pin, INPUT);

    // start the wifi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
        Serial.print("... ");
    }
    Serial.println("Connected!");

    // start the mDNS
    if (!MDNS.begin("spm")) {
        Serial.println("Error setting up MDNS responder!");
    }
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS responder started");

    // routes
    server.on("/", [](){server.send(200, "text/html", webpage);});

    // turn on the servers
    server.begin();
    webSocket.begin();

    // setup the websocket event
    webSocket.onEvent(webSocketEvent);

    // instantiate total time
    total_time = 0;
}

// loop func
void loop(){
    webSocket.loop();
    server.handleClient();
    MDNS.update();
    high_time = pulseIn(sensor_pin, HIGH, 450000);
    low_time = pulseIn(sensor_pin, LOW, 450000);
    total_time = high_time + low_time;
}