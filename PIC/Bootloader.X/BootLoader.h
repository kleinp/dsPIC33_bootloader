#ifndef __BOOTLOADER_H__
#define __BOOTLOADER_H__

void JumpToApp(void);
BOOL ValidAppPresent(void);
void blinkLEDs(void);

#endif
