/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    App/app_zigbee.h
  * @author  MCD Application Team
  * @brief   Header for Zigbee Application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef APP_ZIGBEE_H
#define APP_ZIGBEE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
/* Private includes ----------------------------------------------------------*/
#include "tl.h"
#include "tl_zigbee_hci.h"
#include "stdbool.h"

/* USER CODE BEGIN Includes */
#include "zigbee.h"
#include "zigbee.startup.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/

/* Application errors                 */
/*------------------------------------*/

/*
 *  List of all errors tracked by the Zigbee application
 *  running on M4. Some of these errors may be fatal
 *  or just warnings
 */
typedef enum
{
  ERR_ZIGBE_CMD_TO_M0,
/* USER CODE BEGIN ERROR_APPLI_ENUM */

/* USER CODE END ERROR_APPLI_ENUM */
  ERR_ZIGBEE_CHECK_WIRELESS
} ErrAppliIdEnum_t;
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/

/* USER CODE BEGIN EC */

/**
  * @brief  APP_ZIGBEE Status structures definition
  */
typedef enum
{
  APP_ZIGBEE_OK       = 0x00,
  APP_ZIGBEE_ERROR    = 0x01,
} APP_ZIGBEE_StatusTypeDef;

/*
 *  OTA structures definition
 */
enum APP_ZIGBEE_OtaFileTypeDef_t{
  fileType_COPRO_WIRELESS = IMAGE_TYPE_FW_COPRO_WIRELESS,
  fileType_APP = IMAGE_TYPE_FW_APP,
};

struct OTA_currentFileVersion{
  enum APP_ZIGBEE_OtaFileTypeDef_t fileType;
  uint8_t fileVersion;
};

struct APP_ZIGBEE_OtaContext_t{
  enum APP_ZIGBEE_OtaFileTypeDef_t  file_type;
  uint8_t file_version;
  uint32_t binary_size;
  uint32_t binary_calc_crc;
  uint32_t binary_srv_crc;
  uint32_t base_address;
  uint32_t magic_keyword;
};

struct APP_ZIGBEE_OtaWriteInfo_t{
  uint8_t firmware_buffer[RAM_FIRMWARE_BUFFER_SIZE];
  uint32_t firmware_buffer_current_offset;
  uint32_t flash_current_offset;
  bool buffer_full;
};

struct Zigbee_OTA_client_info {
  struct APP_ZIGBEE_OtaContext_t ctx;
  struct APP_ZIGBEE_OtaWriteInfo_t write_info;
  uint16_t image_type;
  uint32_t current_file_version;
  uint32_t requested_image_size;
  uint32_t download_time;
};

struct zigbee_app_info
{
  bool has_init;
  struct ZigBeeT *zb;
  enum ZbStartType startupControl;
  enum ZbStatusCodeT join_status;
  uint32_t join_delay;
  bool init_after_join;
  uint8_t join_retry_counter;
  uint32_t persistNumWrites;
  bool fresh_startup;

  /*Lightbulb related clusters*/
  struct ZbZclClusterT *onOff_server_1;
  struct ZbZclClusterT *identify_server_1;
  struct ZbZclClusterT *scenes_server_1;
  struct ZbZclClusterT *groups_server_1;

  /*Lightswitch related cluster*/
  struct ZbZclClusterT *onOff_client_1;

  /*Thermostat related cluster*/
  struct ZbZclClusterT *thermostat_server_1;

  /*Extra clusters*/
  struct ZbZclClusterT *ota_client;
};

/* USER CODE END EC */

/* External variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Exported macros ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions ------------------------------------------------------- */
void APP_ZIGBEE_Init(void);
void APP_ZIGBEE_Error(uint32_t ErrId, uint32_t ErrCode);
void APP_ZIGBEE_RegisterCmdBuffer(TL_CmdPacket_t *p_buffer);
void APP_ZIGBEE_TL_INIT(void);
void Pre_ZigbeeCmdProcessing(void);
void Post_ZigbeeCmdProcessing(void);
/* USER CODE BEGIN EF */

/* USER CODE END EF */
#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* APP_ZIGBEE_H */
