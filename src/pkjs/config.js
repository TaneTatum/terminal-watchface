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
