
/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    App/app_zigbee.c
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
#include "zcl/general/zcl.onoff.h"
#include "zcl/general/zcl.identify.h"

/* USER CODE BEGIN Includes */
#include "ee.h"
#include "hw_flash.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private defines -----------------------------------------------------------*/
/*Defines for application configuration, many other important configuration
 * related to zigbee application are in app_conf.h and app_zigbee.h 		  */

/*Set one of the desired configuration defines to 1.*/
#define APP_ZIGBEE_LIGHTSWITCH_CONFIG                0
#define APP_ZIGBEE_LIGHTBULB_CONFIG                  0
#define APP_ZIGBEE_THERMOSTAT_CONFIG	             1

/*You can select available additional functionalities as persistent data management and OTA client */
#define APP_ZIGBEE_NVM_SUPPORT                       1  /* Persistence data management */
#define APP_ZIGBEE_OTA_SUPPORT                       0	/* OTA Client availability */


/*Typically in case of using touchlink commissioning lightswitchs are are working as touchlik initiator and
 * lightbulbs as touchlink targets  */
#if APP_ZIGBEE_LIGHTSWITCH_CONFIG==1
#define APP_ZIGBEE_TOUCHLINK_TARGET                  0  /*0- for initiator 1- for target*/
#else
#define APP_ZIGBEE_TOUCHLINK_TARGET                  1  /*0- for initiator 1- for target*/
#endif



#define APP_ZIGBEE_STARTUP_FAIL_DELAY               500U

#define APP_ZIGBEE_TOUCHLINK_ENDPOINT              200
#define APP_ZIGBEE_TOUCHLINK_MIN_RSSI              -60

/* A subset of WPAN_CHANNELMASK_2400MHZ_HA (HA preferred channels) */
#define APP_ZIGBEE_CHANNELMASK_2400MHZ_TCLK_PRIMARY  0x02108800 /* Channels 11, 15, 20, 25*/

/*For debug we isolate only 1 channel */
#define APP_ZIGBEE_DEBUG_TOUCHLINK                   0
#if (APP_ZIGBEE_DEBUG_TOUCHLINK == 1)
#define APP_ZIGBEE_TOUCHLINK_CHANNEL                15u
#define APP_ZIGBEE_TOUCHLINK_CHANNEL_MASK           ( 1u << APP_ZB_TOUCHLINK_CHANNEL )
#else
#define APP_ZIGBEE_TOUCHLINK_CHANNEL_MASK           APP_ZIGBEE_CHANNELMASK_2400MHZ_TCLK_PRIMARY
#endif
/* USER CODE BEGIN PD */
#define SW1_GROUP_ADDR                              0x0001
#define APP_ZIGBEE_NETWORK_JOIN_MAX_RETRY           10U
/* USER CODE END PD */

/* Private macros ------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* External definition -------------------------------------------------------*/
enum ZbStatusCodeT ZbStartupWait(struct ZigBeeT *zb, struct ZbStartupT *config);

/* USER CODE BEGIN ED */
/* USER CODE END ED */

/* Private function prototypes -----------------------------------------------*/
static void APP_ZIGBEE_StackLayersInit(void);
static void APP_ZIGBEE_ConfigEndpoints(void);
static void APP_ZIGBEE_NwkForm(void);

static void APP_ZIGBEE_TraceError(const char *pMess, uint32_t ErrCode);
static void APP_ZIGBEE_CheckWirelessFirmwareInfo(void);

static void Wait_Getting_Ack_From_M0(void);
static void Receive_Ack_From_M0(void);
static void Receive_Notification_From_M0(void);

static void APP_ZIGBEE_ProcessNotifyM0ToM4(void * argument);
static void APP_ZIGBEE_ProcessRequestM0ToM4(void * argument);
static void APP_ZIGBEE_ProcessNwkForm(void * argument);

/* USER CODE BEGIN PFP */
static void APP_ZIGBEE_ConfigGroupAddr(void);
static enum ZbStatusCodeT APP_ZIGBEE_ZbStartupPersist(struct ZigBeeT *zb);
static void APP_ZIGBEE_persist_notify_cb(struct ZigBeeT *zb, void *cbarg);
static void APP_ZIGBEE_PersistCompleted_callback(enum ZbStatusCodeT status,void *arg);
static bool APP_ZIGBEE_persist_save(void);
static bool APP_ZIGBEE_persist_load(void);
static enum ZclStatusCodeT APP_ZIGBEE_RestoreClusterAttr(struct ZbZclClusterT *clusterPtr);
#if APP_ZIGBEE_NVM_SUPPORT==1
static void APP_ZIGBEE_NVM_Init(void);
static bool APP_ZIGBEE_NVM_Read(void);
static bool APP_ZIGBEE_NVM_Write(void);
static void APP_ZIGBEE_NVM_Erase(void);
static void APP_ZIGBEE_NVM_FactoryReset(void);
#endif /* APP_ZIGBEE_NVM_SUPPORT*/

static void APP_ZIGBEE_ExitTouchlinkTargetCommissioning(void);

/*Push button related functions*/
static void APP_ZIGBEE_ProcessPushButton(void *argument);
void APP_ZIGBEE_LaunchPushButtonTask(APP_ZIGBEE_PUSH_BUTTON_TaskTypeDef taskFlag);
static void APP_ZIGBEE_SW1_Process(void);
static void APP_ZIGBEE_SW2_Process(void);
static void APP_ZIGBEE_SW3_ShortPress_Process(void);
static void APP_ZIGBEE_SW3_LongPress_Process(void);

#if APP_ZIGBEE_OTA_SUPPORT==1
/* OTA application related functions */
extern void APP_ZIGBEE_OTA_Client_Init(void);
extern void APP_ZIGBEE_OTA_ConfigEndpoints(void);
extern void APP_ZIGBEE_OTA_ThreadsInit(void);
#endif /*APP_ZIGBEE_OTA_SUPPORT==1*/

#if APP_ZIGBEE_LIGHTBULB_CONFIG==1
/* Lightbulb application related functions */
extern void APP_ZIGBEE_LightBulb_ConfigEndpoints(void);
#endif /*APP_ZIGBEE_LIGHTBULB_CONFIG==1*/

#if APP_ZIGBEE_THERMOSTAT_CONFIG==1
/* Thermostat application related functions */
extern void APP_ZIGBEE_Thermostat_ConfigEndpoints(void);
#endif /*APP_ZIGBEE_THERMOSTAT_CONFIG*/

#if APP_ZIGBEE_LIGHTSWITCH_CONFIG==1
/*Lightswitch application related functions*/
static void APP_ZIGBEE_Lightswitch_Toggle(void);
#endif /*APP_ZIGBEE_LIGHTSWITCH==1*/
/* USER CODE END PFP */

/* Private variables ---------------------------------------------------------*/
static TL_CmdPacket_t   *p_ZIGBEE_otcmdbuffer;
static TL_EvtPacket_t   *p_ZIGBEE_notif_M0_to_M4;
static TL_EvtPacket_t   *p_ZIGBEE_request_M0_to_M4;
static __IO uint32_t    CptReceiveNotifyFromM0 = 0;
static __IO uint32_t    CptReceiveRequestFromM0 = 0;

static osThreadId_t   OsTaskNotifyM0ToM4Id;
static osThreadId_t   OsTaskRequestM0ToM4Id;
static osThreadId_t   OsTaskNwkFormId;
static osThreadId_t   OsTaskPushButtonId;
static osMutexId_t    MtxZigbeeId;

