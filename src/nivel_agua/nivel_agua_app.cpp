#include <Arduino.h>
#include "nivel_agua_sensor.h"

#ifdef NIVEL_AGUA_APP

// =============================================
//  VARIÁVEIS GLOBAIS
// =============================================
NivelAguaSensor *sensor_nivel = nullptr;

// Pinos do sensor (GPIO5 = D1, GPIO4 = D2)
int pino_sensor_inferior = 5; // D1
int pino_sensor_superior = 4; // D2

// Variáveis de controle
unsigned long ultimo_log = 0;
const unsigned long INTERVALO_LOG = 1000; // 1 segundo

// Variáveis de estado
unsigned long tempo_ultima_mudanca = 0;
NivelAgua nivel_anterior_armazenado = VAZIO;

// =============================================
//  FUNÇÕES AUXILIARES
// =============================================

/**
 * Processa ações baseadas no nível de água
 */
void processarNivelAgua()
{
    NivelAgua nivel = sensor_nivel->getNivel();

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
//  SETUP
// =============================================
void setup_nivel_agua()
{
    Serial.begin(115200);
    delay(500);

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

    // Leitura inicial
    delay(100);
    sensor_nivel->atualizar();
    Serial.printf("[Setup] Estado inicial: %s\n\n", sensor_nivel->getNivelString().c_str());
}

// =============================================
//  LOOP PRINCIPAL
// =============================================
unsigned long intervalo = LOOP_INTERVALO; // 5 minutos em ms
unsigned long ultimoTempo = 0;
void loop_nivel_agua()
{
    if (millis() - ultimoTempo >= intervalo)
    {
        ultimoTempo = millis();
        // Atualizar leitura do sensor
        sensor_nivel->atualizar();

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
    }
    // Pequeno delay para evitar consumo excessivo de CPU
    delay(50);
}

#endif // NIVEL_AGUA_APP
