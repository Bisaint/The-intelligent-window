#include <WiFi.h>
#include <PubSubClient.h> 
#include <ArduinoJson.h>
#include "DHT.h"

#define DHTTYPE DHT11

int relayPin0 = 14; 
int relayPin1 = 12; 
int relayPin2 = 13; 
int relayPin3 = 15; 

int DHTPIN = 4;  //连接dht11的引脚
DHT dht(DHTPIN, DHTTYPE);

#define DEFAULT_STASSID  "Mi 10"
//WIFI密码
#define DEFAULT_STAPSW "bisheng1579"

//三元组
#define PRODUCT_KEY       "a1Vdq6zBzvJ"
#define DEVICE_NAME       "8YIVdrpI1e1A6j1GKeyk"
#define DEVICE_SECRET     "341a684812f2ef08784edc37f34383d7"
#define REGION_ID         "cn-shanghai"
 
//线上环境域名和端口号
#define MQTT_SERVER       PRODUCT_KEY ".iot-as-mqtt." REGION_ID ".aliyuncs.com"
#define MQTT_PORT         1883
#define MQTT_USRNAME      DEVICE_NAME "&" PRODUCT_KEY
 
// 生成passwd：加密明文是参数和对应的值（clientId${esp8266}deviceName${deviceName}productKey${productKey}timestamp${1234567890}）按字典顺序拼接,密钥是设备的DeviceSecret
#define CLIENT_ID    "ESP8266|securemode=3,signmethod=hmacsha1,timestamp=1234567890|"
#define MQTT_PASSWD       "953ECE66BE2FB60799B8DB1B23FE57319CD3888E"

//topic-发布和订阅
#define ALINK_BODY_FORMAT_POST        "{\"id\":\"123\",\"version\":\"1.0\",\"method\":\"thing.event.property.post\",\"params\":%s}"   //消息体字符串格式和method参数
#define ALINK_TOPIC_PROP_POST         "/sys/" PRODUCT_KEY "/" DEVICE_NAME "/thing/event/property/post"   //发布，设备属性上报
#define ALINK_TOPIC_PROP_SET          "/sys/" PRODUCT_KEY "/" DEVICE_NAME "/thing/service/property/set"  //订阅，设备属性设置

unsigned long lastMs = 0;
WiFiClient espClient;     // 创建WiFiClient实例
PubSubClient  client(espClient);  //创建Client实例
 
//监听云端下发的指令及解析订阅的topic
void callback(char *topic, byte *payload, unsigned int length)
{
    if (strstr(topic, ALINK_TOPIC_PROP_SET)){ 
      Serial.print("Message arrived [");
      Serial.print(topic);
      Serial.print("] ");
      payload[length] = '\0';
      Serial.println((char *)payload);
      
      DynamicJsonDocument doc(100);
      DeserializationError error = deserializeJson(doc, payload);
      if (error){
        Serial.println("parse json failed");
        return;
        }
        
        //将字符串payload转换为json格式的对象
        // {"method":"thing.service.property.set","id":"282860794","params":{"jidianqi":1},"version":"1.0.0"}
        JsonObject setAlinkMsgObj = doc.as<JsonObject>();       
        int val = setAlinkMsgObj["params"]["powerstate"];
        if (val == HIGH){
          digitalWrite(relayPin0,LOW);
          digitalWrite(relayPin1,HIGH);
          digitalWrite(relayPin2,LOW);
          digitalWrite(relayPin3,HIGH);
          }
          else if (val == LOW){
               digitalWrite(relayPin0,HIGH);
               digitalWrite(relayPin1,LOW);
               digitalWrite(relayPin2,HIGH);
               digitalWrite(relayPin3,LOW);
            }
     }
}
 
//连接Wifi 
void wifiInit()
{
    WiFi.mode(WIFI_STA);
      WiFi.begin(DEFAULT_STASSID, DEFAULT_STAPSW);
      /*
    WiFi.beginSmartConfig();
while (1)
  {
    Serial.print(".");
    delay(500);
    if (WiFi.smartConfigDone())
    {
      Serial.println("SmartConfig Success/连接成功");
      Serial.printf("SSID:%s\r\n", WiFi.SSID().c_str());
      Serial.printf("PSW:%s\r\n", WiFi.psk().c_str());
      WiFi.setAutoConnect(true);  // 设置自动连接
      break;
    }
  }
 */
    Serial.println("Connected to AP");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    client.setServer(MQTT_SERVER, MQTT_PORT);   //连接WiFi之后，连接MQTT服务器
    client.setCallback(callback);   // 设置回调监听云端下发的指令
}
 
//连接Mqtt 
void mqttCheckConnect()
{
    while (!client.connected())
    {
        Serial.println("Connecting to MQTT Server ...");
        if (client.connect(CLIENT_ID, MQTT_USRNAME, MQTT_PASSWD))
        {
            Serial.println("MQTT Connected!");
            client.subscribe(ALINK_TOPIC_PROP_SET); // 订阅属性设置Topic
            Serial.println("subscribe done");
        }
        else
        {
            Serial.print("MQTT Connect err:");
            Serial.println(client.state());
            delay(5000);
        }
    }
}
 
//上报数据
void mqttIntervalPost()
{
    char param[32];
    char jsonBuf[128];
    // read without samples.
      float h = dht.readHumidity();
   // 读取温度或湿度大约需要250毫秒
   float t = dht.readTemperature();
   // 将温度读取为摄氏温度（默认值）
   float f = dht.readTemperature(true);

   //热量指数
     // 计算华氏温度 (默认)
   float hif = dht.computeHeatIndex(f, h);
   // 计算摄氏温度 (Fahreheit = false)
   float hic = dht.computeHeatIndex(t, h, false);
      if (isnan(h) || isnan(t) || isnan(f)) {
      Serial.println("没有从DHT1传感器上获取数据!");
      return;
   }   
    
    //sprintf是格式化字符串
    sprintf(param, "{\"powerstate\":%d,\"h\":%f,\"t\":%f,\"f\":%f,\"hif\":%f,\"hic\":%f}",digitalRead(relayPin1),h,t,f,hif,hic);  //产品标识符,数据上传    
    sprintf(jsonBuf, ALINK_BODY_FORMAT_POST, param);
    
    Serial.println(jsonBuf);
    boolean a_POST = client.publish(ALINK_TOPIC_PROP_POST, jsonBuf);//上报属性Topic数据
    Serial.print("publish:0失败;1成功;");
    Serial.println(a_POST);
}

void setup() 
{
 pinMode(relayPin0,OUTPUT);
 pinMode(relayPin1,OUTPUT);
 pinMode(relayPin2,OUTPUT);
 pinMode(relayPin3,OUTPUT);
    digitalWrite(relayPin0,LOW); //初始化继电器高电平
    digitalWrite(relayPin1,LOW); //初始化继电器高电平
    digitalWrite(relayPin2,LOW); //初始化继电器高电平
    digitalWrite(relayPin3,LOW); //初始化继电器高电平
    Serial.begin(115200);
    Serial.println("Demo Start");
    wifiInit();
}

void loop()
{
    if (millis() - lastMs >= 5000)
    {
        lastMs = millis();
        mqttCheckConnect(); // MQTT上云
        mqttIntervalPost();//上报消息心跳周期
    }
    client.loop();
}