osSemaphoreId_t       TransferToM0Semaphore;
osSemaphoreId_t       StartupEndSemaphore;

PLACE_IN_SECTION("MB_MEM1") ALIGN(4) static TL_ZIGBEE_Config_t ZigbeeConfigBuffer;
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static TL_CmdPacket_t ZigbeeOtCmdBuffer;
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static uint8_t ZigbeeNotifRspEvtBuffer[sizeof(TL_PacketHeader_t) + TL_EVT_HDR_SIZE + 255U];
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static uint8_t ZigbeeNotifRequestBuffer[sizeof(TL_PacketHeader_t) + TL_EVT_HDR_SIZE + 255U];

struct zigbee_app_info zigbee_app_info;

/* FreeRtos stacks attributes */
const osThreadAttr_t TaskNotifyM0ToM4_attr =
{
    .name = CFG_TASK_NOTIFY_M0_TO_M4_PROCESS_NAME,
    .attr_bits = CFG_TASK_PROCESS_ATTR_BITS,
    .cb_mem = CFG_TASK_PROCESS_CB_MEM,
    .cb_size = CFG_TASK_PROCESS_CB_SIZE,
    .stack_mem = CFG_TASK_PROCESS_STACK_MEM,
    .priority = osPriorityAboveNormal,
    .stack_size = CFG_TASK_PROCESS_STACK_SIZE
};

const osThreadAttr_t TaskRequestM0ToM4_attr =
{
    .name = CFG_TASK_REQUEST_M0_TO_M4_PROCESS_NAME,
    .attr_bits = CFG_TASK_PROCESS_ATTR_BITS,
    .cb_mem = CFG_TASK_PROCESS_CB_MEM,
    .cb_size = CFG_TASK_PROCESS_CB_SIZE,
    .stack_mem = CFG_TASK_PROCESS_STACK_MEM,
    .priority = osPriorityAboveNormal,
    .stack_size = CFG_TASK_PROCESS_STACK_SIZE
};

const osThreadAttr_t TaskNwkForm_attr =
{
    .name = CFG_TASK_NWK_FORM_PROCESS_NAME,
    .attr_bits = CFG_TASK_PROCESS_ATTR_BITS,
    .cb_mem = CFG_TASK_PROCESS_CB_MEM,
    .cb_size = CFG_TASK_PROCESS_CB_SIZE,
    .stack_mem = CFG_TASK_PROCESS_STACK_MEM,
    .priority = osPriorityBelowNormal,
    .stack_size = CFG_TASK_PROCESS_STACK_SIZE
};

const osThreadAttr_t TaskPushButton_attr =
{
    .name = CFG_TASK_PUSH_BUTTON_PROCESS_NAME,
    .attr_bits = CFG_TASK_PROCESS_ATTR_BITS,
    .cb_mem = CFG_TASK_PROCESS_CB_MEM,
    .cb_size = CFG_TASK_PROCESS_CB_SIZE,
    .stack_mem = CFG_TASK_PROCESS_STACK_MEM,
    .priority = osPriorityAboveNormal,
    .stack_size = CFG_TASK_PROCESS_STACK_SIZE
};

/* USER CODE BEGIN PV */
/* ZLL Master Key*/
/* You need to be part of the CSA lighting group to obtain the ZLL master key */
/* Once you have the appropriate key you can past it here*/
const uint8_t sec_key_touchlink_master[ZB_SEC_KEYSIZE] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};



/* NVM variables */
/* cache in uninit RAM to store/retrieve persistent data */
union cache
{
  uint8_t  U8_data[ST_PERSIST_MAX_ALLOC_SZ];     // in bytes
  uint32_t U32_data[ST_PERSIST_MAX_ALLOC_SZ/4U]; // in U32 words
};
__attribute__ ((section(".noinit"))) union cache cache_persistent_data;

__attribute__ ((section(".noinit"))) union cache cache_diag_reference;

/* USER CODE END PV */
/* Functions Definition ------------------------------------------------------*/

/**
 * @brief  Zigbee application initialization
 * @param  None
 * @retval None
 */
void APP_ZIGBEE_Init(void)
{
  SHCI_CmdStatus_t ZigbeeInitStatus;

  APP_DBG("APP_ZIGBEE_Init");

  /* Check the compatibility with the Coprocessor Wireless Firmware loaded */
  APP_ZIGBEE_CheckWirelessFirmwareInfo();

  /* Register cmdbuffer */
  APP_ZIGBEE_RegisterCmdBuffer(&ZigbeeOtCmdBuffer);

  /* Init config buffer and call TL_ZIGBEE_Init */
  APP_ZIGBEE_TL_INIT();

  /* NVM Init */
#if APP_ZIGBEE_NVM_SUPPORT==1
  APP_ZIGBEE_NVM_Init();
#endif /*APP_ZIGBEE_NVM_SUPPORT*/

  /* Initialize the mutex */
  MtxZigbeeId = osMutexNew( NULL );

  /* Initialize the semaphores */
  StartupEndSemaphore = osSemaphoreNew( 1, 0, NULL ); /*< Create the semaphore and make it busy at initialization */
  TransferToM0Semaphore = osSemaphoreNew( 1, 0, NULL );

  /* Register task */
  /* Create the different tasks */
  OsTaskNotifyM0ToM4Id = osThreadNew(APP_ZIGBEE_ProcessNotifyM0ToM4, NULL,&TaskNotifyM0ToM4_attr);
  OsTaskRequestM0ToM4Id = osThreadNew(APP_ZIGBEE_ProcessRequestM0ToM4, NULL,&TaskRequestM0ToM4_attr);

  /* Task associated with push button SW1 */
  OsTaskPushButtonId = osThreadNew(APP_ZIGBEE_ProcessPushButton, NULL,&TaskPushButton_attr);

  /* Task associated with network creation process */
  OsTaskNwkFormId = osThreadNew(APP_ZIGBEE_ProcessNwkForm, NULL,&TaskNwkForm_attr);

  /* USER CODE BEGIN APP_ZIGBEE_INIT */
#if APP_ZIGBEE_OTA_SUPPORT==1
  APP_ZIGBEE_OTA_ThreadsInit();
#endif /*APP_ZIGBEE_OTA_SUPPORT==1*/
  /* USER CODE END APP_ZIGBEE_INIT */
  
  /* Start the Zigbee on the CPU2 side */
  ZigbeeInitStatus = SHCI_C2_ZIGBEE_Init();
  /* Prevent unused argument(s) compilation warning */
  UNUSED(ZigbeeInitStatus);

  /* Initialize Zigbee stack layers */
  APP_ZIGBEE_StackLayersInit();

}

/**
 * @brief  Initialize Zigbee stack layers
 * @param  None
 * @retval None
 */
