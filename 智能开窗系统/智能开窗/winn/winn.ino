//DHT11数据上传+LED远程控制 //不限于DHT11,可以接其他传感器，也可以接多个传感器，这里只是例程
//DHT11数据上传主题temp   //根据自己在控制台命的名字可自己随意修改
//LED灯控制主题light002   //根据自己命的名字可自己随意修改
/*
程序讲解：ESP8266 有两个角色，一个是temp(传感器数据)主题消息的发布者，esp8266往这个主题推送消息，手机app订阅temp主题，就可以收到传感器数据了。
esp8266联网后，订阅light002，手机往这个主题推送消息，esp8266就能收到手机的控制的指令了。
*/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include "DHT.h"
#include "MQ135.h"
#define DHTTYPE DHT11

//巴法云服务器地址默认即可
#define TCP_SERVER_ADDR "bemfa.com"
//服务器端口//TCP创客云端口8344//TCP设备云端口8340
#define TCP_SERVER_PORT "8344"

///****************需要修改的地方*****************///

//WIFI名称，区分大小写，不要写错
#define DEFAULT_STASSID  "Mi 10"
//WIFI密码
#define DEFAULT_STAPSW "bisheng1579"
//用户私钥，可在控制台获取,修改为自己的UID
String UID = "22ce0563f226aaac9cf0a7db0ff7cef1";
//主题名字，可在控制台新建
String TOPIC = "WINDOW1DATAN"; //用于传输温湿度的主题
String TOPIC2  = "WINDOW1DATAN";
//DHT11引脚值
int DHTPIN = 4;  //连接dht11的引脚
MQ135 q1 = MQ135(0);
///*********************************************///
//led 控制函数
void turnOnLed();
void turnOffLed();

//设置上传速率2s（1s<=upDataTime<=60s）
//下面的2代表上传间隔是2秒
#define upDataTime 2*1000


// for DHT11, 
//      VCC: 5V or 3V
//      GND: GND
//      DATA: 2

DHT dht(DHTPIN, DHTTYPE);


//最大字节数
#define MAX_PACKETSIZE 512





//tcp客户端相关初始化，默认即可
WiFiClient TCPclient;
String TcpClient_Buff = "";
unsigned int TcpClient_BuffIndex = 0;
unsigned long TcpClient_preTick = 0;
unsigned long preHeartTick = 0;//心跳
unsigned long preTCPStartTick = 0;//连接
bool preTCPConnected = false;



//相关函数初始化
//连接WIFI
void doWiFiTick();
void startSTA();

//TCP初始化连接
void doTCPClientTick();
void startTCPClient();
void sendtoTCPServer(String p);





/*
  *发送数据到TCP服务器
 */
void sendtoTCPServer(String p){
  
  if (!TCPclient.connected()) 
  {
    Serial.println("Client is not readly");
    return;
  }
  TCPclient.print(p);
  Serial.println("[Send to TCPServer]:String");
  Serial.println(p);
}


/*
  *初始化和服务器建立连接
*/
void startTCPClient(){
  if(TCPclient.connect(TCP_SERVER_ADDR, atoi(TCP_SERVER_PORT))){
    Serial.print("\nConnected to server:");
    Serial.printf("%s:%d\r\n",TCP_SERVER_ADDR,atoi(TCP_SERVER_PORT));
    String tcpTemp="";
    tcpTemp = "cmd=1&uid="+UID+"&topic="+TOPIC2+"\r\n";

    sendtoTCPServer(tcpTemp);
    preTCPConnected = true;
    preHeartTick = millis();
    TCPclient.setNoDelay(true);
  }
  else{
    Serial.print("Failed connected to server:");
    Serial.println(TCP_SERVER_ADDR);
    TCPclient.stop();
    preTCPConnected = false;
  }
  preTCPStartTick = millis();
}



