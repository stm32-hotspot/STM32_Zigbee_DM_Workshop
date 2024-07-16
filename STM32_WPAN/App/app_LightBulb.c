
/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    App/zigbee.c
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
#include "zcl/general/zcl.scenes.h"
#include "zcl/general/zcl.groups.h"

/* USER CODE BEGIN Includes */
#include "timers.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN PD */
#define PERIOD_BLINK                                    100
/* USER CODE END PD */

/* Private macros ------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* External definition -------------------------------------------------------*/
extern struct zigbee_app_info zigbee_app_info;

/* USER CODE BEGIN ED */
/* USER CODE END ED */

/* Private function prototypes -----------------------------------------------*/


/* USER CODE BEGIN PFP */
/* OnOff server 1 custom callbacks */
enum ZclStatusCodeT onOff_server_1_off(struct ZbZclClusterT *cluster, struct ZbZclAddrInfoT *srcInfo, void *arg);
enum ZclStatusCodeT onOff_server_1_on(struct ZbZclClusterT *cluster, struct ZbZclAddrInfoT *srcInfo, void *arg);
enum ZclStatusCodeT onOff_server_1_toggle(struct ZbZclClusterT *cluster, struct ZbZclAddrInfoT *srcInfo, void *arg);
static void APP_ZIGBEE_IdentifyCallback(struct ZbZclClusterT *cluster, enum ZbZclIdentifyServerStateT state, void *arg);
static void APP_LED_ToggleCallback(TimerHandle_t xTimer);

/* USER CODE END PFP */

/* Private variables ---------------------------------------------------------*/
struct ZbZclOnOffServerCallbacksT OnOffServerCallbacks_1 =
{
  .off = onOff_server_1_off,
  .on = onOff_server_1_on,
  .toggle = onOff_server_1_toggle,
};

 static TimerHandle_t blinkTimer;
/* FreeRtos stacks attributes */

/* USER CODE BEGIN PV */
 static const struct ZbZclAttrT optional_attr_list[] = {
 {ZCL_ONOFF_ATTR_ONOFF, ZCL_DATATYPE_BOOLEAN,
 ZCL_ATTR_FLAG_REPORTABLE|ZCL_ATTR_FLAG_PERSISTABLE, 0, NULL,
 {0x0, 0x1}, {0, 0}
 }};
/* USER CODE END PV */
/* Functions Definition ------------------------------------------------------*/

/**
 * @brief  Configure Zigbee light bulb application endpoint
 * @param  None
 * @retval None
 */
void APP_ZIGBEE_LightBulb_ConfigEndpoints(void)
{
  /* OnOff server */
  zigbee_app_info.onOff_server_1 = ZbZclOnOffServerAlloc(zigbee_app_info.zb, APP_LIGHTBULB_ENDPOINT, &OnOffServerCallbacks_1, NULL);
  assert(zigbee_app_info.onOff_server_1 != NULL);
  ZbZclClusterEndpointRegister(zigbee_app_info.onOff_server_1);

  /*add onOff attribute as persistable*/
  if (ZbZclAttrAppendList(zigbee_app_info.onOff_server_1, optional_attr_list,
  ZCL_ATTR_LIST_LEN(optional_attr_list)) != ZCL_STATUS_SUCCESS) {
  APP_DBG("Adding onoff as persistable not successful");
  }


  /* Identify server */
  zigbee_app_info.identify_server_1 = ZbZclIdentifyServerAlloc( zigbee_app_info.zb, APP_LIGHTBULB_ENDPOINT, NULL);
  assert(zigbee_app_info.identify_server_1 != NULL);
  ZbZclClusterEndpointRegister(zigbee_app_info.identify_server_1);

  /* Groups server */
  zigbee_app_info.groups_server_1 = ZbZclGroupsServerAlloc(zigbee_app_info.zb, APP_LIGHTBULB_ENDPOINT);
  assert(zigbee_app_info.groups_server_1 != NULL);
  ZbZclClusterEndpointRegister(zigbee_app_info.groups_server_1);

  /* Scenes server */
  zigbee_app_info.scenes_server_1 = ZbZclScenesServerAlloc(zigbee_app_info.zb, APP_LIGHTBULB_ENDPOINT, 10);
  assert(zigbee_app_info.scenes_server_1 != NULL);
  ZbZclClusterEndpointRegister(zigbee_app_info.scenes_server_1);

  ZbZclIdentifyServerSetCallback(zigbee_app_info.identify_server_1, APP_ZIGBEE_IdentifyCallback);

  blinkTimer = xTimerCreate("Blinking Timer", // Just a text name, not used by the kernel.
                            PERIOD_BLINK,   // The timer period in ticks.
                            pdTRUE,  // The timers will auto-reload themselves when they expire.
                            (void*) 1, // Assign each timer a unique id equal to its array index.
                            APP_LED_ToggleCallback // Each timer calls the same callback when it expires.
							);
}

