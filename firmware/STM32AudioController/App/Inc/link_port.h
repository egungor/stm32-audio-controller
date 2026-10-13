/**
 * @file    link_port.h
 * @brief   Connects the protocol client (firmware/link) to the USB HID
 *          interface (usb_link.c) and the UI (firmware/ui).
 *
 * Call LinkPort_Init() after Ui_Init() and LinkPort_Run() from the UI
 * task loop; both use LVGL through the UI module, so they must run in the
 * task that calls lv_timer_handler().
 */
#ifndef LINK_PORT_H
#define LINK_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

/** Build the UI with the host link as its data source. */
void LinkPort_Init(void);

/** Handle USB state changes, received frames, retries and timeouts. */
void LinkPort_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* LINK_PORT_H */
