#include <LiquidCrystal_I2C.h>
#include "hc12.h"

namespace Pin
{
  const uint8_t setPin = 15;
};

typedef  struct
{
  float node1Pwr;
  uint16_t node2Pwr;
}pwr_t;

//RTOS Handle(s)
TaskHandle_t appTaskHandle;
TaskHandle_t nodeTaskHandle;
QueueHandle_t nodeToAppQueue;
//QueueHandle_t nodeToMqttQueue;

void setup() {
  // put your setup code here, to run once:
  setCpuFrequencyMhz(80);
  Serial.begin(115200);
  Serial.println("Energy monitor");
  nodeToAppQueue = xQueueCreate(1,sizeof(pwr_t));
  if(nodeToAppQueue != NULL)
  {
    Serial.println("Node-Application Queue successfully creeated");
  }
  else
  {
    Serial.println("Node-Application Queue creation failed");
  }
  //Tasks
  xTaskCreatePinnedToCore(NodeTask,"",30000,NULL,1,&nodeTaskHandle,1);
  xTaskCreatePinnedToCore(ApplicationTask,"",25000,NULL,1,&appTaskHandle,1);  
}

void loop() {
  // put your main code here, to run repeatedly:

}

void ApplicationTask(void* pvParameters)
{
  LiquidCrystal_I2C lcd(0x27,20,4);
  static pwr_t pwr;

  lcd.init();
  lcd.backlight();
  lcd.print(" ENERGY MONITOR");
  vTaskDelay(pdMS_TO_TICKS(1500));
  lcd.clear();
  lcd.print("INITIALIZING...");
  vTaskDelay(pdMS_TO_TICKS(1500));
  lcd.clear();
  vTaskResume(nodeTaskHandle);

  while(1)
  {
    //Receive sensor Data from Node-Application Queue.
    if(xQueueReceive(nodeToAppQueue,&pwr,0) == pdPASS)
    {
      Serial.println("--Application task received data from Node task\n");
    }
    uint16_t totalPwr = pwr.node1Pwr + pwr.node2Pwr;
    lcd.setCursor(0,0);
    lcd.print("Node 1: ");
    lcd.print(pwr.node1Pwr);
    lcd.print(" W");
    lcd.setCursor(0,1);
    lcd.print("Node 2: ");
    lcd.print(pwr.node2Pwr);
    lcd.print(" W");
    lcd.setCursor(0,2);
    lcd.print("Total PWR: ");
    lcd.print(totalPwr);
    lcd.print(" W");
    lcd.setCursor(0,3);
    lcd.print("Source used: ");
    lcd.print("xxxx");
  }
}

/**
 * @brief Handles communication between the master and the node via a 
 * serial interface (i.e. MNI).
 * MNI is an acronym for 'Master-Node-Interface'.
*/
void NodeTask(void* pvParameters)
{
  vTaskSuspend(NULL);
  static HC12 hc12(&Serial2,Pin::setPin);
  static pwr_t pwr; 
  uint8_t node = HC12::Node1Addr;
  uint32_t nodeTime = millis();
  hc12.SetChannel(CHANNEL_20);
  
  while(1)
  {
    if(millis() - nodeTime >= 1200)
    {
      hc12.EncodeData(HC12::QUERY,HC12::TxDataId::DATA_QUERY);
      hc12.EncodeData(node,HC12::TxDataId::DEST_ADDR);
      hc12.TransmitData();
      Serial.print("--Query sent to ");
      Serial.println(node);
     // node = (node == MNI::node1Addr) ? MNI::node2Addr : MNI::node1Addr;
      nodeTime = millis();
    }
    
    if(hc12.ReceivedData())
    {
      if(hc12.DecodeData(HC12::RxDataId::DATA_ACK) == HC12::ACK)
      {
        if(hc12.DecodeData(HC12::RxDataId::SRC_ADDR) == HC12::Node1Addr)
        {
          Serial.println("Data Received from node 1");
          pwr.node1Pwr = hc12.DecodeData(HC12::RxDataId::POWER) / 100.0;
          xQueueSend(nodeToAppQueue,&pwr,0);
          Serial.println(pwr.node1Pwr);
        }
        else if(hc12.DecodeData(HC12::RxDataId::SRC_ADDR) == HC12::Node2Addr)
        {
          Serial.println("Data Received from node 2");
          pwr.node2Pwr = hc12.DecodeData(HC12::RxDataId::POWER);
          xQueueSend(nodeToAppQueue,&pwr,0);
          Serial.println(pwr.node2Pwr);
        }
      }   
    }
  }
}