static void APP_ZIGBEE_StackLayersInit(void)
{
  APP_DBG("APP_ZIGBEE_StackLayersInit");
  enum ZbStatusCodeT status = ZB_STATUS_SUCCESS;

  zigbee_app_info.zb = ZbInit(0U, NULL, NULL);
  assert(zigbee_app_info.zb != NULL);

  /* Stack config */
  //VST_COMMENT: ingoring LQI is just a workaround for demo to improve the performance in noisy environment
  /* Enhance range by ignoring LQI cost during join */
  uint32_t val32 = ZB_BDB_FLAG_IGNORE_COST_DURING_JOIN;
  status = ZbBdbSet(zigbee_app_info.zb, ZB_BDB_Flags, &val32, sizeof(val32));
  UNUSED(status);
  /* Create the endpoint and cluster(s) */
  APP_ZIGBEE_ConfigEndpoints();

  /* USER CODE BEGIN APP_ZIGBEE_StackLayersInit */
  BSP_LED_Off(LED_RED);
  BSP_LED_Off(LED_GREEN);
  BSP_LED_Off(LED_BLUE);
  /* USER CODE END APP_ZIGBEE_StackLayersInit */

  /* Configure the joining parameters */
  zigbee_app_info.join_status = (enum ZbStatusCodeT) 0x01; /* init to error status */
  zigbee_app_info.join_delay = HAL_GetTick(); /* now */
  zigbee_app_info.startupControl = ZbStartTypeJoin; //ZbStartTypeForm;

  /* Initialization Complete */
  zigbee_app_info.has_init = true;
  /* Define if we need to do a fresh start */
  zigbee_app_info.fresh_startup = true;

#if APP_ZIGBEE_NVM_SUPPORT==1
  /* First we disable the persistent notification */
  ZbPersistNotifyRegister(zigbee_app_info.zb,NULL,NULL);

  /* Call a startup from persistence */
  status = APP_ZIGBEE_ZbStartupPersist(zigbee_app_info.zb);
  if(status == ZB_STATUS_SUCCESS)
  {
    /* No fresh startup needed anymore */
    zigbee_app_info.fresh_startup = false;
    APP_DBG("ZbStartupPersist: SUCCESS, restarted from persistence");
    ZbStartupRejoin(zigbee_app_info.zb, NULL, NULL);
    BSP_LED_On(LED_BLUE);
  }
  else
  {
    /* Start-up form persistence failed, perform a fresh ZbStartup */
    APP_DBG("ZbStartupPersist: FAILED to restart from persistence with status: 0x%02x",status);
  }
#endif /*APP_ZIGBEE_NVM_SUPPORT==1*/

  if(zigbee_app_info.fresh_startup)
  {
    /* Go for fresh start */
#if APP_ZIGBEE_TOUCHLINK_TARGET==1
	zigbee_app_info.startupControl=ZbStartTypeTouchlink;
    osThreadFlagsSet(OsTaskNwkFormId,1);
#endif /*APP_ZIGBEE_TOUCHLINK_TARGET==1*/

  }
}

/**
 * @brief  Configure Zigbee application endpoints
 * @param  None
 * @retval None
 */
static void APP_ZIGBEE_ConfigEndpoints(void)
{
  struct ZbApsmeAddEndpointReqT req;
  struct ZbApsmeAddEndpointConfT conf;

  memset(&req, 0, sizeof(req));
  /* Endpoint: APP_ZIGBEE_ENDPOINT*/
  req.profileId = ZCL_PROFILE_HOME_AUTOMATION;

#if APP_ZIGBEE_LIGHTSWITCH_SUPPORT==1
  req.deviceId = ZCL_DEVICE_ONOFF_SWITCH;
#elif APP_ZIGBEE_LIGHTBULB_CONFIG==1
  req.deviceId = ZCL_DEVICE_ONOFF_LIGHT;
#elif APP_ZIGBEE_THERMOSTAT_CONFIG==1
  req.deviceId = ZCL_DEVICE_THERMOSTAT;
#endif

  req.endpoint = APP_ZIGBEE_ENDPOINT;
  ZbZclAddEndpoint(zigbee_app_info.zb, &req, &conf);
  assert(conf.status == ZB_STATUS_SUCCESS);

  /* USER CODE BEGIN CONFIG_ENDPOINT */

#if APP_ZIGBEE_LIGHTSWITCH_CONFIG==1
	/* OnOff client */
	zigbee_app_info.onOff_client_1 = ZbZclOnOffClientAlloc(zigbee_app_info.zb,APP_LIGHTSWITCH_ENDPOINT);
	assert(zigbee_app_info.onOff_client_1 != NULL);
	ZbZclClusterEndpointRegister(zigbee_app_info.onOff_client_1);
#endif /*APP_ZIGBEE_LIGHTSWITCH_SUPPORT==1*/

#if APP_ZIGBEE_LIGHTBULB_CONFIG==1
  /* Adding lightbulb clusters */
  APP_ZIGBEE_LightBulb_ConfigEndpoints();
#endif /*APP_ZIGBEE_LIGHTBULB_CONFIG==1*/

#if APP_ZIGBEE_THERMOSTAT_CONFIG==1
  /*Adding thermostat server cluster*/
  APP_ZIGBEE_Thermostat_ConfigEndpoints();
#endif /*APP_ZIGBEE_THERMOSTAT_SUPPORT==1*/

#if APP_ZIGBEE_OTA_SUPPORT==1
  /* Adding OTA client cluster*/
  APP_ZIGBEE_OTA_ConfigEndpoints();
#endif /*APP_ZIGBEE_OTA_SUPPORT*/

  /* USER CODE END CONFIG_ENDPOINT */
}

/**
 * @brief  Handle Zigbee network forming and joining task for FreeRTOS
 * @param  None
 * @retval None
 */
static void APP_ZIGBEE_ProcessNwkForm(void *argument)
{
  UNUSED(argument);

  for(;;)
  {
    osThreadFlagsWait(1,osFlagsWaitAll,osWaitForever);
    APP_ZIGBEE_NwkForm();
  }
}

/**
 * @brief  Handle Zigbee network forming and joining
 * @param  None
 * @retval None
 */