/*
  *检查数据，发送数据
*/
void doTCPClientTick(){
 //检查是否断开，断开后重连
   if(WiFi.status() != WL_CONNECTED) return;

  if (!TCPclient.connected()) {//断开重连

  if(preTCPConnected == true){

    preTCPConnected = false;
    preTCPStartTick = millis();
    Serial.println();
    Serial.println("TCP Client disconnected.");
    TCPclient.stop();
  }
  else if(millis() - preTCPStartTick > 1*1000)//重新连接
    startTCPClient();
  }
  else
  {
    if (TCPclient.available()) {//收数据
      char c =TCPclient.read();
      TcpClient_Buff +=c;
      TcpClient_BuffIndex++;
      TcpClient_preTick = millis();
      
      if(TcpClient_BuffIndex>=MAX_PACKETSIZE - 1){
        TcpClient_BuffIndex = MAX_PACKETSIZE-2;
        TcpClient_preTick = TcpClient_preTick - 200;
      }
      preHeartTick = millis();
    }
    if(millis() - preHeartTick >= upDataTime){//上传数据
      preHeartTick = millis();
/*****************获取气体数据*****************/
float ppm = q1.getPPM();
   if (isnan(ppm)) {
      Serial.println("没有从气体传感器上获取数据!");
      return;
   }
      /*****************获取DHT11 温湿度*****************/
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
      /*********************数据上传*******************/
      /*
        数据用#号包裹，以便app分割出来数据，&msg=#23#80#on#\r\n，即#温度#湿度#按钮状态#，app端会根据#号分割字符串进行取值，以便显示
        如果上传的数据不止温湿度，可在#号后面继续添加&msg=#23#80#data1#data2#data3#data4#\r\n,app字符串分割的时候，要根据上传的数据进行分割
      */
      String upstr = "";
      upstr = "cmd=2&uid="+UID+"&topic="+TOPIC+"&msg=#"+t+"#"+h+"#"+f+"#"+hif+"#"+hic+"#"+ppm+"#"+"\r\n";
      sendtoTCPServer(upstr);
      upstr = "";
    }
  }
  if((TcpClient_Buff.length() >= 1) && (millis() - TcpClient_preTick>=200))
  {//data ready
    TCPclient.flush();
    Serial.println("Buff");
    Serial.println(TcpClient_Buff);
    //////字符串匹配，检测发了的字符串TcpClient_Buff里面是否包含&msg=on，如果有，则打开开关
    if((TcpClient_Buff.indexOf("&msg=on") > 0)) {
      turnOnLed();
    //////字符串匹配，检测发了的字符串TcpClient_Buff里面是否包含&msg=off，如果有，则关闭开关
    }else if((TcpClient_Buff.indexOf("&msg=off") > 0)) {
      turnOffLed();
    }
   TcpClient_Buff="";//清空字符串，以便下次接收
   TcpClient_BuffIndex = 0;
  }
}


//打开灯泡
void turnOnLed(){
  Serial.println("Turn ON");
}
//关闭灯泡
void turnOffLed(){
  Serial.println("Turn OFF");
}


void startSTA(){
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(DEFAULT_STASSID, DEFAULT_STAPSW);
}



/**************************************************************************
                                 WIFI
***************************************************************************/
/*
  WiFiTick
  检查是否需要初始化WiFi
  检查WiFi是否连接上，若连接成功启动TCP Client
  控制指示灯
*/
void doWiFiTick(){
  static bool startSTAFlag = false;
  static bool taskStarted = false;
  static uint32_t lastWiFiCheckTick = 0;

  if (!startSTAFlag) {
    startSTAFlag = true;
    startSTA();
    Serial.printf("Heap size:%d\r\n", ESP.getFreeHeap());
  }

  //未连接1s重连
  if ( WiFi.status() != WL_CONNECTED ) {
    if (millis() - lastWiFiCheckTick > 1000) {
      lastWiFiCheckTick = millis();
    }
  }
  //连接成功建立
  else {
    if (taskStarted == false) {
      taskStarted = true;
      Serial.print("\r\nGet IP Address: ");
      Serial.println(WiFi.localIP());
      startTCPClient();
    }
  }
}



// 初始化，相当于main 函数
void setup() {
  Serial.begin(115200);

  //初始化引脚为输出
  pinMode(A0,INPUT);
}

//循环
void loop() {
  doWiFiTick();
  doTCPClientTick();
}
