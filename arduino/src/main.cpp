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
constexpr int sensor_pin{3};

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
    var COUNT = 0;                       // starting value
    var SAMPLING_RATE = 1000;            // ms
    var IS_PAUSED = true;                // paused flag
    var IS_CONNECTED = false;            // connection flag
    // filter variables
    var MICROS = 0;                        // microseconds
    var POWER_OLD = 0;                     // last power value
    var POWER_NEW = 0;                     // current power value
    var POWER_PROPORTIONAL_CUTOFF = 1000;  // watts
    var POWER_DERIVATIVE_CUTOFF = 100;     // dwatts/dt

    function set_connection_state(state) {
      if (state != IS_CONNECTED){
        // collect the objects
        svg = document.getElementById('plug_svg');
        poly = document.getElementById('plug_poly');
        points = poly.points;
        // set the delta based on state
        let delta = (state === true) ? -10 : 10;
        // modify the poly
        for (let p of points) {p.x += delta};
        // modify the svg
        svg.width += delta;
        // update the state upon exit
        IS_CONNECTED = state;
      }
    }

    function micros2watts(micros) {
      if (micros === 0) {
        return 0;
      }
      else {
        return 635891.991888686*Math.PI/micros + 34161356213001.9*Math.pow(Math.PI, 3)/Math.pow(micros, 3);
      }
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
      'plot',                    // div
      [{x:[COUNT/60], y:[COUNT], // data
        mode: 'lines',}],        // type
      layout,
      config,
    );

    // ================= collect the nonlocal objects =================
    let watts = document.getElementById('watts');  // text object
    let timer = document.getElementById('timer');  // text object
    let POWER = 0;      // y value on the plot
    let time = 0;       // x value on the plot


    // ================ file reader and event handlers ================
    let binary_reader = new FileReader();
    binary_reader.addEventListener('load', function(){
      // read in hte data
      let data = new Uint32Array(binary_reader.result);  // read the binary
      MICROS = Number(data);                             // convert to number
      POWER_NEW = micros2watts(MICROS);                  // calculate power in watts
      // apply the filter
      if (POWER_NEW < POWER_PROPORTIONAL_CUTOFF) {                        // proportional check
        if (Math.abs(POWER_NEW - POWER_OLD) < POWER_DERIVATIVE_CUTOFF) {  // derivative check
          POWER = POWER_NEW;      // update the text
          POWER_OLD = POWER_NEW;  // update power values
          return;
        }
      }
      POWER = POWER_OLD;  // assign text to last valid power value
      return;
    });

    // ================= websocket and event handlers =================
    let socket = new WebSocket('ws://' + location.hostname + ':81/', ['arduino']);

    socket.onmessage = function(event) {binary_reader.readAsArrayBuffer(event.data);}
    socket.onopen = function(event) {set_connection_state(true);}
    socket.onerror = function(event) {set_connection_state(false);}
    socket.onclose = function(event) {set_connection_state(false);}

    function loop() {
      // sends a single byte of data, the arduino returns the
      // time in micros from the infrared sensor
      if (IS_CONNECTED  && !IS_PAUSED){
        // set the text power and timer text
        watts.innerHTML = Math.round(POWER);
        timer.innerHTML = new Date(time * 1000).toISOString().substr(11, 8);

        // update the time
        time += SAMPLING_RATE/1000;

        // update the plot
        Plotly.extendTraces('plot', {x: [[time/60]], y: [[POWER]],}, [0]);

        // set the default power value for the next interval
        POWER = 0;

        // send the signal to update the power sometime between now
        // and the next time the loop function is called.
        socket.send('c');
      }
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
              // print the recieved messag
              Serial.println(_payload);
              // print the datat to send
              Serial.println(total_time);
              // send the data
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

// setup func
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
  // handle the servers
  webSocket.loop();
  server.handleClient();
  MDNS.update();
  // update the total time
  high_time = pulseIn(sensor_pin, HIGH, 1000000);
  low_time = pulseIn(sensor_pin, LOW, 1000000);
  total_time = high_time + low_time;
}