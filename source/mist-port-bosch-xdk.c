/*----------------------------------------------------------------------------*/
/*
* Licensee agrees that the example code provided to Licensee has been developed and released by Bosch solely as an example to be used as a potential reference for Licensee�s application development. 
* Fitness and suitability of the example code for any use within Licensee�s applications need to be verified by Licensee on its own authority by taking appropriate state of the art actions and measures (e.g. by means of quality assurance measures).
* Licensee shall be responsible for conducting the development of its applications as well as integration of parts of the example code into such applications, taking into account the state of the art of technology and any statutory regulations and provisions applicable for such applications. Compliance with the functional system requirements and testing there of (including validation of information/data security aspects and functional safety) and release shall be solely incumbent upon Licensee. 
* For the avoidance of doubt, Licensee shall be responsible and fully liable for the applications and any distribution of such applications into the market.
* 
* 
* Redistribution and use in source and binary forms, with or without 
* modification, are permitted provided that the following conditions are 
* met:
* 
*     (1) Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer. 
* 
*     (2) Redistributions in binary form must reproduce the above copyright
*     notice, this list of conditions and the following disclaimer in
*     the documentation and/or other materials provided with the
*     distribution.  
*     
*     (3)The name of the author may not be used to
*     endorse or promote products derived from this software without
*     specific prior written permission.
* 
*  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
*  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
*  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
*  DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
*  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
*  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
*  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
*  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
*  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
*  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
*  POSSIBILITY OF SUCH DAMAGE.
*/
/*
 * Copyright (C) Bosch Connected Devices and Solutions GmbH.
 * All Rights Reserved. Confidential.
 *
 * Distribution only to people who need to know this information in
 * order to do their job.(Need-to-know principle).
 * Distribution to persons outside the company, only if these persons
 * signed a non-disclosure agreement.
 * Electronic transmission, e.g. via electronic mail, must be made in
 * encrypted form.
 */
/*----------------------------------------------------------------------------*/
/**
 * @ingroup APPS_LIST
 *
 * @defgroup WLAN_NETWORK_MANAGEMENT Wlan Network Management
 * @{
 *
 * @brief Wlan Network Management demonstrates how to use the XDK WLAN Abstraction to scan for networks, join networks, set a static IP address or dynamically obtain an IP address via DHCP.
 *
 * @details This application does the following:
 *       - scans for networks
 *       - set an IP (either static or DHCP)
 *       - connects to a known WPA network
 *       - disconnects after a few seconds
 *
 * This cycle repeats over and over until the OS timer expires.
 *
 * @file
 **/

/* module includes ********************************************************** */
#include "XDKAppInfo.h"
#undef BCDS_MODULE_ID  /* Module ID define before including Basics package*/
#define BCDS_MODULE_ID XDK_APP_MODULE_ID_SCAN_WIFI_NW

/* own header files */
#include "mist-port-bosch-xdk.h"




/* system header files */
#include <stdio.h>

/* additional interface header files */
#include "BCDS_WlanConnect.h"
#include "BCDS_NetworkConfig.h"
#include "BCDS_CmdProcessor.h"
#include "BCDS_Assert.h"
#include "BCDS_BSP_LED.h"
#include "BSP_BoardType.h"
#include "FreeRTOS.h"
#include "timers.h"

#include "my_tests.h"
#include "spiffs_integration.h"

/* constant definitions ***************************************************** */

/* local variables ********************************************************** */

/**
 * Timer handle for periodically scan the wifi network
 */
xTimerHandle ScanwifiNetworkHandle = NULL;

CmdProcessor_T *AppCmdProcessorHandle; /**< Application Command Processor Handle */

#if (ONE == NCI_DHCP_MODE)
static uint8_t dhcpFlag_mdu8; /**< callback flag for DHCP; let user know that the DHCP IP was acquired */
#endif
/* global variables ********************************************************* */

/* inline functions ********************************************************* */

