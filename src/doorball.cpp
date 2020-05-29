#include <Arduino.h>

#include "config.h"
#include "hw.h"

#include <multiball/app.h>
#include <multiball/wifi.h>
#include <multiball/ota_updates.h>
#include <multiball/homebus.h>

#include "doorball.h"

#include <SparkFunMPU9250-DMP.h>
#include <TinyPICO.h>

static MPU9250_DMP imu;
static TinyPICO tp;

void doorball_setup() {
    pinMode(ACCEL_INTERRUPT, INPUT_PULLUP);

    if(imu.begin() != INV_SUCCESS) {
      // indicate error... somehow
    }

    imu.setSensors(INV_XYZ_GYRO | INV_XYZ_ACCEL | INV_XYZ_COMPASS);
    imu.setSampleRate(4); // Set accel/gyro sample rate to 4Hz
    imu.setCompassSampleRate(4);
    imu.enableInterrupt();
    imu.setIntLevel(INT_ACTIVE_LOW);
    imu.setIntLatched(INT_LATCHED);
}

static boolean doorball_battery_update(char* buf, size_t buf_len) {
  snprintf(buf,
	   buf_len,
	   "{ \"id\": \"%s\", \"org.homebus.experimental.battery\": { \"voltage\": %.1f, \"charging\": %s } }",
	   homebus_uuid(),
	   tp.GetBatteryVoltage(), tp.IsChargingBattery() ? "true" : "false");

  Serial.println(buf);

  return true;
}


/*
 * we do this once at startup, and not again unless our IP address changes
 */
static boolean doorball_system_update(char* buf, size_t buf_len) {
  static IPAddress oldIP = IPAddress(0, 0, 0, 0);
  static String mac_address = WiFi.macAddress();
  IPAddress local = WiFi.localIP();

  if(oldIP == local)
    return false;

  snprintf(buf,
	   buf_len,
	   "{ \"id\": \"%s\", \"org.homebus.experimental.doorball-system\": { \"name\": \"%s\", \"build\": \"%s\", \"ip\": \"%d.%d.%d.%d\", \"mac_addr\": \"%s\" } }",
	   homebus_uuid(),
	   App.hostname().c_str(), App.build_info().c_str(), local[0], local[1], local[2], local[3], mac_address.c_str()
	   );

#ifdef VERBOSE
  Serial.println(buf);
#endif

  return true;
}

static boolean doorball_diagnostic_update(char* buf, size_t buf_len) {
  snprintf(buf, buf_len, "{ \"id\": \"%s\", \"org.homebus.experimental.doorball-diagnostic\": { \"freeheap\": %d, \"uptime\": %lu, \"rssi\": %d, \"reboots\": %d, \"wifi_failures\": %d } }",
	   homebus_uuid(),
	   ESP.getFreeHeap(), App.uptime()/1000, WiFi.RSSI(), App.boot_count(), App.wifi_failures());

#ifdef VERBOSE
  Serial.println(buf);
#endif

  return true;
}


void doorball_loop() {
  static unsigned long next_loop = 0;

  if(next_loop > millis())
    return;

  next_loop = millis() + UPDATE_DELAY;

#define BUFFER_LENGTH 600
  char buffer[BUFFER_LENGTH];

  if(doorball_battery_update(buffer, BUFFER_LENGTH))
    homebus_publish_to("org.homebus.experimental.air-sensor", buffer);

  if(doorball_system_update(buffer, BUFFER_LENGTH))
    homebus_publish_to("org.homebus.experimental.doorball-system", buffer);

  if(doorball_diagnostic_update(buffer, BUFFER_LENGTH))
    homebus_publish_to("org.homebus.experimental.doorball-diagnostic", buffer);
}

/* 
 * this callback is used to stream sensor data for diagnostics
 */
#ifdef USE_DIAGNOSTICS
void doorball_stream() {
  static uint8_t count = 0;

  if(count == 0)
    Serial.println("TEMP PRES HUMD TVOC   IR VISB FULL  LUX  1.0  2.5 10.0  SMAX  SMIN  SAVG  SCNT  PIR");

  if(++count == 10)
    count = 0;

  bme680.handle();
  tsl2561.handle();
  pms5003.handle();
  sound_level.handle();

  Serial.printf( "%03.1f %4.0f %4.0f %4.0f %4d %4d %4d %4d %4d %4d %4d %5d %5d %5d %5d    %c\n",
		 bme680.temperature(),
		 bme680.pressure(),
		 bme680.humidity(),
		 bme680.gas_resistance(),
		 tsl2561.ir(),
		 tsl2561.visible(),
		 tsl2561.full(),
		 tsl2561.lux(),
		 pms5003.density_1_0(),
		 pms5003.density_2_5(),
		 pms5003.density_10_0(),
		 sound_level.sound_max(),
		 sound_level.sound_min(),
		 sound_level.sound_level(),
		 sound_level.sample_count(),
		 pir.presence() ? '1' : '0');

  if(0) {
  Serial.println("[system]");
  Serial.printf("  Uptime %.2f seconds\n", uptime.uptime() / 1000.0);
  Serial.printf("  Free heap %u bytes\n", ESP.getFreeHeap());
  Serial.printf("  Wifi RSSI %d\n", WiFi.RSSI());
  }
}
#endif
