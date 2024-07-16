/**
  @page Zigbee Demo application
  
  @verbatim
  ******************************************************************************
  * @file    Zigbee/Zigbee_Demo/readme.txt 
  * @author  MCD Application Team
  * @brief   Description of the Zigbee OnOff Cluster application as a client  
  *          using a centralized network. 
  ******************************************************************************
  *
  * Copyright (c) 2020-2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  @endverbatim

@par Application Description 

This application demonstrates how to use the STM32WB55 Nucleo board in various configurations,
showcasing different real-world use case scenarios. The configurations are set in the app_zigbee.c file.

Configuration Options
You can select one of the following configurations by setting the respective define to 1:

	Light Switch (On/Off Client)
	Light Bulb (On/Off Server)
	Thermostat (Thermostat Server Cluster)

Example:
#define APP_ZIGBEE_LIGHTSWITCH_CONFIG                1
#define APP_ZIGBEE_LIGHTBULB_CONFIG                  0
#define APP_ZIGBEE_THERMOSTAT_CONFIG                 0

Additional Functionalities
You can also enable additional functionalities such as persistent data management and OTA client support:

Example:
#define APP_ZIGBEE_NVM_SUPPORT                       1  /* Persistent data management */
#define APP_ZIGBEE_OTA_SUPPORT                       0  /* OTA Client availability */

Touchlink Commissioning
For touchlink commissioning, light switches act as touchlink initiators, and light bulbs act as touchlink targets. 
This is configured using the following define:

#define APP_ZIGBEE_TOUCHLINK_TARGET                  1  


Touchlink Initiator: Long press button SW3 to initialize the touchlink procedure. Short press for Z3.0 commissioning.
Touchlink Target: The device automatically starts the touchlink procedure and waits for the initiator. You can 
cancel touchlink commissioning and select Z3.0 commissioning by short pressing button SW3.

Z3.0 Commissioning
Short press button SW3 to join an existing network created by a third-party coordinator or the OTA_Server example.

Pairing with Third-Party Devices
To pair with commercially available Zigbee light bulbs or light switches, you need to obtain and use 
the Zigbee Light Link (ZLL) master key. This key is shared between all devices supporting the ZLL profile 
and is distributed by the ZigBee Alliance under a non-disclosure agreement (NDA).

Project Requirements
This application requires the stm32wb5x_Zigbee_FFD_fw.bin binary to be flashed on the Wireless Coprocessor.

You can either:

Use one STM32WB55xx board loaded with the demo example built with the selected configuration and connect it to a third-party coordinator.
Use two STM32WB55xx boards, one configured as a Light Bulb and the other as a Light Switch.

Button Functions
SW1 [Toggle]: Used in Light Switch configuration (On/Off Client) to send a toggle request to the On/Off Server (Light Bulb).
SW2 [Factory Reset]: Deletes persistent data from flash and resets the device. If APP_ZIGBEE_NVM_SUPPORT == 1, the device will try to restore the previously saved configuration and rejoin the network it was previously joined to. Button press is indicated by short blinking of all LEDs.
SW3 [Join Network]: Short press to join an available Zigbee network. Long press to start touchlink commissioning.

LED Indicators
RED LED: Indicates the state of the On/Off attribute of the On/Off Server cluster (similar to a light bulb).
GREEN LED: Used for Light Bulb commissioning, indicating the identify callback (similar to light bulb blinking when joining two devices together).
BLUE LED: Indicates that the device has joined a Zigbee network.

Touchlink commissionning 
	               Device 1                                       Device 2
        	      (Lightswitch)                                   (Lightbulb)
        
	                ---------                                        --------
            	       |         |       touchlink commissioning        |        |
Long presss PushB SW3=>|Initiator| -----------------------------------> | Target | => Blue LED switched on
        	       |         |                                      |        |
	               |         |                                      |        |
	                ---------                                        --------
  

Led toggling
		           Device 1                                      Device 2
	  	          (Lightswitch)                                (Lightbulb)
        
	  	           ---------                                      ---------
	  	           |       |       ZbZclOnOffClientToggleReq      |       |
	 	PushB SW3=>|Client | -----------------------------------> |Server | => RED LED
	  	           |       |                                      |       |
	     	           |       |                                      |       |
	    	            --------                                      ---------

To setup the application :

Open the project select prefered configuration by editing respective defines in app_zigbee.c build it and load your generated application on your STM32WB devices.
  

 Note: when LED1, LED2 and LED3 are toggling it is indicating an error has occurred on application.

@par Keywords

Zigbee
 
@par Hardware and Software environment

  - This example runs on STM32WB55xx devices.
  
  - This example has been tested with an STMicroelectronics STM32WB55RG_Nucleo 
    board and can be easily tailored to any other supported device 
    and development board.
    
  - On STM32WB55RG_Nucleo, the jumpers must be configured as described
    in this section. Starting from the top left position up to the bottom 
    right position, the jumpers on the Board must be set as follows: 

     CN11:    GND         [OFF]
     JP4:     VDDRF       [ON]
     JP6:     VC0         [ON]
     JP2:     +3V3        [ON] 
     JP1:     USB_STL     [ON]   All others [OFF]
     CN12:    GND         [OFF]
     CN7:     <All>       [OFF]
     JP3:     VDD_MCU     [ON]
     JP5:     GND         [OFF]  All others [ON]
     CN10:    <All>       [OFF]


@par How to use it ? 

=> Loading of the stm32wb5x_Zigbee_FFD_fw.bin binary

  This application requests having the stm32wb5x_Zigbee_FFD_fw.bin binary flashed on the Wireless Coprocessor.
  If it is not the case, you need to use STM32CubeProgrammer to load the appropriate binary.
  All available binaries are located under /Projects/STM32_Copro_Wireless_Binaries directory.
  Refer to UM2237 to learn how to use/install STM32CubeProgrammer.
  Refer to /Projects/STM32_Copro_Wireless_Binaries/ReleaseNote.html for the detailed procedure to change the
  Wireless Coprocessor binary. 

=> Getting traces
  To get the traces you need to connect your Board to the Hyperterminal (through the STLink Virtual COM Port).
  The UART must be configured as follows:

    - BaudRate = 115200 baud  
    - Word Length = 8 Bits 
    - Stop Bit = 1 bit
    - Parity = none
    - Flow control = none

=> Running the application

  Refer to the Application description at the beginning of this readme.txt

 * <h3><center>&copy; COPYRIGHT STMicroelectronics</center></h3>