/* local functions ********************************************************** */
/**
 * @brief        WLI Disconnect callback function. This function is called by
 *               the device when a disconnect event takes place.
 *
 * @param  [in]  returnStatus
 *               Enumeration element containing the connection return status.
 *
 * @returns      none.
 *
 ******************************************************************************/
void myAllDisconnectCallbackFunc(WlanConnect_Status_T returnStatus)
{
    if (WLAN_DISCONNECTED == returnStatus)
    {
        /* Network disconnection successfully from WlanConnect_Disconnect function*/
        printf("[NCA] : Callback Function : Disconnected by event or by function!\n\r");
    }
    else
    {
        /* Disconnection failed, still connected */
    }
}

#if (ONE == NCI_DHCP_MODE)
/******************************************************************************/
/**
 * @brief        NCI DHCP Callback function. This function is called by
 *               the device when an IP was acquired using DHCP.
 *
 * @param  [in]  returnStatus
 *               Enumeration element containing the IP return status.
 *
 * @returns      none.
 *
 ******************************************************************************/
void myDhcpIpCallbackFunc(NetworkConfig_IpStatus_T returnStatus)
{
    /* set the DHCP flag*/
    if (NETWORKCONFIG_IPV4_ACQUIRED == returnStatus)
    {
        dhcpFlag_mdu8 = NETWORKCONFIG_DHCP_FLG_ACQ;
        printf("[NCA] : Callback Function : IP was acquired using DHCP\n\r");
    }
    else
    {
        /* DHCP failed */
    }
}
#endif

bool network_init = false;
/******************************************************************************/
/**
 * @brief        This function does the following :
 *                 - disconnects from a network if connected
 *                 - sets an IP (either static or DHCP),
 *                 - connects to a known network
 *                 - get the IP setting
 *                 - disconnects from WLAN network
 *
 * @param        none.
 *
 * @return       none.
 ******************************************************************************/
