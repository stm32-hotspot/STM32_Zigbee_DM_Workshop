
/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    App/zigbee_ota.c
  * @author  MCD Application Team
  * @brief   Zigbee Application.
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

/* Includes ------------------------------------------------------------------*/
#include "app_common.h"
#include "app_entry.h"
#include "dbg_trace.h"
#include "app_zigbee.h"
#include "zigbee_interface.h"
#include "shci.h"
#include "stm_logging.h"
#include "app_conf.h"
#include "stm32wbxx_core_interface_def.h"
#include "zigbee_types.h"
#include "cmsis_os.h"

/* Private includes -----------------------------------------------------------*/
#include <assert.h>
#include "zcl/zcl.h"
#include "zcl/general/zcl.ota.h"
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macros ------------------W------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* External definition -------------------------------------------------------*/


/* USER CODE BEGIN ED */

osThreadId_t OsTaskOTARequestUpgradeId;
osThreadId_t OsTaskOTAStartDownloadId;
osThreadId_t OsTaskOTAServerDiscoveryId;
osThreadId_t OsTaskPerformResetId;


extern osSemaphoreId_t       TransferToM0Semaphore;
extern osSemaphoreId_t       StartupEndSemaphore;
osSemaphoreId_t     		 OtaServerFoundSemaphore;

/* OTA app variables */
static struct Zigbee_OTA_client_info OTA_client_info;
extern struct zigbee_app_info zigbee_app_info;

const struct OTA_currentFileVersion OTA_currentFileVersionTab[] = {
  {fileType_COPRO_WIRELESS, CURRENT_FW_COPRO_WIRELESS_FILE_VERSION},
  {fileType_APP, CURRENT_FW_APP_FILE_VERSION},
};

/* USER CODE END ED */

/* Private function prototypes -----------------------------------------------*/


/* USER CODE BEGIN PFP */
/* ZCL OTA cluster related functions */
void APP_ZIGBEE_OTA_Client_Init(void);
void APP_ZIGBEE_OTA_Client_ServerDiscovery( void );

void APP_ZIGBEE_OTA_Client_DiscoverComplete_cb(struct ZbZclClusterT *clusterPtr, enum ZclStatusCodeT status,void *arg);
enum ZclStatusCodeT APP_ZIGBEE_OTA_Client_ImageNotify_cb(struct ZbZclClusterT *clusterPtr, uint8_t payload_type,
                                                                uint8_t jitter, struct ZbZclOtaImageDefinition *image_definition,
                                                                struct ZbApsdeDataIndT *data_ind, struct ZbZclHeaderT *zcl_header);
void APP_ZIGBEE_OTA_Client_QueryNextImage_cb(struct ZbZclClusterT *clusterPtr, enum ZclStatusCodeT status,
                                                    struct ZbZclOtaImageDefinition *image_definition, uint32_t image_size, void *arg);
enum ZclStatusCodeT APP_ZIGBEE_OTA_Client_WriteImage_cb(struct ZbZclClusterT *clusterPtr, struct ZbZclOtaHeader *header,
                                                               uint8_t length, uint8_t *data, void *arg);
//enum ZclStatusCodeT APP_ZIGBEE_OTA_Client_WriteTag_cb(struct ZbZclClusterT * clusterPtr, struct ZbZclOtaHeader * header,
//                                                             uint16_t tag_id, uint32_t tag_length, uint8_t data_length,
//                                                             uint8_t * data, void * arg);
enum ZclStatusCodeT APP_ZIGBEE_OTA_Client_ImageValidate_cb(struct ZbZclClusterT *clusterPtr, struct ZbZclOtaHeader *header, void *arg);
void APP_ZIGBEE_OTA_Client_Reboot_cb(struct ZbZclClusterT *clusterPtr, void *arg);
enum ZclStatusCodeT APP_ZIGBEE_OTA_Client_AbortDownload_cb(struct ZbZclClusterT *clusterPtr, enum ZbZclOtaCommandId commandId, void *arg);

/* OTA application related functions */
static inline int APP_ZIGBEE_FindImageType(unsigned int fileType);
static inline void APP_ZIGBEE_OTA_Client_Request_Upgrade(void);
static inline void APP_ZIGBEE_OTA_Client_StartDownload(void);
static inline APP_ZIGBEE_StatusTypeDef APP_ZIGBEE_OTA_Client_WriteFirmwareData(struct Zigbee_OTA_client_info* client_info);
static inline APP_ZIGBEE_StatusTypeDef APP_ZIGBEE_CheckDeviceCapabilities(void);
void APP_ZIGBEE_PerformReset(void);

static inline uint32_t GetFirstSecureSector(void);
static inline void Delete_Sectors(void);

void APP_ZIGBEE_Process_OTA_Client_Request_Upgrade(void *argument);
void APP_ZIGBEE_Process_OTA_Client_StartDownload(void *argument);
void APP_ZIGBEE_Process_OTA_Client_ServerDiscovery(void *argument);
void APP_ZIGBEE_Process_PerformReset(void *argument);

/* USER CODE END PFP */

/* Private variables ---------------------------------------------------------*/
static struct ZbZclOtaClientConfig client_config = {
  .profile_id = ZCL_PROFILE_HOME_AUTOMATION,
  .endpoint = APP_OTA_ENDPOINT,
  .activation_policy = ZCL_OTA_ACTIVATION_POLICY_SERVER,
  .timeout_policy = ZCL_OTA_TIMEOUT_POLICY_APPLY_UPGRADE,
};

