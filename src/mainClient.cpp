#include <ESP8266WiFi.h>
#include <WiFiManager.h> // by tzapu
#include <ESP8266WebServer.h>
#define ESPALEXA_DEBUG // Ativa debug do Espalexa
#include <Espalexa.h>  // by Aircoookie
#include <EEPROM.h>
#include <ESP8266mDNS.h> // Para mDNS (acesso via .local)
#include <WebSocketsServer.h>
#include "portal.h"
#include "client.h"

#ifdef MQTT_CLIENT

// =============================================
//  VARIAVEIS GLOBAIS
// =============================================
Espalexa espalexa;
EspalexaDevice *lampada_device = nullptr; // Ponteiro para o dispositivo Espalexa
ESP8266WebServer *server = nullptr;       // Ponteiro para o servidor web
bool lampadaLigada = false;
int rele_pin = DEFAULT_RELE_PIN;
int button_pin = DEFAULT_BUTTON_PIN;
String nome_alexa = "";
unsigned long lastSerialActivity = 0;
unsigned long lastBlink = 0;
bool ledState = false;
bool mdnsIniciado = false;
bool shouldSaveConfig = false;
bool portalAtivo = false;

// Variável para controle de timeout do Wi-Fi
unsigned long wifiDisconnectedStartTime = 0;
bool wasWiFiConnected = false;

WebSocketsServer webSocket(81); // Porta 81 para WebSocket

void enviarInformacoesDispositivo(uint8_t clientNum = 255);
bool acionarRele(bool ligar);

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
    String status = "{\"status\":\"connected\",\"device\":\"" + nome_alexa + "\"}";
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
      acionarRele(!lampadaLigada);
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