static void APP_ZIGBEE_NwkForm(void)
{
  //VST_COMMENT: maybe the startup control flow chart could be described somewhere
  if ((zigbee_app_info.join_status != ZB_STATUS_SUCCESS) && (HAL_GetTick() >= zigbee_app_info.join_delay))
  {
    struct ZbStartupT config;
    enum ZbStatusCodeT status;

    /* Configure Zigbee Logging */

    ZbSetLogging(zigbee_app_info.zb, ZB_LOG_MASK_LEVEL_ALL, NULL);

    /* Attempt to join a zigbee network */
    ZbStartupConfigGetProDefaults(&config);

    config.startupControl = zigbee_app_info.startupControl;

    if(config.startupControl == ZbStartTypeTouchlink)
    {
      /* Using the Uncertified Distributed Global Key (d0:d1:d2:d3:d4:d5:d6:d7:d8:d9:da:db:dc:dd:de:df) */
      /* This key can be used as an APS Link Key between Touchlink devices.*/
      memcpy(config.security.distributedGlobalKey, sec_key_distrib_uncert, ZB_SEC_KEYSIZE);

      /* Use the Touchlink Certification Key (c0:c1:c2:c3:c4:c5:c6:c7:c8:c9:ca:cb:cc:cd:ce:cf) stored in sec_key_touchlink_cert.
      * This key is "mashed" with the Touchlink session data to generate the Network Key
      * To connect to 3rd party devices via touchlink you will need to obtain and use here zigbee light link master key.
      * ZLL master key is shared between all devices that support the ZLL profile and is distributed to manufacturers by
      *  the ZigBee Alliance and protected by a non-disclosure agreement (NDA)*/
      ZbBdbSet(zigbee_app_info.zb, ZB_BDB_TLKey, sec_key_touchlink_cert, ZB_SEC_KEYSIZE);

      /* Set the "Key Encryption Algorithm" to Certification. In case you have acquired ZLL master key you would choose key index
       * TOUCHLINK_KEY_INDEX_PRODUCTION*/
      enum ZbBdbTouchlinkKeyIndexT keyIdx = TOUCHLINK_KEY_INDEX_CERTIFICATION;

      ZbBdbSet(zigbee_app_info.zb, ZB_BDB_TLKeyIndex, &keyIdx, sizeof(keyIdx));

      int8_t val8 = APP_ZIGBEE_TOUCHLINK_MIN_RSSI;
      ZbBdbSet(zigbee_app_info.zb, ZB_BDB_TLRssiMin, &val8, sizeof(val8));

      /* Set the centralized network */
      APP_DBG("Network config : APP_STARTUP_CENTRALIZED_COORDINATOR");
      /* Configure the startup to use Touchlink */
      config.bdbCommissioningMode |= BDB_COMMISSION_MODE_TOUCHLINK;
      config.touchlink.tl_endpoint = APP_ZIGBEE_TOUCHLINK_ENDPOINT;
      config.touchlink.bind_endpoint = APP_ZIGBEE_ENDPOINT;

#if APP_ZIGBEE_TOUCHLINK_TARGET == 1
      config.touchlink.flags = ZCL_TL_FLAGS_IS_TARGET;
#endif /*APP_ZIGBEE_TOUCHLINK_TARGET == 1*/

      config.touchlink.zb_info = ZCL_TL_ZBINFO_RX_ON_IDLE;
#if APP_ZIGBEE_LIGHTSWITCH_CONFIG
      config.touchlink.zb_info |= ZCL_TL_ZBINFO_TYPE_END_DEVICE;
      config.touchlink.deviceId = ZCL_DEVICE_ONOFF_SWITCH;
#elif APP_ZIGBEE_LIGHTBULB_CONFIG
      config.touchlink.zb_info |= ZCL_TL_ZBINFO_TYPE_ROUTER;
      config.touchlink.deviceId = ZCL_DEVICE_ONOFF_LIGHT;
#elif APP_ZIGBEE_THERMOSTAT_CONFIG
      config.touchlink.zb_info |= ZCL_TL_ZBINFO_TYPE_ROUTER;
      config.touchlink.deviceId = ZCL_DEVICE_THERMOSTAT;
#endif
      /* Touchlink uses the BDB Primary and Secondary channel masks, which we leave as defaults. */
      /* Touchlink primary channels are:  11, 15, 20, and 25. All other channels constitute the secondary ZLL channels */
      config.channelList.count = 1;
      config.channelList.list[0].page = 0;
      config.channelList.list[0].channelMask = APP_ZIGBEE_TOUCHLINK_CHANNEL_MASK; /*Channels in use */
    }
    else if(config.startupControl == ZbStartTypeJoin)
    {
      APP_DBG("Network config : APP_STARTUP_CENTRALIZED_ROUTER");

      /* Using the default HA preconfigured Link Key */
      memcpy(config.security.preconfiguredLinkKey, sec_key_ha,ZB_SEC_KEYSIZE);
      config.channelList.count = 1;
      config.channelList.list[0].page = 0;
      config.channelList.list[0].channelMask = WPAN_CHANNELMASK_2400MHZ;
    }
    else {
      /* We should never get here */
      APP_DBG("ERROR : Network config not set !!!");
      return;
    }
    /* Using ZbStartupWait (blocking) */
    status = ZbStartupWait(zigbee_app_info.zb, &config);

    APP_DBG("ZbStartup Callback (status = 0x%02x)", status);
    zigbee_app_info.join_status = status;

    if (status == ZB_STATUS_SUCCESS)
    {
      zigbee_app_info.join_delay = 0U;
      zigbee_app_info.init_after_join = true;
      APP_DBG("Startup done !\n");
      /* USER CODE BEGIN 3 */
       BSP_LED_On(LED_BLUE);
#if APP_ZIGBEE_OTA_SUPPORT==1
      /* Prepare for OTA */
       APP_ZIGBEE_OTA_Client_Init();
#endif/*APP_ZIGBEE_OTA_SUPPORT==1*/
      /* Register Persistent data change notification */
      ZbPersistNotifyRegister(zigbee_app_info.zb,APP_ZIGBEE_persist_notify_cb,NULL);
      /* Call the callback once here to save persistence data */
      APP_ZIGBEE_persist_notify_cb(zigbee_app_info.zb,NULL);
      /* USER CODE END 3 */
    }
    else
    {
      APP_DBG("Startup failed, attempting again after a short delay (%d ms)", APP_ZIGBEE_STARTUP_FAIL_DELAY);
      zigbee_app_info.join_delay = HAL_GetTick() + APP_ZIGBEE_STARTUP_FAIL_DELAY;
      /* USER CODE BEGIN 4 */
            zigbee_app_info.join_retry_counter++;
      /* USER CODE END 4 */
    }
  }

  /* If Network forming/joining was not successful reschedule the current task to retry the process */
  if (zigbee_app_info.join_status != ZB_STATUS_SUCCESS)
  {
    if( zigbee_app_info.join_retry_counter < APP_ZIGBEE_NETWORK_JOIN_MAX_RETRY)
    {
      osThreadFlagsSet(OsTaskNwkFormId,1);
    }
    else
    {
      APP_DBG("Startup fail reached max permissible retries:  %d!\n",zigbee_app_info.join_retry_counter);
      /* Reset join retry counter */
      zigbee_app_info.join_retry_counter=0;
      return;
    }
  }
  /* USER CODE BEGIN NW_FORM */
  else
  {
    zigbee_app_info.init_after_join = false;

    /* Assign ourselves to the group addresses */
    APP_ZIGBEE_ConfigGroupAddr();

    /* Since we're using group addressing (broadcast), shorten the broadcast timeout */
    uint32_t bcast_timeout = 3;
    ZbNwkSet(zigbee_app_info.zb, ZB_NWK_NIB_ID_NetworkBroadcastDeliveryTime, &bcast_timeout, sizeof(bcast_timeout));
  }
  /* USER CODE END NW_FORM */
}

/*************************************************************
 * ZbStartupWait Blocking Call
 *************************************************************/
struct ZbStartupWaitInfo
{
  bool active;
  enum ZbStatusCodeT status;
};

static void ZbStartupWaitCb(enum ZbStatusCodeT status, void *cb_arg)
{
  struct ZbStartupWaitInfo *info = cb_arg;

  info->status = status;
  info->active = false;
  osSemaphoreRelease(StartupEndSemaphore);
}

enum ZbStatusCodeT ZbStartupWait(struct ZigBeeT *zb, struct ZbStartupT *config)
{
  struct ZbStartupWaitInfo *info;
  enum ZbStatusCodeT status;

  info = malloc(sizeof(struct ZbStartupWaitInfo));
  if (info == NULL)
  {
    return ZB_STATUS_ALLOC_FAIL;
  }
  memset(info, 0, sizeof(struct ZbStartupWaitInfo));

  info->active = true;
  status = ZbStartup(zb, config, ZbStartupWaitCb, info);
  if (status != ZB_STATUS_SUCCESS)
  {
    free(info);
    return status;
  }

  osSemaphoreAcquire(StartupEndSemaphore, osWaitForever);
  status = info->status;
  free(info);
  return status;
}

/**
 * @brief  Trace the error or the warning reported.
 * @param  ErrId :
 * @param  ErrCode
 * @retval None
 */
void APP_ZIGBEE_Error(uint32_t ErrId, uint32_t ErrCode)
{
  switch (ErrId)
  {
    default:
      APP_ZIGBEE_TraceError("ERROR Unknown ", 0);
      break;
  }
}

/*************************************************************
 *
 * LOCAL FUNCTIONS
 *
 *************************************************************/

