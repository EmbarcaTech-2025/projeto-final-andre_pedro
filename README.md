# SonoFree Mini

Sistema Inteligente de Detecção de Presença Humana com Sensor mmWave

## Sobre o Projeto

O SonoFree Mini é um interruptor inteligente que utiliza tecnologia mmWave (24GHz) para detecção precisa de presença humana. Diferentemente de sensores PIR convencionais, detecta pessoas completamente imóveis através de micromovimentos como respiração.

**Características principais:**
- Detecção de presença via sensor mmWave LD2420 (24GHz)
- Conectividade IoT via WiFi e MQTT
- Integração com Home Assistant
- Três modos de operação: Presença, Manual e Horário
- Encapsulamento compacto impresso em 3D

**Caminhos:**
- [Documentação](etapa_4/relatorio_tecnico.pdf)
- [Código fonte](etapa_4/codigo_esp32/sonofree.ino)
- [Modelo 3D](etapa_4/Part%Studio%1)
- [Lâmpada simulada (BitDogLab)](etapa_4/codigo_lampada_pico_w)
- [Imagens e vídeos](etapa_4/imagens_e_videos)
- [Apresetnção técnica](etapa_4/slides/sonofree_mini_tecnica.pdf)
- [Elevator pitch](etapa_4/slides/sonofree_mini_pitch.pdf)

## Arquitetura

```
Home Assistant ←→ MQTT ←→ ESP32 S3 ←→ LD2420 mmWave
                              ↓
                         Pico W ←→ LEDs WS2818B
```

### Componentes

- **ESP32 S3 SuperMini**: Microcontrolador principal, WiFi e MQTT
- **LD2420**: Sensor mmWave 24GHz para detecção de presença
- **Raspberry Pi Pico W**: Controle de LEDs (simulação de lâmpada)
- **LEDs WS2818B**: Array de 25 LEDs para indicação visual

## Instalação

### Hardware

**Conexões ESP32:**
```
ESP32 Pino 4  → LD2420 (UART)
ESP32 3V3     → LD2420 (VCC)
ESP32 GND     → LD2420 (GND)
```

**Conexões Pico W:**
```
Pico Pino 18  → Entrada digital
Pico Pino 7   → LEDs WS2818B (DIN)
```

### Software

**1. ESP32:**
- Edite credenciais WiFi e MQTT em `etapa_4/codigo_esp32/mmwave.ino`
- Compile e faça upload via Arduino IDE

**2. Pico W:**
```bash
cd etapa_4/codigo_lampada_pico_w
mkdir build && cd build
cmake ..
make
```

**3. Home Assistant:**
- Configure broker MQTT
- Adicione entidades via configuração MQTT

## Modos de Operação

### Modo Presença
Detecção automática via sensor mmWave. Liga ao detectar presença, desliga após timeout configurável (1-600 segundos).

### Modo Manual
Controle direto via interface Home Assistant. Switch ON/OFF simples.

### Modo Horário
Programação temporal com horários de início/fim configuráveis.

## Tópicos MQTT

- `home/sensor/cmd` - Mudança de modo
- `home/sensor/luz/cmd/presenca` - Controle luz modo presença
- `home/sensor/luz/cmd/manual` - Controle luz modo manual
- `home/sensor/luz/cmd/hora` - Controle luz modo horário
- `home/sensor/presenca/state` - Estado detecção presença
- `home/sensor/presenca/tempo_s/set` - Configuração timeout

## Calibração

O sensor LD2420 requer calibração prévia com software proprietário Windows:
1. Conecte sensor via UART TTL ao PC
2. Configure noise gates e threshold
3. Teste e salve configuração

## Limitações

### Hardware
- Relé não implementado (causa reinicializações na ESP32)
- Alimentação via USB-C necessária
- Calibração manual obrigatória

### Software
- Configuração WiFi hardcoded no código
- Dependência do Home Assistant para interface
- Comunicação ESP32-Pico W limitada a GPIO digital

## Especificações Técnicas

### ESP32 S3 SuperMini

### Sensor LD2420

### Encapsulamento
- Dimensões: 32,6mm × 27,6mm × 22mm
- Material: PLA (impressão 3D)

## Estrutura do Projeto

```
├── etapa_4/
│   ├── codigo_esp32/           # Firmware ESP32
│   ├── codigo_lampada_pico_w/  # Código Pico W
│   ├── slides/                 # Apresentações
│   └── videos/                 # Demonstrações
├── documentacao_tecnica.typ    # Documentação completa
└── README.md                   # Este arquivo
```

## Troubleshooting

**ESP32 não conecta WiFi:**
- Verifique credenciais no código
- Confirme alcance do sinal

**Sensor não detecta:**
- Verifique calibração do LD2420
- Confirme conexões GPIO 4

**MQTT desconectado:**
- Verifique broker ativo
- Confirme credenciais MQTT

## Equipe

**Desenvolvedores:**
- Andre Melo
- Pedro Rocha

**Projeto Final EmbarcaTech - 2025**

## Documentação

Para especificações técnicas detalhadas, consulte `documentacao_tecnica.typ`.
