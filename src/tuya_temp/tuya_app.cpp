#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <TuyaWifi.h>

// ==========================================
// SEUS DADOS DA TUYA
// ==========================================
unsigned char pid[] = "ulo08ycj8t1pmknv";
unsigned char mcu_ver[] = "1.0.0";

// ==========================================
// PINOS DOS SENSORES - CORRIGIDOS (SEM D3)
// ==========================================
#define SENSOR_BAIXO D1     // GPIO5 - Seguro
#define SENSOR_MEDIO D2     // GPIO4 - Seguro
#define SENSOR_ALTO D5      // GPIO14 - Seguro (ANTIGO D3)
#define RESET_CONFIG_PIN D6 // GPIO12 - Seguro

// ==========================================
// CONFIGURAÇÃO DOS DPs DA TUYA
// ==========================================
#define DPID_STATUS 1 // Liquid level status (Enum)
#define DPID_DEPTH 2  // Liquid level depth (Value 0-10000)
#define DPID_RATIO 22 // Liquid level ratio (Value 0-100)

// Tabela de DPs
unsigned char dp_array[][2] = {
    {DPID_STATUS, DP_TYPE_ENUM},
    {DPID_DEPTH, DP_TYPE_VALUE},
    {DPID_RATIO, DP_TYPE_VALUE}};

TuyaWifi my_device;
WiFiManager wifiManager;

// ==========================================
// VARIÁVEIS DE ESTADO
// ==========================================
unsigned char nivel_status = 0;
int profundidade_cm = 0;
int percentual = 0;
bool wifiConfigurado = false;
bool modoPareamentoAtivo = false;

#define ALTURA_MAX_CM 200 // Ajuste conforme sua caixa

// ==========================================
// CALLBACK DO MODO DE CONFIGURAÇÃO
// ==========================================
void configModeCallback(WiFiManager *myWiFiManager)
{
    Serial.println("==========================================");
    Serial.println("📲 MODO DE CONFIGURAÇÃO DO WIFI ATIVADO!");
    Serial.println("==========================================");
    Serial.print("📡 SSID do AP: ");
    Serial.println(myWiFiManager->getConfigPortalSSID());
    Serial.println("🌐 IP do portal: 192.168.4.1");
    Serial.println("==========================================");
}

// ==========================================
// FUNÇÃO PARA ENTRAR EM MODO DE PAREAMENTO
// ==========================================
void entrarModoPareamento()
{
    if (!modoPareamentoAtivo)
    {
        modoPareamentoAtivo = true;
        Serial.println("\n==========================================");
        Serial.println("📲 ATIVANDO MODO DE PAREAMENTO DA TUYA");
        Serial.println("==========================================");
        Serial.println("1. Abra o App Smart Life");
        Serial.println("2. Toque em '+' ou 'Adicionar dispositivo'");
        Serial.println("3. Selecione 'Sensor' → 'Sensor Wi-Fi'");
        Serial.println("4. Digite a senha do Wi-Fi quando solicitado");
        Serial.println("==========================================\n");

        my_device.mcu_set_wifi_mode(SMART_CONFIG);
    }
}

// ==========================================
// RESETAR CONFIGURAÇÕES DO WIFI
// ==========================================
void resetWiFiSettings()
{
    Serial.println("\n🔄 RESETANDO CONFIGURAÇÕES DO WIFIMANAGER");
    wifiManager.resetSettings();
    wifiConfigurado = false;
    modoPareamentoAtivo = false;
    Serial.println("✅ Configurações resetadas! Reiniciando...");
    delay(2000);
    ESP.restart();
}

