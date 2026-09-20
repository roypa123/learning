/* ===========================================================================
 *  nimbus/drivers/keyboard.c  --  PS/2, scancode set 1
 * ===========================================================================
 *
 *  A keyboard does not send characters. It sends *scancodes*: one number when
 *  a key goes down and a different number when it comes up, identifying a
 *  physical switch on the board and nothing else. Key 30 is "the key below Tab
 *  and left of S", which on a US layout is A, on a French one is Q, and on a
 *  Dvorak one is also A because Dvorak is remapped in software.
 *
 *  Turning switch positions into text is therefore a software problem with no
 *  correct answer, only conventions. This file implements the simplest useful
 *  set of them: scancode set 1, a US layout, shift, caps lock and control.
 *
 *  Explained in: docs/19-keyboard.md
 * =========================================================================== */

#include <nimbus/keyboard.h>
#include <nimbus/console.h>
#include <nimbus/irq.h>
#include <nimbus/isr.h>
#include <nimbus/io.h>
#include <nimbus/kernel.h>
#include <nimbus/sched.h>

/* ---------------------------------------------------------------------------
 *  The keymaps
 *
 *  Index by scancode, get a character. Two tables, because shift changes 47 of
 *  them and a branch per key is cheaper than a table lookup plus a conditional
 *  transformation that has to know about digits, punctuation and letters
 *  separately.
 *
 *  Entry 0 means "no character": either an unused scancode or a key that does
 *  not produce text, which the switch below handles by name.
 * ------------------------------------------------------------------------- */
static const char keymap_normal[128] = {
    0,   27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t','q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0,   '*', 0,   ' ',
    /* the rest -- function keys, keypad, and a lot of nothing -- stay zero */
};

static const char keymap_shift[128] = {
    0,   27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t','Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',
    0,   '*', 0,   ' ',
};

/*  Scancodes for the keys we track by identity rather than by character.      */
#define SC_ESCAPE     0x01
#define SC_LSHIFT     0x2A
#define SC_RSHIFT     0x36
#define SC_LCTRL      0x1D
#define SC_LALT       0x38
#define SC_CAPSLOCK   0x3A
#define SC_F1         0x3B
#define SC_NUMLOCK    0x45

/*  Extended keys -- arrows, Delete, the right-hand Ctrl and Alt -- arrive as
 *  two bytes: 0xE0 then the scancode. The prefix exists because scancode set 1
 *  ran out of single-byte codes when the 101-key keyboard arrived in 1986 and
 *  IBM needed backwards compatibility with software that read one byte.       */
#define SC_EXTENDED   0xE0

#define SC_RELEASE    0x80   /* bit 7 set = the key was released               */

/* ---------------------------------------------------------------------------
 *  A ring buffer between the interrupt and whoever is reading
 *
 *  The IRQ handler cannot block, cannot allocate, and must return in
 *  microseconds. The reader may be a task that is asleep. A fixed-size ring
 *  buffer is the standard bridge: the producer writes and advances head, the
 *  consumer reads and advances tail, and neither ever waits for the other.
 *
 *  On one CPU with the producer in an interrupt handler, correctness needs
 *  only that the consumer disables interrupts while it touches the indices --
 *  there is no other writer to race with.
 * ------------------------------------------------------------------------- */
#define KBD_BUFFER_SIZE 128

static volatile int      kbd_buffer[KBD_BUFFER_SIZE];
static volatile uint32_t kbd_head = 0;
static volatile uint32_t kbd_tail = 0;

static uint8_t modifiers = 0;
static bool    extended  = false;

static void kbd_push(int key)
{
    uint32_t next = (kbd_head + 1) % KBD_BUFFER_SIZE;

    /*  Full: drop the new key rather than overwrite the oldest. Dropping the
     *  newest loses the character you just typed; dropping the oldest loses
     *  one you typed earlier and already saw echoed, which is worse. Either
     *  way a full 128-entry keyboard buffer means something is very wrong. */
    if (next == kbd_tail) return;

    kbd_buffer[kbd_head] = key;
    kbd_head = next;
}

/* ---------------------------------------------------------------------------
 *  The interrupt handler
 * ------------------------------------------------------------------------- */
