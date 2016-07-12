# vzero
VZero - the Wireless zero-config controller for volkszaehler.org

[![Build Status](https://travis-ci.org/andig/vzero.svg?branch=master)](https://travis-ci.org/andig/vzero)

## Plugins

VZero has an extensible plugin framework. Out of the box the following sensor plugins are supported:

  - analog reading (e.g. battery voltage)
  - DHT (temperature and humidity)
  - 1wire (temperature)
  - wifi (signal strength)

Already planned is support for IO events (S0).

## API description

The VZero frontend uses a json API to communicate with the Arduino backend.

### Actions

  - `/api/wlan` set WiFi configuration (`GET`)
  - `/api/restart` restart (`POST`)
  - `/api/settings` save settings (restarts) (`POST`)

### Other services

  - `/api/scan` WiFi scan (`GET`)
  - `/api/status` system health (`GET`)
  - `/api/plugin` overview of plugins and sensors (`GET`)
  - `/api/<plugin_name>/<sensor_address>` individual sensors (`GET`)

## Screenshots

### Welcome Screen

After initial startup, the welcome screen allows to customize WiFi credentials:
![Welcome Screen](/../gh-pages/img/1.png?raw=true)

In addition, the Volkszaehler middleware can be configured- either connecting to http://demo.volkszaehler.org which is the default or any other middleware.

### Home Screen

The home screen presents an overview of the configured sensors:
![Home Screen](/../gh-pages/img/2.png?raw=true)

### Sensors Screen

The sensors screen is used to connect available sensors to the middleware:
![Sensors Screen](/../gh-pages/img/3.png?raw=true)

### Status Screen

The status screen shows health information of the VZero and allows to restart the device:
![Sensors Screen](/../gh-pages/img/4.png?raw=true)

