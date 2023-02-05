#include <bsec2.h>
#include "bsec_sensor.h"
#include <Preferences.h>

#define STATE_SAVE_PERIOD UINT32_C(360 * 60 * 1000) /* 360 minutes - 4 times a day */
#define TEMPERATURE_OFFSET 2

bsecSensor sensorList[] = {
    BSEC_OUTPUT_RAW_TEMPERATURE,
    BSEC_OUTPUT_RAW_PRESSURE,
    BSEC_OUTPUT_RAW_HUMIDITY,
    BSEC_OUTPUT_RAW_GAS,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_STATIC_IAQ,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
    BSEC_OUTPUT_CO2_EQUIVALENT
};

RTC_DATA_ATTR Bsec2 envSensor;
RTC_DATA_ATTR uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE];
RTC_DATA_ATTR uint16_t stateUpdateCounter = 0;

BSEC_DATA sensorOutput;
bool initDone = false;
Preferences preferences;

void checkBsecStatus(Bsec2 bsec)
{
    if (bsec.status < BSEC_OK)
    {
        Serial.println("BSEC error code : " + String(bsec.status));
        //errLeds(); /* Halt in case of failure */
    } else if (bsec.status > BSEC_OK)
    {
        Serial.println("BSEC warning code : " + String(bsec.status));
    }

    if (bsec.sensor.status < BME68X_OK)
    {
        Serial.println("BME68X error code : " + String(bsec.sensor.status)  + " " + bsec.sensor.statusString());
        //errLeds(); /* Halt in case of failure */
    } else if (bsec.sensor.status > BME68X_OK)
    {
        Serial.println("BME68X warning code : " + String(bsec.sensor.status) + " " + bsec.sensor.statusString());
    }
};

bool saveState(Bsec2 bsec) {
    Serial.println("Writing state to Preferences");
    preferences.putBytes("bsec_status", bsecState, BSEC_MAX_STATE_BLOB_SIZE);
    return true;
}

bool loadState(Bsec2 bsec)
{
    size_t result = preferences.getBytes("bsec_status", bsecState, BSEC_MAX_STATE_BLOB_SIZE);
    if (result == BSEC_MAX_STATE_BLOB_SIZE) {
        Serial.println("Reading state from Preferences");
        if (!bsec.setState(bsecState))
            return false;
    }
    return true;
}

void updateBsecState(Bsec2 bsec)
{
    if (!bsec.getState(bsecState)) {
        checkBsecStatus(bsec);
        return;
    }

    bool update = false;

    if (!stateUpdateCounter || (stateUpdateCounter * STATE_SAVE_PERIOD) < millis())
    {
        /* Update every STATE_SAVE_PERIOD minutes */
        update = true;
        stateUpdateCounter++;
    }

    if (update && !saveState(bsec))
        checkBsecStatus(bsec);
}

void newDataCallback(const bme68xData data, const bsecOutputs outputs, Bsec2 bsec)
{
    if (!outputs.nOutputs)
        return;

    uint64_t timestamp = outputs.output[0].time_stamp / INT64_C(1000000);
    sensorOutput.timestamp = timestamp;
    Serial.println("BSEC outputs:\n\ttimestamp = " + String(timestamp));
    for (uint8_t i = 0; i < outputs.nOutputs; i++)
    {
        const bsecData output  = outputs.output[i];
        switch (output.sensor_id)
        {
            case BSEC_OUTPUT_RAW_TEMPERATURE:
                Serial.println("\ttemperature = " + String(output.signal));
                break;
            case BSEC_OUTPUT_RAW_PRESSURE:
                Serial.println("\tpressure = " + String(output.signal));
                sensorOutput.pressure = output.signal;
                break;
            case BSEC_OUTPUT_RAW_HUMIDITY:
                Serial.println("\thumidity = " + String(output.signal));
                break;
            case BSEC_OUTPUT_RAW_GAS:
                Serial.println("\tgas resistance = " + String(output.signal));
                sensorOutput.gas = output.signal;
                break;
            case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
                Serial.println("\tcompensated temperature = " + String(output.signal));
                sensorOutput.temperature = output.signal;
                break;
            case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
                Serial.println("\tcompensated humidity = " + String(output.signal));
                sensorOutput.humidity = output.signal;
                break;
            case BSEC_OUTPUT_IAQ:
                Serial.println("\tIAQ = " + String(output.signal));
                Serial.println("\tAccuracy = " + String(output.accuracy));
                sensorOutput.iaq = output.signal;
                sensorOutput.accuracy = output.accuracy;
                break;
            case BSEC_OUTPUT_STATIC_IAQ:
                Serial.println("\tStatic IAQ = " + String(output.signal));
                sensorOutput.static_iaq = output.signal;
                break;
            case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
                Serial.println("\tTVOC = " + String(output.signal));
                sensorOutput.voc = output.signal;
                break;
            case BSEC_OUTPUT_CO2_EQUIVALENT:
                Serial.println("\tCO2 = " + String(output.signal));
                sensorOutput.co2 = output.signal;
                break;
            default:
                break;
        }
    }
    sensorOutput.updated = true;
};

void initSensor() {
    Wire.begin(19, 18);
    preferences.begin("bsec", false);
    /* Initialize the library and interfaces */
    if (!envSensor.begin(BME68X_I2C_ADDR_HIGH, Wire)) {
        checkBsecStatus(envSensor);
    }

    envSensor.setTemperatureOffset(TEMPERATURE_OFFSET);

    Serial.println("Begin passed");

      /* Copy state from the Preferences to the algorithm */
    if(!stateUpdateCounter) {
        if (!loadState(envSensor)) {
            checkBsecStatus (envSensor);
        }
    }

    /* Subscribe for the desired BSEC2 outputs */
    if (!envSensor.updateSubscription(sensorList, ARRAY_LEN(sensorList), BSEC_SAMPLE_RATE_ULP))
    {
        checkBsecStatus (envSensor);
    }

    /* Whenever new data is available call the newDataCallback function */
    envSensor.attachCallback(newDataCallback);

    Serial.println("\nBSEC library version " + \
            String(envSensor.version.major) + "." \
            + String(envSensor.version.minor) + "." \
            + String(envSensor.version.major_bugfix) + "." \
            + String(envSensor.version.minor_bugfix));
};

BSEC_DATA get_sensor_data() {
    if(!initDone) {
        initSensor();
        initDone = true;
    }

    if(sensorOutput.updated == true) {
        sensorOutput.updated = false;
        return sensorOutput;
    }

    /* Call the run function often so that the library can
     * check if it is time to read new data from the sensor
     * and process it.
     */
    while (!envSensor.run() || !sensorOutput.updated) {
        checkBsecStatus (envSensor);
        Serial.println("looping");
        delay(3000);
    }

    sensorOutput.updated = false;
    updateBsecState(envSensor);
    return sensorOutput;
};