// ==========================================
// IMPRIMIR INFORMAÇÕES
// ==========================================
void printSystemInfo()
{
    Serial.println("\n==========================================");
    Serial.println("📊 INFORMAÇÕES DO SISTEMA");
    Serial.println("==========================================");
    Serial.printf("Chip ID: %08X\n", ESP.getChipId());
    Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Wi-Fi Status: %s\n", WiFi.status() == WL_CONNECTED ? "CONECTADO" : "DESCONECTADO");
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
        Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
    }

    Serial.println("\n🔹 PINOS DOS SENSORES:");
    Serial.println("   - Sensor Baixo (Alarme inferior): D1 (GPIO5)");
    Serial.println("   - Sensor Médio: D2 (GPIO4)");
    Serial.println("   - Sensor Alto (Alarme superior): D5 (GPIO14)");

    Serial.println("\n🔹 ESTADO DOS SENSORES:");
    Serial.printf("   - Sensor Baixo (D1): %s\n", digitalRead(SENSOR_BAIXO) == LOW ? "MOLHADO ⚠️" : "SECO ✅");
    Serial.printf("   - Sensor Médio (D2): %s\n", digitalRead(SENSOR_MEDIO) == LOW ? "MOLHADO" : "SECO");
    Serial.printf("   - Sensor Alto (D5): %s\n", digitalRead(SENSOR_ALTO) == LOW ? "MOLHADO ⚠️" : "SECO ✅");

    Serial.println("\n🔹 ESTADO DA CAIXA D'ÁGUA:");
    Serial.printf("   - Status: %s\n",
                  nivel_status == 0 ? "NORMAL" : (nivel_status == 1 ? "ALARME BAIXO" : "ALARME ALTO"));
    Serial.printf("   - Profundidade: %.2f m\n", profundidade_cm / 100.0);
    Serial.printf("   - Percentual: %d %%\n", percentual);

    Serial.println("==========================================");
}

// ==========================================
// PROCESSAR COMANDOS SERIAL
// ==========================================
void processSerialCommands()
{
    if (Serial.available())
    {
        char command = Serial.read();

        if (command >= 'a' && command <= 'z')
        {
            command = command - 'a' + 'A';
        }

        if (command == 'P')
        {
            Serial.println("\n📲 Comando 'P' recebido!");
            entrarModoPareamento();
        }
        else if (command == 'A')
        {
            Serial.println("\n📲 Ativando modo AP (Hotspot)");
            my_device.mcu_set_wifi_mode(AP_CONFIG);
            Serial.println("Conecte-se ao hotspot 'SmartLife_XXXX' no seu celular");
        }
        else if (command == 'R')
        {
            Serial.println("\n🔄 Comando 'R' recebido!");
            Serial.println("⚠️ Digite 'Y' para confirmar reset: ");

            unsigned long timeout = millis() + 5000;
            bool confirmed = false;
            while (millis() < timeout && !confirmed)
            {
                if (Serial.available())
                {
                    char confirm = Serial.read();
                    if (confirm == 'Y' || confirm == 'y')
                    {
                        confirmed = true;
                    }
                    break;
                }
            }

            if (confirmed)
            {
                resetWiFiSettings();
            }
            else
            {
                Serial.println("❌ Reset cancelado");
            }
        }
        else if (command == 'I')
        {
            Serial.println("\n📊 Comando 'I' recebido!");
            printSystemInfo();
        }

        while (Serial.available())
        {
            Serial.read();
        }
    }
}

// ==========================================
// CONFIGURAR WI-FI
// ==========================================
void setupWiFi()
{
    Serial.println("\n📡 Inicializando WiFiManager...");

    if (WiFi.SSID().length() > 0)
    {
        wifiConfigurado = true;
        Serial.printf("📡 Configuração encontrada: %s\n", WiFi.SSID().c_str());
    }
    else
    {
        wifiConfigurado = false;
        Serial.println("⚠️ Nenhuma configuração Wi-Fi encontrada!");
    }

    wifiManager.setConfigPortalTimeout(180);
    wifiManager.setAPCallback(configModeCallback);

    String apName = "CaixaAgua_" + String(ESP.getChipId(), HEX);
    Serial.print("📱 Nome do AP: ");
    Serial.println(apName);

    bool connected = wifiManager.autoConnect(apName.c_str(), "12345678");

    if (connected)
    {
        Serial.println("✅ Conectado ao Wi-Fi!");
        Serial.print("📡 IP: ");
        Serial.println(WiFi.localIP());
        wifiConfigurado = true;
        modoPareamentoAtivo = false;
    }
    else
    {
        Serial.println("❌ Sem configuração de Wi-Fi.");
        wifiConfigurado = false;
    }
}

