
/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    App/zigbee_thermostat.c
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

/* USER CODE BEGIN Includes */
#include "zcl/general/zcl.therm.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private defines -----------------------------------------------------------*/
#define APP_THERMOSTAT_MIN_TEMP 0
#define APP_THERMOSTAT_MAX_TEMP 5000
/* USER CODE BEGIN PD */
enum ZclStatusCodeT Thermostat_Server_WR_CB(struct ZbZclClusterT *cluster, struct ZbZclAttrCbInfoT* cb_args);

void APP_ZIGBEE_Thermostat_ConfigEndpoints(void);
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


/* USER CODE END PFP */

/* Private variables ---------------------------------------------------------*/
/* THERMO  Attributes list */
static const struct ZbZclAttrT zcl_thermostat_attr[] = {
    {
    	ZCL_THERM_SVR_ATTR_LOCAL_TEMP, ZCL_DATATYPE_SIGNED_16BIT,//only this attr is reportable on HVAC
		ZCL_ATTR_FLAG_WRITABLE|ZCL_ATTR_FLAG_REPORTABLE|ZCL_ATTR_FLAG_CB_WRITE, 0, Thermostat_Server_WR_CB, {APP_THERMOSTAT_MIN_TEMP, APP_THERMOSTAT_MAX_TEMP}, {0, 0}

    },
	{
			ZCL_THERM_SVR_ATTR_OCCUP_HEAT_SETPOINT, ZCL_DATATYPE_SIGNED_16BIT,
			ZCL_ATTR_FLAG_REPORTABLE|ZCL_ATTR_FLAG_WRITABLE|ZCL_ATTR_FLAG_CB_WRITE, 0, Thermostat_Server_WR_CB, {0, 0}, {0, 0}
	},
	{
			ZCL_THERM_SVR_ATTR_OCCUP_COOL_SETPOINT, ZCL_DATATYPE_SIGNED_16BIT,
			ZCL_ATTR_FLAG_REPORTABLE|ZCL_ATTR_FLAG_WRITABLE|ZCL_ATTR_FLAG_CB_WRITE, 0, Thermostat_Server_WR_CB, {0, 0}, {0, 0}
	},
	{
			ZCL_THERM_SVR_ATTR_SYSTEM_MODE, ZCL_DATATYPE_ENUMERATION_8BIT,
			ZCL_ATTR_FLAG_REPORTABLE|ZCL_ATTR_FLAG_WRITABLE|ZCL_ATTR_FLAG_CB_WRITE, 0, Thermostat_Server_WR_CB, {0, 0}, {0, 0}
	},
	{
			ZCL_THERM_SVR_ATTR_RUNNING_STATE, ZCL_DATATYPE_BITMAP_16BIT,/* this one is frequently changing , should not be persisted nor reported*/
			ZCL_ATTR_FLAG_REPORTABLE|ZCL_ATTR_FLAG_NONE, 0, Thermostat_Server_WR_CB, {0, 0}, {0, 0}
	},


};

/* FreeRtos stacks attributes */


/* USER CODE BEGIN PV */
/* USER CODE END PV */
/* Functions Definition ------------------------------------------------------*/


/**
 * @brief  Configure Zigbee thermostat application endpoint
 * @param  None
 * @retval None
 */
void APP_ZIGBEE_Thermostat_ConfigEndpoints(void)
{

	enum ZclStatusCodeT status = ZCL_STATUS_SUCCESS ;
	/* Thermostat server */
	zigbee_app_info.thermostat_server_1 = ZbZclThermServerAlloc(zigbee_app_info.zb, APP_THERMOSTAT_ENDPOINT,NULL,NULL);
	assert(zigbee_app_info.thermostat_server_1 != NULL);
	ZbZclClusterEndpointRegister(zigbee_app_info.thermostat_server_1);

	/* append attributes to cluster */
	if (ZbZclAttrAppendList(zigbee_app_info.thermostat_server_1, zcl_thermostat_attr, ZCL_ATTR_LIST_LEN(zcl_thermostat_attr))) {
		APP_DBG("ZbZclAttrAppendList: Failed to append zcl_thermostat_attr attributes list, out of RAM?");
		assert(0);
	}

	/* Write dummy values for local temperature 22.5 C° (typically this is onboard temp sensor) & set state to active*/


	status |= ZbZclAttrIntegerWrite(zigbee_app_info.thermostat_server_1, ZCL_THERM_SVR_ATTR_LOCAL_TEMP, 2250);
	status |= ZbZclAttrIntegerWrite(zigbee_app_info.thermostat_server_1, ZCL_THERM_SVR_ATTR_RUNNING_STATE, 1);

	if(status == ZCL_STATUS_SUCCESS){
		APP_DBG("\033[1;36m Thermostat_Server_1 init success");
	}


}

/**
 * @brief  Thermostat_Server_WR_CB callback on attributes write
 * @param  cluster: pointer to cluster of interest
 * @param  cb_args: attribute write CB arguments
 * @retval ZCL response
 */
enum ZclStatusCodeT Thermostat_Server_WR_CB(struct ZbZclClusterT *cluster, struct ZbZclAttrCbInfoT* cb_args)
{
	uint32_t  temp;
	enum ZclStatusCodeT status;
	switch  (cb_args->src->mode)
	{
	case ZB_APSDE_ADDRMODE_NOTPRESENT :
		APP_DBG("[Thermostat_Server_WR_CB] info: local write");
		break;
	default :
		//APP_DBG("[Thermostat_Server_WR_CB] info: remote write from address = %016llx ",cb_args->src->extAddr);
		APP_DBG("[Thermostat_Server_WR_CB] info: remote write");

		break;

	}
	//APP_DBG("[Thermostat_Server_WR_CB] info: incoming write attribute request for cluster id = 0x%04x , attribute id =  0x%04x. \n",cluster->clusterId,cb_args->info->attributeId );

	/*Warning, before writing to local attributes, received data should be validated (sanity check ect..)*/
	/* For demo purpose and since data is coming from a trusted device, we do memcpy directly */
	//some sanity check
	switch  (cb_args->info->attributeId)
	{
	case ZCL_THERM_SVR_ATTR_LOCAL_TEMP :
	case ZCL_THERM_SVR_ATTR_OCCUP_HEAT_SETPOINT :
	case ZCL_THERM_SVR_ATTR_OCCUP_COOL_SETPOINT :

		temp = ZbZclParseInteger(cb_args->info->dataType,cb_args->zcl_data,&status);
		//temp = cb_args->attr_data;
		if(temp > APP_THERMOSTAT_MIN_TEMP && temp < APP_THERMOSTAT_MAX_TEMP && status == ZCL_STATUS_SUCCESS) //max temp allowed is 50 C°
		{

			memcpy(cb_args->attr_data,cb_args->zcl_data,cb_args->zcl_len);
			APP_DBG("[Thermostat_Server_WR_CB] info: new temperature value =  %.2d . \n",temp );

		}
		break;
	default :
		memcpy(cb_args->attr_data,cb_args->zcl_data,cb_args->zcl_len);

		break;

	}

	// after checks , if new SET POINT value is valide , update PID process ....
	//update_pid(new_set_point);

	/* should return ZCL_STATUS_SUCCESS for the stack to continue processing this request , otherwise it would be dropped   */
	return ZCL_STATUS_SUCCESS;
}


