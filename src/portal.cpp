

#ifdef PORTAL
#include "portal.h"
#include <EEPROM.h>

// Lista de pinos seguros no ESP8266
const int pinos_validos[] = {2, 3, 4, 5, 12, 13, 14, 15};
const int num_pinos_validos = 7;

// =============================================
//  FUNCAO PARA GERAR NOME DO AP BASEADO NO MAC
// =============================================
String gerarAPName()
{
    uint8_t mac[6];
    WiFi.macAddress(mac);

    char ap_nome[32];
    snprintf(ap_nome, sizeof(ap_nome), "ESP-%02X%02X", mac[4], mac[5]);

    return String(ap_nome);
}

// =============================================
//  VERIFICAR SE O PINO É SEGURO
// =============================================
bool isPinoSeguro(int pino)
{
    if (pino == 0)
    {
        Serial.print("Atenção: Pino ");
        Serial.print(pino);
        Serial.println(" tem restrições no boot!");
        return false;
    }

    for (int i = 0; i < num_pinos_validos; i++)
    {
        if (pino == pinos_validos[i])
            return true;
    }
    return false;
}

// =============================================
//  FUNCAO PARA SALVAR PINO NA EEPROM
// =============================================
void salvarPinoEEPROM(int pino)
{
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(PIN_CONFIG_ADDR, pino);
    EEPROM.write(MAGIC_NUMBER_ADDR, MAGIC_NUMBER);
    EEPROM.commit();
    EEPROM.end();
    Serial.print("Pino ");
    Serial.print(pino);
    Serial.println(" salvo na EEPROM");
}

// =============================================
//  FUNCAO PARA SALVAR NOME NA EEPROM
// =============================================
void salvarNomeEEPROM(String nome)
{
    EEPROM.begin(EEPROM_SIZE);

    if (nome.length() > 32)
    {
        nome = nome.substring(0, 32);
    }

    for (int i = 0; i < nome.length(); i++)
    {
        EEPROM.write(NOME_CONFIG_ADDR + i, nome[i]);
    }
    EEPROM.write(NOME_CONFIG_ADDR + nome.length(), '\0');

    EEPROM.commit();
    EEPROM.end();

    Serial.print("Nome '");
    Serial.print(nome);
    Serial.println("' salvo na EEPROM");
}

// =============================================
//  FUNCAO PARA SALVAR TIMEOUT NA EEPROM
// =============================================
void salvarTimeoutEEPROM(int timeout)
{
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(TIMEOUT_CONFIG_ADDR, timeout);
    EEPROM.write(MAGIC_NUMBER_ADDR, MAGIC_NUMBER);
    EEPROM.commit();
    EEPROM.end();
    Serial.print("Timeout ");
    Serial.print(timeout);
    Serial.println(" minutos salvo na EEPROM");
}

// =============================================
//  CALLBACKS DO WIFIMANAGER
// =============================================

// Callback quando entra no modo de configuração
void configModeCallback(WiFiManager *myWiFiManager)
{
    Serial.println("\n=== PORTAL DE CONFIGURACAO INICIADO ===");
    Serial.println("Conecte-se a rede Wi-Fi: " + myWiFiManager->getConfigPortalSSID());
    Serial.println("IP do portal: " + WiFi.softAPIP().toString());

    // Piscar LED rapidamente para indicar portal ativo
    portalAtivo = true;
}

// Callback quando configurações são salvas
void saveConfigCallback()
{
    Serial.println("\n=== CONFIGURACOES SALVAS ===");
    shouldSaveConfig = true;
}

