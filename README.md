# VIEWE 4.3" ESP32-S3 RGB Touch Display Dashboard — Zephyr RTOS 4.5.0

Porte e aplicação de painel interativo (*dashboard*) em **Zephyr RTOS 4.5.0** (com **Zephyr SDK 1.0.1**) para as placas **VIEWE 4.3" ESP32-S3 RGB Smart Display**, utilizando interface **LVGL 9** e touchscreen capacitivo **Goodix GT911**.

Este projeto nasceu para suprir a ausência de suporte nativo a displays **LCD RGB paralelos (DPI / dot-clock)** para ESP32-S3 no Zephyr RTOS, trazendo as definições completas de hardware (*custom boards* HWMv2) e uma interface LVGL funcional já testada em hardware real.

> **Status Atual (Apresentação Inicial)**:
> * **Neste repositório**: Definição completa das *custom boards* (`boards/viewe/uedx48270043e_wb_a` e `boards/viewe/uedx80480043e_wb_a`), configurações de PSRAM/LVGL/GT911 e aplicação inicial de validação (`src/main.c`).
> * **Driver Paralelo RGB no Zephyr (`espressif,esp32-lcd-cam-rgb`)**: Desenvolvido diretamente na árvore local do Zephyr 4.5.0 (`LCD_CAM` + `GDMA` com *bounce buffers* em SRAM interna) e **mantido offline por enquanto** enquanto realizamos mais testes de estabilidade em hardware antes de publicá-lo ou submetê-lo como contribuição (*upstream PR*) ao projeto Zephyr RTOS.

