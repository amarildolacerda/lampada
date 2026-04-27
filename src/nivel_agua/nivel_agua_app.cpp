#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h> // by tzapu
#include <ESP8266WebServer.h>
#include <Espalexa.h> // by Aircoookie
#include <EEPROM.h>
#include <ESP8266mDNS.h> // Para mDNS (acesso via .local)
#include <WebSocketsServer.h>

#ifdef NIVEL_AGUA_APP
#include "nivel_agua_sensor.h"
#include "portal.h"
#include "client.h"
#include "comum.h"

Espalexa espalexa;
bool mdnsIniciado = false;
String nome_alexa = NOME_ID;
bool portalAtivo = false;
bool shouldSaveConfig = false;
int timeout_minutes = MAX_TIMED_ON_MINUTES;

// =============================================
//  VARIÁVEIS GLOBAIS
// =============================================
NivelAguaSensor *sensor_nivel = nullptr;

EspalexaDevice *alexa_device = nullptr;              // Ponteiro para o dispositivo Espalexa
ESP8266WebServer *server = new ESP8266WebServer(80); // Ponteiro para o servidor web

WebSocketsServer webSocket(81); // Porta 81 para WebSocket

// Pinos do sensor (GPIO5 = D1, GPIO4 = D2)
int pino_sensor_inferior = 5; // D1
int pino_sensor_superior = 4; // D2
int rele_pin = pino_sensor_inferior;
bool lampadaLigada = false;
String nivel_sensor_string = "";

// Variáveis de controle
unsigned long ultimo_log = 0;
const unsigned long INTERVALO_LOG = 1000; // 1 segundo

// Variáveis de estado
unsigned long tempo_ultima_mudanca = 0;
NivelAgua nivel_anterior_armazenado = VAZIO;

// =============================================
//  FUNÇÕES AUXILIARES
// =============================================
void enviarInformacoesDispositivo(uint8_t clientNum = 255);

// =============================================
//  FUNCAO PARA CARREGAR NOME NA EEPROM
// =============================================
String carregarNomeEEPROM()
{
    EEPROM.begin(EEPROM_SIZE);

    char nome_buffer[33] = {0};
    int i = 0;

    while (i < 32)
    {
        char c = EEPROM.read(NOME_CONFIG_ADDR + i);
        if (c == '\0')
            break;
        nome_buffer[i] = c;
        i++;
    }
    nome_buffer[i] = '\0';

    EEPROM.end();

    String nome = String(nome_buffer);
    if (nome.length() == 0)
    {
        Serial.println("Nenhum nome encontrado na EEPROM, gerando baseado no MAC");
        return Comum::gerarNomeBaseadoNoMAC();
    }

    Serial.print("Nome carregado da EEPROM: ");
    Serial.println(nome);
    return nome;
}

void iniciarMDNS()
{
    if (mdnsIniciado)
    {
        MDNS.end();
        delay(100);
    }

    String nome_mdns = Comum::gerarNomeMDNS(nome_alexa);

    Serial.println("\n=== INICIANDO mDNS ===");
    Serial.print("Nome mDNS gerado: ");
    Serial.print(nome_mdns);
    Serial.println(".local");

    if (MDNS.begin(nome_mdns.c_str()))
    {
        MDNS.addService("http", "tcp", 80);
        MDNS.addService("espalexa", "tcp", 80);
        MDNS.addService("ws", "tcp", 81); // Adicionar serviço WebSocket

        Serial.println("mDNS iniciado com sucesso!");
        Serial.print("Acesse: http://");
        Serial.print(nome_mdns);
        Serial.println(".local");
        Serial.print("WebSocket: ws://");
        Serial.print(nome_mdns);
        Serial.println(".local:81");

        mdnsIniciado = true;
    }
    else
    {
        Serial.println("ERRO: Falha ao iniciar mDNS!");
        mdnsIniciado = false;
    }
    Serial.println("========================\n");
}

/**
 * Processa ações baseadas no nível de água
 */
