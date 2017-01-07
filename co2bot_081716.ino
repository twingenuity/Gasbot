// This #include statement was automatically added by the Particle IDE.
#include "lib1.h"

// This #include statement was automatically added by the Particle IDE.
#include "SparkFunMAX17043/SparkFunMAX17043.h"

// This #include statement was automatically added by the Particle IDE.
#include "SparkFunPhant/SparkFunPhant.h"


PRODUCT_ID(2567);  //change me 
PRODUCT_VERSION(1);

#define  RECEIVE_TIMEOUT  (1000)
uint8_t cmd_measure[9] = {0xFF,0x01,0x9C,0x00,0x00,0x00,0x00,0x00,0x63};
uint32_t ppm;
uint8_t buf[9];

//Global Variables
String device_uid = ""; // photon hard coded device id, UID
double voltage = 0; // Variable to keep track of LiPo voltage
double soc = 0; // Variable to keep track of LiPo state-of-charge (SOC)


uint8_t measure()
{
    uint8_t i;
    uint32_t start = millis();

Serial.println("About to try measuring.");
Serial.write("Sending following to sensor: ");
    Serial1.flush();

    for (i=0; i<9; i++) {
        Serial.write(cmd_measure[i]);
        Serial1.write(cmd_measure[i]);
    }

Serial.println("");
Serial.write("Getting response: ");
delay(50);

    for (i=0; i<9;) {
        if (Serial1.available()) {
            buf[i++] = Serial1.read();
            Serial.write(buf[i-1]);
        }

        if (millis() - start > RECEIVE_TIMEOUT) {
            return false;
        }
    }
Serial.println("Going to parse(buf) function.");
    if (parse(buf)) {
        return true;
    }

    return false;
}


uint8_t parse(uint8_t *pbuf)
{
    uint8_t i;
    uint8_t checksum = 0;

    for (i=0; i<9; i++) {
        checksum += pbuf[i];
    }

    if (pbuf[0] == 0xFF && pbuf[1] == 0x9C && checksum == 0xFF) {
        ppm = (uint32_t)pbuf[2] << 24 | (uint32_t)pbuf[3] << 16 | (uint32_t)pbuf[4] << 8 | pbuf[5];
        Serial.println("parse returning true");
        return true;
    } else {
        Serial.println("parse returning false");
        return false;
    }
}

///////////PHANT STUFF//////////////////////////////////////////////////////////////////
const char server[] = "ec2-52-40-13-117.us-west-2.compute.amazonaws.com";
const char path[] = "api/metrics/gas"; 
const char port[] = "8080"; 
PhantRest phant(server, path, port);
/////////////////////////////////////////////////////////////////////////////////////////


void setup()
{
    Serial.begin(9600);
    Serial1.begin(9600);
    Serial.println("Wait 10 seconds for sensor initialization...");
    delay(10000);
    
    { /// get deviceID. code block isolated in brackets
      device_uid=System.deviceID();
      Serial.print("Deviceid: ");
      Serial.println(device_uid);
    }
	
// Set up PArticle variables (voltage, soc, and alert):
    Particle.variable("voltage", &voltage, DOUBLE);
    Particle.variable("soc", &soc, DOUBLE);

// Set up the MAX17043 LiPo fuel gauge:
	lipo.begin(); // Initialize the MAX17043 LiPo fuel gauge
// Quick start restarts the MAX17043 in hopes of getting a more accurate
// guess for the SOC.
	lipo.quickStart();
	
}

void loop() {
    if (measure()) {
        Serial.print("CO2 Concentration is ");
        Serial.print(ppm);
        Serial.println("ppm");
    } else {
        Serial.println("Sensor communication error.");
    }

    delay(10000);
	
	getSensorData();//Get readings from all sensors
	postToPhant();//upload data to Phant

   delay(15000);//stay awake for 30 seconds to allow for App updates
    //Power down between sends to save power, measured in seconds.
   System.sleep(SLEEP_MODE_DEEP, 1000);  //for Particle Photon 17 minutes
    //(includes 20 sec update delay) between postings-change this to alter update rate
	
}

void getSensorData()
{

    getBattery();//Updates battery voltage, state of charge and alert threshold



}

//---------------------------------------------------------------
void getBattery()
{
  // lipo.getVoltage() returns a voltage value (e.g. 3.93)
voltage = lipo.getVoltage();
// lipo.getSOC() returns the estimated state of charge (e.g. 79%)
soc = lipo.getSOC();


}
//---------------------------------------------------------------

//---------------------------------------------------------------
int postToPhant()//sends datat to data.sparkfun.com
{
    
   
    
    phant.add("battery", soc);
    phant.add("carbondioxide", ppm);
    phant.add("deviceid", device_uid);
    phant.add("volts", voltage);

    TCPClient client;
    char response[512];
    int i = 0;
    int retVal = 0;

    if (client.connect(server, 8080))
    {
        Serial.println("Posting!");
        client.print(phant.post());
        delay(1000);
        while (client.available())
        {
            char c = client.read();
            Serial.print(c);
            if (i < 512)
                response[i++] = c;
        }
        if (strstr(response, "200 OK"))
        {
            Serial.println("Post success!");
            retVal = 1;
        }
        else if (strstr(response, "400 Bad Request"))
        {
            Serial.println("Bad request");
            retVal = -1;
        }
        else
        {
            retVal = -2;
        }
    }
    else
    {
        Serial.println("connection failed");
        retVal = -3;
    }
    client.stop();
    return retVal;

}