// =============================================
//  INICIAR PORTAL DE CONFIGURACAO
// =============================================
void iniciarPortalConfiguracao()
{
    if (portalAtivo)
        return;

    Serial.println("\n=== INICIANDO PORTAL DE CONFIGURACAO ===");
    portalAtivo = true;
    digitalWrite(LED_BUILTIN, HIGH);

    // Desligar o relé durante configuração
    digitalWrite(rele_pin, LOW);
    lampadaLigada = false;
    delay(100);

    // Criar objeto WiFiManager
    WiFiManager wifiManager;

    // Configurar callbacks
    wifiManager.setAPCallback(configModeCallback);
    wifiManager.setSaveConfigCallback(saveConfigCallback);

    // Configurar timeouts
    wifiManager.setConnectTimeout(10);
    wifiManager.setConfigPortalTimeout(300); // 5 minutos

    // Personalizar cabeçalho CSS/JS do portal
    wifiManager.setCustomHeadElement(
        "<style>"
        "body { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); font-family: 'Segoe UI', Arial, sans-serif; margin: 0; padding: 20px; }"
        ".container { background: white; border-radius: 10px; padding: 20px; box-shadow: 0 10px 30px rgba(0,0,0,0.2); max-width: 500px; margin: auto; }"
        "button, input[type='submit'] { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); border: none; border-radius: 5px; color: white; padding: 10px 20px; cursor: pointer; transition: transform 0.3s; font-size: 16px; }"
        "button:hover, input[type='submit']:hover { transform: scale(1.05); }"
        "input[type='text'], input[type='password'] { border: 2px solid #ddd; border-radius: 5px; padding: 8px; width: 100%; margin: 5px 0; font-size: 14px; }"
        "h1 { color: #667eea; text-align: center; }"
        ".info { background: #f0f0f0; padding: 10px; border-radius: 5px; margin: 10px 0; font-size: 14px; border-left: 4px solid #667eea; }"
        "</style>");

    // Configurar título da página
    wifiManager.setTitle("ESP Alexa Lampada - Configuracao");

    // Criar lista de opções para o usuário
    String opcoes_pinos = "Pinos validos: ";
    for (int i = 0; i < num_pinos_validos; i++)
    {
        opcoes_pinos += String(pinos_validos[i]);
        if (i < num_pinos_validos - 1)
            opcoes_pinos += ", ";
    }

    // Parâmetro para o pino do relé
    WiFiManagerParameter custom_rele_pin("rele_pin",
                                         opcoes_pinos.c_str(),
                                         String(rele_pin).c_str(), 3);

    // Parâmetro para o nome do dispositivo Alexa
    WiFiManagerParameter custom_nome_alexa("nome_alexa",
                                           "Nome do dispositivo Alexa (max 32 caracteres)",
                                           nome_alexa.c_str(), 33);

    // Parâmetro informativo
    WiFiManagerParameter info_text("<div class='info'><strong>📌 Informações:</strong><br>"
                                   "✓ Use nome sem caracteres especiais<br>"
                                   "✓ O dispositivo usara mDNS com o mesmo nome<br>"
                                   "✓ Apos configurar, o dispositivo reiniciara</div>");

    // Adicionar parâmetros ao WiFiManager
    wifiManager.addParameter(&info_text);
    wifiManager.addParameter(&custom_rele_pin);
    wifiManager.addParameter(&custom_nome_alexa);

    // Gerar nome único para o AP baseado no MAC
    String ap_nome = gerarAPName();

    // Mensagem no serial
    Serial.println("📡 Conecte-se a rede Wi-Fi: " + ap_nome);
    Serial.println("🌐 Acesse 192.168.4.1 no navegador");

    // Iniciar portal
    wifiManager.startConfigPortal(ap_nome.c_str());

    // Processar nova configuração do pino
    String novo_pino_str = custom_rele_pin.getValue();
    if (novo_pino_str.length() > 0)
    {
        int novo_pino = novo_pino_str.toInt();

        if (isPinoSeguro(novo_pino) && novo_pino != rele_pin)
        {
            rele_pin = novo_pino;
            salvarPinoEEPROM(rele_pin);
            pinMode(rele_pin, OUTPUT);
            digitalWrite(rele_pin, LOW);
            Serial.print("✅ Pino do rele atualizado para: GPIO");
            Serial.println(rele_pin);
        }
        else if (!isPinoSeguro(novo_pino))
        {
            Serial.print("❌ ERRO: Pino invalido: ");
            Serial.println(novo_pino);
        }
    }

    // Processar nova configuração do nome
    String novo_nome = custom_nome_alexa.getValue();
    novo_nome.trim();

    if (novo_nome.length() > 0 && novo_nome != nome_alexa)
    {
        bool nome_valido = true;
        for (int i = 0; i < novo_nome.length(); i++)
        {
            char c = novo_nome[i];
            if (!(isalnum(c) || c == ' ' || c == '_' || c == '-'))
            {
                nome_valido = false;
                break;
            }
        }

        if (nome_valido && novo_nome.length() <= 32)
        {
            nome_alexa = novo_nome;
            salvarNomeEEPROM(nome_alexa);
            Serial.print("✅ Nome Alexa atualizado para: '");
            Serial.print(nome_alexa);
            Serial.println("'");
            shouldSaveConfig = true;
        }
    }

    portalAtivo = false;
    Serial.println("=== PORTAL DE CONFIGURACAO ENCERRADO ===");

    if (shouldSaveConfig)
    {
        Serial.println("🔄 Configuracoes alteradas! Reiniciando em 2 segundos...");
        delay(2000);
        ESP.restart();
    }
}
#endif