/**
 * @brief  Warn the user that an error has occurred.
 *
 * @param  pMess  : Message associated to the error.
 * @param  ErrCode: Error code associated to the module (Zigbee or other module if any)
 * @retval None
 */
static void APP_ZIGBEE_TraceError(const char *pMess, uint32_t ErrCode)
{
  APP_DBG("**** Fatal error = %s (Err = %d)", pMess, ErrCode);
  /* USER CODE BEGIN TRACE_ERROR */
  while (1U == 1U) 
  {
    BSP_LED_Toggle(LED1);
    HAL_Delay(500U);
    BSP_LED_Toggle(LED2);
    HAL_Delay(500U);
    BSP_LED_Toggle(LED3);
    HAL_Delay(500U);
  }
  /* USER CODE END TRACE_ERROR */

}

/**
 * @brief Check if the Coprocessor Wireless Firmware loaded supports Zigbee
 *        and display associated information
 * @param  None
 * @retval None
 */
static void APP_ZIGBEE_CheckWirelessFirmwareInfo(void)
{
  WirelessFwInfo_t wireless_info_instance;
  WirelessFwInfo_t *p_wireless_info = &wireless_info_instance;

  if (SHCI_GetWirelessFwInfo(p_wireless_info) != SHCI_Success)
  {
    APP_ZIGBEE_Error((uint32_t)ERR_ZIGBEE_CHECK_WIRELESS, (uint32_t)ERR_INTERFACE_FATAL);
  }
  else
  {
    APP_DBG("**********************************************************");
    APP_DBG("WIRELESS COPROCESSOR FW:");
    /* Print version */
    APP_DBG("VERSION ID = %d.%d.%d", p_wireless_info->VersionMajor, p_wireless_info->VersionMinor, p_wireless_info->VersionSub);

    switch (p_wireless_info->StackType)
    {
      case INFO_STACK_TYPE_ZIGBEE_FFD:
        APP_DBG("FW Type : FFD Zigbee stack");
        break;

      case INFO_STACK_TYPE_ZIGBEE_RFD:
        APP_DBG("FW Type : RFD Zigbee stack");
        break;

      default:
        /* No Zigbee device supported ! */
        APP_ZIGBEE_Error((uint32_t)ERR_ZIGBEE_CHECK_WIRELESS, (uint32_t)ERR_INTERFACE_FATAL);
        break;
    }

    /* print the application name */
    char *__PathProject__ = (strstr(__FILE__, "Zigbee") ? strstr(__FILE__, "Zigbee") + 7 : __FILE__);
    char *pdel = NULL;
    if((strchr(__FILE__, '/')) == NULL)
    {
      pdel = strchr(__PathProject__, '\\');
    }
    else
    {
      pdel = strchr(__PathProject__, '/');
    }

    int index = (int)(pdel - __PathProject__);
    APP_DBG("Application flashed: %*.*s", index, index, __PathProject__);

    /* print channel */
    //APP_DBG("Channel used: %d", APP_ZIGBEE_TOUCHLINK_CHANNEL);
    /* print Link Key */
    APP_DBG("Link Key: %.16s", sec_key_ha);
    /* print Link Key value hex */
    char Z09_LL_string[ZB_SEC_KEYSIZE*3+1];
    Z09_LL_string[0] = 0;
    for (int str_index = 0; str_index < ZB_SEC_KEYSIZE; str_index++)
    {
      sprintf(&Z09_LL_string[str_index*3], "%02x ", sec_key_ha[str_index]);
    }
    APP_DBG("Link Key value: %s", Z09_LL_string);
    APP_DBG("**********************************************************");
  }
}

/*************************************************************
 *
 * WRAP FUNCTIONS
 *
 *************************************************************/

void APP_ZIGBEE_RegisterCmdBuffer(TL_CmdPacket_t *p_buffer)
{
  p_ZIGBEE_otcmdbuffer = p_buffer;
}

Zigbee_Cmd_Request_t * ZIGBEE_Get_OTCmdPayloadBuffer(void)
{
  return (Zigbee_Cmd_Request_t *)p_ZIGBEE_otcmdbuffer->cmdserial.cmd.payload;
}

Zigbee_Cmd_Request_t * ZIGBEE_Get_OTCmdRspPayloadBuffer(void)
{
  return (Zigbee_Cmd_Request_t *)((TL_EvtPacket_t *)p_ZIGBEE_otcmdbuffer)->evtserial.evt.payload;
}

Zigbee_Cmd_Request_t * ZIGBEE_Get_NotificationPayloadBuffer(void)
{
  return (Zigbee_Cmd_Request_t *)(p_ZIGBEE_notif_M0_to_M4)->evtserial.evt.payload;
}

Zigbee_Cmd_Request_t * ZIGBEE_Get_M0RequestPayloadBuffer(void)
{
  return (Zigbee_Cmd_Request_t *)(p_ZIGBEE_request_M0_to_M4)->evtserial.evt.payload;
}

/**
 * @brief  This function is used to transfer the commands from the M4 to the M0.
 *
 * @param   None
 * @return  None
 */
void ZIGBEE_CmdTransfer(void)
{
  Zigbee_Cmd_Request_t *cmd_req = (Zigbee_Cmd_Request_t *)p_ZIGBEE_otcmdbuffer->cmdserial.cmd.payload;

  /* Zigbee OT command cmdcode range 0x280 .. 0x3DF = 352 */
  p_ZIGBEE_otcmdbuffer->cmdserial.cmd.cmdcode = 0x280U;
  /* Size = otCmdBuffer->Size (Number of OT cmd arguments : 1 arg = 32bits so multiply by 4 to get size in bytes)
   * + ID (4 bytes) + Size (4 bytes) */
  p_ZIGBEE_otcmdbuffer->cmdserial.cmd.plen = 8U + (cmd_req->Size * 4U);

  TL_ZIGBEE_SendM4RequestToM0();

  /* Wait completion of cmd */
  Wait_Getting_Ack_From_M0();
}

/**
 * @brief  This function is called when the M0+ acknowledge the fact that it has received a Cmd
 *
 *
 * @param   Otbuffer : a pointer to TL_EvtPacket_t
 * @return  None
 */
void TL_ZIGBEE_CmdEvtReceived(TL_EvtPacket_t *Otbuffer)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(Otbuffer);

  Receive_Ack_From_M0();
}

/**
 * @brief  This function is called when notification from M0+ is received.
 *
 * @param   Notbuffer : a pointer to TL_EvtPacket_t
 * @return  None
 */
void TL_ZIGBEE_NotReceived(TL_EvtPacket_t *Notbuffer)
{
  p_ZIGBEE_notif_M0_to_M4 = Notbuffer;

  Receive_Notification_From_M0();
}

/**
 * @brief  This function is called before sending any zigbee command to the M0
 *         core.
 * @param  None
 * @retval None
 */
void Pre_ZigbeeCmdProcessing(void)
{
  osMutexAcquire(MtxZigbeeId, osWaitForever);
}

/**
 * @brief  This function is called just after having received the result associated to the command
 *         send to the M0
 * @param  None
 * @retval None
 */
void Post_ZigbeeCmdProcessing(void)
{
  osMutexRelease(MtxZigbeeId);
}
/**
 * @brief  This function waits for getting an acknowledgment from the M0.
 *
 * @param  None
 * @retval None
 */
static void Wait_Getting_Ack_From_M0(void)
{
  osSemaphoreAcquire(TransferToM0Semaphore, osWaitForever);
}

/**
 * @brief  Receive an acknowledgment from the M0+ core.
 *         Each command send by the M4 to the M0 are acknowledged.
 *         This function is called under interrupt.
 * @param  None
 * @retval None
 */
