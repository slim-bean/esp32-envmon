#include <Arduino.h>
#include <SensirionI2CSen5x.h>
#include <Wire.h>
#include <ArduinoOTA.h>
#include <PrometheusArduino.h>
#include "config.h"
#include "certificates.h"
#include <esp_task_wdt.h>

#define I2C_SDA 14
#define I2C_SCL 13


// Create a transport and client object for sending our data.
PromLokiTransport transport;
PromClient prom(transport);

SensirionI2CSen5x sen5x;


TimeSeries pm1(1, "pm1", "{job=\"envmon\",sensor=\"7\"}");
TimeSeries pm2_5(1, "pm2_5", "{job=\"envmon\",sensor=\"7\"}");
TimeSeries pm4(1, "pm4", "{job=\"envmon\",sensor=\"7\"}");
TimeSeries pm10(1, "pm10", "{job=\"envmon\",sensor=\"7\"}");
TimeSeries voc(1, "voc", "{job=\"envmon\",sensor=\"7\"}");
TimeSeries temp(1, "temp", "{job=\"envmon\",sensor=\"7\"}");
TimeSeries humidity(1, "humidity", "{job=\"envmon\",sensor=\"7\"}");
WriteRequest series(7, 1024);

void printModuleVersions()
{
  uint16_t error;
  char errorMessage[256];

  unsigned char productName[32];
  uint8_t productNameSize = 32;

  error = sen5x.getProductName(productName, productNameSize);

  if (error)
  {
    Serial.print("Error trying to execute getProductName(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  }
  else
  {
    Serial.print("ProductName:");
    Serial.println((char *)productName);
  }

  uint8_t firmwareMajor;
  uint8_t firmwareMinor;
  bool firmwareDebug;
  uint8_t hardwareMajor;
  uint8_t hardwareMinor;
  uint8_t protocolMajor;
  uint8_t protocolMinor;

  error = sen5x.getVersion(firmwareMajor, firmwareMinor, firmwareDebug,
                           hardwareMajor, hardwareMinor, protocolMajor,
                           protocolMinor);
  if (error)
  {
    Serial.print("Error trying to execute getVersion(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  }
  else
  {
    Serial.print("Firmware: ");
    Serial.print(firmwareMajor);
    Serial.print(".");
    Serial.print(firmwareMinor);
    Serial.print(", ");

    Serial.print("Hardware: ");
    Serial.print(hardwareMajor);
    Serial.print(".");
    Serial.println(hardwareMinor);
  }
}

void printSerialNumber()
{
  uint16_t error;
  char errorMessage[256];
  unsigned char serialNumber[32];
  uint8_t serialNumberSize = 32;

  error = sen5x.getSerialNumber(serialNumber, serialNumberSize);
  if (error)
  {
    Serial.print("Error trying to execute getSerialNumber(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  }
  else
  {
    Serial.print("SerialNumber:");
    Serial.println((char *)serialNumber);
  }
}

void setup()
{
  Serial.begin(115200);

  // Start the watchdog timer, sometimes connecting to wifi or trying to set the time can fail in a way that never recovers
  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(NULL);

  // Wait 5s for serial connection or continue without it
  // some boards like the esp32 will run whether or not the
  // serial port is connected, others like the MKR boards will wait
  // for ever if you don't break the loop.
  uint8_t serialTimeout = 0;
  while (!Serial && serialTimeout < 50)
  {
    delay(100);
    serialTimeout++;
  }

  transport.setWifiSsid(WIFI_SSID);
  transport.setWifiPass(WIFI_PASS);
  transport.setNtpServer(NTP);
  transport.setUseTls(true);
  transport.setCerts(lokiCert, strlen(lokiCert));
  transport.setDebug(Serial); // Remove this line to disable debug logging of the transport layer.
  if (!transport.begin())
  {
    Serial.println(transport.errmsg);
    while (true)
    {
    };
  }

  // Configure the client
  prom.setUrl(PROM_URL);
  prom.setPath((char *)PROM_PATH);
  prom.setPort(443);
  prom.setDebug(Serial); // Remove this line to disable debug logging of the client.
  if (!prom.begin())
  {
    Serial.println(prom.errmsg);
    while (true)
    {
    };
  }
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(500000L);

  sen5x.begin(Wire);

  uint16_t error;
  char errorMessage[256];
  error = sen5x.deviceReset();
  if (error)
  {
    Serial.print("Error trying to execute deviceReset(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  }

  // Start Measurement
  error = sen5x.startMeasurement();
  if (error)
  {
    Serial.print("Error trying to execute startMeasurement(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  }

  series.addTimeSeries(pm1);
  series.addTimeSeries(pm2_5);
  series.addTimeSeries(pm4);
  series.addTimeSeries(pm10);
  series.addTimeSeries(voc);
  series.addTimeSeries(temp);
  series.addTimeSeries(humidity);
  series.setDebug(Serial);
}

void loop()
{
  // Reset watchdog, this also gives the most time to handle OTA
  // be careful if your wdt reset is too quick you may want to disable
  // it before OTA.
  esp_task_wdt_reset();
  uint16_t error;
  char errorMessage[256];

  // Read Measurement
  float massConcentrationPm1p0;
  float massConcentrationPm2p5;
  float massConcentrationPm4p0;
  float massConcentrationPm10p0;
  float ambientHumidity;
  float ambientTemperature;
  float vocIndex;
  float noxIndex;

  // We get a fair number of CRC errors, not sure why but try a few times to get a reading.
  for (uint8_t i = 0; i <= 5; i++)
  {
    error = sen5x.readMeasuredValues(
        massConcentrationPm1p0, massConcentrationPm2p5, massConcentrationPm4p0,
        massConcentrationPm10p0, ambientHumidity, ambientTemperature, vocIndex,
        noxIndex);

    if (error)
    {
      Serial.print("Error trying to execute readMeasuredValues(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
      delay(1);
    }
    else
    {
      break;
    }
  }

  Serial.print("MassConcentrationPm1p0:");
  Serial.print(massConcentrationPm1p0);
  Serial.print("\t");
  Serial.print("MassConcentrationPm2p5:");
  Serial.print(massConcentrationPm2p5);
  Serial.print("\t");
  Serial.print("MassConcentrationPm4p0:");
  Serial.print(massConcentrationPm4p0);
  Serial.print("\t");
  Serial.print("MassConcentrationPm10p0:");
  Serial.print(massConcentrationPm10p0);
  Serial.print("\t");
  Serial.print("AmbientHumidity:");
  if (isnan(ambientHumidity))
  {
    Serial.print("n/a");
  }
  else
  {
    Serial.print(ambientHumidity);
  }
  Serial.print("\t");
  Serial.print("AmbientTemperature:");
  if (isnan(ambientTemperature))
  {
    Serial.print("n/a");
  }
  else
  {
    Serial.print(ambientTemperature);
  }
  Serial.print("\t");
  Serial.print("VocIndex:");
  if (isnan(vocIndex))
  {
    Serial.print("n/a");
  }
  else
  {
    Serial.print(vocIndex);
  }
  Serial.print("\t");
  Serial.print("NoxIndex:");
  if (isnan(noxIndex))
  {
    Serial.println("n/a");
  }
  else
  {
    Serial.println(noxIndex);
  }


  int64_t ptime = transport.getTimeMillis();
  if (!pm1.addSample(ptime, massConcentrationPm1p0))
  {
    Serial.println(pm1.errmsg);
  }
  if (!pm2_5.addSample(ptime, massConcentrationPm2p5))
  {
    Serial.println(pm2_5.errmsg);
  }
  if (!pm4.addSample(ptime, massConcentrationPm4p0))
  {
    Serial.println(pm4.errmsg);
  }
  if (!pm10.addSample(ptime, massConcentrationPm10p0))
  {
    Serial.println(pm10.errmsg);
  }
  if (!voc.addSample(ptime, vocIndex))
  {
    Serial.println(voc.errmsg);
  }
  if (!temp.addSample(ptime, ambientTemperature))
  {
    Serial.println(temp.errmsg);
  }
  if (!humidity.addSample(ptime, ambientHumidity))
  {
    Serial.println(humidity.errmsg);
  }

  // Send the message, we build in a few retries as well.
  uint64_t start = millis();
  for (uint8_t i = 0; i <= 5; i++)
  {
    PromClient::SendResult res = prom.send(series);
    if (!res == PromClient::SendResult::SUCCESS)
    {
      Serial.println(prom.errmsg);
      delay(250);
    }
    else
    {
      // Batches are not automatically reset so that additional retry logic could be implemented by the library user.
      // Reset batches after a succesful send.
      pm1.resetSamples();
      pm2_5.resetSamples();
      pm4.resetSamples();
      pm10.resetSamples();
      voc.resetSamples();
      temp.resetSamples();
      humidity.resetSamples();
      uint32_t diff = millis() - start;
      Serial.print("Prom send succesful in ");
      Serial.print(diff);
      Serial.println("ms");
      break;
    }
  }


  uint64_t delayms = 15000 - (millis() - start);
  // If the delay is longer than 5000ms we likely timed out and the send took longer than 5s so just send right away.
  if (delayms > 15000)
  {
    delayms = 0;
  }
  Serial.print("Sleeping ");
  Serial.print(delayms);
  Serial.println("ms");
  delay(delayms);
}
/*
MassConcentrationPm1p0:4.10     MassConcentrationPm2p5:4.30     MassConcentrationPm4p0:4.30     MassConcentrationPm10p0:4.30    AmbientHumidity:50.05   AmbientTemperature:24.72        VocIndex:13.00  NoxIndex:n/a
Begin serialization: Free Heap: 205872
Bytes used for serialization: 387
After serialization: Free Heap: 205872
After Compression Init: Free Heap: 205344
Required buffer size for compression: 483
Compressed Len: 177
After Compression: Free Heap: 205872
Sending To Prometheus
Connection already open
Sent, waiting for response
Prom Send Succeeded
Server: nginx/1.14.1
Date: Mon, 16 May 2022 12:26:46 GMT
Content-Length: 0
Connection: keep-alive

Prom send succesful in 251ms
Begin serialization: Free Heap: 205872
Bytes used for serialization: 141
After serialization: Free Heap: 205872
After Compression Init: Free Heap: 205344
Required buffer size for compression: 196
Compressed Len: 143
After Compression: Free Heap: 205872
Sending To Loki
Connection already open
Sent, waiting for response
Loki Send Succeeded
Server: nginx/1.14.1
Date: Mon, 16 May 2022 12:26:47 GMT
Connection: keep-alive

Loki send succesful in 250ms
Sleeping 498ms
*/