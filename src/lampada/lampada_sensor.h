// criar classe lampada_sensor.h
#ifndef LAMPADA_SENSOR_H
#define LAMPADA_SENSOR_H

class LampadaSensor
{
private:
    int pin;
    bool estado;

public:
    LampadaSensor(int sensorPin);

    void inicializar();
    bool lerEstado();

    bool getEstado() const;

    void ligar();

    void desligar();

    void alternar();
};

#endif // LAMPADA_SENSOR_H
