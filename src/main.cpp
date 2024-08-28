// Programa: NodeMCU e MQTT - Controle e Monitoramento IoT
// Autor: Danilo Andrade
 
#include <WiFi.h> // Importa a Biblioteca ESP8266WiFi
#include <PubSubClient.h> // Importa a Biblioteca PubSubClient

// defines:
// defines de id mqtt e tópicos para publicação e subscribe
#define TOPICO_SUBSCRIBE "MQTT_TANK_IN"     // tópico MQTT de escuta
#define TOPICO_PUBLISH   "TANQUE_RENASCENCA" // tópico MQTT de envio de informações para Broker
#define ID_MQTT  "TANQUE"                     // id mqtt (para identificação de sessão)

//======================================================================================================
//                                  DEFINE OS PINOS DO ESP32
//======================================================================================================

#define D0    16
#define D1    5
#define D2    4
#define D3    0
#define D4    2
#define D5    14
#define D6    12
#define D7    13
#define D8    15
#define D9    3
#define D10   1
#define A0    35 // Definindo o pino do sensor como A0

#define NUM_READINGS 30  // Número de leituras a serem feitas para calcular a média
 
// WIFI
const char* SSID = "4G-MIFI-614"; // SSID / nome da rede WI-FI que deseja se conectar
const char* PASSWORD = "1234567890"; // Senha da rede WI-FI que deseja se conectar
  
// MQTT
const char* BROKER_MQTT = "broker.hivemq.com"; // URL do broker MQTT que se deseja utilizar
int BROKER_PORT = 1883; // Porta do Broker MQTT
 
// Variáveis e objetos globais
WiFiClient espClient; // Cria o objeto espClient
PubSubClient MQTT(espClient); // Instancia o Cliente MQTT passando o objeto espClient
char EstadoSaida = '0';  // variável que armazena o estado atual da saída
float leitura;
float pressao;
char payload[23] = "00.0";

// Função para calcular a média das leituras analógicas
int readAnalogAverage(int pin) {
    long sum = 0;  // Usamos long para evitar overflow
    for (int i = 0; i < NUM_READINGS; i++) {
        sum += analogRead(pin);
        delay(10);  // Pequeno atraso para permitir estabilização
    }
    return sum / NUM_READINGS;  // Retorna a média
}

// Prototypes
void initSerial();
void initWiFi();
void initMQTT();
void reconectWiFi(); 
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void VerificaConexoesWiFIEMQTT(void);
void InitOutput(void);
void EnviaEstadoOutputMQTT(void);

// Implementações das funções
void setup() 
{
    // Inicializações:
    InitOutput();
    initSerial();
    initWiFi();
    initMQTT();
}
  
// Função: inicializa comunicação serial com baudrate 115200
void initSerial() 
{
    Serial.begin(115200);
}
 
// Função: inicializa e conecta-se na rede WI-FI desejada
void initWiFi() 
{
    delay(10);
    Serial.println("------Conexao WI-FI------");
    Serial.print("Conectando-se na rede: ");
    Serial.println(SSID);
    Serial.println("Aguarde");
     
    reconectWiFi();
}
  
// Função: inicializa parâmetros de conexão MQTT
void initMQTT() 
{
    MQTT.setServer(BROKER_MQTT, BROKER_PORT);
    MQTT.setCallback(mqtt_callback);
}
  
// Função: função de callback 
void mqtt_callback(char* topic, byte* payload, unsigned int length) 
{
    String msg;

    // Obtem a string do payload recebido
    for(int i = 0; i < length; i++) 
    {
        char c = (char)payload[i];
        msg += c;
    }
   
    // Toma ação dependendo da string recebida
    if (msg.equals("L"))
    {
        digitalWrite(D0, LOW);
        EstadoSaida = '1';
    }
 
    if (msg.equals("D"))
    {
        digitalWrite(D0, HIGH);
        EstadoSaida = '0';
    }
}
  
// Função: reconecta-se ao broker MQTT
void reconnectMQTT() 
{
    while (!MQTT.connected()) 
    {
        Serial.print("* Tentando se conectar ao Broker MQTT: ");
        Serial.println(BROKER_MQTT);
        if (MQTT.connect(ID_MQTT)) 
        {
            Serial.println("Conectado com sucesso ao broker MQTT!");
            MQTT.subscribe(TOPICO_SUBSCRIBE); 
        } 
        else
        {
            Serial.println("Falha ao reconectar no broker.");
            Serial.println("Haverá nova tentativa de conexão em 2s");
            delay(2000);
        }
    }
}
  
// Função: reconecta-se ao WiFi
void reconectWiFi() 
{
    if (WiFi.status() == WL_CONNECTED)
        return;
         
    WiFi.begin(SSID, PASSWORD); // Conecta na rede WI-FI
     
    while (WiFi.status() != WL_CONNECTED) 
    {
        delay(100);
        Serial.print(".");
    }
   
    Serial.println();
    Serial.print("Conectado com sucesso na rede ");
    Serial.print(SSID);
    Serial.println(" IP obtido: ");
    Serial.println(WiFi.localIP());
}
 
// Função: verifica o estado das conexões WiFi e ao broker MQTT
void VerificaConexoesWiFIEMQTT(void)
{
    if (!MQTT.connected()) 
        reconnectMQTT(); // se não há conexão com o Broker, a conexão é refeita
     
    reconectWiFi(); // se não há conexão com o WiFi, a conexão é refeita
}
 
// Função: envia ao Broker o estado atual do output 
void EnviaEstadoOutputMQTT(void)
{
    if (EstadoSaida == '1')
        MQTT.publish(TOPICO_PUBLISH, "L");
}

// Função: inicializa o output em nível lógico baixo
void InitOutput(void)
{
    pinMode(D0, OUTPUT);
    digitalWrite(D0, HIGH);          
}
 
// Programa principal
void loop() 
{   
    // Lê a média do sensor de pressão
    int rawValue = readAnalogAverage(A0);
    
    // Converte o valor bruto para uma tensão (ajuste conforme necessário)
    float voltage = rawValue * (3.3 / 4095.0);  // Considerando VCC = 3.3V e ADC de 12 bits
    
    // Converte a tensão em corrente (4-20 mA)
    float current_mA = (voltage / 3.3) * 20;  // Simplificado para um range de 4-20 mA
    
    // Converte a corrente em pressão (16 a 1600 mbar)
    float pressure_mbar = 16 + ((current_mA - 4) * (2325) / (20 - 4));
    
    // Imprime a pressão no monitor serial
    Serial.print("Pressure: ");
    Serial.print(pressure_mbar);
    Serial.println(" mbar");

    // Prepara o payload para envio via MQTT
    char TEMP[5];   
    String CUR = String(pressure_mbar, 1);  
    CUR.toCharArray(TEMP, 4);            
    strcpy(payload, TEMP);
    strcat(payload, " mbar"); // Adiciona unidade ao payload

    // Publica a pressão no tópico MQTT
    MQTT.publish(TOPICO_PUBLISH, payload);

    // Garante funcionamento das conexões WiFi e ao broker MQTT
    VerificaConexoesWiFIEMQTT();
 
    // Envia o status de todos os outputs para o Broker no protocolo esperado
    EnviaEstadoOutputMQTT();
 
    // Keep-alive da comunicação com broker MQTT
    MQTT.loop();
    delay(1000); // Atraso de 1 segundo entre as leituras
}