static void keyboard_callback(registers_t *regs UNUSED)
{
    uint8_t scancode = inb(PS2_DATA);

    /*  Reading port 0x60 is what *clears* the interrupt at the keyboard
     *  controller. Miss it and IRQ 1 never fires again -- the keyboard appears
     *  to die after exactly one keystroke, which is the classic symptom of a
     *  handler that returns early on some path.                               */

    if (scancode == SC_EXTENDED) {
        extended = true;
        return;
    }

    bool released = (scancode & SC_RELEASE) != 0;
    uint8_t code  = scancode & 0x7F;

    if (extended) {
        extended = false;
        if (!released) {
            switch (code) {
            case 0x48: kbd_push(KEY_UP);       return;
            case 0x50: kbd_push(KEY_DOWN);     return;
            case 0x4B: kbd_push(KEY_LEFT);     return;
            case 0x4D: kbd_push(KEY_RIGHT);    return;
            case 0x47: kbd_push(KEY_HOME);     return;
            case 0x4F: kbd_push(KEY_END);      return;
            case 0x49: kbd_push(KEY_PAGEUP);   return;
            case 0x51: kbd_push(KEY_PAGEDOWN); return;
            case 0x52: kbd_push(KEY_INSERT);   return;
            case 0x53: kbd_push(KEY_DELETE);   return;
            default: return;
            }
        }
        /* extended release: the right Ctrl and Alt come through here */
        if (code == SC_LCTRL) modifiers &= ~MOD_CTRL;
        if (code == SC_LALT)  modifiers &= ~MOD_ALT;
        return;
    }

    /* ---- modifier keys: state, not characters ----------------------------- */
    switch (code) {
    case SC_LSHIFT:
    case SC_RSHIFT:
        if (released) modifiers &= ~MOD_SHIFT; else modifiers |= MOD_SHIFT;
        return;
    case SC_LCTRL:
        if (released) modifiers &= ~MOD_CTRL;  else modifiers |= MOD_CTRL;
        return;
    case SC_LALT:
        if (released) modifiers &= ~MOD_ALT;   else modifiers |= MOD_ALT;
        return;
    case SC_CAPSLOCK:
        /*  Caps lock toggles on press and does nothing on release. Shift is
         *  the opposite: it is a state, not a toggle. Treating them the same
         *  way is why some hobby kernels have a caps lock that only works
         *  while held down.                                                   */
        if (!released) modifiers ^= MOD_CAPS;
        return;
    default:
        break;
    }

    if (released) return;        /* we do not report key-up for normal keys */

    if (code >= 128) return;

    /* ---- function keys ---------------------------------------------------- */
    if (code >= SC_F1 && code < SC_F1 + 10) {
        kbd_push(KEY_F1 + (code - SC_F1));
        return;
    }

    /* ---- ordinary characters ---------------------------------------------- */
    bool shift = (modifiers & MOD_SHIFT) != 0;
    char c = shift ? keymap_shift[code] : keymap_normal[code];
    if (c == 0) return;

    /*  Caps lock affects letters only -- not digits, not punctuation. That is
     *  why it cannot be implemented by just setting the shift flag, which is
     *  the shortcut that makes caps lock turn 1 into !.                       */
    if (modifiers & MOD_CAPS) {
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        else if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    }

    /*  Ctrl maps a letter to control code 1-26: Ctrl-A is 1, Ctrl-C is 3,
     *  Ctrl-D is 4. That mapping is not a convention invented by terminals --
     *  it is literally the ASCII table, where the control characters occupy
     *  the same low five bits as their letters. Ctrl clears bit 6 and 7.      */
    if (modifiers & MOD_CTRL) {
        if (c >= 'a' && c <= 'z')      c = (char)(c - 'a' + 1);
        else if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 1);
        else if (c == '[')             c = 27;
    }

    kbd_push((int)(unsigned char)c);

    /*  Hand it to the console, which does echoing and line editing. Doing that
     *  work here would tangle a hardware driver with terminal semantics; doing
     *  it in the reader would mean nothing is echoed until someone reads.     */
    console_input((int)(unsigned char)c);

    /*  Wake anyone blocked waiting for a keystroke. Waking from an interrupt
     *  handler is safe -- it only moves a task onto the run queue -- whereas
     *  switching to it from here would not be.                                */
    sched_wake((void *)&kbd_buffer);
}

int keyboard_getkey_nonblock(void)
{
    uint32_t flags = irq_save();

    int key = -1;
    if (kbd_tail != kbd_head) {
        key = kbd_buffer[kbd_tail];
        kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
    }

    irq_restore(flags);
    return key;
}

int keyboard_getkey(void)
{
    for (;;) {
        int key = keyboard_getkey_nonblock();
        if (key >= 0) return key;

        /*  Nothing waiting. Block on the buffer's address as a wait channel.
         *  Before the scheduler exists this would spin forever, so sched_block
         *  falls back to `hlt` in that case -- which parks the CPU until the
         *  next interrupt, and the next interrupt might be the key we want.   */
        sched_block((void *)&kbd_buffer);
    }
}

uint8_t keyboard_modifiers(void) { return modifiers; }

void keyboard_init(void)
{
    /*  Drain whatever the BIOS left in the controller's output buffer. If a
     *  byte is sitting there when we unmask IRQ 1, no new interrupt will ever
     *  be generated -- the controller will not raise the line again until the
     *  pending byte is read -- and the keyboard is dead before it starts.     */
    while (inb(PS2_STATUS) & 0x01)
        (void)inb(PS2_DATA);

    kbd_head = kbd_tail = 0;
    modifiers = 0;
    extended = false;

    irq_register(IRQ_KEYBOARD, keyboard_callback);
    LOG_INFO("keyboard: ps/2 set 1, us layout");
}
