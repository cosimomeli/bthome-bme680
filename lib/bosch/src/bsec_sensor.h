struct BSEC_DATA {
    float    temperature;
    float    humidity;
    float    pressure;
    float    gas;
    float    iaq;
    float    static_iaq;
    float    voc;
    float    co2;
    uint8_t  accuracy;
    bool     updated;
    uint64_t timestamp;
};

BSEC_DATA get_sensor_data();