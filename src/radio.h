#pragma once

#include <Arduino.h>

// Tamanho máximo do payload de rádio em bytes.
static const size_t RADIO_PAYLOAD_MAX = 64;

// Endereço de broadcast usado para todos os dispositivos.
static const uint8_t RADIO_BROADCAST_ADDR = 0xFF;

// Estrutura de pacote de rádio que contém origem, destino e payload.
struct RadioFrame
{
    uint8_t origin;
    uint8_t destination;
    uint8_t hope;
    uint8_t length;
    uint8_t payload[RADIO_PAYLOAD_MAX];

    RadioFrame();
    void clear();
    bool setPayload(const uint8_t *data, size_t size);
    bool setPayload(const String &data);
    String payloadAsString() const;
    size_t serialize(uint8_t *buffer, size_t bufferSize) const;
    bool deserialize(const uint8_t *buffer, size_t bufferSize);
    static size_t requiredSize(size_t payloadSize)
    {
        return 3 + payloadSize;
    }
};

// Interface abstrata de rádio para implementar o envio e recebimento de frames.
class RadioInterface
{
public:
    virtual ~RadioInterface() = default;

    // Inicializa o rádio e retorna true se a inicialização foi bem sucedida.
    virtual bool begin() = 0;

    // Envia um frame de rádio.
    virtual bool sendFrame(const RadioFrame &frame) = 0;

    // Retorna true se houver um frame pronto para leitura.
    virtual bool available() = 0;

    // Recebe o próximo frame disponível.
    virtual bool receiveFrame(RadioFrame &frame) = 0;

    // Deve ser chamado no loop principal para processar eventos de rádio.
    virtual void loop() = 0;
};
