// Terminal Watchface — PebbleKit JS
// Weather via Open-Meteo API (free, no key required)

var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var clay = new Clay(clayConfig);

// 0 = imperial (°F, mph), 1 = metric (°C, km/h)
var s_units = parseInt(localStorage.getItem('SETTINGS_UNITS') || '0');

var WMO_CODES = {
  0:  'CLEAR',
  1:  'PCLOUDY', 2: 'PCLOUDY', 3: 'CLOUDY',
  45: 'FOG',     48: 'FOG',
  51: 'DRIZZLE', 53: 'DRIZZLE', 55: 'DRIZZLE',
  56: 'DRIZZLE', 57: 'DRIZZLE',
  61: 'RAIN',    63: 'RAIN',    65: 'RAIN',
  66: 'RAIN',    67: 'RAIN',
  71: 'SNOW',    73: 'SNOW',    75: 'SNOW',
  77: 'SNOW',
  80: 'SHOWERS', 81: 'SHOWERS', 82: 'SHOWERS',
  85: 'SNOW SH', 86: 'SNOW SH',
  95: 'TSTORM',  96: 'TSTORM',  99: 'TSTORM'
};

var COMPASS = ['N','NNE','NE','ENE','E','ESE','SE','SSE',
               'S','SSW','SW','WSW','W','WNW','NW','NNW'];

function degreesToCompass(deg) {
  var ix = Math.floor((deg + 11.25) / 22.5) % 16;
  return COMPASS[ix];
}

function xhrRequest(url, type, callback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function() { callback(this.responseText); };
  xhr.onerror = function() {
    console.log('XHR error for: ' + url);
    callback(null);
  };
  xhr.open(type, url);
  xhr.send();
}

function getWeather() {
  var metric = (s_units === 1);
  navigator.geolocation.getCurrentPosition(
    function(pos) {
      var url = 'https://api.open-meteo.com/v1/forecast' +
        '?latitude='  + pos.coords.latitude.toFixed(4) +
        '&longitude=' + pos.coords.longitude.toFixed(4) +
        '&current=temperature_2m,weather_code,wind_speed_10m,wind_direction_10m' +
        (metric
          ? '&temperature_unit=celsius&wind_speed_unit=kmh'
          : '&temperature_unit=fahrenheit&wind_speed_unit=mph');

      console.log('Fetching: ' + url);

      xhrRequest(url, 'GET', function(responseText) {
        if (!responseText) {
          console.log('No response from weather API');
          return;
        }

        var json;
        try { json = JSON.parse(responseText); }
        catch(e) { console.log('JSON parse error'); return; }

        var cur = json.current;
        if (!cur) { console.log('No current data in response'); return; }

        var temp, wmoCode, condition, windSpd, windDir, windStr;
        try {
          temp      = Math.round(cur.temperature_2m);
          wmoCode   = cur.weather_code;
          console.log('WMO code: ' + wmoCode);
          condition = WMO_CODES[wmoCode] || 'CLOUDY';
          windSpd   = Math.round(cur.wind_speed_10m);
          windDir   = degreesToCompass(cur.wind_direction_10m);
          windStr   = windSpd + (metric ? 'km/h' : 'mph') + ' ' + windDir;
        } catch(e) { console.log('Parse error: ' + e); return; }

        console.log('Temp: ' + temp + ', Cond: ' + condition + ', Wind: ' + windStr);

        var dictionary = {
          'TEMPERATURE': temp,
          'CONDITIONS':  condition,
          'WIND':        windStr
        };

        Pebble.sendAppMessage(dictionary,
          function() { console.log('Weather sent to Pebble'); },
          function(e) { console.log('Send failed: ' + JSON.stringify(e)); }
        );
      });
    },
    function(err) {
      console.log('Geolocation error: ' + err.message);
    },
    { timeout: 15000, maximumAge: 300000 }
  );
}

function sendPhoneBattery() {
  if (navigator.getBattery) {
    navigator.getBattery().then(function(b) {
      Pebble.sendAppMessage({'PHONE_BATTERY': Math.round(b.level * 100)},
        function() { console.log('Phone battery sent'); },
        function(e) { console.log('Phone batt send failed: ' + e); });
    }).catch(function(e) { console.log('Battery API error: ' + e); });
  }
}

Pebble.addEventListener('ready', function() {
  console.log('Terminal watchface JS ready');
  getWeather();
  sendPhoneBattery();
});

Pebble.addEventListener('appmessage', function(e) {
  if (e.payload['REQUEST_WEATHER']) {
    console.log('Weather refresh requested');
    if (e.payload['SETTINGS_UNITS'] !== undefined) {
      s_units = e.payload['SETTINGS_UNITS'];
      localStorage.setItem('SETTINGS_UNITS', String(s_units));
    }
    getWeather();
    sendPhoneBattery();
  }
});