static void Receive_Ack_From_M0(void)
{
  osSemaphoreRelease(TransferToM0Semaphore);
}

/**
 * @brief  Receive a notification from the M0+ through the IPCC.
 *         This function is called under interrupt.
 * @param  None
 * @retval None
 */
static void Receive_Notification_From_M0(void)
{
  CptReceiveNotifyFromM0++;
  osThreadFlagsSet(OsTaskNotifyM0ToM4Id,1);
}

/**
 * @brief  This function is called when a request from M0+ is received.
 *
 * @param   Notbuffer : a pointer to TL_EvtPacket_t
 * @return  None
 */
void TL_ZIGBEE_M0RequestReceived(TL_EvtPacket_t *Reqbuffer)
{
  p_ZIGBEE_request_M0_to_M4 = Reqbuffer;

  CptReceiveRequestFromM0++;
  osThreadFlagsSet(OsTaskRequestM0ToM4Id,1);
}

/**
 * @brief Perform initialization of TL for Zigbee.
 * @param  None
 * @retval None
 */
void APP_ZIGBEE_TL_INIT(void)
{
  ZigbeeConfigBuffer.p_ZigbeeOtCmdRspBuffer = (uint8_t *)&ZigbeeOtCmdBuffer;
  ZigbeeConfigBuffer.p_ZigbeeNotAckBuffer = (uint8_t *)ZigbeeNotifRspEvtBuffer;
  ZigbeeConfigBuffer.p_ZigbeeNotifRequestBuffer = (uint8_t *)ZigbeeNotifRequestBuffer;
  TL_ZIGBEE_Init(&ZigbeeConfigBuffer);
}

/**
 * @brief Process the messages coming from the M0.
 * @param  None
 * @retval None
 */
static void APP_ZIGBEE_ProcessNotifyM0ToM4(void * argument)
{
  UNUSED(argument);

  for(;;)
  {
    osThreadFlagsWait(1,osFlagsWaitAll,osWaitForever);
    if (CptReceiveNotifyFromM0 != 0)
    {
      /* Reset counter */
      CptReceiveNotifyFromM0 = 0;
      Zigbee_CallBackProcessing();
    }
  }
}

/**
 * @brief Process the requests coming from the M0.
 * @param None
 * @return None
 */
static void APP_ZIGBEE_ProcessRequestM0ToM4(void * argument)
{
  UNUSED(argument);

  for(;;)
  {
    osThreadFlagsWait(1,osFlagsWaitAll,osWaitForever);
    if (CptReceiveRequestFromM0 != 0)
    {
      CptReceiveRequestFromM0 = 0;
      Zigbee_M0RequestProcessing();
    }
  }
}

/* USER CODE BEGIN FD_LOCAL_FUNCTIONS */
/**
 * @brief  Set group addressing mode
 * @param  None
 * @retval None
 */
static void APP_ZIGBEE_ConfigGroupAddr(void)
{
  struct ZbApsmeAddGroupReqT req;
  struct ZbApsmeAddGroupConfT conf;

  memset(&req, 0, sizeof(req));
  req.endpt = APP_ZIGBEE_ENDPOINT;
  req.groupAddr = SW1_GROUP_ADDR;
  ZbApsmeAddGroupReq(zigbee_app_info.zb, &req, &conf);

}

void APP_ZIGBEE_LaunchPushButtonTask(APP_ZIGBEE_PUSH_BUTTON_TaskTypeDef taskFlag)
{
  osThreadFlagsSet(OsTaskPushButtonId,taskFlag);
} /* APP_ZIGBEE_LaunchPushButtonTask */


/**
 * @brief  Function used to process push of different SW buttons
 * @param  None
 * @retval None
 */
static void APP_ZIGBEE_ProcessPushButton(void *argument) {
	UNUSED(argument);
	uint32_t flags = 0;
	for (;;) {
		uint32_t expected_fags;
		expected_fags = APP_ZIGBEE_PUSH_BUTTON_TASK_ALL;

		osThreadFlagsWait(expected_fags, osFlagsWaitAny | osFlagsNoClear,
				osWaitForever);
		flags = osThreadFlagsGet();
		if (flags & APP_ZIGBEE_PUSH_BUTTON_TASK_SW1) {
			APP_ZIGBEE_SW1_Process();
			osThreadFlagsClear(APP_ZIGBEE_PUSH_BUTTON_TASK_SW1);
		}

		if (flags & APP_ZIGBEE_PUSH_BUTTON_TASK_SW2) {
			APP_ZIGBEE_SW2_Process();
			osThreadFlagsClear(APP_ZIGBEE_PUSH_BUTTON_TASK_SW2);
		}

		if (flags & APP_ZIGBEE_PUSH_BUTTON_TASK_SW3_SHORT) {
			APP_ZIGBEE_SW3_ShortPress_Process();
			osThreadFlagsClear(APP_ZIGBEE_PUSH_BUTTON_TASK_SW3_SHORT);
		}

		if (flags & APP_ZIGBEE_PUSH_BUTTON_TASK_SW3_LONG) {
			APP_ZIGBEE_SW3_LongPress_Process();
			osThreadFlagsClear(APP_ZIGBEE_PUSH_BUTTON_TASK_SW3_LONG);
		}
	}
} /* APP_ZIGBEE_ProcessPushButton */

/**
 * @brief  Function used to process push of SW1 button
 * @param  None
 * @retval None
 */
static void APP_ZIGBEE_SW1_Process(void)
{
  APP_DBG("APP_ZIGBEE_SW1_Process.");

#if APP_ZIGBEE_LIGHTSWITCH_CONFIG==1
  APP_ZIGBEE_Lightswitch_Toggle();
#endif /*APP_ZIGBEE_LIGHTSWITCH==1*/

} /* APP_ZIGBEE_SW1_Process */

/**
 * @brief  Function used to process push of SW2 button
 * @param  None
 * @retval None
 */
static void APP_ZIGBEE_SW2_Process(void)
{
  APP_DBG("APP_ZIGBEE_SW2_Process.");
#if APP_ZIGBEE_NVM_SUPPORT==1
  APP_ZIGBEE_NVM_FactoryReset();
#endif /*APP_ZIGBEE_NVM_SUPPORT==1*/
} /* APP_ZIGBEE_SW2_Process */

static void APP_ZIGBEE_SW3_ShortPress_Process(void)
{
#if (APP_ZIGBEE_TOUCHLINK_TARGET == 1)
	if(zigbee_app_info.startupControl==ZbStartTypeTouchlink) {
		APP_ZIGBEE_ExitTouchlinkTargetCommissioning();
	}
#endif/*APP_ZIGBEE_TOUCHLINK_TARGET==1*/
	zigbee_app_info.join_retry_counter = 0;
	zigbee_app_info.startupControl = ZbStartTypeJoin;
	osThreadFlagsSet(OsTaskNwkFormId, 1);

} /* APP_ZIGBEE_LaunchPushButton3ShortPressTask */

static void APP_ZIGBEE_SW3_LongPress_Process(void)
{	zigbee_app_info.join_retry_counter=0;
	zigbee_app_info.startupControl=ZbStartTypeTouchlink;
	osThreadFlagsSet(OsTaskNwkFormId, 1);
} /* APP_ZIGBEE_SW3_LongPress_Process */

