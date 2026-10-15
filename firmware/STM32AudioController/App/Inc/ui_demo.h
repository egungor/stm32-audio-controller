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

/** Flip button: turn the display and keep the UI's settings record (the
 *  settings page's switch) in step. */
void UiDemo_ToggleFlip(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_DEMO_H */