const osThreadAttr_t Task_OtaClientUpdateReq_attr =
{
    .name = CFG_TASK_OTA_CLIENT_UPDATE_REQ_PROCESS_NAME,
    .attr_bits = CFG_TASK_PROCESS_ATTR_BITS,
    .cb_mem = CFG_TASK_PROCESS_CB_MEM,
    .cb_size = CFG_TASK_PROCESS_CB_SIZE,
    .stack_mem = CFG_TASK_PROCESS_STACK_MEM,
    .priority = osPriorityBelowNormal,
    .stack_size = CFG_TASK_PROCESS_STACK_SIZE
};
const osThreadAttr_t Task_OtaClientStartDownload_attr =
{
    .name = CFG_TASK_OTA_CLIENT_START_DOWNLOAD_PROCESS_NAME,
    .attr_bits = CFG_TASK_PROCESS_ATTR_BITS,
    .cb_mem = CFG_TASK_PROCESS_CB_MEM,
    .cb_size = CFG_TASK_PROCESS_CB_SIZE,
    .stack_mem = CFG_TASK_PROCESS_STACK_MEM,
    .priority = osPriorityBelowNormal,
    .stack_size = CFG_TASK_PROCESS_STACK_SIZE
};
const osThreadAttr_t Task_OtaClientServerDisc_attr =
{
    .name = CFG_TASK_OTA_CLIENT_SERVER_DISC_PROCESS_NAME,
    .attr_bits = CFG_TASK_PROCESS_ATTR_BITS,
    .cb_mem = CFG_TASK_PROCESS_CB_MEM,
    .cb_size = CFG_TASK_PROCESS_CB_SIZE,
    .stack_mem = CFG_TASK_PROCESS_STACK_MEM,
    .priority = osPriorityBelowNormal,
    .stack_size = CFG_TASK_PROCESS_STACK_SIZE
};
const osThreadAttr_t Task_OtaClientPerformReset_attr =
{
    .name = CFG_TASK_OTA_CLIENT_PERFORM_RESET_PROCESS_NAME,
    .attr_bits = CFG_TASK_PROCESS_ATTR_BITS,
    .cb_mem = CFG_TASK_PROCESS_CB_MEM,
    .cb_size = CFG_TASK_PROCESS_CB_SIZE,
    .stack_mem = CFG_TASK_PROCESS_STACK_MEM,
    .priority = osPriorityBelowNormal,
    .stack_size = CFG_TASK_PROCESS_STACK_SIZE
};

/* FreeRtos stacks attributes */


/* USER CODE BEGIN PV */
/* USER CODE END PV */
/* Functions Definition ------------------------------------------------------*/


/**
 * @brief  Configure Zigbee OTA client application endpoint
 * @param  None
 * @retval None
 */
void APP_ZIGBEE_OTA_ConfigEndpoints(void)
{
	/* OTA client callbacks*/
	ZbZclOtaClientGetDefaultCallbacks(&client_config.callbacks);
	client_config.callbacks.discover_complete = APP_ZIGBEE_OTA_Client_DiscoverComplete_cb;
	client_config.callbacks.image_notify = APP_ZIGBEE_OTA_Client_ImageNotify_cb;
	client_config.callbacks.query_next = APP_ZIGBEE_OTA_Client_QueryNextImage_cb;
	//  .callbacks.update_raw = APP_ZIGBEE_OTA_Client_UpdateRaw_cb;
	//  client_config.callbacks.write_tag = APP_ZIGBEE_OTA_Client_WriteTag_cb;
	client_config.callbacks.write_image = APP_ZIGBEE_OTA_Client_WriteImage_cb;
	client_config.callbacks.image_validate = APP_ZIGBEE_OTA_Client_ImageValidate_cb;
	//  client_config.callbacks.upgrade_end = APP_ZIGBEE_OTA_Client_UpgradeEnd_cb;
	client_config.callbacks.reboot = APP_ZIGBEE_OTA_Client_Reboot_cb;
	client_config.callbacks.abort_download = APP_ZIGBEE_OTA_Client_AbortDownload_cb;

	/* OTA Client */
	zigbee_app_info.ota_client = ZbZclOtaClientAlloc(zigbee_app_info.zb, &client_config, &OTA_client_info);
	assert(zigbee_app_info.ota_client != NULL);
	ZbZclClusterEndpointRegister(zigbee_app_info.ota_client);

}

/**
 * @brief  Init Zigbee OTA client FreeRtos threads
 * @param  None
 * @retval None
 */
