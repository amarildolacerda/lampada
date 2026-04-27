#include <Arduino.h>
#include "lampada_sensor.h"

LampadaSensor::LampadaSensor(int sensorPin) : pin(sensorPin), estado(false)
{
    pinMode(pin, INPUT);
}

void LampadaSensor::inicializar()
{
    digitalWrite(pin, LOW);
}

bool LampadaSensor::lerEstado()
{
    estado = digitalRead(pin);
    return estado;
}

bool LampadaSensor::getEstado() const
{
    return estado;
}

void LampadaSensor::ligar()
{
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
    estado = true;
}

void LampadaSensor::desligar()
{
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    estado = false;
}

void LampadaSensor::alternar()
{
    estado ? desligar() : ligar();
}
