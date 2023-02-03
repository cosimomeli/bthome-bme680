#include "NimBLEDevice.h"
#include "NimBLEBeacon.h"
#include "esp_sleep.h"
#include "advertisement.h"
#include "battery_voltage.h"
#include "bsec_sensor.h"

#define SLEEP_SECONDS 5 * 60 // sleep 5 minutes and then wake up
#define BEACON_UUID "4820879c-9f5d-11ed-a8fc-0242ac120002" // UUID 1 128-Bit (may use linux tool uuidgen or random numbers via https://www.uuidgenerator.net/)

NimBLEExtAdvertising *pAdvertising;
uint64_t last_timestamp;

uint8_t build_data_advert(uint8_t data[])
{
  bthome::Advertisement advertisement(std::string("Tom"));
  BSEC_DATA sensorOutput = get_sensor_data();
  last_timestamp = sensorOutput.timestamp;
  bthome::Measurement temperature(bthome::constants::ObjectId::TEMPERATURE_PRECISE, sensorOutput.temperature);
  bthome::Measurement humidity(bthome::constants::ObjectId::HUMIDITY_PRECISE, sensorOutput.humidity);
  bthome::Measurement pressure(bthome::constants::ObjectId::PRESSURE, sensorOutput.pressure / 100);
  bthome::Measurement co2(bthome::constants::ObjectId::CO2, sensorOutput.co2);
  bthome::Measurement voc(bthome::constants::ObjectId::CO2, sensorOutput.voc);
  bthome::Measurement gas(bthome::constants::ObjectId::COUNT_SMALL, sensorOutput.gas/1000);
  bthome::Measurement iaq(bthome::constants::ObjectId::COUNT_MEDIUM, sensorOutput.iaq);
  bthome::Measurement accuracy(bthome::constants::ObjectId::COUNT_SMALL, (uint64_t)sensorOutput.accuracy);

  bthome::Measurement battery(bthome::constants::ObjectId::VOLTAGE, get_battery_voltage());

  advertisement.addMeasurement(temperature);
  advertisement.addMeasurement(humidity);
  advertisement.addMeasurement(pressure);
  advertisement.addMeasurement(gas);
  advertisement.addMeasurement(accuracy);
  advertisement.addMeasurement(battery);
  advertisement.addMeasurement(co2);
  advertisement.addMeasurement(voc);
  advertisement.addMeasurement(iaq);

  memcpy(&data[0], advertisement.getPayload(), advertisement.getPayloadSize());

  return advertisement.getPayloadSize();
}

void setBeacon()
{

  BLEBeacon oBeacon = BLEBeacon();
  oBeacon.setManufacturerId(0x4C00); // fake Apple 0x004C LSB (ENDIAN_CHANGE_U16!)
  oBeacon.setProximityUUID(BLEUUID(BEACON_UUID));
  NimBLEExtAdvertisement oAdvertisementData = NimBLEExtAdvertisement();
  // NimBLEExtAdvertisement oScanResponseData = NimBLEExtAdvertisement();

  // oAdvertisementData.setFlags(0x06); // this is 00000110. Bit 1 and bit 2 are 1, meaning: Bit 1: “LE General Discoverable Mode” Bit 2: “BR/EDR Not Supported”
  oAdvertisementData.setConnectable(false);
  oAdvertisementData.setScannable(false);

  // Encode sensor data
  uint8_t advertData[251];
  size_t dataLength = build_data_advert(&advertData[0]);
  Serial.println("Advert size: " + String(dataLength));

  oAdvertisementData.setData(advertData, dataLength);
  //pAdvertising->setAdvertisementData(oAdvertisementData);
  pAdvertising->setInstanceData(0, oAdvertisementData);
  // pAdvertising->setScanResponseData(0, oScanResponseData);

  /**  pAdvertising->setAdvertisementType(ADV_TYPE_NONCONN_IND);
   *    Advertising mode. Can be one of following constants:
   *  - BLE_GAP_CONN_MODE_NON (non-connectable; 3.C.9.3.2).
   *  - BLE_GAP_CONN_MODE_DIR (directed-connectable; 3.C.9.3.3).
   *  - BLE_GAP_CONN_MODE_UND (undirected-connectable; 3.C.9.3.4).
   */
  // pAdvertising->setAdvertisementType(BLE_GAP_CONN_MODE_NON);
}

uint64_t get_sleep_duration() {
  uint64_t current = millis();
  uint64_t result = SLEEP_SECONDS * 1000;
  if(current >= last_timestamp) {
    result -= current - last_timestamp;
  }
  return result;
}

void setup()
{

  Serial.begin(115200);

  Serial.println("Setup done");
}

void loop()
{
  // Create the BLE Device
  BLEDevice::init("");
  pAdvertising = BLEDevice::getAdvertising();

  setBeacon();
  // Start advertising
  pAdvertising->start(0);
  Serial.println("Advertizing started...");
  delay(2000);
  pAdvertising->stop(0);
  Serial.println("Advertizing stopped");
  uint64_t sleep_ms = get_sleep_duration();
  Serial.println("enter light sleep for " + String(sleep_ms/1000) + "seconds");
  delay(10);
  esp_sleep_enable_timer_wakeup(sleep_ms * 1000);
  esp_light_sleep_start();
  Serial.println("end of sleep");
}