static void setAndGetIp(void)
{
    /* local variables */
    NetworkConfig_IpSettings_T myIpGet;
    Retcode_T retStatus[MAX_SIZE_BUFFER];
    WlanConnect_SSID_T connectSSID;
    WlanConnect_PassPhrase_T connectPassPhrase;

    /* WPA-PSK and WPA2-PSK wireless network SSID */
    connectSSID = (WlanConnect_SSID_T) WLAN_CONNECT_WPA_SSID;
    /* WPA-PSK and WPA2-PSK wireless network Password */
    connectPassPhrase = (WlanConnect_PassPhrase_T) WLAN_CONNECT_WPA_PASS;

    /* ************************************************************************/
    /*   Set IPV4 parameters
     **************************************************************************/
#if (ZERO == NCI_DHCP_MODE)
    /* local variable */
    NetworkConfig_IpSettings_T myIpSet;
    /* set static IP parameters*/
    myIpSet.isDHCP = (uint8_t) NETWORKCONFIG_DHCP_DISABLED; /* Disable DHCP*/
    myIpSet.ipV4 = NetworkConfig_Ipv4Value(192, 168, 1, 111);
    myIpSet.ipV4DnsServer = NetworkConfig_Ipv4Value(192, 168, 1, 1);
    myIpSet.ipV4Gateway = NetworkConfig_Ipv4Value(192, 168, 1, 1);
    myIpSet.ipV4Mask = NetworkConfig_Ipv4Value(255, 255, 255, 0);
#endif /* (ZERO == NCI_DHCP_MODE) */

#if (ONE == NCI_DHCP_MODE)
    /* local variable */
    NetworkConfig_IpCallback_T myIpCallback;
    /* Set the IP callback*/
    myIpCallback = myDhcpIpCallbackFunc;
#endif /* (ONE == NCI_DHCP_MODE) */

#if (TWO == NCI_DHCP_MODE)
    /* Default mode*/
#endif /* (TWO == NCI_DHCP_MODE) */

    /* ************************************************************************/
    /*    Disconnect from previous connect if any
     **************************************************************************/
    WlanConnect_DisconnectCallback_T myAllDisconnectCallback;
    myAllDisconnectCallback = myAllDisconnectCallbackFunc;

    if (DISCONNECTED_IP_NOT_ACQUIRED
            != WlanConnect_GetCurrentNwStatus()
            && (DISCONNECTED_AND_IPV4_ACQUIRED
                    != WlanConnect_GetCurrentNwStatus()))
    {
        /* If the there are any valid profiles stored, the board could connect
         * immediately after power up; therefore a disconnect must be called*/
        retStatus[0] = (Retcode_T) WlanConnect_Disconnect(myAllDisconnectCallback);

        if (RETCODE_OK == retStatus[0])
        {
            printf("[NCA] : Disconnect successful.\n\r");
        }
        else
        {
            printf("[NCA] : Disconnect has failed\n\r");
        }
    }

    /* ************************************************************************/
    /*   Call Set IP functions
     **************************************************************************/
#if (ZERO == NCI_DHCP_MODE)
    /* Set the static IP */
    retStatus[1] = NetworkConfig_SetIpStatic(myIpSet);

    if (RETCODE_OK == retStatus[1])
    {
        printf("[NCA] : Static IP is set to : %u.%u.%u.%u\n\r",
                (unsigned int) (NetworkConfig_Ipv4Byte(myIpSet.ipV4, 3)),
                (unsigned int) (NetworkConfig_Ipv4Byte(myIpSet.ipV4, 2)),
                (unsigned int) (NetworkConfig_Ipv4Byte(myIpSet.ipV4, 1)),
                (unsigned int) (NetworkConfig_Ipv4Byte(myIpSet.ipV4, 0)));
    }
    else
    {
        printf("[NCA] : NetworkConfig_SetIpStatic API has failed\n\r");
    }
#endif /* (ZERO == NCI_DHCP_MODE) */
#if (ONE == NCI_DHCP_MODE)
    printf("[NCA] : DHCP is set\n\r");
    /*reset DHCP flag and let callback set it to 0s*/
    dhcpFlag_mdu8 = NETWORKCONFIG_DHCP_FLG_NOT_ACQ;
    /* Set the DHCP with callback*/
    retStatus[2] = NetworkConfig_SetIpDhcp(myIpCallback);
    if (RETCODE_OK == retStatus[2])
    {
        printf("[NCA] : IP will be set using DHCP with callback. Waiting for IP ...\n\r");
    }
    else
    {
        printf("[NCA] : NetworkConfig_SetIpDhcp API has failed\n\r");
    }
#endif /* (NCI_DHCP_MODE == 1) */
#if (TWO == NCI_DHCP_MODE)
    /* Set the DHCP without callback */
    retStatus[3] = NetworkConfig_SetIpDhcp(ZERO);

    if (RETCODE_OK == retStatus[3])
    {
        printf("[NCA] : IP will be set using DHCP. Waiting for IP ...\n\r");
    }
    else
    {
        printf("[NCA] : NetworkConfig_SetIpDhcp API has failed!\n\r");
    }
#endif /* (TWO == NCI_DHCP_MODE) */

    /* ************************************************************************/
    /*   Connect to a WLAN WPA network
     **************************************************************************/
    printf("\n\r[NCA] : XDK will connect to %s network.\n\r",
    WLAN_CONNECT_WPA_SSID);
    retStatus[4] = (Retcode_T) WlanConnect_WPA(connectSSID, connectPassPhrase,
    NULL);
    if (RETCODE_OK == retStatus[4])
    {
        printf("[NCA] : Connected successfully.\n\r");
    }
    else if ((Retcode_T) RETCODE_CONNECTION_ERROR == Retcode_GetCode(retStatus[4]))
    {
        printf("[NCA] : WLI_connectWPA API SSID has failed!\n\r");
    }
    else if ((Retcode_T) RETCODE_ERROR_WRONG_PASSWORD == Retcode_GetCode(retStatus[4]))
    {
        printf("[NCA] : WLI_connectWPA API PASSWORD has failed!\n\r");
    }
    else
    {
        printf("[NCA] : WLI_connectWPA API  has failed!\n\r");
    }
    /* ************************************************************************/
    /*   Get the IP settings for both Static and DHCP
     **************************************************************************/
#if (ZERO == NCI_DHCP_MODE)
    /* Get Static IP settings and store them locally*/
    retStatus[5] = NetworkConfig_GetIpSettings(&myIpGet);

    if (RETCODE_OK == retStatus[5])
    {
        printf("[NCA] :  - The static IP was retrieved : %u.%u.%u.%u \n\r",
                (unsigned int) (NetworkConfig_Ipv4Byte(myIpGet.ipV4, 3)),
                (unsigned int) (NetworkConfig_Ipv4Byte(myIpGet.ipV4, 2)),
                (unsigned int) (NetworkConfig_Ipv4Byte(myIpGet.ipV4, 1)),
                (unsigned int) (NetworkConfig_Ipv4Byte(myIpGet.ipV4, 0)));
    }
    else
    {
        printf("[NCA] : NetworkConfig_GetIpSettings API has failed\n\r");
    }

#endif /* (ZERO == NCI_DHCP_MODE) */

#if (ONE == NCI_DHCP_MODE)

    /* Give time to acquired IP*/
    static_assert((portTICK_RATE_MS != 0), "Tick rate MS is zero");
    vTaskDelay((portTickType) DELAY_5_SEC / portTICK_RATE_MS);

    if (NETWORKCONFIG_DHCP_FLG_ACQ == dhcpFlag_mdu8)
    {
        retStatus[7] = NetworkConfig_GetIpSettings(&myIpGet);

        if (RETCODE_OK == retStatus[7])
        {
            printf("[NCA] :  - The DHCP IP (with callback) : %u.%u.%u.%u \n\r",
                    (unsigned int) (NetworkConfig_Ipv4Byte(myIpGet.ipV4, 3)),
                    (unsigned int) (NetworkConfig_Ipv4Byte(myIpGet.ipV4, 2)),
                    (unsigned int) (NetworkConfig_Ipv4Byte(myIpGet.ipV4, 1)),
                    (unsigned int) (NetworkConfig_Ipv4Byte(myIpGet.ipV4, 0)));
        }
        else
        {
            printf("[NCA] : NetworkConfig_GetIpSettings API has failed\n\r");
        }
    }
#endif /* (ONE == NCI_DHCP_MODE) */

#if (TWO == NCI_DHCP_MODE)
    retStatus[6] = NetworkConfig_GetIpSettings(&myIpGet);

    if (RETCODE_OK == retStatus[6])
    {
        printf("[NCA] :  - The DHCP IP (no callback) : %u.%u.%u.%u \n\r",
                (unsigned int) (NetworkConfig_Ipv4Byte(myIpGet.ipV4, 3)),
                (unsigned int) (NetworkConfig_Ipv4Byte(myIpGet.ipV4, 2)),
                (unsigned int) (NetworkConfig_Ipv4Byte(myIpGet.ipV4, 1)),
                (unsigned int) (NetworkConfig_Ipv4Byte(myIpGet.ipV4, 0)));

        network_init = true;
    }
    else
    {
        printf("[NCA] : NetworkConfig_GetIpSettings API has failed\n\r");
    }
#endif /* (TWO == NCI_DHCP_MODE) */

#if 0
    /* ************************************************************************/
    /*   Disconnect
     **************************************************************************/
    static_assert((portTICK_RATE_MS != 0), "Tick rate MS is zero");
    vTaskDelay((portTickType) DELAY_5_SEC / portTICK_RATE_MS);

    retStatus[8] = WlanConnect_Disconnect(NULL);

    if (RETCODE_OK == retStatus[8])
    {
        /* Disconnected successfully */
        printf("[NCA] : Disconnection successfully.\n\r");
    }
    else
    {
        printf("[NCA] : Disconnection has failed.\n\r");
    }

    printf("[NCA] : ###################################################\n\r");
    printf("[NCA] : ## User can configure the application by using : ##\n\r");
    printf("[NCA] : ##                  NCI_DHCP_MODE                ##\n\r");
    printf("[NCA] : ###################################################\n\r");
#endif
}