void sendMsg(String msg)
{
  // Envia mensagem para o cliente WebSocket (broadcast)
  if (webSocket.connectedClients() > 0)
  {
    webSocket.broadcastTXT(msg);
  }
  Serial.println(msg);
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
  info += "\"rele_pin\":" + String(rele_pin) + ",";
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

void resetarConfiguracoes();

// =============================================
//  FUNCAO PARA GERAR NOME BASEADO NO MAC
// =============================================
String gerarNomeBaseadoNoMAC()
{
  uint8_t mac[6];
  WiFi.macAddress(mac);

  char nome[32];
  snprintf(nome, sizeof(nome), "esp_%02X%02X", mac[4], mac[5]);

  Serial.print("Nome gerado baseado no MAC: ");
  Serial.println(nome);

  return String(nome);
}

// =============================================
//  FUNCAO PARA GERAR NOME mDNS VÁLIDO
// =============================================
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

// =============================================
//  FUNCAO PARA INICIAR mDNS
// =============================================
void iniciarMDNS()
{
  if (mdnsIniciado)
  {
    MDNS.end();
    delay(100);
  }

  String nome_mdns = gerarNomeMDNS(nome_alexa);

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
    return gerarNomeBaseadoNoMAC();
  }

  Serial.print("Nome carregado da EEPROM: ");
  Serial.println(nome);
  return nome;
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
    html += "<title>ESP Alexa Lampada</title>";
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
    html += "<h1>💡 ESP Alexa Lampada</h1>";
    html += "<div class='info'>";
    html += "<p><strong>Dispositivo:</strong> " + nome_alexa + "</p>";
    html += "<p><strong>Estado:</strong> <span id='status' class='" + String(lampadaLigada ? "on" : "off") + "'>" + String(lampadaLigada ? "LIGADA" : "DESLIGADA") + "</span></p>";
    html += "<p><strong>Pino do Relé:</strong> GPIO" + String(rele_pin) + "</p>";
    html += "<p><strong>IP:</strong> " + WiFi.localIP().toString() + "</p>";
    html += "<p><strong>MAC:</strong> " + WiFi.macAddress() + "</p>";
    html += "</div>";
    html += "<button onclick='toggle()'>🔄 Alternar Lampada</button><br>";
    html += "<button onclick=\"window.location.href='/info'\">ℹ️ Informacoes</button>";
    html += "<button onclick=\"window.location.href='/websocket'\">🌐 WebSocket Test</button><br>";
    html += "<button onclick=\"window.location.href='/reset'\">⚠️ Reset Configuracoes</button>";
    html += "<button onclick=\"window.location.href='/config'\">⚙️ Configurar Pino</button>";
    html += "</div>";
    html += "</body></html>";
    server->send(200, "text/html", html); });

  // Rota para alternar lampada via API
  server->on("/toggle", []()
             {
    lampadaLigada = !lampadaLigada;
    digitalWrite(rele_pin, lampadaLigada ? HIGH : LOW);
    server->send(200, "text/plain", lampadaLigada ? "ON" : "OFF");
    Serial.print("Web: Lampada ");
    Serial.println(lampadaLigada ? "LIGADA" : "DESLIGADA"); });

  // Rota para informações detalhadas
  server->on("/info", []()
             {
    String json = "{";
    json += "\"device\":\"ESP Alexa Lampada\",";
    json += "\"version\":\"3.4\",";
    json += "\"nome_alexa\":\"" + nome_alexa + "\",";
    json += "\"pino_rele\":" + String(rele_pin) + ",";
    json += "\"estado\":" + String(lampadaLigada ? "true" : "false") + ",";
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
  server->on("/status", []()
             {
    String status = "Dispositivo: " + nome_alexa + "\n";
    status += "Estado: " + String(lampadaLigada ? "LIGADA" : "DESLIGADA") + "\n";
    status += "IP: " + WiFi.localIP().toString() + "\n";
    status += "RSSI: " + String(WiFi.RSSI()) + " dBm\n";
    server->send(200, "text/plain", status); });

  // Rota para teste WebSocket
  server->on("/websocket", []()
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

      if (isPinoSeguro(novo_pino) && novo_pino != rele_pin)
      {
        rele_pin = novo_pino;
        salvarPinoEEPROM(rele_pin);
        pinMode(rele_pin, OUTPUT);
        digitalWrite(rele_pin, LOW);
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
      html += "<form method='POST'><label>Pino GPIO: <select name='pino'>";
      for (int i = 0; i < num_pinos_validos; i++)
      {
        int p = pinos_validos[i];
        html += "<option value='" + String(p) + "'";
        if (p == rele_pin)
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
  Serial.println("   Rotas disponiveis: /, /toggle, /info, /status, /reset");
  Serial.println("   Rotas Espalexa: /espalexa, /api/...");
}

// =============================================
//  FUNCAO PARA CARREGAR PINO DA EEPROM
// =============================================
int carregarPinoEEPROM()
{
  EEPROM.begin(EEPROM_SIZE);
  int magic = EEPROM.read(MAGIC_NUMBER_ADDR);

  if (magic != MAGIC_NUMBER)
  {
    EEPROM.end();
    Serial.println("Nenhuma configuracao valida, usando padrao");
    return DEFAULT_RELE_PIN;
  }

  int pino = EEPROM.read(PIN_CONFIG_ADDR);
  EEPROM.end();

  if (isPinoSeguro(pino))
  {
    Serial.print("Pino carregado da EEPROM: GPIO");
    Serial.println(pino);
    return pino;
  }

  Serial.println("Pino configurado invalido, usando padrao");
  return DEFAULT_RELE_PIN;
}

bool acionarRele(bool ligar)
{
  digitalWrite(rele_pin, ligar ? HIGH : LOW);
  lampadaLigada = digitalRead(rele_pin) == HIGH;

  // Sincronizar estado com Espalexa (para Alexa saber do botão)
  if (lampada_device != nullptr)
  {
    lampada_device->setValue(lampadaLigada ? 255 : 0);
    Serial.print("[Botao] Lampada ");
    Serial.print(lampadaLigada ? "LIGADA" : "DESLIGADA");
    Serial.println(" - Estado sincronizado com Espalexa");
  }

  // Enviar informações via WebSocket
  enviarInformacoesDispositivo();

  return lampadaLigada;
}

// =============================================
//  CALLBACK DA ALEXA
// =============================================
void callbackLampada(EspalexaDevice *d)
{
  if (d == nullptr)
    return;

  acionarRele(d->getValue() > 0);

  Serial.print("[Alexa] -> Lampada ");
  Serial.print(lampadaLigada ? "LIGADA" : "DESLIGADA");
  Serial.print(" (Pino GPIO");
  Serial.print(rele_pin);
  Serial.print(") - Nome: ");
  Serial.println(nome_alexa);
}

// =============================================
//  CALLBACK DA ALEXA
// =============================================

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
//  COMANDOS VIA SERIAL
// =============================================
void processarComandosSerial()
{
#ifdef ENABLED_SERIAL
  if (Serial.available())
  {
    String comando = Serial.readStringUntil('\n');
    comando.trim();
    comando.toUpperCase();

    if (comando == "R")
    {
      Serial.println("\n>>> RESETANDO CONFIGURACOES... <<<\n");
      resetarConfiguracoes();
    }
    else if (comando == "CONFIG" || comando == "C")
    {
      Serial.println("\n>>> Abrindo portal de configuracao... <<<\n");
      iniciarPortalConfiguracao();
    }
    else if (comando == "STATUS" || comando == "S")
    {
      Serial.println("\n========================================");
      Serial.println("           STATUS ATUAL");
      Serial.println("========================================");
      Serial.print("📡 Wi-Fi: ");
      Serial.println(WiFi.SSID());
      Serial.print("🌐 IP: ");
      Serial.println(WiFi.localIP());
      Serial.print("🏷️ Nome Alexa: ");
      Serial.println(nome_alexa);
      Serial.print("🔌 Pino Rele: GPIO");
      Serial.println(rele_pin);
      Serial.print("💡 Estado: ");
      Serial.println(lampadaLigada ? "LIGADA" : "DESLIGADA");
      Serial.print("🌍 mDNS: ");
      Serial.println(mdnsIniciado ? "ATIVO" : "INATIVO");
      if (mdnsIniciado)
      {
        Serial.print("   http://");
        Serial.print(gerarNomeMDNS(nome_alexa));
        Serial.println(".local");
      }
      Serial.println("========================================\n");
    }
    else if (comando == "ON" || comando == "1")
    {
      acionarRele(HIGH);
      Serial.println("💡 Lampada LIGADA");
    }
    else if (comando == "OFF" || comando == "0")
    {
      acionarRele(LOW);
      Serial.println("💡 Lampada DESLIGADA");
    }
    else if (comando == "HELP" || comando == "H")
    {
      Serial.println("\nCOMANDOS: R=Reset, C=Config, S=Status, ON/OFF, HELP");
    }
  }
#endif
}
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

// =============================================
//  INDICADOR LED DE STATUS
// =============================================
void indicarStatus()
{
  if (!portalAtivo && WiFi.status() != WL_CONNECTED)
  {
    // Piscar somente quando desconectado do Wi - Fi
    if (millis() - lastBlink > 2000)
    {
      ledState = !ledState;
      lastBlink = millis();
      digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
    }
  }
  else if (portalAtivo)
  {
    if (millis() - lastBlink > 200)
    {
      ledState = !ledState;
      lastBlink = millis();
      digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
    }
  }
  else if (WiFi.status() != WL_CONNECTED)
  {
    if (millis() - lastBlink > 1000)
    {
      ledState = !ledState;
      lastBlink = millis();
      digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
    }
  }
  else if (digitalRead(rele_pin) == LOW && WiFi.status() == WL_CONNECTED && !portalAtivo)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  }
}

// =============================================
//  MOSTRAR BANNER DE BOOT
// =============================================
void mostrarBanner()
{
  Serial.println("\n");
  Serial.println("========================================");
  Serial.println("ESP8266 Lampada Alexa Configuravel v3.4");
  Serial.println("========================================");
  Serial.println("✨ Funcionalidades:");
  Serial.println("- Controle via Alexa");
  Serial.println("- mDNS com o nome do dispositivo");
  Serial.println("- Servidor web com API REST");
  Serial.println("- Configuracao via web (/config)");
  Serial.println("========================================\n");
}

// =============================================
//  SETUP CLIENT
// =============================================
void setup_client()
{
  Serial.begin(115200);
  delay(100);

  mostrarBanner();

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  rele_pin = carregarPinoEEPROM();
  nome_alexa = carregarNomeEEPROM();

  if (rele_pin == 0)
  {
    rele_pin = DEFAULT_RELE_PIN;
    salvarPinoEEPROM(rele_pin);
  }

  pinMode(rele_pin, OUTPUT);
  digitalWrite(rele_pin, LOW);

  pinMode(button_pin, INPUT_PULLDOWN_16);

  String ap_nome = gerarAPName();

  WiFiManager wifiManager;
  wifiManager.setConnectTimeout(10);
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.setAPCallback(configModeCallback);

  Serial.println("[WiFi] Conectando ao Wi-Fi salvo...");
  if (!wifiManager.autoConnect(ap_nome.c_str()))
  {
    Serial.println("[WiFi] Falha na conexao. Abrindo portal de configuracao...");
    iniciarPortalConfiguracao();
  }

  Serial.println("\n=== CONFIGURACOES ATUAIS ===");
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("Pino Rele: GPIO");
  Serial.println(rele_pin);
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

  // Configurar Espalexa com o servidor existente
  uint8 n = espalexa.addDevice(nome_alexa.c_str(), callbackLampada, EspalexaDeviceType::onoff);
  lampada_device = espalexa.getDevice(n - 1);
  espalexa.setDiscoverable(true);
  espalexa.begin(server); // Passa o servidor para o Espalexa

  Serial.println("\n✅ Sistema pronto!");
  Serial.print("🎤 Alexa: Diga 'Alexa, ligar ");
  Serial.print(nome_alexa);
  Serial.println("'");
  Serial.print("🌐 Acesso Web: http://");
  Serial.print(gerarNomeMDNS(nome_alexa));
  Serial.println(".local");
  Serial.println("   Rotas: /, /toggle, /info, /status, /espalexa");
#ifdef ENABLED_SERIAL
  Serial.println("\n💡 Comandos Serial: HELP para lista");
  Serial.println();
#endif
  portalAtivo = false;
  digitalWrite(button_pin, LOW);
}

// =============================================
//  VARIAVEIS GLOBAIS PARA O BOTAO (VERSÃO SIMPLIFICADA)
//  Adicione junto com as outras variáveis globais no início do arquivo
// =============================================
unsigned long buttonPressStartTime = 0;
bool buttonWasPressed = false;

// =============================================
//  VERIFICAR BOTAO - VERSÃO SIMPLIFICADA
//  Clique = toggle, Pressionamento longo (5s) = reset
// =============================================
void verificarButton()
{
#ifdef ENBALED_BUTTON_PIN
  // Assumindo botão com pull-up: LOW quando pressionado, HIGH quando solto
  bool isPressed = (digitalRead(button_pin) == HIGH);

  if (isPressed && !buttonWasPressed)
  {
    // Botão acabou de ser pressionado
    buttonPressStartTime = millis();
    buttonWasPressed = true;

    // Feedback: LED acende
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("[Botao] Pressionado");
  }
  else if (!isPressed && buttonWasPressed)
  {
    // Botão acabou de ser solto
    unsigned long pressDuration = millis() - buttonPressStartTime;

    if (pressDuration >= BUTTON_HOLD_TIME_MS)
    {
      // Pressionamento longo - RESET
      Serial.println("\n*** BOTAO PRESSIONADO POR LONGO TEMPO - RESETANDO ***");

      // Feedback visual: pisca 3 vezes
      for (int i = 0; i < 3; i++)
      {
        digitalWrite(LED_BUILTIN, LOW);
        delay(200);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(200);
      }

      delay(1000);
      resetarConfiguracoes();
    }
    else if (pressDuration >= 50)
    {
      // Clique curto (mais de 50ms para evitar bounce) - TOGGLE
      acionarRele(digitalRead(rele_pin) == LOW);
      Serial.println("[Botao] Alternando lampada (clique unico)");
    }

    buttonWasPressed = false;

    // Restaura LED ao estado normal
    if (WiFi.status() != WL_CONNECTED)
    {
      // Se desconectado, o indicarStatus() vai cuidar do piscar
      digitalWrite(LED_BUILTIN, HIGH);
    }
  }

  // Feedback visual durante pressionamento longo
  if (buttonWasPressed)
  {
    unsigned long pressDuration = millis() - buttonPressStartTime;

    if (pressDuration >= 3000)
    {
      // Piscar rápido após 3 segundos (feedback de que está prestes a resetar)
      if ((millis() % 200) < 100)
      {
        digitalWrite(LED_BUILTIN, LOW); // LED aceso
      }
      else
      {
        digitalWrite(LED_BUILTIN, HIGH); // LED apagado
      }
    }
    else if (pressDuration >= 1000)
    {
      // Piscar lento entre 1 e 3 segundos
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
#endif
}
// =============================================
//  LOOP CLIENT
// =============================================
void loop_client()
{
  espalexa.loop();

  if (server)
  {
    server->handleClient();
  }

  webSocket.loop();

  processarComandosSerial();
  verificarWiFi();
  indicarStatus();
  verificarButton();

  if (mdnsIniciado)
  {
    MDNS.update();
  }

  delay(10);
}

#endif
