# vzero
VZero - the Wireless zero-config controller for volkszaehler.org

## API description

The VZero frontend uses a json API to communicate with the Arduino backend.

### Actions

  - `/api/wlan` set WLAN configuration (`GET`)
  - `/api/restart` restart VZero (`POST`)

### Other services

  - `/api/status` system health (`GET`)
  - `/api/plugin` overview of plugins and sensors (`GET`)
  - `/api/<plugin_name>/<sensor_address>` individual sensors (`GET`)