/******************************************************************************/
/**
 * @brief        Local function for scanning networks. This function also calls
 *               the setAndGetIp function in order to set/get IP settings
 *               and connect/disconnect from a network.
 * @param        none.
 *
 * @return       none.
 ******************************************************************************/
static void scanNetwork(void)
{
    /* local variables */
    Retcode_T retScanStatus[MAX_SIZE_BUFFER];
    WlanConnect_ScanInterval_T scanInterval = SCAN_INTERVAL;
    WlanConnect_ScanList_T scanList;

    /* Turn on led before scanning*/
    if (RETCODE_OK != BSP_LED_SwitchAll((uint32_t) BSP_LED_COMMAND_ON))
    {
        printf("LED's ON Failed \n\r");
    }

    retScanStatus[0] = WlanConnect_ScanNetworks(scanInterval, &scanList);

    if (RETCODE_OK == retScanStatus[0])
    {
        printf("\n\r[NCA] : Hey User! XDK found the following networks:\n\r");

        for (int i = ZERO; i < WLANCONNECT_MAX_SCAN_INFO_BUF; i++)
        {
            if (ZERO != scanList.ScanData[i].SsidLength)
            {
                printf("[NCA] :  - found SSID number %d is : %s\n\r", i,
                        scanList.ScanData[i].Ssid);
                static_assert((portTICK_RATE_MS != 0), "Tick rate MS is zero");
                vTaskDelay((portTickType) DELAY_500_MSEC / portTICK_RATE_MS);
            }
        }
        printf("[NCA] : Finished scan successfully \n\r");
    }
    else if ((Retcode_T) RETCODE_NO_NW_AVAILABLE == Retcode_GetCode(retScanStatus[0]))
    {
        printf("[NCA] : Scan function did not found any network\n\r");
    }
    else /*(RETCODE_FAILURE == retScanStatus[0])*/
    {
        printf("[NCA] : Scan function failed\n\r");
    }
    /* Turn off the led when the scan has finished and results are printed */
    if (RETCODE_OK != BSP_LED_SwitchAll((uint32_t) BSP_LED_COMMAND_OFF))
    {
        printf("LED's OFF failed\n\r");
    }
}

