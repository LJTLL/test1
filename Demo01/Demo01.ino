/*********************引入库函数********************/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SimpleDHT.h>
#include <Ticker.h>


/*********************引脚定义********************/

const int pinDHT11 = 2 ;                                // GPIO2（D4）        温湿度引脚
const int pinLightSensor = A0;                          // ADC引脚输入（A0）   光敏传感器引脚
const int pinlight = 0 ;                                // GPIO0（D3）        补光灯控制引脚
SimpleDHT11 dht11(pinDHT11);                            // 设定温湿度使用的引脚为定义

/**********************网络设置项 **********************/
const char* ssid = "atest";                             //路由器名字
const char* password = "1qazmlp0";                      //路由器密码
const char* mqtt_server = "101.43.0.73";                //MQTT服务器IP
const int mqttPort = 1883;                              //MQTT服务器端口

/*****************用户信息项 (全部为自定义项)*****************/
const char* ESP8266_ID = "8266";                        //自定义ID
const char* ESP8266_user = "admin";                     //用户名
const char* ESP8266_password = "public";                //密码
const char* ESP8266_pub = "firstdata";                  //ESP8266发布信息的主题（对方的订阅主题）
const char* ESP8266_sub = "testtopic";                  //ESP8266订阅信息的主题（对方的发送主题）

/*************************变量定义**********************/

byte temperature = 0;               //温度数值存放变量
byte humidity = 0;                  //湿度数值存放变量
int lightSensorValue = 0;           // 最终转化的光照强度输出值
int Value = 0;                      // 记录ADC采集的电压值
int count;                          // Ticker计数用变量

WiFiClient espClient;               // 定义wifiClient实例
PubSubClient client(espClient);     // 定义PubSubClient的实例
Ticker ticker;                      // 定义Ticker的实例

/*************************初始化函数*********************/

void setup()
{
  Serial.begin(9600);
  WiFi.mode(WIFI_STA);                //设置ESP8266为无线终端模式（Station）

  pinMode(pinLightSensor, INPUT);     //初始化光照传感器
  pinMode(pinlight, OUTPUT);          //将继电器引脚设为输出模式，
  digitalWrite(pinlight,1);

  gotowifi();                         //先连接到指定路由器
  initMQTT();                         //初始化MQTT客户端
  gotoMQTT();                         //连接到指定MQTT服务器，并订阅指定主题
  ticker.attach(1, tickerCount);      //初始化定时函数，每一秒执行一次tickerCount计数函数
}

/*************************循环函数*********************/

void loop()
{
  checkLink();                         //循环执行连接检测函数，防止连接中断

  if (count >= 3) {                    //每3秒执行一次数据采集，消息发布
    Light();
    dht11Detect(lightSensorValue);
    count = 0;
  }
}

/*************************计数函数*********************/

void tickerCount() {
  count++;
}

/*
   检测连接函数
   功能：检查WIFI、MQTT服务器连接，
        如果WIFI或者MQTT服务器连接断开，自动重连，
        确定服务器连接正常，则保持心跳
*/

void checkLink() {
  if (WiFi.status() != WL_CONNECTED) {        //自己记得添加对WiFi连接状态的判断（路由器断线重连）
    delay(500);
    Serial.println("WiFi已断开，正在重连");
    Serial.println("现在执行WiFi连接程序");
    gotowifi();
    Serial.println("WIFI连上了，再连MQTT服务器");
    gotoMQTT();
  } else {
    if (!client.connected()) {

      Serial.println("服务器断了");
      Serial.println("服务器正在重连...");
      delay(3000);

      gotoMQTT();
    } else {

      client.loop();//持续运行MQTT运行函数，完成接收数据和定时发送心跳包
    }
  }
}
/*
   void gotowifi()
    功能：连接WiFi
*/

void gotowifi()              //连接WIFI
{
  WiFi.begin(ssid, password);
  Serial.print("连接WiFi中");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi连接成功");
}

/*
   void initMQTT()
   功能：初始化MQTT服务器
*/

void initMQTT()
{
  //指定客户端要连接的MQTT服务器IP和端口
  client.setServer(mqtt_server, mqttPort);
  //绑定数据回调函数
  client.setCallback(receiveCallback);
}

/*
   void gotoMQTT()
   功能：连接MQTT服务器，添加主题订阅
*/

void gotoMQTT()
{
  //用while循环执行到连接MQTT成功
  while (!client.connected()) {
    Serial.println("连接MQTT服务器中");
    //连接MQTT服务器，提交ID，用户名，密码
    bool is = client.connect(ESP8266_ID, ESP8266_user, ESP8266_password);
    if (is) {
      Serial.println("连接MQTT服务器成功");
    } else {
      Serial.print("失败原因 ");
      Serial.print(client.state());
      delay(1000);
    }
  }
  client.subscribe(ESP8266_sub, 1);//添加订阅
}

/*void receiveCallback(char* topic, byte* payload, unsigned int length)
  功能：回调函数，处理MQTT主题信息
*/

void receiveCallback(char* topic, byte* payload, unsigned int length)
{
  Serial.print("消息来自订阅主题: ");
  Serial.println(topic);
  Serial.print("消息:");

  String data = "";
  for (int i = 0; i < length; i++) {
    data += (char)payload[i];
  }
  Serial.println(data);     //串口打印接收的信息
  Serial.println();

  MQTT_Handler(data);//把接收的数据，传入处理函数执行分析处理
}

/*
   void MQTT_Handler(String data)
   功能：判断订阅主题中消息内容
   data：订阅主题中传回的数据
*/

void MQTT_Handler(String data)          
{
  if (data == "")
  {
    return;
  }
  if (data == "开启")
  {
    Open();
  }
  else if (data == "关闭")
  {
    Close();
  }
}
/*
   void dht11Detect(int a)
   功能：温湿度传感器数据采集，同时传入光敏传感器数值，
   通过消息发布函数，将数据信息发布到MQTT主题中
   int a: 传入光敏传感器数值
*/
void dht11Detect(int a) {
  //如果DHT11采集数据有误，显示错误值
  int err = SimpleDHTErrSuccess;

  if ((err = dht11.read(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
    Serial.print("Read DHT11 failed, err=");
    Serial.println(err); delay(1000);
    return;
  }

  String messageString = "现在的温度是：" + (String )temperature + " " + "现在的湿度是："     //将采集的数据封装成字符数组发送到指定Topic中
                           + (String )humidity + " " + "光照强度为：" + (String)a ;         //messgeString 需要发送的字符

 
  char publishMsg[messageString.length() + 1];       //定义一个比messageString字符长度大的数组,用于存放需要发送的字符                                      
  
  strcpy(publishMsg, messageString.c_str());         //通过c_str将String类型的对象，转化为字符串类型

  Serial.println(messageString);
  
  client.publish(ESP8266_pub, publishMsg);           //发布至指定Topic中
}
/*
   int Light()
   功能：光敏电阻传感器采集函数
   return:int类型光照强度数值
*/
int Light() {

  Value = analogRead(pinLightSensor);          //读取ADC转换的数值；
  lightSensorValue = 1220 - ( Value - 10) * 1.2;    //根据传感器传回数值，计算光照强度

  Serial.print("sensor = "); Serial.print(lightSensorValue); Serial.println("LX");

  return lightSensorValue;
}

/**开启函数**/

void Open() {
  digitalWrite(pinlight, 0);     //并将使引脚输出低电平，继电器吸合
}

/**关闭函数**/

void Close() {
  digitalWrite(pinlight, 1);     //并将使引脚输出高电平，继电器释放
}