void processarNivelAgua()
{
    NivelAgua nivel = sensor_nivel->getNivel();
    nivel_sensor_string = sensor_nivel->getNivelString();

    switch (nivel)
    {
    case VAZIO:
        Serial.println("[NivelAgua] Caixa VAZIA! Ligar bomba se necessário.");
        // Adicione aqui lógica para ligar bomba, disparar alarme, etc
        break;

    case MEIO:
        Serial.println("[NivelAgua] Nível no MEIO. Sistema funcionando normalmente.");
        // Status intermediário
        break;

    case CHEIO:
        Serial.println("[NivelAgua] Caixa CHEIA! Parar enchimento.");
        // Adicione aqui lógica para parar bomba, desligar etc
        break;
    }
}

/**
 * Envia informações do sensor via Serial (para debug/monitoramento)
 */
void enviarStatusNivel()
{
    unsigned long agora = millis();

    if (agora - ultimo_log >= INTERVALO_LOG)
    {
        String status = "{";
        status += "\"sensor\":\"nivel_agua\",";
        status += "\"nivel\":\"" + sensor_nivel->getNivelString() + "\",";
        status += "\"mudou\":" + String(sensor_nivel->houveMudanca() ? "true" : "false") + ",";
        status += "\"tempo\":" + String(agora / 1000);
        status += "}";

        Serial.println(status);
        ultimo_log = agora;
    }
}

// =============================================
//  CALLBACK DA ALEXA
// =============================================
void callbackNivel(EspalexaDevice *d)
{
    if (d == nullptr)
        return;

    // acionarRele(d->getValue() > 0);
    sensor_nivel->atualizar();
    nivel_sensor_string = sensor_nivel->getNivelString();
    Serial.print("[Alexa] -> Nivel Liquido: ");
    Serial.print(nivel_sensor_string);
    Serial.print(" (Pino Vazio GPIO");
    Serial.print(pino_sensor_inferior);
    Serial.print(") - Nome: ");
    Serial.println(nome_alexa);
}

// =============================================
//  PROCEDIMENTO CENTRALIZADO PARA ENVIAR INFORMACOES DO DISPOSITIVO
// =============================================
void enviarInformacoesDispositivo(uint8_t clientNum)
{
    String info = "{";
    info += "\"device\":\"" + nome_alexa + "\",";
    info += "\"state\":" + String(lampadaLigada ? "true" : "false") + ",";
    info += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    info += "\"mac\":\"" + WiFi.macAddress() + "\",";
    info += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    info += "\"pino_sensor_inferior\":" + String(pino_sensor_inferior) + ",";
    info += "\"uptime\":" + String(millis() / 1000) + ",";
    info += "\"mdns\":" + String(mdnsIniciado ? "true" : "false");
    info += "}";

    if (clientNum == 255)
    {
        // Broadcast para todos os clientes conectados
        webSocket.broadcastTXT(info);
        Serial.println("[WebSocket] Informações broadcast: " + info);
    }
    else
    {
        // Enviar para cliente específico
        webSocket.sendTXT(clientNum, info);
        Serial.println("[WebSocket] Informações enviadas para cliente " + String(clientNum) + ": " + info);
    }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
    switch (type)
    {
    case WStype_DISCONNECTED:
        Serial.printf("[WebSocket] Cliente %u desconectado!\n", num);
        break;
    case WStype_CONNECTED:
    {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[WebSocket] Cliente %u conectado de %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
        // Send initial status
        // adicionar gpio e nome do dispositivo
        String status = "{\"status\":\"connected\",\"device\":\"" + nome_alexa + "\",\"gpio_empty\":" + String(pino_sensor_inferior) + "}";
        // String status = "{\"status\":\"connected\",\"device\":\"" + nome_alexa + ",\"}";
        webSocket.sendTXT(num, status);
        // Enviar informações completas
        enviarInformacoesDispositivo(num);
        break;
    }
    case WStype_TEXT:
    {
        Serial.printf("[WebSocket] Recebido do cliente %u: %s\n", num, payload);
        String message = String((char *)payload);
        if (message == "restart")
        {
            Serial.println("[WebSocket] Comando restart recebido. Reiniciando ESP...");
            webSocket.sendTXT(num, "{\"response\":\"restarting\"}");
            delay(100);
            ESP.restart();
        }
        else if (message == "status")
        {
            enviarInformacoesDispositivo(num);
        }
        else if (message == "toggle")
        {
            // acionarRele(!lampadaLigada);
            String response = "{\"response\":\"toggled\",\"state\":" + String(lampadaLigada ? "true" : "false") + "}";
            webSocket.sendTXT(num, response);
        }
        break;
    }
    case WStype_BIN:
        Serial.printf("[WebSocket] Mensagem binária recebida do cliente %u\n", num);
        break;
    }
}

