# Controle de Robô Seguidor de Linha Hermes via BLE

Este projeto consiste em um robô seguidor de linha baseado em ESP32 (Hermes) controlado remotamente por uma aplicação Python com interface gráfica via Bluetooth Low Energy (BLE).

## Visão Geral

O sistema é dividido em duas partes principais:

1.  **`hermesBLE/`**: Contém o firmware Arduino (`.ino`) para o microcontrolador ESP32.
    *   Responsável pela lógica de baixo nível do robô:
        *   Leitura dos sensores de linha e laterais.
        *   Controle dos motores utilizando um driver (ex: L298N ou similar).
        *   Implementação de um controlador PID para seguir a linha.
        *   Máquina de estados para gerenciar diferentes comportamentos do robô (seguir linha, parar, manobras em curvas/marcações).
        *   Servidor BLE para comunicação com a aplicação Python:
            *   Recebe comandos para alterar o estado do robô (Iniciar/Parar).
            *   Recebe comandos para ajustar os parâmetros do PID (Kp, Ki, Kd) e a velocidade base (Bs) em tempo real.
            *   Envia o estado atual do robô (Parado, Seguindo Linha, etc.) para a interface Python via notificações BLE.

2.  **`controlePythonBLE/`**: Contém a aplicação desktop em Python com interface gráfica (`customtkinter`).
    *   Permite ao usuário:
        *   Escanear e conectar-se ao robô Hermes via BLE.
        *   Iniciar e parar o movimento do robô.
        *   Ajustar os parâmetros do PID (Kp, Ki, Kd) e a velocidade base (Bs) através de sliders e campos de entrada.
        *   Enviar os parâmetros ajustados para o ESP32 em tempo real (modo automático) ou manualmente.
        *   Visualizar o estado atual do robô recebido do ESP32.
        *   Ver logs de comunicação e status.

## Funcionalidades

### Firmware ESP32 (`hermesBLE.ino`)
*   Comunicação BLE robusta com características para recepção (RX) e transmissão (TX) de dados.
*   Controle PID ajustável para seguir a linha.
*   Máquina de estados para diferentes fases da pista (ex: seguir linha, detectar marcações laterais).
*   Tratamento de perda de linha.
*   Notificação do estado atual do robô para o cliente BLE.
*   Parsing de comandos recebidos via BLE para atualização de Kp, Ki, Kd, Velocidade Base e Estado.

### Aplicação Python (`controlePythonBLE.py`)
*   Interface gráfica amigável construída com `customtkinter`.
*   Conexão e desconexão BLE com o ESP32.
*   Controles interativos (sliders e campos de texto) para ajuste dos parâmetros:
    *   `Kp`: Ganho Proporcional
    *   `Ki`: Ganho Integral
    *   `Kd`: Ganho Derivativo
    *   `Bs`: Velocidade Base dos motores
*   Botões para "Iniciar Robô" e "Parar Robô".
*   Exibição do estado atual do robô (recebido via notificações BLE).
*   Opção de envio automático de parâmetros ao modificar os sliders/campos ou envio manual.
*   Janela de log para depuração e feedback.

## Requisitos e Configuração

### Para o Firmware do ESP32 (`hermesBLE/`)

1.  **Hardware:**
    *   Microcontrolador ESP32.
    *   Sensores de linha.
    *   Sensores laterais.
    *   Driver de motor.
    *   Motores DC com rodas.
    *   Fonte de alimentação adequada.
2.  **Software (Arduino IDE):**
    *   Arduino IDE instalado.
    *   Suporte à placa ESP32 instalado na Arduino IDE (via Gerenciador de Placas).
    *   Bibliotecas Arduino necessárias (geralmente a biblioteca BLE nativa do ESP32 core já é suficiente, como `BLEDevice.h`).
3.  **Procedimento:**
    *   Abra o arquivo `hermesBLE/hermesBLE.ino` na Arduino IDE.
    *   Selecione a placa ESP32 correta e a porta COM.
    *   Compile e carregue o firmware no ESP32.
    *   Após carregar, o ESP32 deve começar a anunciar-se como "Hermes_BLE".

