#include "nivel_agua_sensor.h"

// =============================================
//  CONSTRUTOR
// =============================================
NivelAguaSensor::NivelAguaSensor(int pino1, int pino2)
    : pino_sensor1(pino1), pino_sensor2(pino2),
      estado_atual(VAZIO), estado_anterior(VAZIO),
      ultimo_tempo_leitura(0)
{
}

// =============================================
//  INICIALIZAR SENSOR
// =============================================
void NivelAguaSensor::iniciar()
{
    pinMode(pino_sensor1, INPUT);
    pinMode(pino_sensor2, INPUT);

    Serial.println("[NivelAgua] Sensor inicializado");
    Serial.printf("[NivelAgua] Pino sensor 1: %d, Pino sensor 2: %d\n", pino_sensor1, pino_sensor2);

    // Leitura inicial
    lerSensores();
}

// =============================================
//  LER SENSORES E DETERMINAR NÍVEL
// =============================================
NivelAgua NivelAguaSensor::lerSensores()
{
    // Lê os dois sensores
    // Sensor 1: inferior (detecta quando há água até meio)
    // Sensor 2: superior (detecta quando há água até cheio)

    int leitura1 = digitalRead(pino_sensor1); // 1 = água presente, 0 = sem água
    int leitura2 = digitalRead(pino_sensor2); // 1 = água presente, 0 = sem água

    // Lógica de determinação do nível:
    // CHEIO: ambos os sensores detectam água (leitura1=1 E leitura2=1)
    // MEIO: apenas sensor inferior detecta água (leitura1=1 E leitura2=0)
    // VAZIO: nenhum sensor detecta água (leitura1=0 E leitura2=0)

    if (leitura2 == 1)
    {
        // Se sensor superior detecta, definitivamente está cheio
        return CHEIO;
    }
    else if (leitura1 == 1)
    {
        // Se sensor inferior detecta mas não o superior, está no meio
        return MEIO;
    }
    else
    {
        // Se nenhum detecta, está vazio
        return VAZIO;
    }
}

// =============================================
//  ATUALIZAR ESTADO DO SENSOR
// =============================================
void NivelAguaSensor::atualizar()
{
    unsigned long agora = millis();

    // Debounce simples
    if (agora - ultimo_tempo_leitura < DEBOUNCE_TIME)
    {
        return;
    }

    ultimo_tempo_leitura = agora;
    estado_anterior = estado_atual;
    estado_atual = lerSensores();

    if (houveMudanca())
    {
        Serial.printf("[NivelAgua] Mudança de estado: %s -> %s\n",
                      nivelParaString(estado_anterior).c_str(),
                      nivelParaString(estado_atual).c_str());
    }
}

// =============================================
//  OBTER NÍVEL ATUAL
// =============================================
NivelAgua NivelAguaSensor::getNivel()
{
    return estado_atual;
}

// =============================================
//  OBTER NÍVEL ANTERIOR
// =============================================
NivelAgua NivelAguaSensor::getNivelAnterior()
{
    return estado_anterior;
}

// =============================================
//  VERIFICAR SE HOUVE MUDANÇA
// =============================================
bool NivelAguaSensor::houveMudanca()
{
    return (estado_atual != estado_anterior);
}

// =============================================
//  OBTER STRING DO NÍVEL ATUAL
// =============================================
String NivelAguaSensor::getNivelString()
{
    return nivelParaString(estado_atual);
}

// =============================================
//  OBTER STRING DO NÍVEL ANTERIOR
// =============================================
String NivelAguaSensor::getNivelAnteriorString()
{
    return nivelParaString(estado_anterior);
}

// =============================================
//  CONVERTER ENUM PARA STRING
// =============================================
String NivelAguaSensor::nivelParaString(NivelAgua nivel)
{
    switch (nivel)
    {
    case CHEIO:
        return "CHEIO";
    case MEIO:
        return "MEIO";
    case VAZIO:
        return "VAZIO";
    default:
        return "DESCONHECIDO";
    }
}
