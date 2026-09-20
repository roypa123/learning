/* ===========================================================================
 *  nimbus/include/nimbus/keyboard.h  --  PS/2 keyboard, scancode set 1
 * ===========================================================================
 *  Explained in: docs/19-keyboard.md
 * =========================================================================== */
#ifndef NIMBUS_KEYBOARD_H
#define NIMBUS_KEYBOARD_H

#include <nimbus/types.h>

#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define PS2_COMMAND 0x64

/*  Keys that are not characters get codes above 0xFF, so that a single int can
 *  carry either "the letter a" or "the up arrow" without a second channel.    */
#define KEY_ESCAPE    0x100
#define KEY_UP        0x101
#define KEY_DOWN      0x102
#define KEY_LEFT      0x103
#define KEY_RIGHT     0x104
#define KEY_HOME      0x105
#define KEY_END       0x106
#define KEY_PAGEUP    0x107
#define KEY_PAGEDOWN  0x108
#define KEY_INSERT    0x109
#define KEY_DELETE    0x10A
#define KEY_F1        0x110   /* F1..F12 are 0x110..0x11B                     */

#define MOD_SHIFT     0x01
#define MOD_CTRL      0x02
#define MOD_ALT       0x04
#define MOD_CAPS      0x08
#define MOD_NUMLOCK   0x10

void keyboard_init(void);

/*  Non-blocking: returns -1 if the buffer is empty. Used before the scheduler
 *  exists.                                                                    */
int  keyboard_getkey_nonblock(void);

/*  Blocking: sleeps on the keyboard's wait channel until a key arrives, so the
 *  CPU is free while nobody is typing. This is the difference between a kernel
 *  and a polling loop.                                                        */
int  keyboard_getkey(void);

uint8_t keyboard_modifiers(void);

#endif /* NIMBUS_KEYBOARD_H */
