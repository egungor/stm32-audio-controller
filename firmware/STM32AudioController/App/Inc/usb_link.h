/**
 * @file    usb_link.h
 * @brief   Vendor-defined USB HID interface carrying protocol frames
 *          (docs/decisions/ADR-003-usb-hid-interface.md).
 *
 * One 64-byte input report (device -> host) and one 64-byte output report
 * (host -> device), no report ID; each report is one protocol frame.
 *
 * Received frames are copied into a FreeRTOS queue by the USB interrupt;
 * the application task takes them with UsbLink_Receive(). UsbLink_Send()
 * is called from the same task. Nothing here blocks in interrupt context
 * or allocates memory.
 */
#ifndef USB_LINK_H
#define USB_LINK_H

#include <stdbool.h>
#include <stdint.h>

#include "protocol.h"
#include "usbd_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Frames buffered between the USB interrupt and the task. */
#define USB_LINK_RX_DEPTH 8U

typedef struct
{
    uint32_t rx_frames;
    uint32_t rx_dropped;  /**< queue full or wrong length */
    uint32_t tx_frames;
    uint32_t tx_timeouts;
} UsbLinkStats;

/**
 * @brief  Initialise the USB device stack with this class and start it.
 *
 * Called from the USB_DEVICE_Init_PreTreatment USER CODE section of
 * MX_USB_DEVICE_Init() (task context), which then returns before the
 * generated demo HID class is registered.
 */
void UsbLink_DeviceInit(USBD_HandleTypeDef *pdev);

/** True while the host has configured the device. */
bool UsbLink_IsConfigured(void);

/**
 * @brief  Take the next received frame.
 * @return false if none arrived within @p timeout_ms (0: do not wait).
 */
bool UsbLink_Receive(uint8_t frame[PROTO_FRAME_SIZE], uint32_t timeout_ms);

/**
 * @brief  Send one frame as an input report.
 * @return false if not configured or the previous report was not
 *         collected by the host within @p timeout_ms.
 */
bool UsbLink_Send(const uint8_t frame[PROTO_FRAME_SIZE], uint32_t timeout_ms);

void UsbLink_GetStats(UsbLinkStats *stats);

#ifdef __cplusplus
}
#endif

#endif /* USB_LINK_H */