/******************************************************************************/
/**
 * @brief        This function calls the
 *               scanNetwork and the setAndGetIp function
 * @param[in]   *param1: a generic pointer to any context data structure which will be passed to the function when it is invoked by the command processor.
 *
 * @param[in]    param2: a generic 32 bit value  which will be passed to the function when it is invoked by the command processor.
 */
static void ScanAndGetIp(void * param1, uint32_t param2)
{
    BCDS_UNUSED(param1);
    BCDS_UNUSED(param2);
    /* Scan for networks*/
    scanNetwork();
    /* Set IP, Connect, Get IP and Disconnect*/
    setAndGetIp();
    /*start the wifi n/w scan timer*/
    if ( xTimerStart(ScanwifiNetworkHandle, TIMER_BLOCK_TIME) != pdTRUE)
    {
        printf("Timer Failed to start \r\n");
        assert(false);
    }
}

bool once = false;
/**
 * @brief        This is a application timer callback function used to enqueue EnqueueScanwifiNetwork function
 *               to the command processor.
 *
 * @param[in]    pvParameters unused parameter.
 */
static void EnqueueScanwifiNetwork(void *pvParameters)
{
    BCDS_UNUSED(pvParameters);

    if (!once) {
    	once = true;
    	Retcode_T retVal = CmdProcessor_enqueue(AppCmdProcessorHandle, ScanAndGetIp, NULL, UINT32_C(0));
		if (RETCODE_OK != retVal)
		{
			printf("Failed to Enqueue EnqueueScanwifiNetwork to Application Command Processor \r\n");
		}
    }
}

