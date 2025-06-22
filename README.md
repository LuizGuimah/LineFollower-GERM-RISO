# HERMES Line Follower - RISO/GERM/UDESC

This project was developed by a group of electrical engineering and computer science students with the goal of creating a high-performance line-following robot using PID control.

## Overview

The line-following robot is designed to detect and follow tracks with high precision. It uses reflectance sensors to monitor the line and dynamically adjust its position, ensuring efficient and fast movement.

![Robot photo](media/photo_superior_robocore2025.jpg)

## Hardware Used

| Component          | Description                                    |
|--------------------|----------------------------------------------|
| **Microcontroller** | ESP32-WROOM                                  |
| **Line Sensors**   | Pololu QTR-8RC sensor array                  |
| **Lateral Sensors** | Two QRE sensors from ROBOCORE               |
| **Motor Driver**   | TB6612FN                            |
| **Motors**        | Pololu N20 10,000 RPM motors with 1:10 gearbox |
| **Encoders**      | Pololu encoders                               |
| **Voltage Booster** | XL6009 Boost Step Up                        |


## Esp32 Used Pinout

![esp32 photo](media/esp32_full_pinout.png)

## Software and Control

The robot's control is based on a PID (Proportional, Integral, and Derivative) algorithm, which adjusts the motor speed according to the data provided by the line sensors. PID ensures the robot remains stable on the track, correcting its position in real-time.

## Inputs/Outputs

Line Sensors (QTR-8RC): Return values between 0 and 4095, where 0 indicates a white surface and 4095 indicates no reflectance.

Lateral Sensors (TCRT5000): Provide four digital inputs: 0 indicates "On Line," and 1 indicates "Off Line."

Encoders: Allow speed measurement of the motors and provide feedback for dynamic PID adjustments.

PWM to Motors: Speed control is achieved through PWM signals sent to the TB6612FN H-Bridge, allowing fine adjustments of speed and direction.

## PID Tuning

Tuning the PID parameters (Kp: Proportional, Ki: Integral, Kd: Derivative) is essential for the robot's performance. Here are some guidelines:

Adjust Kp: Increase until the robot oscillates around the line.

Adjust Kd: Add a value to reduce oscillations.

Adjust Ki: Small values can help correct systematic deviations.

## Melhorias pós-Robocore 2025

* Modularizar os sensores da frente dianteiros com 8 sensores de reflectância QRE da robocore, disponível em: [Sensor QRE Robocore](https://www.robocore.net/sensor-robo/sensor-de-linha-qre-analogico/com-barra-soldada?gad_source=1&gad_campaignid=16517456855&gbraid=0AAAAADzrkI4hDZskGFMA3gS0u2SYhlMdw&gclid=Cj0KCQjw097CBhDIARIsAJ3-nxdo5wJS_0_qzqoj_105IrNbTG1M5sZ5zkBkiOPcLkSAlXD_NjUZNIIaAnjyEALw_wcB)

* Alterar os Pneus de borracha para o modelo StickyMAX S20 (Necessita de muito teste), disponível em: [Roda S20 macia Robocore](https://www.robocore.net/roda/roda-stickymax-s20-22mm?srsltid=AfmBOopxZ_wZKX5jpfMzeoPc_oW4sheJgAukbF4VQag6QChXWixrqtTk)

* Trocar e anexar diretamente na placa os encoders dos motores para melhorar a fiação do robo;

* Adquirir novos esps com melhor conexão WIRELESS (Na competição o bluetooth e o BLE não funcionam com perfeição no modelo padrão do ESP32 que estamos usando);

* Adquirir/trocar a bateria por uma de menor peso e melhor eficência (A tamandutech, a omega e a Raiju usam baterias de 300mAh);

* Melhorar a conexão da bateria com a placa (O jeito atual fica pegando levemente na roda);

