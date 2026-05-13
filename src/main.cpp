/*
Auteurs: Vincent Bélisle BELV1805,
Elliot Gaulin GAUE1909
*/


// BLE client -> send "GET" on notify
#include <Arduino.h>
#include <BLEDevice.h>

HardwareSerial Link(2); // UART2
const int RX_PIN = 16;
const int TX_PIN = 17;

// DataReady UUID (must match server)
static const char *kDataReadyCharacteristicUuid = "FFFFFFFF-FFFF-FFFF-FFFF-000000000001";
static const char *kServerName = "Weather Station";

BLEClient *pClient;
BLERemoteCharacteristic *pRemoteCharacteristic;
bool connected = false;

// Received in csv format: seq, tempC, humidity%, pressurehPa, windSpeedMps, windDirectionDeg, illuminanceLux, totalRainfallMm
/*
void UARTManager::sendCurrentData() {
  Serial.println("Sending current sensor data over UART...");
  if (!stream_) return;
  Serial.print("Data seq: ");
  Serial.println(latestData_.sequence);
  // Simple CSV: seq,tempC,hum%,pres_hPa,wind_mps,wind_deg,light, rain_mm
  char buf[200];
  int n = snprintf(buf, sizeof(buf), "%lu,%.2f,%.2f,%.1f,%.2f,%.1f,%.0f,%.2f",
                   (unsigned long)latestData_.sequence,
                   latestData_.temperatureC,
                   latestData_.humidityPercent,
                   latestData_.pressurehPa,
                   latestData_.windSpeedMps,
                   latestData_.windDirectionDeg,
                   latestData_.illuminanceLux,
                   latestData_.totalRainfallMm);
  stream_->println(buf);
}
*/
struct SensorData
{
  float temperatureC = 0.0f;
  float humidityPercent = 0.0f;
  float pressurehPa = 0;
  float windSpeedMps = 0.0f;
  float windDirectionDeg = 0.0f;
  float illuminanceLux = 0.0f;
  float totalRainfallMm = 0.0f;
  uint32_t sequence = 0;
};

void readSensorDataFromCSV(const std::string &csv, SensorData &data)
{
  sscanf(csv.c_str(), "%lu,%f,%f,%f,%f,%f,%f,%f",
         &data.sequence,
         &data.temperatureC,
         &data.humidityPercent,
         &data.pressurehPa,
         &data.windSpeedMps,
         &data.windDirectionDeg,
         &data.illuminanceLux,
         &data.totalRainfallMm);
}

void notifyCallback(BLERemoteCharacteristic *chr, uint8_t *data, size_t length, bool isNotify)
{
  Serial.println("Received DataReady notification");
  // Send GET over UART
  Link.println("GET");
  Serial.println("Sent over UART: GET");
}

bool connectToServer(BLEAddress address)
{
  Serial.print("Forming connection to ");
  Serial.println(address.toString().c_str());

  pClient = BLEDevice::createClient();
  if (!pClient->connect(address))
  {
    Serial.println("Failed to connect");
    return false;
  }

  BLEUUID serviceUUID((uint16_t)0x181A); // Environmental Sensing service advertised by server
  BLERemoteService *pRemoteService = nullptr;
  try
  {
    pRemoteService = pClient->getService(serviceUUID);
  }
  catch (...)
  {
    Serial.println("Service not found");
    pClient->disconnect();
    return false;
  }
  if (pRemoteService == nullptr)
  {
    Serial.println("Failed to find service on server");
    pClient->disconnect();
    return false;
  }

  pRemoteCharacteristic = pRemoteService->getCharacteristic(BLEUUID(kDataReadyCharacteristicUuid));
  if (pRemoteCharacteristic == nullptr)
  {
    Serial.println("Failed to find DataReady characteristic");
    pClient->disconnect();
    return false;
  }

  if (pRemoteCharacteristic->canNotify())
  {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
    Serial.println("Registered for notifications on DataReady");
  }
  else
  {
    Serial.println("Characteristic does not support notify");
  }

  connected = true;
  return true;
}

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("BLE Client starting...");

  // UART to send GET
  Link.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);

  BLEDevice::init("");
  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  Serial.println("Scanning for server...");
  BLEScanResults foundDevices = pBLEScan->start(5);
  for (int i = 0; i < foundDevices.getCount(); ++i)
  {
    BLEAdvertisedDevice adv = foundDevices.getDevice(i);
    if (adv.haveName() && adv.getName() == kServerName)
    {
      Serial.print("Found ");
      Serial.println(kServerName);
      if (connectToServer(adv.getAddress()))
      {
        break;
      }
    }
  }
}

void loop()
{
  // If disconnected, try quick rescan/ reconnect
  if (!connected)
  {
    BLEScan *pBLEScan = BLEDevice::getScan();
    BLEScanResults foundDevices = pBLEScan->start(3);
    for (int i = 0; i < foundDevices.getCount(); ++i)
    {
      BLEAdvertisedDevice adv = foundDevices.getDevice(i);
      if (adv.haveName() && adv.getName() == kServerName)
      {
        connectToServer(adv.getAddress());
        break;
      }
    }
    delay(1000);
    return;
  }

  // keep client alive
  if (pClient && !pClient->isConnected())
  {
    Serial.println("Lost connection");
    connected = false;
    // will attempt reconnect in next loop
  }
  // Print anything received from the other ESP32
  while (Link.available())
  {

    std::string csvData;
    while (Link.available())
    {
      char c = Link.read();
      if (c == '\n')
        break;
      csvData += c;
      Serial.print(c); // Echo received data to Serial
    }
    if (!csvData.empty())
    {
      SensorData data;
      readSensorDataFromCSV(csvData, data);
      Serial.println("Parsed Sensor Data");
      Serial.print("Temp: ");
      Serial.print(data.temperatureC);
      Serial.println(" C");
      Serial.print("Humidity: ");
      Serial.print(data.humidityPercent);
      Serial.println(" %");
      Serial.print("Pressure: ");
      Serial.print(data.pressurehPa);
      Serial.println(" hPa");
      Serial.print("Wind Speed: ");
      Serial.print(data.windSpeedMps);
      Serial.println(" m/s");
      Serial.print("Wind Dir: ");
      Serial.print(data.windDirectionDeg);
      Serial.println(" deg");
      Serial.print("Light: ");
      Serial.print(data.illuminanceLux);
      Serial.println(" raw");
      Serial.print("Rainfall: ");
      Serial.print(data.totalRainfallMm);
      Serial.println(" mm");
    }
  }

  delay(10);
}