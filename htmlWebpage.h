// HTML webpage served fromnthe flash memory
const char webpage_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>ESP32 IoT Control Dashboard</title>
  <style>
    :root{
      --blue:#2F6FEB; /* primary blue */
      --blue-dark:#0E3C91; /* dark blue for headings */
      --blue-muted:#BFD0FF;
      --bg:#ffffff;
      --text:#0f1a2a;
      --radius:18px;
      --font: Helvetica, Arial, sans-serif;
    }
    *{box-sizing:border-box}
    html,body{
    margin:0;
    padding:0;
    background:var(--bg);
    color:var(--text);
    font-family:var(--font);
    }

    .wrap{
    min-height:100svh;
    display:grid;
    place-items:center;
    padding:24px
    }

    .card{
    width:min(680px,92vw);
    display:flex;
    flex-direction:column;
    align-items:center;
    gap:28px
    }

    h1{
      margin:0;
      font-weight:700;
      letter-spacing:.2px;
      font-size:clamp(22px,3.2vw,34px);
      color:#14325f
      text-align: center;
      }


    /* Gauge */
    .gauge{
    position:relative;
    width:min(380px,85vw);
    aspect-ratio:1/0.72;
    display:grid;
    place-items:center
    
    }

    .gauge svg{
    width:100%;
    height:auto;
    overflow:visible
    }
    .gauge .value{
      position:absolute;
      top:80%;
      left:50%;
      transform:translate(-50%,-50%);
      font-weight:800;font-size:clamp(36px,7vw,64px);
      color:#0e2a55
    }

    .label{
      margin-top:-8px;
      font-weight:600;
      color:var(--blue-dark);
      font-size:clamp(16px,2.4vw,22px)
      }

    /* Row blocks */
    .row{
      width:100%;
      display:flex;
      align-items:center;
      justify-content:space-between;
      gap:16px
      }
      
    .row + .row{
      margin-top:14px
      }

    .title{
      font-weight:700;
      color:#0d2b59;
      font-size:clamp(16px,2.4vw,20px)
      }

    /* Toggle */
    .switch{
      position:relative;
      display:inline-block;
      width:64px;height:32px
      }

    .switch input{
      display:none
      }

    .slider{
      position:absolute;
      cursor:pointer;
      inset:0;
      background:#d9e3ff;
      transition:.2s;
      border-radius:999px
    }

    .slider:before{
      content:"";
      position:absolute;
      height:24px;
      width:24px;
      left:4px;
      top:4px;
      background:#fff;
      transition:.2s;
      border-radius:50%;
      box-shadow:0 1px 3px rgba(0,0,0,.2)
      }

    .slider::after {
      position: absolute;
      top: 7px;
      font-size: 14px;
      font-weight: bold;
      color: white;
      transition: .4s;
    }

    input:checked + .slider::after {
      content: "ON";
      left: 12px;
    }

    input:not(:checked) + .slider::after {
      content: "OFF";
      right: 12px;
    }

    input:checked + .slider{background:var(--blue)}
    input:checked + .slider:before{transform:translateX(32px)}

    /* Slider */
    .range-wrap{
      width:100%;
      display:flex;
      align-items:center;
      gap:14px
      }

    input[type=range]{
      -webkit-appearance:none;
      width:100%;
      height:8px;
      border-radius:999px;
      background:linear-gradient(to right,var(--blue) var(--val,50%), #e8eeff var(--val,47%));
      outline:none
      }

    input[type=range]::-webkit-slider-thumb{
      -webkit-appearance:none;
    appearance:none;
    width:22px;
    height:22px;
    border-radius:50%;
    background:var(--blue);
    border:2px solid #fff;
    box-shadow:0 1px 3px rgba(0,0,0,.25);
    cursor:pointer
    }

    .pct{
      min-width:56px;
      text-align:right;
      font-weight:700
      }

    /* Spacing helpers */
    .spacer{
      height:6px
      }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      <h1>ESP32 IoT Control Dashboard</h1>

      <!-- Radial Gauge -->
      <div class="gauge">
        <svg viewBox="0 0 100 60" aria-hidden="true">
          <!-- background arc -->
          <path d="M10,60 A40,40 0 0 1 90,60" fill="none" stroke="#e6edff" stroke-width="10" stroke-linecap="round"/>
          <!-- value arc (stroke-dasharray set by JS) -->
          <path id="arc" d="M10,60 A40,40 0 0 1 90,60" fill="none" stroke="var(--blue)" stroke-width="10" stroke-linecap="round" stroke-dasharray="0 157"/>
        </svg>
        <div id="gaugeVal" class="value">100%</div>
      </div>
      <div class="label">Analog Sensor Value</div>

      <div class="spacer"></div>

      <!-- LED Status Row -->
      <div class="row">
        <div class="title">LED Status:</div>
        <label class="switch">
          <input id="ledToggle" type="checkbox" checked>
          <span class="slider"></span>
        </label>
      </div>

      <!-- LED Brightness Row -->
      <div class="row" style="align-items:flex-end;">
        <div class="title">LED Brightness</div>
        <div class="range-wrap">
          <input id="brightness" type="range" min="0" max="100" value="50">
          <div id="brightnessPct" class="pct">50%</div>
        </div>
      </div>

    </div>
  </div>

  <script>
    const ws = new WebSocket(`ws://${location.host.replace(/:\\d+$/, '')}:81/`);

    const ledToggle = document.getElementById('ledToggle');
    const brightness = document.getElementById('brightness');
    const brightnessPct = document.getElementById('brightnessPct');

    const arc = document.getElementById('arc');
    const gaugeVal = document.getElementById('gaugeVal');

    // SVG arc length for half-circle (approx):
    const ARC_LEN = 157; // calibrated to the path above

    function setGauge(percent){
      const clamped = Math.max(0, Math.min(100, percent));
      const dash = (clamped / 100) * ARC_LEN;
      arc.setAttribute('stroke-dasharray', `${dash} ${ARC_LEN}`);
      gaugeVal.textContent = `${Math.round(clamped)}%`;
    }

    // Update slider background fill visually
    function paintRange(){
      const v = parseInt(brightness.value, 10);
      brightness.style.setProperty('--val', v + '%');
      brightnessPct.textContent = v + '%';
    }

    paintRange();

    ws.onopen = () => {
      // request current state once connected
      ws.send(JSON.stringify({type:'hello'}));
    };

    // Incoming messages
    ws.onmessage = (ev) => {
      try{
        const msg = JSON.parse(ev.data);
        if('led1' in msg){
          ledToggle.checked = !!msg.led1;
        }
        if('brightness' in msg){
          brightness.value = msg.brightness;
          paintRange();
        }
        if('potPct' in msg){
          setGauge(msg.potPct);
        }
      }catch(e){ console.warn('WS parse error', e); }
    };

    // Outgoing: toggle LED1
    ledToggle.addEventListener('change', () => {
      ws.send(JSON.stringify({type:'set_led1', value: ledToggle.checked}));
    });

    // Outgoing: LED2 brightness
    let tHandle;
    brightness.addEventListener('input', () => {
      paintRange();
      clearTimeout(tHandle);
      tHandle = setTimeout(() => {
        ws.send(JSON.stringify({type:'set_brightness', value: parseInt(brightness.value,10)}));
      }, 40);
    });
  </script>
</body>
</html>
)rawliteral";
