#include <WiFi.h>
#include <PubSubClient.h>
#include <MFRC522.h>      
#include <SPI.h>    
#include <NTPClient.h> //Biblioteca NTPClient modificada
#include <WiFiUdp.h> //Socket UDP

//WIFI
const char* ssid = "Bandeirantes";
const char* password = "59648219";
WiFiClient wifiClient; 

//MQTT Server
const char* BROKER_MQTT = "test.mosquitto.org"; 
int BROKER_PORT = 1883;
#define ID_MQTT "RF_ID_01" 
#define TOPIC_PUBLISH "MicroEEL/VSM"
PubSubClient MQTT(wifiClient);

#define SS_PIN    21
#define RST_PIN   22

#define SIZE_BUFFER     18
#define MAX_SIZE_BLOCK  16

#define pinVerde     12
#define pinVermelho  32

//esse objeto 'chave' é utilizado para autenticação
MFRC522::MIFARE_Key key;
//código de status de retorno da autenticação
MFRC522::StatusCode status;

// Definicoes pino modulo RC522
MFRC522 mfrc522(SS_PIN, RST_PIN); 

//GERAL
int LED = 2;

//Fuso Horário, no caso horário de verão de Brasília 
int timeZone = -2;

//Struct com os dados do dia e hora
struct Date{
    int dayOfWeek;
    int day;
    int month;
    int year;
    int hours;
    int minutes;
    int seconds;
};

//Socket UDP que a lib utiliza para recuperar dados sobre o horário
WiFiUDP udp;

//Objeto responsável por recuperar dados sobre horário
NTPClient ntpClient(
    udp,                    //socket udp
    "0.br.pool.ntp.org",    //URL do servwer NTP
    timeZone*3600,          //Deslocamento do horário em relacão ao GMT 0
    60000);                 //Intervalo entre verificações online

//Nomes dos dias da semana
char* dayOfWeekNames[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

void configMFRC();
void configMQTT();
void conectaWiFi();
void conectaNTP();
void conectaMQTT();
void conexao_http();
void ler_RFID();
void enviar_dados(const char* id_carta, const char* id_esp32, const char* tempo_ss);
void removerSpacos(char str[]);
Date getDate();
String getTime();
void led(int flag);

void setup() {  
  Serial.begin(9600);   
  SPI.begin();          

  conectaWiFi();  

  configMQTT();      
  configMFRC();  
  
  pinMode(LED, OUTPUT);     
  pinMode(pinVerde, OUTPUT);
  pinMode(pinVermelho, OUTPUT);   

  randomSeed(analogRead(0));
}

void loop() {   
  led(0); delay(1000); led(1); 
    
  conectaWiFi();   
  conectaMQTT();        
    
  if (!mfrc522.PICC_IsNewCardPresent())     
    return;        
  else if (mfrc522.PICC_ReadCardSerial()) {           
    Serial.println();
    Serial.println("Cartao RFID identificado...");
    Serial.println();  
    conectaNTP();    
    ler_RFID();    
    
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }                
}

void configMFRC(){
  mfrc522.PCD_Init();   
}

void configMQTT() { 
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);   
}

void conectaWiFi() {  
  if (WiFi.status() == WL_CONNECTED) 
     return;  
  else {
  
    Serial.println();
    Serial.print("Conectando-se a ");
    Serial.print(ssid);
    WiFi.begin(ssid, password);
 
    while (WiFi.status() != WL_CONNECTED) {
      led(1);      
      Serial.print(".");    
    }
 
    Serial.println("");
    Serial.print("WiFi conectada no endereço de IP: ");
    Serial.println(WiFi.localIP());
  }    
}

void conectaNTP()
{
    Serial.println();
    Serial.println("Sincronizando data/tempo com Protocolo NTPClient...");
    
    //Inicializa o client NTP
    ntpClient.begin();
    
    //Espera pelo primeiro update online
    Serial.println("Esperando pela primeira atualizacao...");
    while(!ntpClient.update())
    {
        Serial.print(".");
        ntpClient.forceUpdate();
        delay(500);
    }    
    Serial.println("Atualizacao completada!");
}

void conectaMQTT() { 
    while (!MQTT.connected()) {
        Serial.println(" ");
        Serial.print("Conectando ao Broker MQTT: ");
        Serial.println(BROKER_MQTT);
        if (MQTT.connect(ID_MQTT))             
            Serial.println("Conectado ao Broker com sucesso!");                    
        else {            
            Serial.println("Nao foi possivel se conectar ao broker.");
            Serial.println("Nova tentatica de conexao em 10 segundos.");
            led(1);            
        }
    }
}

