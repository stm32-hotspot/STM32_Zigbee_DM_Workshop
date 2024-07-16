# Zigbee Demo application

## Overview
This application demonstrates how to use the STM32WB55 Nucleo board in various Zigbee configurations,
showcasing different real-world use case scenarios. The configurations are set in the app_zigbee.c file.

You can find PDF slideset with demo description in folder pdf_slides or you can watch education video which demonstrates the functinality on ST youtube channel (recording of Zigbee Demo Workshop):

[Zigbee demo WS](https://youtu.be/8X_jKmInjCI)
[Zigbee demo WS Q&A](https://youtu.be/xn5pyrNBuGY)

## Boards Needed
  * as Zigbee accessory P-NUCLEO-WB55
    * [NUCLEO-WB55RG](https://www.st.com/en/evaluation-tools/nucleo-wb55rg.html)
    
	
## Configuration Options
You can select one of the following configurations by setting the respective define to 1:

Light Switch (On/Off Client)
Light Bulb (On/Off Server)
Thermostat (Thermostat Server Cluster)

Example:
```
#define APP_ZIGBEE_LIGHTSWITCH_CONFIG                1
#define APP_ZIGBEE_LIGHTBULB_CONFIG                  0
#define APP_ZIGBEE_THERMOSTAT_CONFIG                 0
```
### <b>Additional Functionalities</b>
You can also enable additional functionalities such as persistent data management and OTA client support:

Example:
```
#define APP_ZIGBEE_NVM_SUPPORT                       1  /* Persistent data management */
#define APP_ZIGBEE_OTA_SUPPORT                       0  /* OTA Client availability */
```
### <b>Touchlink Commissioning</b>
For touchlink commissioning, light switches act as touchlink initiators, and light bulbs act as touchlink targets. 
This is configured using the following define:
```
#define APP_ZIGBEE_TOUCHLINK_TARGET                  1  
```

Touchlink Initiator: Long press button SW3 to initialize the touchlink procedure. Short press for Z3.0 commissioning.
Touchlink Target: The device automatically starts the touchlink procedure and waits for the initiator. You can 
cancel touchlink commissioning and select Z3.0 commissioning by short pressing button SW3.

### <b>Z3.0 Commissioning</b>
Short press button SW3 to join an existing network created by a third-party coordinator or the OTA_Server example.

### <b>Pairing with Third-Party Devices</b>
To pair with commercially available Zigbee light bulbs or light switches, you need to obtain and use 
the Zigbee Light Link (ZLL) master key. This key is shared between all devices supporting the ZLL profile 
and is distributed by the ZigBee Alliance under a non-disclosure agreement (NDA).

### <b>Project Requirements</b>
This application requires the stm32wb5x_Zigbee_FFD_fw.bin binary to be flashed on the Wireless Coprocessor.

You can either:

Use one STM32WB55xx board loaded with the demo example built with the selected configuration and connect it to a third-party coordinator.
Use two STM32WB55xx boards, one configured as a Light Bulb and the other as a Light Switch.

### <b>Button Functions</b>
SW1 [Toggle]: Used in Light Switch configuration (On/Off Client) to send a toggle request to the On/Off Server (Light Bulb).
SW2 [Factory Reset]: Deletes persistent data from flash and resets the device. If APP_ZIGBEE_NVM_SUPPORT == 1, the device will try to restore the previously saved configuration and rejoin the network it was previously joined to. Button press is indicated by short blinking of all LEDs.
SW3 [Join Network]: Short press to join an available Zigbee network. Long press to start touchlink commissioning.

### <b>LED Indicators</b>
RED LED: Indicates the state of the On/Off attribute of the On/Off Server cluster (similar to a light bulb).
GREEN LED: Used for Light Bulb commissioning, indicating the identify callback (similar to light bulb blinking when joining two devices together).
BLUE LED: Indicates that the device has joined a Zigbee network.

### <b>To setup the application:</b>

Open the project select preferred configuration by editing respective defines in app_zigbee.c build it and load your generated application on your STM32WB devices.
  

 Note: when LED1, LED2 and LED3 are toggling it is indicating an error has occurred on application.

 
## How to use it?

<b>Loading of the stm32wb5x_Zigbee_FFD_fw.bin binary</b>

  This application requests having the stm32wb5x_Zigbee_FFD_fw.bin binary flashed on the Wireless Coprocessor.
  If it is not the case, you need to use STM32CubeProgrammer to load the appropriate binary.
  All available binaries are located under /Projects/STM32_Copro_Wireless_Binaries directory.
  Refer to UM2237 to learn how to use/install STM32CubeProgrammer.
  Refer to /Projects/STM32_Copro_Wireless_Binaries/ReleaseNote.html for the detailed procedure to change the
  Wireless Coprocessor binary. 

<b>Getting traces</b>

  To get the traces you need to connect your Board to the Hyperterminal (through the STLink Virtual COM Port).
  The UART must be configured as follows:
```
    - BaudRate = 115200 baud  
    - Word Length = 8 Bits 
    - Stop Bit = 1 bit
    - Parity = none
    - Flow control = none
```

## Keywords

Zigbee, IoT, Internet of Things, Network, Connectivity, FreeRTOS, commissioning, persistence, CSA, Connectivity Standard Alliance, STM32, P-NUCLEO-WB55, Touch Link, NVM, OTA

## Known issues
n/a