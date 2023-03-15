/*******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for the PSoC6 MCU Transition To RMA
*              example for ModusToolbox.
*
* Related Document: See README.md
*
*
********************************************************************************
* Copyright 2021-2023, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

/*******************************************************************************
* Header Files
*******************************************************************************/
#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

/*******************************************************************************
* Macros
*******************************************************************************/
#define CY_TRANSIT_TO_RMA_OPCODE            (0x3B000000UL)                      /* The SROM API opcode for RMA lifcycle stage conversion .
                                                                                   Refer 'System Call APIs' section in device Technical Reference Manual*/
#define CY_OPCODE_SUCCESS                   (0xA0000000UL)                      /* The command completed with no errors */
#define CY_OPCODE_STS_Msk                   (0xF0000000UL)                      /* The status mask of the SROM API return value */

#define GPIO_INTERRUPT_PRIORITY (7u)
#define CY_IPC_STRUCT                       (Cy_IPC_Drv_GetIpcBaseAddress(CY_IPC_CHAN_SYSCALL))
#define CY_IPC_NOTIFY_STRUCT0               (0x1UL << CY_IPC_INTR_SYSCALL1)

#define SUCCESS                             (0)
#define FAILED                              (!SUCCESS)
#define JWT_LENGTH                          (367)                               /* Length of the JWT */
#define LIFECYCLE_EFUSE_OFFSET              (0x02B)

/* Refer to Appendix B of the 'PSoC 6 MCU Programming Specifications' (002-15554) for efuse mapping*/
#define NORMAL_STAGE_EFUSE_BIT               0
#define SECURE_STAGE_EFUSE_BIT               2
#define SECURE_DEB_STAGE_EFUSE_BIT           1
#define RMA_STAGE_EFUSE_BIT                  3

#define DELAY_BUTTON_DEBOUNCE_MS             500u                                /*delay in millisecond*/
#define IPC_STATUS_WAIT_TIME_S               60u                                 /*maximum wait time for IPC Lock*/
#define DELAY_IPC_STATUS_CHECK_MS            1000u                               /*delay for checking the IPC status*/
/*******************************************************************************
* Structures
*******************************************************************************/
typedef struct
{
    uint32_t opCode;
    uint32_t api_param_addr;
}transit_to_RMA_param_t;


typedef struct
{
    uint32_t length;
    unsigned char jwt[JWT_LENGTH];
}jwt_param_t;


