#pragma once

#include <Arduino.h>

// =============================================
//  ENUMS PARA ESTADOS DO SENSOR
// =============================================
enum NivelAgua
{
    VAZIO = 0,
    MEIO = 1,
    CHEIO = 2
};

// =============================================
//  CLASSE PARA CONTROLE DO SENSOR DE NÍVEL
// =============================================
class NivelAguaSensor
{
private:
    int pino_sensor1; // Pino para sensor 1 (posição inferior)
    int pino_sensor2; // Pino para sensor 2 (posição superior)
    NivelAgua estado_atual;
    NivelAgua estado_anterior;
    unsigned long ultimo_tempo_leitura;
    const unsigned long DEBOUNCE_TIME = 50; // ms de debounce

    /**
     * Lê os sensores e determina o nível
     */
    NivelAgua lerSensores();

public:
    /**
     * Construtor do sensor de nível de água
     * @param pino1 Pino GPIO para sensor inferior
     * @param pino2 Pino GPIO para sensor superior
     */
    NivelAguaSensor(int pino1, int pino2);

    /**
     * Inicializa o sensor (configura pinos)
     */
    void iniciar();

    /**
     * Atualiza o estado do sensor (deve ser chamado no loop)
     */
    void atualizar();

    /**
     * Retorna o nível atual de água
     */
    NivelAgua getNivel();

    /**
     * Retorna o nível anterior
     */
    NivelAgua getNivelAnterior();

    /**
     * Verifica se houve mudança de estado
     */
    bool houveMudanca();

    /**
     * Retorna string com descrição do nível
     */
    String getNivelString();

    /**
     * Retorna string com descrição do nível anterior
     */
    String getNivelAnteriorString();

    /**
     * Função auxiliar para converter enum em string
     */
    static String nivelParaString(NivelAgua nivel);
};
