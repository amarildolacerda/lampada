# Sensores ESP8266 - Sistemas Inteligentes Modularizados

Projeto modular de aplicações embarcadas para ESP8266 (WeMos D1 Mini).

## 🎯 Objetivos

- **Modularização**: Código organizado em aplicações independentes compiláveis separadamente
- **Controle remoto**: Integração com Alexa (via Espalexa), WiFi e portal web
- **Automação**: Sensor de nível de água com respostas automáticas
- **Configuração**: Portal web para configuração sem código

## 📦 Aplicações Disponíveis

### 🔆 LAMPADA_APP
Controle de lâmpada inteligente via relé com Alexa, portal web, WebSocket e EEPROM.

### 💧 NIVEL_AGUA_APP
Sistema de monitoramento de nível de água com sensor dual e automação.

## 🔧 Como Compilar

### Via PlatformIO CLI

\\\ash
cd sensores

# Compilar LAMPADA_APP
pio run -e lampada_d1_mini

# Compilar NIVEL_AGUA_APP
pio run -e nivel_agua_d1_mini

# Upload
pio run -e nivel_agua_d1_mini -t upload
\\\

### Via VS Code

1. Abrir a pasta do projeto
2. Clicar no ícone PlatformIO
3. Selecionar ambiente
4. Clicar "Build" ou "Upload"

## 📚 Documentação

- [QUICKSTART.md](QUICKSTART.md) - Início rápido (5 min)
- [INSTALACAO.md](INSTALACAO.md) - Guia de instalação
- [ARQUITETURA.md](ARQUITETURA.md) - Detalhes técnicos
- [ESTRUTURA.md](ESTRUTURA.md) - Organização do código
- [CONTRIBUINDO.md](CONTRIBUINDO.md) - Como contribuir
- [CHANGELOG.md](CHANGELOG.md) - Histórico de versões

## 📊 Uso da Memória

### NIVEL_AGUA_APP
- RAM: 35.5% (29108 / 81920 bytes)
- Flash: 25.9% (270611 / 1044464 bytes)

---

**Versão**: 1.0 | **Status**: Ativo
