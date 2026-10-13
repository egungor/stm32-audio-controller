/**
 * @file    ui_demo.h
 * @brief   Stand-in for the Windows host until USB HID exists (M3).
 *
 * Feeds the UI a fixed device list and confirms every selection after a
 * short delay, as the host will do with DEVICE_CHANGED. Logged as
 * "UI DEMO" so a demo build is never mistaken for a connected host.
 * Remove when the HID transport (M3) drives the UI.
 */
#ifndef UI_DEMO_H
#define UI_DEMO_H

#ifdef __cplusplus
extern "C" {
#endif

/** Build the UI with demo data. Call after UiPort_Init(). */
void UiDemo_Start(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_DEMO_H */