// ==========================================
// CALCULAR NÍVEIS
// ==========================================
void calcularNiveis()
{
    bool baixo = digitalRead(SENSOR_BAIXO);
    bool medio = digitalRead(SENSOR_MEDIO);
    bool alto = digitalRead(SENSOR_ALTO);

    unsigned char novo_status = nivel_status;
    int novo_profundidade = profundidade_cm;
    int novo_percentual = percentual;

    // Lógica com HIGH/LOW padrão (LOW = molhado)
    if (alto == LOW)
    {
        novo_status = 2;
        novo_profundidade = ALTURA_MAX_CM;
        novo_percentual = 100;
        Serial.println("💧 Nível: CHEIO (ALARME SUPERIOR)");
    }
    else if (medio == LOW)
    {
        novo_status = 0;
        novo_profundidade = (ALTURA_MAX_CM * 66) / 100;
        novo_percentual = 66;
        Serial.println("💧 Nível: MÉDIO (NORMAL)");
    }
    else if (baixo == LOW)
    {
        novo_status = 1;
        novo_profundidade = (ALTURA_MAX_CM * 33) / 100;
        novo_percentual = 33;
        Serial.println("💧 Nível: BAIXO (ALARME INFERIOR) ⚠️");
    }
    else
    {
        novo_status = 1;
        novo_profundidade = 0;
        novo_percentual = 0;
        Serial.println("💧 Nível: VAZIO (ALARME INFERIOR) 🚨");
    }

    // Atualiza DPs se houver mudança
    if (novo_status != nivel_status)
    {
        nivel_status = novo_status;
        // my_device.mcu_dp_update(DPID_STATUS, nivel_status, 1);
        //  Na função calcularNiveis(), após cada mcu_dp_update()
        int resultado = my_device.mcu_dp_update(DPID_STATUS, nivel_status, 1);
        Serial.printf("DEBUG: Envio DP%d resultado = %d\n", DPID_STATUS, resultado);
    }

    if (novo_profundidade != profundidade_cm)
    {
        profundidade_cm = novo_profundidade;
        my_device.mcu_dp_update(DPID_DEPTH, profundidade_cm, 4);
    }

    if (novo_percentual != percentual)
    {
        percentual = novo_percentual;
        my_device.mcu_dp_update(DPID_RATIO, percentual, 4);
    }
}

// ==========================================
// SETUP E LOOP
// ==========================================
void tuya_setup()
{
    Serial.begin(115200);
    delay(100);

    Serial.println("\n==========================================");
    Serial.println("🚰 SENSOR DE NÍVEL DA CAIXA D'ÁGUA");
    Serial.println("==========================================");
    Serial.println("PID: ulo08ycj8t1pmknv");
    Serial.println("\n🔌 PINAGEM CORRIGIDA (SEM D3):");
    Serial.println("   - Sensor Baixo: D1 (GPIO5)");
    Serial.println("   - Sensor Médio: D2 (GPIO4)");
    Serial.println("   - Sensor Alto:  D5 (GPIO14) ← ANTIGO D3");
    Serial.println("   - Reset Config: D6 (GPIO12)");

    pinMode(SENSOR_BAIXO, INPUT_PULLUP);
    pinMode(SENSOR_MEDIO, INPUT_PULLUP);
    pinMode(SENSOR_ALTO, INPUT_PULLUP);
    pinMode(RESET_CONFIG_PIN, INPUT_PULLUP);

    setupWiFi();

    my_device.set_dp_cmd_total(dp_array, 3);
    my_device.init(pid, mcu_ver);

    // Se não tem Wi-Fi configurado, entra em modo de pareamento
    if (!wifiConfigurado)
    {
        entrarModoPareamento();
    }

    printSystemInfo();
    Serial.println("\n💡 Comandos: P=Pareamento | R=Reset WiFi | I=Info\n");
}

void tuya_loop()
{
    processSerialCommands();

    if (digitalRead(RESET_CONFIG_PIN) == LOW)
    {
        delay(100);
        if (digitalRead(RESET_CONFIG_PIN) == LOW)
        {
            resetWiFiSettings();
        }
    }

    calcularNiveis();
    my_device.uart_service();

    delay(500);
}

// Funções principais (chame estas no seu main.cpp)
// void setup() { tuya_setup(); }//
// void loop() { tuya_loop(); }