/******************************************************************************/
/**
 *  @brief       Function to Initialize the Network Configuration Abstraction
 *               application. Create timer task to start scanNetwork
 *               function after one second.
 *  @param       none
 *
 *  @returns     none
 ******************************************************************************/
static void init(void)
{
    Retcode_T retval = RETCODE_OK;
    static_assert((portTICK_RATE_MS != 0), "Tick rate MS is zero");

    retval = WlanConnect_Init();

    if (RETCODE_OK == retval)
    {
        /* create timer task to get and transmit accel data via BLE for every one second automatically*/
        ScanwifiNetworkHandle = xTimerCreate((char * const ) "ScanwifiNetwork", pdMS_TO_TICKS(WIFI_NW_SCAN_DELAY), TIMER_AUTORELOAD_OFF, NULL, EnqueueScanwifiNetwork);
    }
    if ((RETCODE_OK != retval) || (NULL == ScanwifiNetworkHandle))
    {
        printf("Failed to initialize because of Wifi/Command Processor/Timer Create fail \r\n");
        assert(false);
    }
    /*start the wifi n/w scan timer*/
    if ( xTimerStart(ScanwifiNetworkHandle, TIMER_BLOCK_TIME) != pdTRUE)
    {
        printf("Timer Failed to start \r\n");
        assert(false);
    }
}



xTaskHandle Application_gdt; 

/*
 * @brief Application to print "hello world" on serial console.
 */
void mist_task_init(void * pvParameters)
{
	(void) pvParameters;
#if 0
	/*
	 * Fill stack with nonsense
	 */
	size_t sz = 12*1024;
	uint8_t buffer[sz];
	memset(buffer, 255, sz);
#endif
	//my_spiffs_erase_area();

	while (!network_init) {
		vTaskDelay((portTickType) 1000 / portTICK_RATE_MS);
	}

	port_main();


}

/* global functions ********************************************************* */


/**
 * HEAP CONFIGURATION should be done in /opt/XDK-Workbench/SDK/xdk110/Common/config/FreeRTOSConfig.h
 *
 * configTOTAL_HEAP_SIZE
 *
 * 96 kbytes seems to be OK. The stack of the Mist task will be allocated from the heap.
 */

/**
 * This calculates the stack size taking into account the width of the stack (seems to be uint16_t on XDK)
 */
#define CALC_STACKSIZE(kbytes) ((size_t) ((kbytes/(sizeof (portSTACK_TYPE))) * 1024 ))
/** This sets the stack size of the Mist task, kbytes */
#define MIST_TASK_STACKSIZE CALC_STACKSIZE(40) //kbytes


/******************************************************************************/
/**
 * @brief        System initialization function, This function is called by the
 *               system at the beginning of the startup procedure.
 *               Called only once
 *
 * @param  [in]  xTimer
 *               The name of the OS timer handle.
 *
 * @returns      none.
 *
 ******************************************************************************/
void appInitSystem(void * CmdProcessorHandle, uint32_t param2)
{
    if (CmdProcessorHandle == NULL)
    {
        printf("Command processor handle is null \n\r");
        assert(false);
    }
    AppCmdProcessorHandle = (CmdProcessor_T *)CmdProcessorHandle;
    BCDS_UNUSED(param2);
    init();

    xTaskCreate(mist_task_init, (const char * const) "Mist task", MIST_TASK_STACKSIZE, NULL,1,&Application_gdt);
}
/**@} */
/** ************************************************************************* */