static void APP_ZIGBEE_ExitTouchlinkTargetCommissioning(void){
	/*Perfoms APS and NWK reset*/
	ZbReset(zigbee_app_info.zb);
	/*PD_COMMENT: Needed to remove the created touchlink endpoint
	 * in order to recognize device as lightbulb through 3rd party gateway
	 * */
	struct ZbApsmeRemoveEndpointReqT removeEndpointReq = { .endpoint =APP_ZIGBEE_TOUCHLINK_ENDPOINT };
	ZbZclRemoveEndpoint(zigbee_app_info.zb, &removeEndpointReq, NULL);
}

#if APP_ZIGBEE_LIGHTSWITCH_CONFIG==1
static void APP_ZIGBEE_Lightswitch_Toggle(void) {
	struct ZbApsAddrT dst;
	uint64_t epid = 0U;

	if (zigbee_app_info.zb == NULL) {
		return;
	}

	/* Check if the router joined the network */
	if (ZbNwkGet(zigbee_app_info.zb, ZB_NWK_NIB_ID_ExtendedPanId, &epid,
			sizeof(epid)) != ZB_STATUS_SUCCESS) {
		APP_DBG("Error, ZbNwkGet failed");
		return;
	}
	if (epid == 0U) {
		APP_DBG("Error, not on a network");
		return;
	}

	memset(&dst, 0, sizeof(dst));
	dst.mode = ZB_APSDE_ADDRMODE_NOTPRESENT; /* send to binding */

	APP_DBG("SW1 PUSHED SENDING TOGGLE TO BINDING");
	if (ZbZclOnOffClientToggleReq(zigbee_app_info.onOff_client_1, &dst, NULL,
			NULL) != ZCL_STATUS_SUCCESS) {
		APP_DBG("Error, ZbZclOnOffClientToggleReq failed (SW1_ENDPOINT)");
	}
}
#endif /*APP_ZIGBEE_LIGHTSWITCH==1*/

/**
 * @brief  notify to save persistent data callback
 * @param  zb: Zigbee device object pointer, cbarg: callback arg pointer
 * @retval None
 */
static void APP_ZIGBEE_persist_notify_cb(struct ZigBeeT *zb, void *cbarg)
{
  APP_DBG("Notification to save persistent data requested from stack");
  /* Save the persistent data */
  APP_ZIGBEE_persist_save();
} /* APP_ZIGBEE_persist_notify_cb */

/**
 * @brief  Save persistent data
 * @param  None
 * @retval true if success , false if fail
 */
static bool APP_ZIGBEE_persist_save(void)
{
  uint32_t len;

  /* Clear the RAM cache before saving */
  memset(cache_persistent_data.U8_data, 0x00, ST_PERSIST_MAX_ALLOC_SZ);

  /* Call the satck API t get current persistent data */
  len = ZbPersistGet(zigbee_app_info.zb, 0, 0);
  /* Check Length range */
  if (len == 0U)
  {
    /* If the persistence length was zero then no data available. */
    APP_DBG("APP_ZIGBEE_persist_save: no persistence data to save !");
    return false;
  }
  if (len > ST_PERSIST_MAX_ALLOC_SZ)
  {
    /* if persistence length to big to store */
    APP_DBG("APP_ZIGBEE_persist_save: persist size too large for storage (%d)", len);
    return false;
  }

  /* Store in cache the persistent data */
  len = ZbPersistGet(zigbee_app_info.zb, &cache_persistent_data.U8_data[ST_PERSIST_FLASH_DATA_OFFSET], len);

  /* Store in cache the persistent data length */
  cache_persistent_data.U32_data[0] = len;

  zigbee_app_info.persistNumWrites++;
  APP_DBG("APP_ZIGBEE_persist_save: Persistence written in cache RAM (num writes = %d) len=%d",
           zigbee_app_info.persistNumWrites, cache_persistent_data.U32_data[0]);

#if APP_ZIGBEE_NVM_SUPPORT==1
  if(!APP_ZIGBEE_NVM_Write())
  {
    return false;
  }
  APP_DBG("APP_ZIGBEE_persist_save: Persistent data FLASHED");
#endif /* APP_ZIGBEE_NVM_SUPPORT==1 */

  return true;
} /* APP_ZIGBEE_persist_save */

/**
 * @brief  Start Zigbee Network from persistent data
 * @param  zb: Zigbee device object pointer
 * @retval Zigbee stack Status code
 */
static enum ZbStatusCodeT APP_ZIGBEE_ZbStartupPersist(struct ZigBeeT* zb)
{
  bool read_status;
  enum ZbStatusCodeT status = ZB_STATUS_SUCCESS;

  /* Restore persistence */
  read_status = APP_ZIGBEE_persist_load();

  if (read_status)
  {
    /* Make sure the EPID is cleared, before we are allowed to restore persistence */
    uint64_t epid = 0U;
    ZbNwkSet(zb, ZB_NWK_NIB_ID_ExtendedPanId, &epid, sizeof(uint64_t));

    /* Start-up from persistence */
    APP_DBG("APP_ZIGBEE_ZbStartupPersist: restoring stack persistence");
    status = ZbStartupPersist(zb, &cache_persistent_data.U8_data[4], cache_persistent_data.U32_data[0],NULL,APP_ZIGBEE_PersistCompleted_callback,NULL);
  }
  else
  {
    /* Failed to restart from persistence */
    APP_DBG("APP_ZIGBEE_ZbStartupPersist: no persistence data to restore");
    status = ZB_STATUS_ALLOC_FAIL;
  }

  /* Only for debug purpose, depending of persistent data, following traces
  could display bytes that are irrelevants to on off cluster */
  if(status == ZB_STATUS_SUCCESS)
  {
    /* read the last bytes of data where the ZCL on off persistent data shall be*/
    uint32_t len = cache_persistent_data.U32_data[0] + 4 ;
    APP_DBG("ClusterID %02x %02x",cache_persistent_data.U8_data[len-9],cache_persistent_data.U8_data[len-10]);
    APP_DBG("Endpoint %02x %02x",cache_persistent_data.U8_data[len-7],cache_persistent_data.U8_data[len-8]);
    APP_DBG("Direction %02x",cache_persistent_data.U8_data[len-6]);
    APP_DBG("AttrID %02x %02x",cache_persistent_data.U8_data[len-4],cache_persistent_data.U8_data[len-5]);
    APP_DBG("Len %02x %02x",cache_persistent_data.U8_data[len-2],cache_persistent_data.U8_data[len-3]);
    APP_DBG("Value %02x",cache_persistent_data.U8_data[len-1]);
  }

  return status;
} /* APP_ZIGBEE_ZbStartupPersist */

/**
 * @brief  Load persistent data
 * @param  None
 * @retval true if success, false if fail
 */
static bool APP_ZIGBEE_persist_load(void)
{
#if APP_ZIGBEE_NVM_SUPPORT==1
  APP_DBG("Retrieving persistent data from FLASH");
  return APP_ZIGBEE_NVM_Read();
#else
  /* Check length range */
  if ((cache_persistent_data.U32_data[0] == 0) ||
      (cache_persistent_data.U32_data[0] > ST_PERSIST_MAX_ALLOC_SZ))
  {
    APP_DBG("No data or too large length : %d",cache_persistent_data.U32_data[0]);
    return false;
  }
  return true;
#endif /* APP_ZIGBEE_NVM_SUPPORT==1 */
} /* APP_ZIGBEE_persist_load */

/**
 * @brief  timer callback to wait end of restore cluster persistence form M0
 * @param  None
 * @retval None
 */
