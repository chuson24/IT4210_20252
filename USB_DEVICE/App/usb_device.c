/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usb_device.c
  * @version        : v1.0_Cube
  * @brief          : This file implements the USB Device
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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

#include "usb_device.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_hid.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

/* USER CODE END PV */

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PFP */

/* USB Device Core handle declaration. */
USBD_HandleTypeDef hUsbDeviceHS;

/*
 * -- Insert your variables declaration here --
 */
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*
 * -- Insert your external function declaration here --
 */
/* USER CODE BEGIN 1 */

/**
 * @brief  Send a standard 4-byte HID mouse report.
 *
 * The report layout follows the USB HID boot-protocol mouse descriptor:
 *   Byte 0 – button bitmask  (bit 0 = left, bit 1 = right, bit 2 = middle)
 *   Byte 1 – relative X      (int8_t, -127..127)
 *   Byte 2 – relative Y      (int8_t, -127..127)
 *   Byte 3 – scroll wheel    (int8_t, positive = scroll down / away from user)
 *
 * The function is non-blocking: it checks that the HID endpoint is idle before
 * queueing the report.  Returns 0 immediately when USB is not yet configured or
 * the endpoint is busy so callers can retry on the next tick.
 */
uint8_t USB_Mouse_TrySend(uint8_t buttons, int8_t deltaX, int8_t deltaY, int8_t scroll)
{
  USBD_HID_HandleTypeDef *hid;
  uint8_t report[4];

  if (hUsbDeviceHS.dev_state != USBD_STATE_CONFIGURED)
  {
    return 0;
  }

  hid = (USBD_HID_HandleTypeDef *)hUsbDeviceHS.pClassData;
  if ((hid == NULL) || (hid->state != USBD_HID_IDLE))
  {
    return 0;
  }

  report[0] = buttons;
  report[1] = (uint8_t)deltaX;
  report[2] = (uint8_t)deltaY;
  report[3] = (uint8_t)scroll;  /* Scroll wheel — was always 0 before */

  (void)USBD_HID_SendReport(&hUsbDeviceHS, report, sizeof(report));
  return 1;
}

/* USER CODE END 1 */

/**
  * Init USB device Library, add supported class and start the library
  * @retval None
  */
void MX_USB_DEVICE_Init(void)
{
  /* USER CODE BEGIN USB_DEVICE_Init_PreTreatment */

  /* USER CODE END USB_DEVICE_Init_PreTreatment */

  /* Init Device Library, add supported class and start the library. */
  if (USBD_Init(&hUsbDeviceHS, &HS_Desc, DEVICE_HS) != USBD_OK)
  {
    Error_Handler();
  }
  if (USBD_RegisterClass(&hUsbDeviceHS, &USBD_HID) != USBD_OK)
  {
    Error_Handler();
  }
  if (USBD_Start(&hUsbDeviceHS) != USBD_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN USB_DEVICE_Init_PostTreatment */

  /* USER CODE END USB_DEVICE_Init_PostTreatment */
}

/**
  * @}
  */

/**
  * @}
  */

