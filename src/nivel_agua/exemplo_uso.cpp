/**
 * EXEMPLO DE USO - SENSOR DE NÍVEL DE ÁGUA
 * =============================================
 *
 * Este arquivo demonstra como usar a classe NivelAguaSensor
 * no seu projeto ESP8266.
 */

#include "nivel_agua_sensor.h"

// =============================================
//  EXEMPLO 1: USO BÁSICO
// =============================================

// Criar instância do sensor com pinos GPIO
// Pino 1 (D1 no ESP8266 = GPIO5): Sensor inferior
// Pino 2 (D2 no ESP8266 = GPIO4): Sensor superior
NivelAguaSensor sensor_nivel(5, 4);

void setup_exemplo_basico()
{
    Serial.begin(115200);
    delay(1000);

    // Inicializar o sensor
    sensor_nivel.iniciar();
}

void loop_exemplo_basico()
{
    // Atualizar o sensor (deve ser chamado continuamente)
    sensor_nivel.atualizar();

    // Verificar se houve mudança de estado
    if (sensor_nivel.houveMudanca())
    {
        Serial.printf("Nível de água mudou: %s\n",
                      sensor_nivel.getNivelString().c_str());
    }

    // Obter nível atual
    NivelAgua nivel = sensor_nivel.getNivel();

    // Executar ações baseado no nível
    switch (nivel)
    {
    case CHEIO:
        Serial.println("Caixa d'água CHEIO!");
        // Desligar bomba, parar enchimento, etc
        break;

    case MEIO:
        Serial.println("Caixa d'água no MEIO!");
        // Status intermediário
        break;

    case VAZIO:
        Serial.println("Caixa d'água VAZIO!");
        // Ligar bomba, alertar, etc
        break;
    }

    delay(100);
}

// =============================================
//  EXEMPLO 2: INTEGRAÇÃO COM WEBSOCKET
// =============================================

void enviarStatusNivelWebSocket()
{
    String status = "{";
    status += "\"sensor\":\"nivel_agua\",";
    status += "\"nivel\":\"" + sensor_nivel.getNivelString() + "\",";
    status += "\"mudou\":" + String(sensor_nivel.houveMudanca() ? "true" : "false");
    status += "}";

    // Enviar via WebSocket (use sua função de broadcast)
    // webSocket.broadcastTXT(status);
    Serial.println(status);
}

// =============================================
//  EXEMPLO 3: ALERTA DE MUDANÇA
// =============================================

unsigned long ultimo_alerta = 0;
const unsigned long INTERVALO_ALERTA = 5000; // 5 segundos

void verificarAlertaNivel()
{
    sensor_nivel.atualizar();

    // Se houve mudança E tempo suficiente passou desde último alerta
    if (sensor_nivel.houveMudanca())
    {
        unsigned long agora = millis();

        if (agora - ultimo_alerta > INTERVALO_ALERTA)
        {
            Serial.printf("ALERTA: Nível de água mudou para %s\n",
                          sensor_nivel.getNivelString().c_str());

            ultimo_alerta = agora;

            // Aqui você pode adicionar:
            // - Enviar notificação
            // - Registrar em log
            // - Disparar alarme sonoro
            // - Enviar mensagem via MQTT
        }
    }
}

// =============================================
//  NOTAS DE IMPLEMENTAÇÃO
// =============================================

/*
 * CONEXÃO DOS SENSORES:
 *
 * Sensor 1 (Inferior) -> GPIO 5 (D1)
 * Sensor 2 (Superior) -> GPIO 4 (D2)
 *
 * Cada sensor deve ter:
 * - Um fio conectado ao VCC (3.3V)
 * - Um fio conectado ao GND
 * - Um fio de sinal conectado ao pino GPIO
 * - Um resistor pull-down de 10k entre o pino GPIO e GND
 *
 * LÓGICA DO SENSOR:
 *
 * O sensor deve fornecer:
 * - Nível ALTO (1) quando há água presente
 * - Nível BAIXO (0) quando não há água
 *
 * ESTADOS:
 *
 * VAZIO: sensor1=0, sensor2=0
 * MEIO:  sensor1=1, sensor2=0
 * CHEIO: sensor1=1, sensor2=1
 *
 * AJUSTES PERSONALIZADOS:
 *
 * Para mudar o tempo de debounce:
 * Edite em nivel_agua_sensor.h:
 *   const unsigned long DEBOUNCE_TIME = 50; // mudar para 100, 200, etc
 *
 * Para adicionar mais sensores (ex: 4 posições):
 * Adicione mais pinos na classe e ajuste a lógica de lerSensores()
 */
