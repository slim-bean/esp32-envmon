# esp32-envmon

This is a simple program to read from a [SEN54](https://sensirion.com/products/catalog/SEN54/) and remote write to a prometheus compatible endpoint.

I purchased mine from [here](https://www.sparkfun.com/products/19325) which comes with a cable. The cable pinout is kind of funky but they have a nice diagram explaining the colors [here](https://cdn.sparkfun.com/assets/8/8/4/6/0/SEN54_Cable_Colors.jpg)

I wired up SDA to GPIO 14 and SCL to GPIO 13, this can be changed in the code, the ESP32 lets you use most any pins for I2C

I used [this esp32](https://www.amazon.com/gp/product/B09D3S7T3M/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) but most any will work, you would need to change the `board = esp32-s3-devkitc-1` to match, google your board and `platformio` and you should find more details. Be careful with a lot of the non-name brand esp32 boards on amazon, it may be tricky to match them to a generic config in platformio. If you stick with espressif produced boards like the one linked you shouldn't have any trouble.

This project uses [PlatformIO](https://platformio.org/install/ide?install=vscode)

To upload the script you need to connect your esp32 and find the com port. Sometimes you have to install drivers for the device to show up as a serial port.  When you have the com port, change this in platformio.ini for both `upload_port` and `monitor_port`

If you are using the esp32 I linked above, connect the cable to the `UART` connector.

You need to create a file called `config.h` and put it in the `/include` directory. This file is in .gitignore because it contains credentials.

```
#define WIFI_SSID ""
#define WIFI_PASS ""

#define NTP "pool.ntp.org"

#define PROM_URL "prometheus-us-central1.grafana.net"
#define PROM_PATH "/api/prom/push"
#define PROM_USER ""
#define PROM_PASS ""

#define WDT_TIMEOUT 300
```

You probably also want to change the sensor ID I used in the labels (around lines 20-30)

You should then be able to `Upload and Monitor` in the platformio extension!