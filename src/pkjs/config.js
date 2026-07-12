module.exports = [
  { "type": "heading", "defaultValue": "Terminal Settings" },
  {
    "type": "section",
    "items": [
      { "type": "heading", "defaultValue": "Display" },
      {
        "type": "toggle",
        "messageKey": "SETTINGS_SCANLINES",
        "label": "Scanlines",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "SETTINGS_CLOCK_CURSOR",
        "label": "Clock Cursor",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "SETTINGS_PROMPT_CURSOR",
        "label": "Prompt Cursor",
        "defaultValue": true
      }
    ]
  },
  {
    "type": "section",
    "items": [
      { "type": "heading", "defaultValue": "Display Lines" },
      {
        "type": "select",
        "messageKey": "SETTINGS_LINE1",
        "label": "Line 1",
        "defaultValue": "0",
        "options": [
          { "label": "Weather",       "value": "0" },
          { "label": "Wind",          "value": "1" },
          { "label": "Date",          "value": "2" },
          { "label": "Step Count",    "value": "3" },
          { "label": "Heart Rate",    "value": "4" },
          { "label": "Bluetooth",     "value": "5" },
          { "label": "Phone Battery", "value": "6" }
        ]
      },
      {
        "type": "select",
        "messageKey": "SETTINGS_LINE2",
        "label": "Line 2",
        "defaultValue": "1",
        "options": [
          { "label": "Weather",       "value": "0" },
          { "label": "Wind",          "value": "1" },
          { "label": "Date",          "value": "2" },
          { "label": "Step Count",    "value": "3" },
          { "label": "Heart Rate",    "value": "4" },
          { "label": "Bluetooth",     "value": "5" },
          { "label": "Phone Battery", "value": "6" }
        ]
      },
      {
        "type": "select",
        "messageKey": "SETTINGS_LINE3",
        "label": "Line 3",
        "defaultValue": "2",
        "options": [
          { "label": "Weather",       "value": "0" },
          { "label": "Wind",          "value": "1" },
          { "label": "Date",          "value": "2" },
          { "label": "Step Count",    "value": "3" },
          { "label": "Heart Rate",    "value": "4" },
          { "label": "Bluetooth",     "value": "5" },
          { "label": "Phone Battery", "value": "6" }
        ]
      }
    ]
  },
  {
    "type": "section",
    "items": [
      { "type": "heading", "defaultValue": "Units" },
      {
        "type": "toggle",
        "messageKey": "SETTINGS_UNITS",
        "label": "Use Metric (°C, km/h)",
        "defaultValue": false
      }
    ]
  },
  {
    "type": "section",
    "items": [
      { "type": "heading", "defaultValue": "Color Scheme" },
      {
        "type": "radiogroup",
        "messageKey": "SETTINGS_COLOR_SCHEME",
        "label": "Theme",
        "defaultValue": "0",
        "options": [
          { "label": "Green Phosphor (default)", "value": "0" },
          { "label": "Amber Phosphor",           "value": "1" },
          { "label": "Cyan",                     "value": "2" },
          { "label": "White",                    "value": "3" }
        ]
      }
    ]
  },
  { "type": "submit", "defaultValue": "Save Settings" }
];