/*******************************************************************************
* Function Definitions
*******************************************************************************/
/*******************************************************************************
* Function Name: TransitionToRMA
********************************************************************************
* Summary:
* This function implements the 'TransitionToRMA' syscall
*
* Parameters:
*  transit_to_RMA_param_t
*
* Return:
*  uint32_t
*
*******************************************************************************/
uint32_t TransitionToRMA(transit_to_RMA_param_t *paramRMA)
{
    uint32_t result = FAILED;
    uint32_t elapsedTime =0;      //count number of seconds elapsed

    /* Send the IPC message */
    if (Cy_IPC_Drv_SendMsgPtr(CY_IPC_STRUCT, CY_IPC_NOTIFY_STRUCT0, (void*)paramRMA) == CY_IPC_DRV_SUCCESS)
    {
        /* Wait for the IPC structure to be freed */
         while (elapsedTime < IPC_STATUS_WAIT_TIME_S)
         {
              if (Cy_IPC_Drv_IsLockAcquired(CY_IPC_STRUCT) == false)
              {
                   break;
              }
              //Delay by 1s before checking again
              Cy_SysLib_Delay(DELAY_IPC_STATUS_CHECK_MS);
              elapsedTime++;
         }


        /* The result of the SROM API call is returned to the opcode variable */
        if ((paramRMA->opCode & CY_OPCODE_STS_Msk) == CY_OPCODE_SUCCESS)
        {
            result = SUCCESS;
        }
        else
        {
            result = FAILED;
        }
    }
    else
    {
        result = FAILED;
    }

    return result;
}
/*******************************************************************************
* Function Name: ReadDeviceLifecycleStage
********************************************************************************
* Summary:
* This function reads and prints the efuse lifecycle stage
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void ReadDeviceLifecycleStage()
{
    uint32_t bit_secure_stage = offsetof(cy_stc_efuse_data_t, LIFECYCLE_STAGE.SECURE);
    uint32_t bit_normal_stage = offsetof(cy_stc_efuse_data_t, LIFECYCLE_STAGE.NORMAL);
    uint32_t bit_secure_debug_stage = offsetof(cy_stc_efuse_data_t, LIFECYCLE_STAGE.SECURE_WITH_DEBUG);
    uint32_t bit_secure_rma = offsetof(cy_stc_efuse_data_t, LIFECYCLE_STAGE.RMA);

    bool bitVal_secure, bitVal_normal, bitVal_secDbg, bitVal_RMA;

    Cy_EFUSE_GetEfuseBit(bit_normal_stage, &bitVal_normal);
    Cy_EFUSE_GetEfuseBit(bit_secure_debug_stage, &bitVal_secDbg);
    Cy_EFUSE_GetEfuseBit(bit_secure_stage, &bitVal_secure);
    Cy_EFUSE_GetEfuseBit(bit_secure_rma, &bitVal_RMA);

    if(bitVal_RMA)
    {
        printf("\r\nLife Cycle Stage : RMA\r\n");
    }
    else if((bitVal_secDbg) || (bitVal_secure))
    {
        if(bitVal_secDbg)
        {
            printf("\r\nLife Cycle Stage : SECURE_DEBUG\r\n");
        }
        else
        {
            printf("\r\nLife Cycle Stage : SECURE\r\n");
        }
    }
    else
    {
        printf("\r\nLife Cycle Stage : NORMAL\r\n");
    }
}

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function for CPU. It transitions the PSoC 64 MCU into RMA mode
* when the user button is pressed
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;
    uint32_t rmaApiResult= FAILED;
    uint32_t length;
    transit_to_RMA_param_t transitToRmaParam;
    jwt_param_t jwt_param;

    //NOTE: See the readme file for instructions to generate this JWT
     char jwt[] = "eyJhbGciOiJFUzI1NiIsInR5cCI6IkpXVCJ9.eyJhdXRoIjp7ImRpZV9pZCI"
               "6eyJtYXgiOnsiZGF5IjoyNTUsImxvdCI6MTY3NzcyMTUsIm1vbnRoIjoyNTUsIndh"
               "ZmVyIjoyNTUsInhwb3MiOjI1NSwieWVhciI6MjU1LCJ5cG9zIjoyNTV9LCJtaW4iO"
               "nsiZGF5IjowLCJsb3QiOjAsIm1vbnRoIjowLCJ3YWZlciI6MCwieHBvcyI6MCwieW"
               "VhciI6MCwieXBvcyI6MH19fX0.z6ePvuJTcY0z3azJFGpzcq0-4bxxpgfL7H-E4V-"
               "Dg6UGpwpLqf8pFXdMIXNXbQKCYW1Pq5HM7npZXNTUDtgEEw";


#if defined (CY_DEVICE_SECURE)
    cyhal_wdt_t wdt_obj;

    /* Clear watchdog timer so that it doesn't trigger a reset */
    result = cyhal_wdt_init(&wdt_obj, cyhal_wdt_get_max_timeout_ms());
    CY_ASSERT(CY_RSLT_SUCCESS == result);
    cyhal_wdt_free(&wdt_obj);
#endif

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize the user button */
    result = cyhal_gpio_init(CYBSP_USER_BTN, CYHAL_GPIO_DIR_INPUT,
                             CYHAL_GPIO_DRIVE_PULLUP, CYBSP_BTN_OFF);


    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port */
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);

    printf("\r\n***** PSoC 6 MCU : TransitionToRMA ***** \r\n\n");


    /* Read the Device Lifecycle */
    ReadDeviceLifecycleStage();

    /* Prepare the sycall params */
    transitToRmaParam.opCode = CY_TRANSIT_TO_RMA_OPCODE;
    transitToRmaParam.api_param_addr = (uint32_t) &jwt_param;

    length = (uint32_t)strlen(jwt);

    jwt_param.length = length;

    for(uint32_t jwtIdx = 0; jwtIdx < length; jwtIdx++)
    {
        jwt_param.jwt[jwtIdx] = jwt[jwtIdx];
    }

    printf("\r\n**Warning : After Device is in RMA, it cannot be moved to other LCS** \r\n");
    printf("\r\nWaiting for User button (SW2) press to transition device to RMA \r\n");

    for (;;)
    {
        /* Read current button state from the user button */
        if (cyhal_gpio_read(CYBSP_USER_BTN) == false)
        {
             /*500ms delay for button debounce on button press*/
             Cy_SysLib_Delay(DELAY_BUTTON_DEBOUNCE_MS);

             /*Execute TransitionToRMA syscall*/
            rmaApiResult = TransitionToRMA(&transitToRmaParam);
            if (rmaApiResult == SUCCESS)
            {
                printf("\r\nTransition to RMA successful\r\n");
            }
            else
            {
                printf("\r\nTransition to RMA failed\r\n");
            }
        }
    }
}


/* [] END OF FILE */