void ler_RFID()
{
  //imprime os detalhes tecnicos do cartão/tag
  Serial.println("Dados de leitura do RFID:"); 
  mfrc522.PICC_DumpDetailsToSerial(&(mfrc522.uid)); 

  //Prepara a chave - todas as chaves estão configuradas para FFFFFFFFFFFFh (Padrão de fábrica).
  for (byte i = 0; i < 6; i++) 
    key.keyByte[i] = 0xFF;

  //buffer para colocar os dados ligos
  byte buffer[SIZE_BUFFER] = {0};

  //bloco que faremos a operação
  byte bloco = 1;
  byte tamanho = SIZE_BUFFER;

  long randNumber = random(5,10);
  float rd = randNumber/10.;
  Serial.println();
  Serial.print("Valor randômico: ");
  Serial.println(rd);
  
  //faz a autenticação do bloco que vamos operar
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, bloco, &key, &(mfrc522.uid)); //line 834 of MFRC522.cpp file
  if (status != MFRC522::STATUS_OK) {
    
    Serial.print(F("Authentication failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));    
    digitalWrite(pinVermelho, HIGH);
    delay(1000);
    digitalWrite(pinVermelho, LOW);
    return;    
  }

  //faz a leitura dos dados do bloco  
  status = mfrc522.MIFARE_Read(bloco, buffer, &tamanho);     
  if (status != MFRC522::STATUS_OK) {
    
    Serial.print(F("Reading failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    
    digitalWrite(pinVermelho, HIGH);
    delay(1000);
    digitalWrite(pinVermelho, LOW);
    return;
    
  } else {
    
    digitalWrite(pinVerde, HIGH);
    delay(1000);
    digitalWrite(pinVerde, LOW);
  }
  
  //Mostra UID na serial
  String id_carta = "";  
  for (byte i = 0; i < mfrc522.uid.size; i++)    
    id_carta.concat(String(mfrc522.uid.uidByte[i], HEX));         
  
  enviar_dados(id_carta.c_str(),"01",getTime().c_str());   //ESP32 código 01 

  //SIMULAÇÃO DA BANDEIJA PASSANDO POR VÁRIOS RFIDs NA ESTEIRA DO CHÃO DE FÁBRICA    
  conectaWiFi(); conectaMQTT(); delay(rd*1200); enviar_dados(id_carta.c_str(),"02",getTime().c_str());   //ESP32 código 02
  conectaWiFi(); conectaMQTT(); delay(rd*5500); enviar_dados(id_carta.c_str(),"03",getTime().c_str());   //ESP32 código 03
  conectaWiFi(); conectaMQTT(); delay(rd*1000); enviar_dados(id_carta.c_str(),"04",getTime().c_str());   //ESP32 código 04
  conectaWiFi(); conectaMQTT(); delay(rd*2100); enviar_dados(id_carta.c_str(),"05",getTime().c_str());   //ESP32 código 04
  conectaWiFi(); conectaMQTT(); delay(rd*1100); enviar_dados(id_carta.c_str(),"06",getTime().c_str());   //ESP32 código 04
  conectaWiFi(); conectaMQTT(); delay(rd*1300); enviar_dados(id_carta.c_str(),"07",getTime().c_str());   //ESP32 código 04
}

void enviar_dados(const char* id_carta, const char* id_esp32, const char* tempo_ss) {     

  char tag[strlen_P(id_carta) + strlen_P(id_esp32) + strlen_P(tempo_ss)+2] = "";
  const char* espaco_v = " ";   
        
  strncat_P(tag,id_carta,strlen_P(id_carta));
  removerSpacos(tag);  
  strncat_P(tag,espaco_v,strlen_P(espaco_v));
  strncat_P(tag,id_esp32,strlen_P(id_esp32));
  strncat_P(tag,espaco_v,strlen_P(espaco_v));
  strncat_P(tag,tempo_ss,strlen_P(tempo_ss));  

  Serial.println("Publicação dos dados do RFID:"); 
  Serial.print("BROKER: ");    
  Serial.println(BROKER_MQTT);
  Serial.print("TOPICO: ");
  Serial.println(TOPIC_PUBLISH);
  Serial.print("DADOS: ");
  Serial.println(tag);     
  MQTT.publish(TOPIC_PUBLISH, tag);   
}

void removerSpacos(char str[]) {
    int j = 1;
    int t = strlen(str);
    for (int i=1; i<t; i++) {
        if (!isWhitespace(str[i])) {
           str[j] = str[i];
           j++;           
        }
    }
    str[j] = '\0';    
}

Date getDate()
{
    //Recupera os dados de data e horário usando o client NTP
    char* strDate = (char*)ntpClient.getFormattedDate().c_str();

    //Passa os dados da string para a struct
    Date date;
    sscanf(strDate, "%d-%d-%dT%d:%d:%dZ", 
                    &date.year, 
                    &date.month, 
                    &date.day, 
                    &date.hours, 
                    &date.minutes,
                    &date.seconds);

    //Dia da semana de 0 a 6, sendo 0 o domingo
    date.dayOfWeek = ntpClient.getDay();
    return date;
}

String getTime(){
    
  Date date = getDate();
  unsigned long time = 3600.*date.hours+60.*date.minutes+date.seconds;

  Serial.println();
  Serial.println("Data/tempo fornecido pelo protocolo NTPClient:");
  Serial.print("NTP DATE: ");
  Serial.print(date.day);
  Serial.print("/");
  Serial.print(date.month);
  Serial.print("/");
  Serial.println(date.year); 
  
  Serial.print("NTP TIME: ");
  Serial.print(date.hours);
  Serial.print(":");
  Serial.print(date.minutes);
  Serial.print(":");
  Serial.println(date.seconds);      
  
  Serial.print("TIME_TOT(s): ");
  Serial.println(time);
  Serial.println();
  
  return String(time, DEC);  
}

void led(int flag){    
  if (flag)
    digitalWrite(LED, HIGH);   
  else
    digitalWrite(LED_BUILTIN, LOW);        
}
