# Smart Home MQTT – Controle de Ar-Condicionado e Lâmpadas com a BitDogLab

Sistema de automação residencial utilizando a placa **BitDogLab** (baseada no Raspberry Pi Pico W), que permite o controle físico e remoto (via MQTT) de um ar-condicionado simulado e lâmpadas RGB em diferentes cômodos da casa. O sistema promove praticidade, baixo custo e integração com o ambiente **IoT**.

---

## Funcionalidades Principais

### Controle Local por Botões e Joystick

- **Botão A**: Diminui a temperatura (mínimo de 16 °C).  
- **Botão B**: Aumenta a temperatura (máximo de 30 °C).  
- **Botão do Joystick**: Liga/Desliga o ar-condicionado.

### Controle Remoto via MQTT

#### Controle do Ar-Condicionado
- **Tópico**: `/ar`
- **Comandos**:
  - `"On"`
  - `"Off"`
  - Temperatura direta (ex: `"27"`)
- **Respostas**:
  - `/ar/state`: `"On"` ou `"Off"`
  - `/ar/temperatura`: Temperatura atual

#### Controle das Lâmpadas
- **Tópicos**:
  - `/lampada/quarto`
  - `/lampada/sala`
  - `/lampada/cozinha`
- **Comandos**:
  - `"On"`, `"Off"`
  - Cores em **hexadecimal** (`#FFAABB`) ou **RGB** (`rgb(255,255,0)`)
- **Respostas**:
  - `/lampada/<cômodo>/state`
  - `/lampada/<cômodo>/cor`

### Persistência de Estado

- A **temperatura** e a **cor das lâmpadas** são preservadas entre os ciclos de desligamento e religamento.

---

## 🔌 Componentes e GPIOs Utilizados

| Componente                 | GPIOs                   | Função                                 |
|---------------------------|-------------------------|----------------------------------------|
| Botão A                   | GPIO 5                  | Diminuir temperatura                   |
| Botão B                   | GPIO 6                  | Aumentar temperatura                   |
| Botão do Joystick         | GPIO 22                 | Liga/desliga o ar-condicionado         |
| Matriz de LEDs WS2812     | GPIO 7                  | Simula visualmente a cor das lâmpadas |
| Display OLED SSD1306 (I2C)| GPIO 14 (SDA), GPIO 15 (SCL) | Exibe status e temperatura         |
| Módulo Wi-Fi CYW43        | Interno                 | Comunicação via MQTT                   |

---

## Estrutura do Código

### Arquivo Principal
- **`main.c`** – Lógica principal do sistema:
  - Inicializa hardware e conexão Wi-Fi
  - Gerencia assinaturas MQTT
  - Controla o display, matriz de LEDs e botões

### Funções Importantes

- `trata_mensagem_lampada()`: Interpreta comandos das lâmpadas e atualiza estado e cor.
- `hexParaRgb(hex_str)`: Converte string hexadecimal para tupla RGB.
- `eh_hex_color_valido()`: Verifica validade de string hexadecimal.
- `extrai_rgb_func()`: Extrai valores RGB de string no formato `rgb(r, g, b)`.
- `eh_rgb_func_valido()`: Verifica se uma string segue o padrão `rgb(r, g, b)`.
- `lampada_id_por_topico()`: Identifica qual lâmpada corresponde ao tópico recebido.
- `inicializar_lampadas()`: Define estado inicial das lâmpadas.
- `atualizar_ar_condicionado()`: Atualiza display e publica estado/temperatura.
- `atualizar_lampadas()`: Atualiza matriz e publica estados/cor no MQTT.

---

## 🌐 Comunicação MQTT

| Tópico             | Comando                         | Ação                                     |
|--------------------|----------------------------------|------------------------------------------|
| `/ar`              | `"On"`, `"Off"`                 | Liga/desliga o ar-condicionado           |
| `/ar`              | `16` a `30`                     | Define temperatura diretamente           |
| `/lampada/sala`    | `"On"`, `"Off"`                 | Liga/desliga lâmpada da sala             |
| `/lampada/sala`    | `#FFAABB`, `rgb(255,255,0)`     | Altera a cor da lâmpada da sala          |
| `/lampada/cozinha` | *(idem)*                        | *(idem para a cozinha)*                  |
| `/lampada/quarto`  | *(idem)*                        | *(idem para o quarto)*                   |


## ⚙️ Instalação e Uso

1. **Pré-requisitos**
   - Clonar o repositório:
     ```bash
     git clone https://github.com/JotaPablo/SmartHomeMQTT.git
     cd SmartHomeMQTT
     ```
   - Instalar o **Visual Studio Code** com as extensões:
     - **C/C++**
     - **Raspberry Pi Pico SDK**
     - **Compilador ARM GCC**
     - **CMake Tools**

2. **Conecte à Rede Wi-Fi e ao Servidor MQTT**
   - Abra o arquivo `main.c` e atualize as credenciais com os seus dados:

     ```c
     #define WIFI_SSID "SEU_SSID"               // Substitua pelo nome da sua rede Wi-Fi
     #define WIFI_PASSWORD "SUA_SENHA_WIFI"     // Substitua pela senha da sua rede Wi-Fi

     #define MQTT_SERVER "SEU_HOST"             // Substitua pelo endereço do broker MQTT (ex: 192.168.1.107)
     #define MQTT_USERNAME "SEU_USUARIO_MQTT"   // Substitua pelo nome de usuário do broker MQTT (caso necessário)
     #define MQTT_PASSWORD "SUA_SENHA_MQTT"     // Substitua pela senha do broker MQTT (caso necessário)
     ```



1. **Compilação**
   - Compile o projeto manualmente via terminal:
     ```bash
     mkdir build
     cd build
     cmake ..
     make
     ```
   - Ou utilize a opção **Build** da extensão da Raspberry Pi Pico no VS Code.

2. **Execução**
   - Conecte o Raspberry Pi Pico ao computador mantendo o botão **BOOTSEL** pressionado.
   - Copie o arquivo `.uf2` gerado na pasta `build` para o dispositivo `RPI-RP2`.