### Para a Aplicação Python (`controlePythonBLE/`)

1.  **Python:**
    *   Python 3.8 ou superior recomendado.
2.  **Bluetooth:**
    *   Um adaptador Bluetooth no seu computador que suporte BLE (Bluetooth 4.0+).
    *   O driver Bluetooth do seu sistema operacional deve estar funcionando corretamente.
3.  **Dependências Python:**
    *   `customtkinter`: Para a interface gráfica.
    *   `bleak`: Para a comunicação BLE.

4.  **Instalação das Dependências:**

    *   **Clone o repositório (se ainda não o fez):**
        ```bash
        git clone <URL_DO_SEU_REPOSITORIO>
        cd <NOME_DA_PASTA_DO_REPOSITORIO>
        ```

    *   **Navegue até a pasta da aplicação Python:**
        ```bash
        cd controlePythonBLE
        ```

    *   **(Opcional, mas recomendado) Crie e ative um ambiente virtual:**
        *   No Linux/macOS:
            ```bash
            python3 -m venv venv
            source venv/bin/activate
            ```
        *   No Windows:
            ```bash
            python -m venv venv
            .\venv\Scripts\activate
            ```

    *   **Instale as bibliotecas necessárias usando pip:**
        ```bash
        pip install customtkinter bleak
        ```
        *Nota: Em alguns sistemas Linux, pode ser necessário instalar pacotes adicionais para o `bleak` funcionar corretamente, como `libglib2.0-dev` e `bluez`.*
        ```bash
        # Exemplo para sistemas baseados em Debian/Ubuntu:
        # sudo apt-get update
        # sudo apt-get install libglib2.0-dev libdbus-1-dev bluez
        ```

## Como Executar

1.  **Ligue o Robô Hermes:** Certifique-se de que o ESP32 com o firmware `hermesBLE.ino` carregado esteja alimentado e funcionando. Ele deve estar anunciando via BLE.
2.  **Execute a Aplicação Python:**
    *   Abra um terminal ou prompt de comando.
    *   Navegue até o diretório `controlePythonBLE/` (e ative o ambiente virtual, se estiver usando um).
    *   Execute o script Python:
        ```bash
        python controlePythonBLE.py
        ```
3.  **Use a Interface:**
    *   Na interface gráfica, clique no botão **"Conectar"**. A aplicação irá procurar pelo dispositivo "Hermes_BLE".
    *   O status da conexão será exibido.
    *   Uma vez conectado, os controles (sliders, botões de iniciar/parar) serão habilitados.
    *   Você poderá ver o "Estado Robô" atualizado conforme ele é enviado pelo ESP32.
    *   Ajuste os parâmetros Kp, Ki, Kd, Bs. Se o "Envio Automático Parâmetros" estiver ligado, eles serão enviados ao ESP32 conforme você os altera (se o robô estiver iniciado). Caso contrário, clique em "Enviar Parâmetros Agora".
    *   Use "Iniciar Robô" para fazer o robô começar a seguir a linha (Estado 0) e "Parar Robô" para interrompê-lo (Estado 4).
    *   Para desconectar, clique em "Desconectar".

## Observações e Solução de Problemas

*   **Permissões Bluetooth:** Em alguns sistemas operacionais (especialmente Linux), sua aplicação Python pode precisar de permissões para acessar o hardware Bluetooth.
*   **Descoberta BLE:** A descoberta de dispositivos BLE pode, às vezes, ser instável. Se o "Hermes_BLE" não for encontrado:
    *   Verifique se o Bluetooth do seu computador está ligado.
    *   Verifique se o robô está ligado e dentro do alcance.
    *   Tente reiniciar o Bluetooth do computador ou o próprio robô.
    *   Verifique os logs na interface para mensagens de erro.
*   **Compatibilidade `bleak`:** `bleak` depende do backend BLE do sistema operacional. Certifique-se de que seu sistema está atualizado.

---