> **Referências de Hardware (Base Pública do Fabricante)**:  
> O levantamento de pinagem, esquemáticos, datasheets e parâmetros de temporização RGB utilizou como base os repositórios públicos oficiais da fabricante para FreeRTOS/ESP-IDF/Arduino:
> * **480×272 (`UEDX48270043E-WB-A` — Placa Principal Testada)**: [VIEWESMART/UEDX48270043E-ESP32-4.3inch-Touch-Display](https://github.com/VIEWESMART/UEDX48270043E-ESP32-4.3inch-Touch-Display)
> * **800×480 (`UEDX80480043E-WB-A` — Variante de Resolução)**: [VIEWESMART/UEDX80480043ESP32-4.3inch-Touch-Display](https://github.com/VIEWESMART/UEDX80480043ESP32-4.3inch-Touch-Display)

---

## 1. Estrutura do Projeto

Seguindo a convenção oficial `boards/viewe/` do Zephyr 4.5.0 (HWMv2), cada variante de resolução possui sua própria definição de placa:

* `boards/viewe/uedx48270043e_wb_a/`: Placa **VIEWE UEDX48270043E-WB-A** (**480×272**, painel `ILI6485A` / `ST7283`, PCLK `12 MHz`, *bounce buffer* padrão de `17` linhas).
  * Targets: `uedx48270043e_wb_a/esp32s3/procpu` e `uedx48270043e_wb_a/esp32s3/appcpu`.
* `boards/viewe/uedx80480043e_wb_a/`: Placa **VIEWE UEDX80480043E-WB-A** (**800×480**, painel `ST7262`, PCLK `16 MHz`, *bounce buffer* padrão de `16` linhas).
  * Targets: `uedx80480043e_wb_a/esp32s3/procpu` e `uedx80480043e_wb_a/esp32s3/appcpu`.
* `src/main.c`: Dashboard LVGL responsivo (se ajusta automaticamente à resolução reportada pelo driver de display) com indicador circular (`lv_arc`), botão de teste e *slider* sincronizado para validação do touch **GT911**.
* `prj.conf`: Configuração do kernel, Octal PSRAM 80 MHz, Data Cache 64 KB (linha 64 B), touch `GT911` em *polling* (`15 ms`), `DISPLAY_ESP32_LCD_CAM_RGB` (`1` *framebuffer* em PSRAM + *bounce buffers* *ping-pong* em SRAM interna) e LVGL.
* `zephyr/module.yml` & `CMakeLists.txt`: Registro local automático do diretório `boards/` via `BOARD_ROOT`.

---

## 2. Especificações de Hardware (ESP32-S3 N16R8)

* **SoC**: `ESP32-S3-WROOM-1-N16R8` (Dual-Core Xtensa LX7 @ 240 MHz)
* **Flash**: 16 MB Quad SPI @ 80 MHz
* **PSRAM**: 8 MB Octal SPI (`OPI PSRAM`) @ 80 MHz (usa internamente `GPIO35`, `GPIO36` e `GPIO37`)
* **Display LCD TFT 4.3" (Conector FPC 40 pinos `J1`)**:
  * Barramento RGB565 de 16 bits paralelo puro (sem SPI de inicialização; `DISP` ligado a `3.3V` na PCB)
  * Backlight (`LP3321B6F`): **`GPIO2`** (ativo alto)
* **Touch Capacitivo (`Goodix GT911`)**:
  * `I2C0` @ `400 kHz`: `SDA` = **`GPIO19`**, `SCL` = **`GPIO20`**
  * `RST` = **`GPIO38`** (ativo baixo)
  * *Observação*: O resistor `R33` do pino `INT` (`GPIO18`) vem aberto (`NC`) de fábrica na PCB; por isso, o driver `goodix,gt911` opera em modo *polling* (`CONFIG_INPUT_GT911_PERIOD_MS=15`) nos endereços `0x5D` / `0x14`.
* **Slot MicroSD (`SPI2`)**: `CS=GPIO10`, `MOSI=GPIO11`, `CLK=GPIO12`, `MISO=GPIO13`
* **Botão BOOT (`K1`) / LED WS2812B (`D5`)**: Compartilham **`GPIO0`** (`spi3` desabilitado por padrão no DTS para liberar `sw0`)
* **USB-UART (`CH340C` na porta `UART`)**: `UART0` (`TX=GPIO43`, `RX=GPIO44` @ `115200` bps)

### Mapeamento Completo do Barramento RGB565 (`LCD_CAM`)

| Sinal | Função ESP32-S3 | GPIO | Observações |
| :--- | :--- | :---: | :--- |
| `PCLK` (`DCLK`) | `LCD_CAM_LCD_CLK` | `GPIO42` | Borda de descida (`pixelclk-active = <1>`) |
| `DE` | `LCD_CAM_H_ENABLE_LCD` | `GPIO40` | Ativo alto (`de-active = <1>`) |
| `VSYNC` | `LCD_CAM_V_SYNC_LCD` | `GPIO41` | Ativo baixo (`vsync-active = <0>`) |
| `HSYNC` | `LCD_CAM_H_SYNC_LCD` | `GPIO39` | Ativo baixo (`hsync-active = <0>`) |
| `BL_EN` | `backlight-gpios` | `GPIO2` | Ativo alto (`GPIO_ACTIVE_HIGH`) |
| `B0..B4` | `LCD_CAM_DATA_OUT0..4` | `8, 3, 46, 9, 1` | `B0..B2` físicos aterrados no FPC |
| `G0..G5` | `LCD_CAM_DATA_OUT5..10` | `5, 6, 7, 15, 16, 4` | `G0..G1` físicos aterrados no FPC |
| `R0..R4` | `LCD_CAM_DATA_OUT11..15` | `45, 48, 47, 21, 14` | `R0..R2` físicos aterrados no FPC |

---

## 3. Como Compilar e Gravar

> **Pré-requisito**: Ambiente Zephyr `4.5.0` + Zephyr SDK `1.0.1` configurado com o suporte ao controlador `espressif,esp32-lcd-cam-rgb`.

```powershell
# Compilar para a placa 480x272 (UEDX48270043E-WB-A)
west build -p always -b uedx48270043e_wb_a/esp32s3/procpu .

# (Alternativa) Compilar para a variante 800x480 (UEDX80480043E-WB-A)
west build -p always -b uedx80480043e_wb_a/esp32s3/procpu .

# Gravar via porta USB "UART" (CH340C)
west flash

# Acompanhar logs pela serial (115200 bps)
west espressif monitor
```

---

## 4. Próximos Passos

* Continuar testes de estresse, estabilidade e *tuning* do driver paralelo RGB (`LCD_CAM` + `GDMA` com *bounce buffers* em SRAM) no Zephyr 4.5.0.
* Publicar e submeter o driver `espressif,esp32-lcd-cam-rgb` e o suporte à placa como contribuição *upstream* para o Zephyr RTOS após validação completa.
* Evoluir a interface do *dashboard* LVGL para a aplicação final.