void APP_ZIGBEE_OTA_ThreadsInit(void)
{
	  OtaServerFoundSemaphore = osSemaphoreNew( 1, 0, NULL );

	  OsTaskOTARequestUpgradeId = osThreadNew(APP_ZIGBEE_Process_OTA_Client_Request_Upgrade, NULL,&Task_OtaClientUpdateReq_attr);
	  OsTaskOTAStartDownloadId = osThreadNew(APP_ZIGBEE_Process_OTA_Client_StartDownload, NULL,&Task_OtaClientStartDownload_attr);
	  OsTaskOTAServerDiscoveryId = osThreadNew(APP_ZIGBEE_Process_OTA_Client_ServerDiscovery, NULL,&Task_OtaClientServerDisc_attr);
	  OsTaskPerformResetId = osThreadNew(APP_ZIGBEE_Process_PerformReset, NULL,&Task_OtaClientPerformReset_attr);

}

 /**
  * @brief  OTA client Discover callback
  * @param  clusterPtr: ZCL Cluster pointer
  * @param  arg: Passed argument
  * @retval None
  */
  void APP_ZIGBEE_OTA_Client_DiscoverComplete_cb(struct ZbZclClusterT *clusterPtr, enum ZclStatusCodeT status,void *arg){
   enum ZclStatusCodeT internal_status = ZCL_STATUS_SUCCESS;
   uint64_t requested_server_ext = 0;
   //UNUSED (status);

   if (status == ZCL_STATUS_SUCCESS )
   {
     /* The OTA server extended address in stored in ZCL_OTA_ATTR_UPGRADE_SERVER_ID attribute */
     requested_server_ext = ZbZclAttrIntegerRead(zigbee_app_info.ota_client, ZCL_OTA_ATTR_UPGRADE_SERVER_ID, NULL, &internal_status);
     UNUSED (requested_server_ext);
     if(internal_status != ZCL_STATUS_SUCCESS){
       APP_DBG("ZbZclAttrIntegerRead failed.\n");
     }

     APP_DBG("OTA Server located ...");
     osSemaphoreRelease(OtaServerFoundSemaphore);
   }
   else
   {
     APP_DBG("OTA Server not found after TimeOut. Retry a discovery");
     osThreadFlagsSet(OsTaskOTAServerDiscoveryId,1);
   }
 }


 /**
  * @brief  OTA client Image Notify callback
  * @param  clusterPtr: ZCL Cluster pointer
  * @param  payload_type: Payload type
  * @param  jitter: Upgrade jitter given to the client
  * @param  image_definition: Image query definition pointer
  * @param  data_ind: APS layer packet info
  * @param  zcl_header: ZCL header
  * @retval ZCL status code
  */
  enum ZclStatusCodeT APP_ZIGBEE_OTA_Client_ImageNotify_cb(struct ZbZclClusterT *clusterPtr, uint8_t payload_type,
                                                                 uint8_t jitter, struct ZbZclOtaImageDefinition *image_definition,
                                                                 struct ZbApsdeDataIndT *data_ind, struct ZbZclHeaderT *zcl_header){
   bool checkFileType = false, checkFileVersion = false;
   int pos = -1;

   APP_DBG("[OTA] Image Notify request received.");

   /* Print message info according to Image Notify request payload type */
   switch(payload_type){
     case ZCL_OTA_NOTIFY_TYPE_FILE_VERSION:
       APP_DBG("[OTA] Available upgrade jitter: %d.", jitter);
       APP_DBG("[OTA] Available upgrade manufacturer code: %d.", image_definition->manufacturer_code);
       APP_DBG("[OTA] Available upgrade image type: 0x%04x.", image_definition->image_type);
       APP_DBG("[OTA] Available upgrade (new) file version: 0x%08x.", image_definition->file_version);

       if(image_definition->manufacturer_code != ST_ZIGBEE_MANUFACTURER_CODE){
         APP_DBG("[OTA] Unauthorized OTA upgrade manufacturer.\n");
         return ZCL_STATUS_FAILURE;
       }

       /* check image type and file version */
       checkFileType = true;
       checkFileVersion = true;
       break;

     default:
       APP_DBG("[OTA] Not supported payload type.\n");
       return ZCL_STATUS_FAILURE;
   }

   /* Check file type */
   if(checkFileType){
     pos = APP_ZIGBEE_FindImageType(image_definition->image_type);
     if(pos<0){
       /* Unknown file type */
       APP_DBG("[OTA] Unknown file type type.\n");
       return ZCL_STATUS_FAILURE;
     }
   }

   /* Check file version */
   if(checkFileVersion){
     if(OTA_currentFileVersionTab[pos].fileVersion == image_definition->file_version){
       APP_DBG("[OTA] Already up-to-date for this file type.\n");
       return ZCL_STATUS_FAILURE;
     }
   }

   APP_DBG("[OTA] Everything is OK, can process to the OTA upgrade.\n");

   /* Requesting an upgrade according to server image type
    * => re-send an Query Next Image request  back to the upgrade server */
   OTA_client_info.image_type = image_definition->image_type;
   OTA_client_info.current_file_version = OTA_currentFileVersionTab[pos].fileVersion;
   osThreadFlagsSet(OsTaskOTARequestUpgradeId,1);

   return ZCL_STATUS_SUCCESS;
 }

 /**
  * @brief  OTA client Query Next Image callback
  * @param  clusterPtr: ZCL Cluster pointer
  * @param  status: ZCL status code returned by the server for the requested image
  * @param  image_definition: Image query definition pointer
  * @param  image_size: OTA file format image total size
  * @param  arg: Passed arg
  * @retval ZCL status code
  */
  void APP_ZIGBEE_OTA_Client_QueryNextImage_cb(struct ZbZclClusterT *clusterPtr, enum ZclStatusCodeT status,
                                                     struct ZbZclOtaImageDefinition *image_definition, uint32_t image_size, void *arg){
   struct Zigbee_OTA_client_info* client_info = (struct Zigbee_OTA_client_info*) arg;

   APP_DBG("[OTA] Client Query Next Image request response received.");
   if(status != ZCL_STATUS_SUCCESS){
     APP_DBG("[OTA] A such image is not available.\n");
     return;
   } else {
     APP_DBG("[OTA] A such image is available.");
   }

   switch(image_definition->image_type){
     case fileType_COPRO_WIRELESS:
       client_info->ctx.base_address = FUOTA_COPRO_FW_BINARY_ADDRESS;
       client_info->ctx.magic_keyword = FUOTA_MAGIC_KEYWORD_COPRO_WIRELESS;
       break;

     case fileType_APP:
       client_info->ctx.base_address = FUOTA_APP_FW_BINARY_ADDRESS;
       client_info->ctx.magic_keyword = FUOTA_MAGIC_KEYWORD_APP;
       break;

     default:
       APP_DBG("[OTA] Error, unknown image type.\n");
       return;
   }
   client_info->requested_image_size = image_size;
   client_info->ctx.binary_srv_crc = 0;
   client_info->ctx.binary_calc_crc = 0;

   if(APP_ZIGBEE_CheckDeviceCapabilities() != APP_ZIGBEE_OK){
     APP_DBG("[OTA] Not enough space. No download.\n");
     return;
   }

   APP_DBG("[OTA] For image type 0x%04x, %d byte(s) will be downloaded.", image_definition->image_type, image_size);
   osThreadFlagsSet(OsTaskOTAStartDownloadId,1);
   APP_DBG("[OTA] Starting download.\n");
 }


 /**
  * @brief  OTA client Calc CRC for a payload
  * @param  data: payload to calc
  * @param  size: length of payload
  */
  void APP_ZIGBEE_OTA_Client_Crc_Calc( struct Zigbee_OTA_client_info * client_info ) {
   uint8_t     modulo;
   uint16_t    index, size;
   uint32_t *  crc_data;

   // -- Prepare pointer & size --
   size = client_info->write_info.firmware_buffer_current_offset;
   crc_data = (uint32_t *)client_info->write_info.firmware_buffer;
   modulo = size % 4u;
   if ( modulo != 0u )
   {
       memset( &crc_data[size], 0, ( 4u - modulo ) );
       size  += ( 4u - modulo );
   }

   for ( index = 0; index < ( size / 4u ); index++ )
   {
     client_info->ctx.binary_calc_crc ^= crc_data[index];
   }
 }


 /**
  * @brief  OTA client Write Image callback
  * @param  clusterPtr: ZCL Cluster pointer
  * @param  header: ZCL OTA file format image header
  * @param  length: received chunk length
  * @param  data: received chunk payload
  * @param  arg: Passed arg
  * @retval ZCL status code
  */
  enum ZclStatusCodeT APP_ZIGBEE_OTA_Client_WriteImage_cb(struct ZbZclClusterT *clusterPtr, struct ZbZclOtaHeader *header,
                                                                uint8_t length, uint8_t * data, void *arg){
   static  uint32_t current_offset = 0;

   struct Zigbee_OTA_client_info* client_info = (struct Zigbee_OTA_client_info*) arg;
   enum ZclStatusCodeT status = ZCL_STATUS_SUCCESS;
   uint8_t size = 0;
   uint8_t remaining_size = 0;
 #ifdef OTA_DISPLAY_TIMING
    uint32_t  lStartTime = 0;
   uint32_t  lStopTime, lTime1, lTime2;
 #endif // OTA_DISPLAY_TIMING
   current_offset += length;
   size = length;

   if(client_info->write_info.firmware_buffer_current_offset + size > RAM_FIRMWARE_BUFFER_SIZE){
     size = RAM_FIRMWARE_BUFFER_SIZE - client_info->write_info.firmware_buffer_current_offset;
     remaining_size = length - size;
     client_info->write_info.buffer_full = true;
   } else if(client_info->write_info.firmware_buffer_current_offset+size == RAM_FIRMWARE_BUFFER_SIZE){
     client_info->write_info.buffer_full = true;
   }

   memcpy((client_info->write_info.firmware_buffer+client_info->write_info.firmware_buffer_current_offset), data, size);
   client_info->write_info.firmware_buffer_current_offset += size;

   if(client_info->write_info.buffer_full){
 #ifdef OTA_DISPLAY_TIMING
     lStopTime = HAL_GetTick();
 #endif // OTA_DISPLAY_TIMING

     /* Write to Flash Memory */
     if ( APP_ZIGBEE_OTA_Client_WriteFirmwareData(client_info) != APP_ZIGBEE_OK ){
       return ZCL_STATUS_FAILURE;
     }

 #ifdef OTA_DISPLAY_TIMING
     /* Display Transfer Progress */
     lTime1 = lStopTime - lStartTime;
     lTime2 = ( HAL_GetTick() - lStopTime );
     APP_DBG("[OTA] FUOTA Transfer (current_offset = 0x%04X, load time = %d ms and save time = %d ms)", current_offset, lTime1, lTime2);
 #else // OTA_DISPLAY_TIMING
     APP_DBG("[OTA] FUOTA Transfer (current_offset = 0x%04X)", current_offset);
 #endif // OTA_DISPLAY_TIMING

     // -- Calc CRC --
     APP_ZIGBEE_OTA_Client_Crc_Calc( client_info );

     memset(client_info->write_info.firmware_buffer, 0, RAM_FIRMWARE_BUFFER_SIZE);
     memcpy(client_info->write_info.firmware_buffer, data+size, remaining_size);
     client_info->write_info.firmware_buffer_current_offset = remaining_size;
     client_info->write_info.buffer_full = false;

 #ifdef OTA_DISPLAY_TIMING
     lStartTime = HAL_GetTick();
 #endif // OTA_DISPLAY_TIMING
   }

   return status;
 }


 ///**
 // * @brief  OTA client WriteTag callback
 // * @param  clusterPtr: ZCL Cluster pointer
 // * @param  header: ZCL OTA file format image header
 // * @param  tag_id: Tag identifier
 // * @param  tag_length : length of Tag
 // * @param  data_length  : length of Tag data
 // * @param  data  : Tag data
 // * @param  arg: Passed arg
 // * @retval ZCL status code
 // */
 // enum ZclStatusCodeT APP_ZIGBEE_OTA_Client_WriteTag_cb(struct ZbZclClusterT * clusterPtr, struct ZbZclOtaHeader * header,
 //                                                             uint16_t tag_id, uint32_t tag_length, uint8_t data_length,
 //                                                             uint8_t * data, void * arg)
 //{
 //    struct Zigbee_OTA_client_info* client_info = (struct Zigbee_OTA_client_info*) arg;
 //    enum ZclStatusCodeT status = ZCL_STATUS_SUCCESS;
 //    UNUSED(tag_length);
 //
 //    switch (tag_id)
 //    {
 //        case ZCL_OTA_SUB_TAG_UPGRADE_IMAGE:
 //            APP_DBG("[OTA] Writing blocks. \n");
 //            status = APP_ZIGBEE_OTA_Client_WriteImage_cb(clusterPtr, header, data_length, data, arg);
 //            break;
 //
 //        case ZCL_OTA_SUB_TAG_IMAGE_INTEGRITY_CODE:
 //            APP_DBG("[OTA] Get check crc. \n");
 //            client_info->ctx.binary_srv_crc = (uint32_t)data[0] | ( (uint16_t)data[1] << 8 );
 //            if ( data_length == 4u )
 //            {
 //              client_info->ctx.binary_srv_crc |= ( (uint32_t)data[2] << 16u ) | ( (uint32_t)data[3] << 24u );
 //            }
 //            break;
 //
 //        default:
 //            status = ZCL_STATUS_INVALID_FIELD;
 //            break;
 //    }
 //    return status;
 //}


 /**
  * @brief  OTA client Image Validate callback
  * @param  clusterPtr: ZCL Cluster pointer
  * @param  header: ZCL OTA file format image header
  * @param  arg: Passed arg
  * @retval ZCL status code
  */
  enum ZclStatusCodeT APP_ZIGBEE_OTA_Client_ImageValidate_cb(struct ZbZclClusterT *clusterPtr, struct ZbZclOtaHeader *header, void *arg){
   struct Zigbee_OTA_client_info* client_info = (struct Zigbee_OTA_client_info*) arg;
   enum ZclStatusCodeT status = ZCL_STATUS_SUCCESS;
   uint64_t last_double_word = 0;
   double l_transfer_throughput = 0;
   uint32_t lTransfertThroughputInt, lTransfertThroughputDec;

   /* Download finished => stop Toggling of the LED */
   BSP_LED_Off(LED_GREEN);
   APP_DBG("LED_GREEN OFF");

   /* Write the last RAM buffer to Flash */
   if(client_info->write_info.firmware_buffer_current_offset != 0){
     /* Write to Flash Memory */
     APP_ZIGBEE_OTA_Client_WriteFirmwareData(client_info);
     client_info->write_info.firmware_buffer_current_offset = 0;
   }

   APP_DBG("**************************************************************\n");
   APP_DBG("[OTA] Validating the image.");

   /* Finding the magic number */

   /* Last double word in Flash
    * => the magic if the firmware is valid
    */
   client_info->write_info.flash_current_offset -= 8;
   memcpy(&last_double_word, (void const*)(client_info->ctx.base_address + client_info->write_info.flash_current_offset), 8);
   if(((last_double_word & 0x00000000FFFFFFFF) != client_info->ctx.magic_keyword)
      && (((last_double_word & 0xFFFFFFFF00000000) >> 32) != client_info->ctx.magic_keyword)){
     APP_DBG("[OTA] Wrong magic keyword: invalid firmware.\n");
     status = ZCL_STATUS_INVALID_IMAGE;
     return status;
   }

   APP_DBG("[OTA] The downloaded firmware is valid.\n");
   client_info->download_time = (HAL_GetTick()- client_info->download_time)/1000;
   l_transfer_throughput = (((double)client_info->requested_image_size/client_info->download_time) / 1000) * 8;
   lTransfertThroughputInt = (uint32_t)l_transfer_throughput;
   lTransfertThroughputDec = (uint32_t)( ( l_transfer_throughput - lTransfertThroughputInt ) * 100 );

   APP_DBG("**************************************************************");
   APP_DBG(" FUOTA : END OF TRANSFER COMPLETED");

   if(client_info->image_type == IMAGE_TYPE_FW_COPRO_WIRELESS){
     APP_DBG("  - M0 wireless coprocessor firmware.");
   } else {
     APP_DBG("  - M4 application firmware.");
   }

   APP_DBG("  - %d bytes downloaded in %d seconds.",  client_info->requested_image_size, client_info->download_time);
   APP_DBG("  - Average throughput = %d.%d kbit/s.", lTransfertThroughputInt, lTransfertThroughputDec );
   APP_DBG("**************************************************************");

   BSP_LED_On(LED_GREEN);
   APP_DBG("LED_GREEN ON");

   return status;
 }

 /**
  * @brief  OTA client Reboot callback
  * @param  clusterPtr: ZCL Cluster pointer
  * @param  arg: Passed arg
  * @retval None
  */
  void APP_ZIGBEE_OTA_Client_Reboot_cb(struct ZbZclClusterT *clusterPtr, void *arg){
   UNUSED(arg);

   APP_DBG("**************************************************************");
   APP_DBG("[OTA] Rebooting.");

   APP_DBG("LED_BLUE OFF");
   BSP_LED_Off(LED_BLUE);
   APP_DBG("LED_GREEN OFF");
   BSP_LED_Off(LED_GREEN);
   APP_DBG("**************************************************************\n");

   HAL_Delay(100);
   osThreadFlagsSet(OsTaskPerformResetId,1);
 }

 /**
  * @brief  OTA client Abort callback
  * @param  clusterPtr: ZCL Cluster pointer
  * @param  commandId: ZCL OTA command ID associated to the block transfer abortion
  * @param  arg: Passed arg
  * @retval ZCL status code
  */
  enum ZclStatusCodeT APP_ZIGBEE_OTA_Client_AbortDownload_cb(struct ZbZclClusterT *clusterPtr, enum ZbZclOtaCommandId commandId, void *arg){
   UNUSED(arg);

   APP_DBG("[OTA] Aborting download.");
   BSP_LED_Off(LED_GREEN);
   BSP_LED_On(LED_RED);

   return ZCL_STATUS_ABORT;
 }

 /**
  * @brief  OTA client request upgrade
  * @param  None
  * @retval None
  */
  void APP_ZIGBEE_Process_OTA_Client_Request_Upgrade(void *argument){
 UNUSED(argument);

 for(;;)
 {
   osThreadFlagsWait(1,osFlagsWaitAll,osWaitForever);
   APP_ZIGBEE_OTA_Client_Request_Upgrade();
 }
 }


  inline void APP_ZIGBEE_OTA_Client_Request_Upgrade(void){
   enum ZclStatusCodeT status;
   struct ZbZclOtaImageDefinition image_definition;

   APP_DBG("[OTA] Requesting an update.");
   APP_DBG("[OTA] Image type: 0x%04x.", OTA_client_info.image_type);
   APP_DBG("[OTA] Current file version: 0x%08x.\n", OTA_client_info.current_file_version);

   /* Image definition configuration */
   memset(&image_definition, 0, sizeof(image_definition));
   image_definition.manufacturer_code = ST_ZIGBEE_MANUFACTURER_CODE;
   image_definition.image_type = OTA_client_info.image_type;
   image_definition.file_version = OTA_client_info.current_file_version;

   /* Sending Discovery request to server */
   APP_DBG("[OTA] Sending Query Next Image request.");
   /* HW version is provided as additional info in Query Next Image request */
   status = ZbZclOtaClientQueryNextImageReq(zigbee_app_info.ota_client, &image_definition,
                                            ZCL_OTA_QUERY_FIELD_CONTROL_HW_VERSION, CURRENT_HARDWARE_VERSION);
   if(status != ZCL_STATUS_SUCCESS){
     APP_DBG("ZbZclOtaClientDiscover failed.\n");
   }
 }

 /**
  * @brief  OTA client start download
  * @param  None
  * @retval None
  */

  void APP_ZIGBEE_Process_OTA_Client_StartDownload(void *argument){
 UNUSED(argument);

 for(;;)
 {
   osThreadFlagsWait(1,osFlagsWaitAll,osWaitForever);
   APP_ZIGBEE_OTA_Client_StartDownload();
 }
 }

  inline void APP_ZIGBEE_OTA_Client_StartDownload(void){
   OTA_client_info.download_time = HAL_GetTick();
   ZbZclOtaClientImageTransferStart(zigbee_app_info.ota_client);
 }




  void APP_ZIGBEE_Process_OTA_Client_ServerDiscovery(void *argument){
 UNUSED(argument);

 for(;;)
 {
   osThreadFlagsWait(1,osFlagsWaitAll,osWaitForever);
   APP_ZIGBEE_OTA_Client_ServerDiscovery();
 }
 }


  void APP_ZIGBEE_OTA_Client_ServerDiscovery( void )
 {
   enum ZclStatusCodeT   status;
   struct ZbApsAddrT     dst;

   /* Destination address configuration */
   memset(&dst, 0, sizeof(dst));
   dst.mode = ZB_APSDE_ADDRMODE_SHORT;
   dst.endpoint = APP_OTA_ENDPOINT;
   dst.nwkAddr = 0x0;

   status = ZbZclOtaClientDiscover(zigbee_app_info.ota_client, &dst);
   if(status != ZCL_STATUS_SUCCESS)
   {
     APP_DBG("ZbZclOtaClientDiscover failed.\n");
   }
 }


 /**
  * @brief  OTA client initialization
  * @param  None
  * @retval None
  */
  void APP_ZIGBEE_OTA_Client_Init(void)
 {
   uint16_t  iShortAddress;

   /* Client info fields set to 0 */
   memset(&OTA_client_info, 0, sizeof(OTA_client_info));

   APP_DBG("Searching for OTA server.");
   BSP_LED_On(LED_GREEN);

   /* Requesting a discovery for any available OTA server */
   osThreadFlagsSet(OsTaskOTAServerDiscoveryId,1);

   /* Wait a Discovery */
   osSemaphoreAcquire(OtaServerFoundSemaphore, osWaitForever);
   BSP_LED_Off(LED_GREEN);

   /**
    * This is a safe clear in case the engi bytes are not all written
    * The error flag should be cleared before moving forward
    */
   __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR);

   APP_DBG("Delete_Sectors");
   Delete_Sectors();

   iShortAddress = ZbShortAddress( zigbee_app_info.zb );
   APP_DBG("OTA Client with Short Address 0x%04X.", iShortAddress );
   APP_DBG("OTA Client init done!\n");

 } /* APP_ZIGBEE_OTA_Client_Init */


 /**
  * @brief  OTA Server find image type helper
  * @param  fileType: requested file type
  * @retval index value
  */
  inline int APP_ZIGBEE_FindImageType(unsigned int fileType)
 {
     for(unsigned int i=0; i<(sizeof(OTA_currentFileVersionTab)/sizeof(*OTA_currentFileVersionTab)); i++)
     {
         if(OTA_currentFileVersionTab[i].fileType == fileType){ return i; }
     }
     return -1;
 }

 /**
  * @brief  OTA client writing firmware data from internal RAM cache to flash
  * @param  client_info: OTA client internal structure
  * @retval Application status code
  */
  inline APP_ZIGBEE_StatusTypeDef APP_ZIGBEE_OTA_Client_WriteFirmwareData(struct Zigbee_OTA_client_info* client_info){
   uint64_t l_read64 = 0;

   /* Write to Flash Memory */
   for(unsigned int flash_index = 0; flash_index < client_info->write_info.firmware_buffer_current_offset; flash_index+=8){
     while( LL_HSEM_1StepLock( HSEM, CFG_HW_FLASH_SEMID ) );
     HAL_FLASH_Unlock();
     while(LL_FLASH_IsActiveFlag_OperationSuspended());

     memcpy(&l_read64, &(client_info->write_info.firmware_buffer[flash_index]), 8);
     if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
         client_info->ctx.base_address + client_info->write_info.flash_current_offset,
         l_read64) == HAL_OK)
     {
       /* Read back value for verification */
       l_read64 = 0;
       l_read64 = *(uint64_t*)(client_info->ctx.base_address + client_info->write_info.flash_current_offset);
       if(l_read64 != (*(uint64_t*)(client_info->write_info.firmware_buffer+flash_index)))
       {
         APP_DBG("FLASH: Comparison failed l_read64 = 0x%jx / ram_array = 0x%jx",
                 l_read64, client_info->write_info.firmware_buffer[flash_index]);
         return APP_ZIGBEE_ERROR;
       }
     }
     else
     {
       APP_DBG("HAL_FLASH_Program FAILED at flash_index = %d", flash_index);
       return APP_ZIGBEE_ERROR;
     }

     HAL_FLASH_Lock();
     LL_HSEM_ReleaseLock( HSEM, CFG_HW_FLASH_SEMID, 0 );

     client_info->write_info.flash_current_offset += 8;
   }

   return APP_ZIGBEE_OK;
 }

 /**
  * @brief  Getting flash first secure sector helper
  * @param  None
  * @retval First flash secure sector
  */
  inline uint32_t GetFirstSecureSector(void)
 {
   uint32_t first_secure_sector_idx, sfsa_field, sbrv_field, sbrv_field_sector;

   /* Read SFSA */
   sfsa_field = (READ_BIT(FLASH->SFR, FLASH_SFR_SFSA) >> FLASH_SFR_SFSA_Pos);
   APP_DBG("SFSA OB = 0x%x", sfsa_field);
   APP_DBG("SFSA Option Bytes set to sector = 0x%x (0x080%x)", sfsa_field, sfsa_field*4096);

   /* Read SBRV */
   /* Contains the word aligned CPU2 boot reset start address offset within the selected memory area by C2OPT. */
   sbrv_field = (READ_BIT(FLASH->SRRVR, FLASH_SRRVR_SBRV) >> FLASH_SRRVR_SBRV_Pos);
   APP_DBG("SBRV OB = 0x%x", sbrv_field);
   /* Divide sbrv_field by 1024 to be compared to SFSA value */
   sbrv_field_sector = sbrv_field / 1024;
   APP_DBG("SBRV Option Bytes set to sector = 0x%x (0x080%x)", sbrv_field_sector, sbrv_field*4);

   /* If SBRV is below SFSA then set first_secure_sector_idx to SBRV */
   if (sbrv_field_sector < sfsa_field)
   {
     first_secure_sector_idx = sbrv_field_sector;
   }
   else
   {
     first_secure_sector_idx = sfsa_field;
   }

   APP_DBG("first_secure_sector_idx = 0x%x", first_secure_sector_idx);

   return first_secure_sector_idx;
 }

 /**
  * @brief  Deleting non secure sectors helper
  * @param  None
  * @retval None
  */
 static inline void Delete_Sectors( void )
 {
   /**
    * The number of sectors to erase is read from SRAM1.
    * It shall be checked whether the number of sectors to erase does not overlap on the secured Flash
    * The limit can be read from the SFSA option byte which provides the first secured sector address.
    */

   uint32_t page_error;
   FLASH_EraseInitTypeDef p_erase_init;
   uint32_t first_secure_sector_idx;

   first_secure_sector_idx = GetFirstSecureSector();

   p_erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
   p_erase_init.Page = *((uint8_t*) SRAM1_BASE + 1);
   if(p_erase_init.Page < (CFG_APP_START_SECTOR_INDEX - 1))
   {
     /**
      * Something has been wrong as there is no case we should delete the OTA application
      * Reboot on the firmware application
      */
     *(uint8_t*)SRAM1_BASE = CFG_REBOOT_ON_DOWNLOADED_FW;
     NVIC_SystemReset();
   }
   p_erase_init.NbPages = *((uint8_t*) SRAM1_BASE + 2);

   if ((p_erase_init.Page + p_erase_init.NbPages) > first_secure_sector_idx)
   {
     p_erase_init.NbPages = first_secure_sector_idx - p_erase_init.Page;
   }

   APP_DBG("Erase FLASH Memory from sector %d (0x080%x) to sector %d (0x080%x)", p_erase_init.Page, p_erase_init.Page*4096, p_erase_init.NbPages+p_erase_init.Page, (p_erase_init.NbPages+p_erase_init.Page)*4096);

   HAL_FLASH_Unlock();

   HAL_FLASHEx_Erase(&p_erase_init, &page_error);

   HAL_FLASH_Lock();

   return;
 }

 /**
  * @brief  Getting available internal flash space size
  * @param  None
  * @retval Application status code
  */
  static inline APP_ZIGBEE_StatusTypeDef APP_ZIGBEE_CheckDeviceCapabilities(void)
 {
   APP_ZIGBEE_StatusTypeDef status = APP_ZIGBEE_OK;

   /* Get Flash memory size available to copy binary from Server */
   uint32_t first_sector_idx;
   uint32_t first_secure_sector_idx;
   uint32_t free_sectors;
   uint32_t free_size;

   APP_DBG("Check Device capabilities");

   first_secure_sector_idx = GetFirstSecureSector();

   first_sector_idx = *((uint8_t*) SRAM1_BASE + 1);
   if (first_sector_idx == 0)
   {
     APP_DBG("ERROR : SRAM1_BASE + 1 == 0");
     first_sector_idx = CFG_APP_START_SECTOR_INDEX;
   }
   APP_DBG("First available sector = %d (0x080%x)", first_sector_idx, first_sector_idx*4096);

   free_sectors = first_secure_sector_idx - first_sector_idx;
   free_size = free_sectors*4096;

   APP_DBG("free_sectors = %d , -> %d bytes of FLASH Free", free_sectors, free_size);

   APP_DBG("Server requests    : %d bytes", OTA_client_info.requested_image_size);
   APP_DBG("Client Free memory : %d bytes", free_size);

   if (free_size < OTA_client_info.requested_image_size)
   {
     status = APP_ZIGBEE_ERROR;
     APP_DBG("WARNING: Not enough Free Flash Memory available to download binary from Server!");
   }
   else
   {
     APP_DBG("Device contains enough Flash Memory to download binary");
   }

   return status;
 }

 /**
  * @brief Task responsible for the reset at the end of OTA transfer.
  * @param  None
  * @retval None
  */
  void APP_ZIGBEE_Process_PerformReset(void *argument){
 UNUSED(argument);

 for(;;)
 {
   osThreadFlagsWait(1,osFlagsWaitAll,osWaitForever);
   APP_ZIGBEE_PerformReset();
 }
 }

  void APP_ZIGBEE_PerformReset(void)
 {
   APP_DBG("*******************************************************");
   APP_DBG(" FUOTA_CLIENT : END OF TRANSFER COMPLETED");

   if (OTA_client_info.image_type == fileType_APP)
   {
     APP_DBG("  --> Request to reboot on FW Application");
     APP_DBG("*******************************************************");

     /* Reboot on Downloaded FW Application */
     *(uint8_t*)SRAM1_BASE = CFG_REBOOT_ON_DOWNLOADED_FW;

     HAL_Delay(100);
     NVIC_SystemReset();
   }
   else
   {
     if (OTA_client_info.image_type == fileType_COPRO_WIRELESS)
     {
       APP_DBG("  --> Request to reboot on FUS");
       APP_DBG("*******************************************************");
       HAL_Delay(100);

       /**
        * Wireless firmware update is requested
        * Request CPU2 to reboot on FUS by sending two FUS command
        */
       SHCI_C2_FUS_GetState( NULL );
       SHCI_C2_FUS_GetState( NULL );
       while(1)
       {
         HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
       }
     }
     else
     {
       APP_DBG("APP_ZIGBEE_PerformReset: OtaContext.file_type not recognized");
       return;
     }
   }
 }


