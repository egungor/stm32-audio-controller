# Windows Host Application

The Windows host application will:

1. Detect the STM32 HID device.
2. Enumerate Windows audio render endpoints.
3. Maintain a host-side audio device model.
4. Change the default playback endpoint.
5. Monitor endpoint/default-device changes.
6. Communicate state to the STM32.

Target language: C++17.

Windows-specific APIs should be isolated behind small interfaces so that the
application logic remains testable.