static void APP_ZIGBEE_PersistCompleted_callback(enum ZbStatusCodeT status,void *arg)
{
  if(status == ZB_WPAN_STATUS_SUCCESS)
  {
    APP_DBG("Persist complete callback entered with SUCCESS");
    /* Restore the on/off value based on persitence loaded */
    if(APP_ZIGBEE_RestoreClusterAttr(zigbee_app_info.onOff_server_1)==ZCL_STATUS_SUCCESS)
    {
      APP_DBG("Read back OnOff cluster attribute : SUCCESS");
    }
    else
    {
      APP_DBG("Read back OnOff cluster attribute : FAILED");
    }
  }
  else
  {
    APP_DBG("Error in persist complete callback %x",status);
  }
  /* Register Persistent data change notification */
  ZbPersistNotifyRegister(zigbee_app_info.zb,APP_ZIGBEE_persist_notify_cb,NULL);

  /* Call the callback once here to save persistence data */
  APP_ZIGBEE_persist_notify_cb(zigbee_app_info.zb,NULL);
} /* APP_ZIGBEE_PersistCompleted_callback */


/**
 * @brief  read on off attribute after a startup form persistence
 * @param  clusterPtr: pointer to cluster
 *
 * @retval stack status code
 */
static enum ZclStatusCodeT APP_ZIGBEE_RestoreClusterAttr(struct ZbZclClusterT *clusterPtr)
{
  uint8_t attrVal;
  if (ZbZclAttrRead(clusterPtr, ZCL_ONOFF_ATTR_ONOFF, NULL,
      &attrVal, sizeof(attrVal), false) != ZCL_STATUS_SUCCESS)
  {
    return ZCL_STATUS_FAILURE;
  }
  if (attrVal)
  {
    APP_DBG("RESTORE LED_RED TO ON");
    BSP_LED_On(LED_RED);
  }
  else
  {
    APP_DBG("RESTORE LED_RED TO OFF");
    BSP_LED_Off(LED_RED);
  }

  return ZCL_STATUS_SUCCESS;
} /* APP_ZIGBEE_RestoreClusterAttr */

#if APP_ZIGBEE_NVM_SUPPORT==1
/**
 * @brief  Init the NVM
 * @param  None
 * @retval None
 */
static void APP_ZIGBEE_NVM_Init(void)
{
  int eeprom_init_status;

  APP_DBG("Flash starting address = %x",HW_FLASH_ADDRESS  + CFG_NVM_BASE_ADDRESS);
  eeprom_init_status = EE_Init( 0 , HW_FLASH_ADDRESS + CFG_NVM_BASE_ADDRESS );

  if(eeprom_init_status != EE_OK)
  {
    /* format NVM since init failed */
    eeprom_init_status= EE_Init( 1, HW_FLASH_ADDRESS + CFG_NVM_BASE_ADDRESS );
  }
  APP_DBG("EE_init status = %d",eeprom_init_status);

} /* APP_ZIGBEE_NVM_Init */

/**
*@brief  Read the persistent data from NVM
* @param  None
* @retval true if success , false if failed
*/
static bool APP_ZIGBEE_NVM_Read(void)
{
  uint16_t num_words = 0;
  bool status = true;
  int ee_status = 0;
  HAL_FLASH_Unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGSERR | FLASH_FLAG_WRPERR | FLASH_FLAG_OPTVERR);

  /* Read the data length from cache */
  ee_status = EE_Read(0, ZIGBEE_DB_START_ADDR, &cache_persistent_data.U32_data[0]);
  if (ee_status != EE_OK)
  {
    APP_DBG("Read -> persistent data length not found ERASE to be done - Read Stopped");
    status = false;
  }
  /* Check length is not too big nor zero */
  else if((cache_persistent_data.U32_data[0] == 0) ||
          (cache_persistent_data.U32_data[0]> ST_PERSIST_MAX_ALLOC_SZ))
  {
    APP_DBG("No data or too large length : %d", cache_persistent_data.U32_data[0]);
    status = false;
  }
  /* Length is within range */
  else
  {
    /* Adjust the length to be U32 aligned */
    num_words = (uint16_t) (cache_persistent_data.U32_data[0]/4) ;
    if (cache_persistent_data.U32_data[0] % 4 != 0)
    {
      num_words++;
    }

    /* copy the read data from Flash to cache including length */
    for (uint16_t local_length = 1; local_length <= num_words; local_length++)
    {
      /* read data from first data in U32 unit */
      ee_status = EE_Read(0, local_length + ZIGBEE_DB_START_ADDR, &cache_persistent_data.U32_data[local_length] );
      if (ee_status != EE_OK)
      {
        APP_DBG("Read not found leaving");
        status = false;
        break;
      }
    }
  }

  HAL_FLASH_Lock();
  if(status)
  {
    APP_DBG("READ PERSISTENT DATA LEN = %d",cache_persistent_data.U32_data[0]);
  }
  return status;
} /* APP_ZIGBEE_NVM_Read */

/**
 * @brief  Write the persistent data in NVM
 * @param  None
 * @retval None
 */
static bool APP_ZIGBEE_NVM_Write(void)
{
  int ee_status = 0;

  uint16_t num_words;
  uint16_t local_current_size;

  num_words = 1U; /* 1 words for the length */
  num_words+= (uint16_t) (cache_persistent_data.U32_data[0]/4);

  /* Adjust the length to be U32 aligned */
  if (cache_persistent_data.U32_data[0] % 4 != 0)
  {
    num_words++;
  }

  /* Save data to flash */
  for (local_current_size = 0; local_current_size < num_words; local_current_size++)
  {
    ee_status = EE_Write(0, (uint16_t)local_current_size + ZIGBEE_DB_START_ADDR, cache_persistent_data.U32_data[local_current_size]);
    if (ee_status != EE_OK)
    {
      if(ee_status == EE_CLEAN_NEEDED) /* Shall not be there if CFG_EE_AUTO_CLEAN = 1*/
      {
        APP_DBG("CLEAN NEEDED, CLEANING");
        EE_Clean(0,0);
      }
      else
      {
        /* Failed to write, an Erase shall be done */
        APP_DBG("APP_ZIGBEE_NVM_Write failed @ %d status %d", local_current_size,ee_status);
        break;
      }
    }
  }

  if(ee_status != EE_OK)
  {
    APP_DBG("WRITE STOPPED, need a FLASH ERASE");
    return false;
  }

  APP_DBG("WRITTEN PERSISTENT DATA LEN = %d",cache_persistent_data.U32_data[0]);
  return true;

} /* APP_ZIGBEE_NVM_Write */

/**
 * @brief  Erase the NVM
 * @param  None
 * @retval None
 */
static void APP_ZIGBEE_NVM_Erase(void)
{
  EE_Init(1, HW_FLASH_ADDRESS + CFG_NVM_BASE_ADDRESS); /* Erase Flash */
} /* APP_ZIGBEE_NVM_Erase */

static void APP_ZIGBEE_NVM_FactoryReset(void){
	APP_DBG("Factory reset");
		BSP_LED_Off(LED_BLUE);
		BSP_LED_Off(LED_RED);
		BSP_LED_Off(LED_GREEN);
		APP_ZIGBEE_NVM_Erase();
		ZbReset(zigbee_app_info.zb);
		for (int i = 0; i < 10; i++) {
			BSP_LED_Toggle(LED_BLUE);
			BSP_LED_Toggle(LED_RED);
			BSP_LED_Toggle(LED_GREEN);
			osDelay(200);
		}
		NVIC_SystemReset();
}
#endif /* APP_ZIGBEE_NVM_SUPPORT==1 */
/* USER CODE END FD_LOCAL_FUNCTIONS */
