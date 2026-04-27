// comum namespace
#ifndef COMUM_H
#define COMUM_H
#include <Arduino.h>
#include <ESP8266WiFi.h>

#define WIFI_TIMEOUT_RESET_MS 120000 // 60 segundos sem Wi-Fi = reset automático

// criar namespace Comum
namespace Comum
{
    bool wasWiFiConnected = false;
    int wifiDisconnectedStartTime = 0;
    // =============================================
    //  VERIFICAR WIFI E RESET AUTOMATICO
    // =============================================
    void verificarWiFi()
    {
        static unsigned long lastWiFiCheck = 0;
        bool isConnected = (WiFi.status() == WL_CONNECTED);

        // Detectar mudança no estado do Wi-Fi
        if (isConnected != wasWiFiConnected)
        {
            if (isConnected)
            {
                // Wi-Fi conectou ou reconectou
                Serial.println("[WiFi] Conectado/Reconectado!");
                wifiDisconnectedStartTime = 0;
            }
            else
            {
                // Wi-Fi perdeu conexão
                Serial.println("[WiFi] Desconectado! Iniciando contagem para reset...");
                wifiDisconnectedStartTime = millis();
            }
            wasWiFiConnected = isConnected;
        }

        // Verificar se está desconectado por muito tempo
        if (!isConnected && wifiDisconnectedStartTime > 0)
        {
            unsigned long disconnectedDuration = millis() - wifiDisconnectedStartTime;

            if (disconnectedDuration >= WIFI_TIMEOUT_RESET_MS)
            {
                Serial.println("\n========================================");
                Serial.print("⚠️  SEM CONEXÃO WI-FI POR ");
                Serial.print(disconnectedDuration / 1000);
                Serial.println(" SEGUNDOS!");
                Serial.println("🔄 EXECUTANDO RESET AUTOMÁTICO...");
                Serial.println("========================================\n");

                // Feedback visual: pisca LED rapidamente 5 vezes antes do reset
                for (int i = 0; i < 5; i++)
                {
                    digitalWrite(LED_BUILTIN, LOW);
                    delay(150);
                    digitalWrite(LED_BUILTIN, HIGH);
                    delay(150);
                }

                delay(1000);
                ESP.restart();
            }
            else if (disconnectedDuration >= 30000)
            {
                // Aviso após 30 segundos
                if ((disconnectedDuration / 1000) % 10 == 0)
                { // A cada 10 segundos
                    Serial.print("[WiFi] Ainda desconectado. Reset em ");
                    Serial.print((WIFI_TIMEOUT_RESET_MS - disconnectedDuration) / 1000);
                    Serial.println(" segundos...");
                }

                // Feedback visual: pisca LED em padrão de angústia (rápido)
                if ((millis() % 500) < 250)
                {
                    digitalWrite(LED_BUILTIN, LOW);
                }
                else
                {
                    digitalWrite(LED_BUILTIN, HIGH);
                }
            }
        }

        // Tentar reconectar se desconectado (mas sem reset)
        if (!isConnected && millis() - lastWiFiCheck > 30000)
        {
            Serial.println("[WiFi] Tentando reconectar...");
            WiFi.reconnect();
            lastWiFiCheck = millis();
        }
    }

    String gerarNomeBaseadoNoMAC()
    {
        String mac = WiFi.macAddress();
        mac.replace(":", "");
        return NOME_ID "_" + mac;
    }

    String gerarNomeMDNS(String nome)
    {
        String nome_mdns = nome;

        nome_mdns.toLowerCase();

        for (int i = 0; i < nome_mdns.length(); i++)
        {
            char c = nome_mdns[i];
            if (!(isalnum(c) || c == '-'))
            {
                nome_mdns.setCharAt(i, '_');
            }
        }

        if (nome_mdns.length() > 0 && !isalpha(nome_mdns[0]))
        {
            nome_mdns = "device_" + nome_mdns;
        }

        if (nome_mdns.length() > 63)
        {
            nome_mdns = nome_mdns.substring(0, 63);
        }

        return nome_mdns;
    }
}

#endif // COMUM_H