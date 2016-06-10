# vzero
VZero - the Wireless zero-config controller for volkszaehler.org

## Plugins

VZero has an entensible plugin framework. Out of the box the following sensor plugins are supported:

  - analog reading (e.g. battery voltage)
  - DHT (temperature and humidity)
  - 1wire (temperature)
  - wifi (signal strength)

Already planned is support for IO events (S0).

## API description

The VZero frontend uses a json API to communicate with the Arduino backend.

### Actions

  - `/api/wlan` set WLAN configuration (`GET`)
  - `/api/restart` restart VZero (`POST`)

### Other services

  - `/api/status` system health (`GET`)
  - `/api/plugin` overview of plugins and sensors (`GET`)
  - `/api/<plugin_name>/<sensor_address>` individual sensors (`GET`)