// =============================================
//  FUNCAO PARA RESETAR CONFIGURACOES
// =============================================
void resetarConfiguracoes()
{
    Serial.println("\n========================================");
    Serial.println("=== RESETANDO TODAS CONFIGURACOES ===");
    Serial.println("========================================\n");

    // Limpar configurações WiFi
    WiFiManager wifiManager;
    wifiManager.resetSettings();

    // Limpar EEPROM
    EEPROM.begin(EEPROM_SIZE);
    for (int i = 0; i < EEPROM_SIZE; i++)
    {
        EEPROM.write(i, 0);
    }
    EEPROM.commit();
    EEPROM.end();

    Serial.println("✅ Configuracoes resetadas com sucesso!");
    Serial.println("\n🔄 Reiniciando em 3 segundos...");

    delay(3000);
    ESP.restart();
}

// =============================================
//  CONFIGURAR SERVIDOR WEB UNIFICADO
// =============================================
void configurarServidorWeb()
{
    // Criar servidor na porta 80
    server = new ESP8266WebServer(80);

    // Rota principal - página inicial
    server->on("/", []()
               {
    String html = "<!DOCTYPE html><html>";
    html += "<head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>ESP Alexa</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; text-align: center; margin: 0; padding: 20px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; }";
    html += ".container { background: white; border-radius: 10px; padding: 20px; max-width: 500px; margin: auto; box-shadow: 0 10px 30px rgba(0,0,0,0.2); }";
    html += "h1 { color: #667eea; }";
    html += ".status { font-size: 24px; margin: 20px 0; }";
    html += ".on { color: #4CAF50; }";
    html += ".off { color: #f44336; }";
    html += "button { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); border: none; border-radius: 5px; color: white; padding: 12px 24px; margin: 10px; cursor: pointer; font-size: 16px; transition: transform 0.3s; }";
    html += "button:hover { transform: scale(1.05); }";
    html += ".info { background: #f0f0f0; padding: 10px; border-radius: 5px; margin: 10px 0; font-size: 14px; }";
    html += "</style>";
    html += "<script>";
    html += "function toggle() {";
    html += "  fetch('/toggle').then(response => response.text()).then(data => {";
    html += "    document.getElementById('status').innerHTML = data === 'ON' ? '<span class=\"on\">LIGADA</span>' : '<span class=\"off\">DESLIGADA</span>';} );";
    html += "}";
    html += "</script>";
    html += "</head><body>";
    html += "<div class='container'>";
    html += "<h1>💡 ESP Alexa</h1>";
    html += "<div class='info'>";
    html += "<p><strong>Dispositivo:</strong> " + nome_alexa + "</p>";
    html += "<p><strong>Estado:</strong> <span id='status' class='" + String(lampadaLigada ? "on" : "off") + "'>" + String(lampadaLigada ? "LIGADA" : "DESLIGADA") + "</span></p>";
    html += "<p><strong>Pino Vazio:</strong> GPIO" + String(pino_sensor_inferior) + "</p>";
    //html += "<p><strong>Timeout Auto:</strong> " + String(timeout_minutes) + " min</p>";
    html += "<p><strong>IP:</strong> " + WiFi.localIP().toString() + "</p>";
    html += "<p><strong>MAC:</strong> " + WiFi.macAddress() + "</p>";
    html += "</div>";
    html += "<button onclick='ler()'>🔄 Ler estado</button><br>";
    html += "<button onclick=\"window.location.href='/info'\">ℹ️ Informacoes</button>";
    html += "<button onclick=\"window.location.href='/ws'\">🌐 WebSocket Test</button><br>";
    html += "<button onclick=\"window.location.href='/reset'\">⚠️ Reset Configuracoes</button>";
    html += "<button onclick=\"window.location.href='/config'\">⚙️ Configurar Pino</button>";
    //html += "<button onclick=\"window.location.href='/timeout'\">⏰ Configurar Timeout</button>";
    html += "</div>";
    html += "</body></html>";
    server->send(200, "text/html", html); });

    // Rota para alternar lampada via API
    server->on("/ler", []()
               {
                   sensor_nivel->atualizar();
                       nivel_sensor_string = sensor_nivel->getNivelString();
                   server->send(200, "text/plain", nivel_sensor_string);
                   Serial.print("Web: Lampada ");
                   Serial.print(nivel_sensor_string);
                   Serial.print(" em: ");
                   Serial.println(pino_sensor_inferior); });

    // Rota para informações detalhadas
    server->on("/status", []()
               {
                       sensor_nivel->atualizar();
                       nivel_sensor_string = sensor_nivel->getNivelString();
                       lampadaLigada = sensor_nivel->getNivel() != VAZIO;


    String json = "{";
    json += "\"device\":\"ESP Alexa\",";
    json += "\"version\":\"3.4\",";
    json += "\"name\":\"" + nome_alexa + "\",";
    json += "\"pino_vazio\":" + String(pino_sensor_inferior) + ",";
    //json += "\"timeout_minutos\":" + String(timeout_minutes) + ",";
//#ifdef ENBALED_BUTTON_PIN
//    json += "\"pino_botao\":" + String(button_pin) + ",";
//#endif
    json += "\"status\":\"" + String(lampadaLigada ? "ON" : "OFF") + "\",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"mac\":\"" + WiFi.macAddress() + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"uptime\":" + String(millis() / 1000);
    json += "}";
    server->send(200, "application/json", json); });

    // Rota para resetar configurações
    server->on("/reset", []()
               {
    if (server->method() == HTTP_POST)
    {
      String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Resetando...</title>";
      html += "<meta http-equiv='refresh' content='3;url=/'></head><body>";
      html += "<h1>🔄 Resetando configurações...</h1>";
      html += "<p>O dispositivo irá reiniciar em 3 segundos.</p>";
      html += "</body></html>";
      server->send(200, "text/html", html);
      delay(100);
      resetarConfiguracoes();
    }
    else
    {
      String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Confirmar Reset</title>";
      html += "<style>body { font-family: Arial, sans-serif; text-align: center; margin: 0; padding: 20px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; }";
      html += ".container { background: white; border-radius: 10px; padding: 20px; max-width: 500px; margin: auto; box-shadow: 0 10px 30px rgba(0,0,0,0.2); }";
      html += "h1 { color: #667eea; } p { font-size: 16px; } button { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); border: none; border-radius: 5px; color: white; padding: 12px 24px; margin: 10px; cursor: pointer; font-size: 16px; }";
      html += "button:hover { transform: scale(1.05); } .cancel { background: #f44336; }</style></head><body>";
      html += "<div class='container'><h1>⚠️ Confirmar Reset</h1>";
      html += "<p>Tem certeza que deseja resetar <strong>TODAS</strong> as configurações?</p>";
      html += "<p>Isso irá limpar: Wi-Fi salvo, nome do dispositivo, pino configurado e reiniciar o ESP.</p>";
      html += "<form method='POST'><input type='submit' value='🚨 Sim, Resetar Tudo'></form>";
      html += "<a href='/'><button type='button' class='cancel'>❌ Cancelar</button></a></div></body></html>";
      server->send(200, "text/html", html);
    } });

    // Rota para status simples
    server->on("/info", []()
               {
    String status = "Dispositivo: " + nome_alexa + "\n";
    status += "Estado: " + String(lampadaLigada ? "LIGADA" : "DESLIGADA") + "\n";
    status += "IP: " + WiFi.localIP().toString() + "\n";
    status += "RSSI: " + String(WiFi.RSSI()) + " dBm\n";
    server->send(200, "text/plain", status); });

    // Rota para teste WebSocket
    server->on("/ws", []()
               {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>WebSocket Test - ESP8266</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; text-align: center; margin: 0; padding: 20px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; color: white; }";
    html += ".container { background: rgba(255,255,255,0.1); border-radius: 10px; padding: 20px; max-width: 600px; margin: auto; box-shadow: 0 10px 30px rgba(0,0,0,0.2); }";
    html += "h1 { color: #fff; }";
    html += ".status { font-size: 18px; margin: 10px 0; padding: 10px; background: rgba(0,0,0,0.2); border-radius: 5px; }";
    html += "button { background: linear-gradient(135deg, #4CAF50 0%, #45a049 100%); border: none; border-radius: 5px; color: white; padding: 12px 24px; margin: 5px; cursor: pointer; font-size: 16px; transition: transform 0.3s; }";
    html += "button:hover { transform: scale(1.05); }";
    html += "button:disabled { background: #666; cursor: not-allowed; }";
    html += ".danger { background: linear-gradient(135deg, #f44336 0%, #d32f2f 100%); }";
    html += "#messages { background: rgba(0,0,0,0.2); border-radius: 5px; padding: 10px; margin: 20px 0; text-align: left; max-height: 300px; overflow-y: auto; font-family: monospace; }";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<h1>🌐 Teste WebSocket</h1>";
    html += "<div class='status' id='connectionStatus'>Desconectado</div>";
    html += "<button id='connectBtn' onclick='connect()'>Conectar</button>";
    html += "<button id='disconnectBtn' onclick='disconnect()' disabled>Desconectar</button><br>";
    html += "<button onclick='sendStatus()'>📊 Status</button>";
    html += "<button onclick='sendToggle()'>🔄 Toggle</button>";
    html += "<button class='danger' onclick='sendRestart()'>🔄 Restart ESP</button><br>";
    html += "<div id='messages'></div>";
    html += "<a href='/'><button style='background: #666;'>← Voltar</button></a>";
    html += "</div>";
    html += "<script>";
    html += "let ws;";
    html += "const statusEl = document.getElementById('connectionStatus');";
    html += "const connectBtn = document.getElementById('connectBtn');";
    html += "const disconnectBtn = document.getElementById('disconnectBtn');";
    html += "const messagesEl = document.getElementById('messages');";
    html += "function log(message) {";
    html += "  messagesEl.innerHTML += '<div>' + new Date().toLocaleTimeString() + ': ' + message + '</div>';";
    html += "  messagesEl.scrollTop = messagesEl.scrollHeight;";
    html += "}";
    html += "function connect() {";
    html += "  const ip = window.location.hostname;";
    html += "  ws = new WebSocket('ws://' + ip + ':81');";
    html += "  ws.onopen = function() {";
    html += "    statusEl.innerHTML = 'Conectado ✅';";
    html += "    statusEl.style.background = 'rgba(76, 175, 80, 0.3)';";
    html += "    connectBtn.disabled = true;";
    html += "    disconnectBtn.disabled = false;";
    html += "    log('Conectado ao WebSocket');";
    html += "  };";
    html += "  ws.onmessage = function(event) {";
    html += "    log('📥 ' + event.data);";
    html += "  };";
    html += "  ws.onclose = function() {";
    html += "    statusEl.innerHTML = 'Desconectado ❌';";
    html += "    statusEl.style.background = 'rgba(244, 67, 54, 0.3)';";
    html += "    connectBtn.disabled = false;";
    html += "    disconnectBtn.disabled = true;";
    html += "    log('Desconectado do WebSocket');";
    html += "  };";
    html += "  ws.onerror = function(error) {";
    html += "    log('Erro: ' + error);";
    html += "  };";
    html += "}";
    html += "function disconnect() {";
    html += "  if (ws) {";
    html += "    ws.close();";
    html += "  }";
    html += "}";
    html += "function sendStatus() {";
    html += "  if (ws && ws.readyState === WebSocket.OPEN) {";
    html += "    ws.send('status');";
    html += "    log('📤 status');";
    html += "  } else {";
    html += "    log('WebSocket não conectado');";
    html += "  }";
    html += "}";
    html += "function sendToggle() {";
    html += "  if (ws && ws.readyState === WebSocket.OPEN) {";
    html += "    ws.send('toggle');";
    html += "    log('📤 toggle');";
    html += "  } else {";
    html += "    log('WebSocket não conectado');";
    html += "  }";
    html += "}";
    html += "function sendRestart() {";
    html += "  if (ws && ws.readyState === WebSocket.OPEN) {";
    html += "    ws.send('restart');";
    html += "    log('📤 restart');";
    html += "  } else {";
    html += "    log('WebSocket não conectado');";
    html += "  }";
    html += "}";
    html += "</script>";
    html += "</body></html>";
    server->send(200, "text/html", html); });

    // Rota para configurar pino
    server->on("/config", []()
               {
    if (server->method() == HTTP_POST)
    {
      String novo_pino_str = server->arg("pino");
      int novo_pino = novo_pino_str.toInt();

      if (isPinoSeguro(novo_pino) && novo_pino != pino_sensor_inferior)
      {
        pino_sensor_inferior = novo_pino;
        salvarPinoEEPROM(pino_sensor_inferior);
        pinMode(pino_sensor_inferior, OUTPUT);
        //digitalWrite(rele_pin, LOW);
        server->send(200, "text/html", "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Pino Atualizado</title><meta http-equiv='refresh' content='3;url=/'></head><body><h1>✅ Pino atualizado! Reiniciando...</h1><p>Redirecionando em 3 segundos...</p></body></html>");
        delay(100);
        ESP.restart();
      }
      else
      {
        server->send(200, "text/html", "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Erro</title></head><body><h1>❌ Erro: Pino inválido</h1><a href='/config'>Voltar</a></body></html>");
      }
    }
    else
    {
      String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Configurar Pino</title>";
      html += "<style>body { font-family: Arial, sans-serif; text-align: center; margin: 0; padding: 20px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; }";
      html += ".container { background: white; border-radius: 10px; padding: 20px; max-width: 400px; margin: auto; box-shadow: 0 10px 30px rgba(0,0,0,0.2); }";
      html += "h1 { color: #667eea; } select { padding: 8px; font-size: 16px; } button { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); border: none; border-radius: 5px; color: white; padding: 10px 20px; cursor: pointer; font-size: 16px; margin: 10px; }</style></head><body>";
      html += "<div class='container'><h1>⚙️ Configurar Pino do Relé</h1>";
      html += "<form method='POST'><label>Pino Vazio GPIO: <select name='pino'>";
      for (int i = 0; i < num_pinos_validos; i++)
      {
        int p = pinos_validos[i];
        html += "<option value='" + String(p) + "'";
        if (p == pino_sensor_inferior)
          html += " selected";
        html += ">GPIO" + String(p) + "</option>";
      }
      html += "</select></label><br><br><input type='submit' value='💾 Salvar'></form>";
      html += "<br><a href='/'><button type='button'>🏠 Voltar</button></a></div></body></html>";
      server->send(200, "text/html", html);
    } });

    // Tratamento para rotas não encontradas - DELEGA PARA ESPALEXA
    server->onNotFound([]()
                       {
    String uri = server->uri();
    String body = server->arg(0);
    
    Serial.print("Rota nao encontrada: ");
    Serial.println(uri);
    
    // Tentar delegar para o Espalexa
    if (espalexa.handleAlexaApiCall(uri, body)) {
      // Espalexa processou a requisição
      Serial.println("Requisicao delegada para Espalexa");
    } else {
      // Rota não é do Espalexa
      server->send(404, "text/plain", "Not Found");
    } });

    server->begin();
    Serial.println("✅ Servidor web iniciado na porta 80");
    Serial.println("   Rotas disponiveis: /, /toggle, /info, /status, /reset, /config");
    Serial.println("   Rotas Espalexa: /espalexa, /api/...");
}