/**
 * @brief  OnOff server off 1 command callback
 * @param  struct ZbZclClusterT *cluster, struct ZbZclAddrInfoT *srcInfo, void *arg
 * @retval  enum ZclStatusCodeT
 */
enum ZclStatusCodeT onOff_server_1_off(struct ZbZclClusterT *cluster, struct ZbZclAddrInfoT *srcInfo, void *arg)
{
  /* USER CODE BEGIN 0 OnOff server 1 off 1 */
  uint8_t endpoint;

  endpoint = ZbZclClusterGetEndpoint(cluster);
  if (endpoint == APP_LIGHTBULB_ENDPOINT)
  {
    APP_DBG("LED_RED OFF");
    BSP_LED_Off(LED_RED);
    (void)ZbZclAttrIntegerWrite(cluster, ZCL_ONOFF_ATTR_ONOFF, 0);
  }
  else
  {
    /* Unknown endpoint */
    return ZCL_STATUS_FAILURE;
  }

  return ZCL_STATUS_SUCCESS;
  /* USER CODE END 0 OnOff server 1 off 1 */
}

/**
 * @brief  OnOff server on 1 command callback
 * @param  struct ZbZclClusterT *cluster, struct ZbZclAddrInfoT *srcInfo, void *arg
 * @retval  enum ZclStatusCodeT
 */
enum ZclStatusCodeT onOff_server_1_on(struct ZbZclClusterT *cluster, struct ZbZclAddrInfoT *srcInfo, void *arg)
{
  /* USER CODE BEGIN 1 OnOff server 1 on 1 */
  uint8_t endpoint;

  endpoint = ZbZclClusterGetEndpoint(cluster);
  if (endpoint == APP_LIGHTBULB_ENDPOINT)
  {
    APP_DBG("LED_RED ON");
    BSP_LED_On(LED_RED);
    (void)ZbZclAttrIntegerWrite(cluster, ZCL_ONOFF_ATTR_ONOFF, 1);
  }
  else
  {
    /* Unknown endpoint */
    return ZCL_STATUS_FAILURE;
  }

  return ZCL_STATUS_SUCCESS;
  /* USER CODE END 1 OnOff server 1 on 1 */
}

/**
 * @brief  OnOff server toggle command callback
 * @param  struct ZbZclClusterT *cluster, struct ZbZclAddrInfoT *srcInfo, void *arg
 * @retval  enum ZclStatusCodeT
 */
enum ZclStatusCodeT onOff_server_1_toggle(struct ZbZclClusterT *cluster, struct ZbZclAddrInfoT *srcInfo, void *arg)
{
  /* USER CODE BEGIN 2 OnOff server 1 toggle 1 */
  uint8_t attrVal;

  if (ZbZclAttrRead(cluster, ZCL_ONOFF_ATTR_ONOFF, NULL,
            &attrVal, sizeof(attrVal), false) != ZCL_STATUS_SUCCESS)
  {
    return ZCL_STATUS_FAILURE;
  }

  if (attrVal != 0)
  {
    return onOff_server_1_off(cluster, srcInfo, arg);
  }
  else
  {
    return onOff_server_1_on(cluster, srcInfo, arg);
  }
  /* USER CODE END 2 OnOff server 1 toggle 1 */
}

/**
 * @brief  Identify command callback
 * @param  struct ZbZclClusterT *cluster, enum ZbZclIdentifyServerStateT state, void *arg
 * @retval none
 */
void APP_ZIGBEE_IdentifyCallback(struct ZbZclClusterT *cluster, enum ZbZclIdentifyServerStateT state, void *arg)
{
  if (state == ZCL_IDENTIFY_START)
  {
    APP_DBG("Identify START");
    /* Start the timer.  No block time is specified, and even if one was
       it would be ignored because the scheduler has not yet been started. */
    if ( xTimerStart( blinkTimer, 0 ) != pdPASS)
    {
      APP_DBG("The timer could not be set into the Active state.");
    }
  }

  if (state == ZCL_IDENTIFY_STOP)
  {
    APP_DBG("Identify STOP");
    if ( xTimerStop( blinkTimer, 0 ) != pdPASS)
    {
      APP_DBG("The timer could not be set into the Active state.");		}
      BSP_LED_Off(LED_GREEN);
    }
}


/**
 * @brief  SW Timer callback function handling the identify cluster LED blinking
 * @param  timer handle
 * @retval none
 */
static void APP_LED_ToggleCallback(TimerHandle_t xTimer)
{
  BSP_LED_Toggle(LED_GREEN);
}