// =============================================
//  SETUP
// =============================================
void setup_nivel_agua()
{
    Serial.begin(115200);
    delay(100);

    WiFiManager wifiManager;
    wifiManager.setConnectTimeout(10);
    wifiManager.setConfigPortalTimeout(180);
    wifiManager.setAPCallback(configModeCallback);

    Serial.println("[WiFi] Conectando ao Wi-Fi salvo...");
    if (!wifiManager.autoConnect(nome_alexa.c_str()))
    {
        Serial.println("[WiFi] Falha na conexao. Abrindo portal de configuracao...");
        iniciarPortalConfiguracao();
    }

    Serial.println("\n=== CONFIGURACOES ATUAIS ===");
    Serial.print("MAC: ");
    Serial.println(WiFi.macAddress());
    Serial.print("Pino Vazio: GPIO");
    Serial.println(pino_sensor_inferior);
    Serial.print("Nome Alexa: '");
    Serial.print(nome_alexa);
    Serial.println("'");
    Serial.print("Wi-Fi: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.println("===============================\n");

    // Enviar informações iniciais via WebSocket
    enviarInformacoesDispositivo();

    // Configurar WebSocket Server
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    Serial.println("[WebSocket] Servidor WebSocket iniciado na porta 81");

    // Configurar servidor web
    configurarServidorWeb();

    // Iniciar mDNS
    iniciarMDNS();

    Serial.println("\n\n");
    Serial.println("╔════════════════════════════════════════╗");
    Serial.println("║   SISTEMA DE NÍVEL DE ÁGUA - ESP8266  ║");
    Serial.println("╚════════════════════════════════════════╝\n");

    // Criar instância do sensor
    sensor_nivel = new NivelAguaSensor(pino_sensor_inferior, pino_sensor_superior);

    if (sensor_nivel == nullptr)
    {
        Serial.println("[ERRO] Falha ao alocar memória para o sensor!");
        while (1)
        {
            delay(1000);
        }
    }

    // Inicializar o sensor
    sensor_nivel->iniciar();

    Serial.println("[Setup] Sensor de nível de água inicializado com sucesso");
    Serial.printf("[Setup] Pino sensor inferior (GPIO%d)\n", pino_sensor_inferior);
    Serial.printf("[Setup] Pino sensor superior (GPIO%d)\n", pino_sensor_superior);
    Serial.println("[Setup] Sistema pronto para operar\n");

    // Configurar Espalexa com o servidor existente
    uint8 n = espalexa.addDevice(nome_alexa.c_str(), callbackNivel, EspalexaDeviceType::dimmable);
    alexa_device = espalexa.getDevice(n - 1);
    espalexa.setDiscoverable(true);
    espalexa.begin(server); // Passa o servidor para o Espalexa

    // Leitura inicial
    delay(100);
    sensor_nivel->atualizar();
    Serial.printf("[Setup] Estado inicial: %s\n\n", sensor_nivel->getNivelString().c_str());
}

// =============================================
//  LOOP PRINCIPAL
// =============================================
unsigned long ultimoTempo = 0;
void loop_nivel_agua()
{

    espalexa.loop();

    if (server)
    {
        server->handleClient();
    }

    webSocket.loop();

    // processarComandosSerial();
    Comum::verificarWiFi();
    // indicarStatus();
    // verificarButton();

    if (mdnsIniciado)
    {
        MDNS.update();
    }

    if (millis() - ultimoTempo >= LOOP_INTERVALO)
    {
        ultimoTempo = millis();
        // Atualizar leitura do sensor
        sensor_nivel->atualizar();
        lampadaLigada = sensor_nivel->getNivel() != VAZIO;
        nivel_sensor_string = sensor_nivel->getNivelString();
        // Processar mudanças de nível
        if (sensor_nivel->houveMudanca())
        {
            Serial.printf("[Mudança] Nível alterado: %s -> %s\n",
                          sensor_nivel->getNivelAnteriorString().c_str(),
                          sensor_nivel->getNivelString().c_str());

            tempo_ultima_mudanca = millis();
            processarNivelAgua();
        }

        // Enviar status periodicamente
        enviarStatusNivel();

        // Pequeno delay para evitar consumo excessivo de CPU
        delay(50);
        if (LOOP_DEEP_SLEEP > 0)
        {
            Serial.printf("Sleeping: %d", LOOP_DEEP_SLEEP);
            ESP.deepSleep(LOOP_DEEP_SLEEP); // 1 minuto
        }
    }

    delay(1);
}

#endif // NIVEL_AGUA_APP
