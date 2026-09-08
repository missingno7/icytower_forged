/* menu_keys.c -- key_to_str(), the options screen's scancode-to-label
 * table.
 *
 * Original source: F:\projects\icytower\trunk\source\menu.c.
 *
 * PROMOTIONS.md batch 15, target 3's first (and cheapest) function: it
 * is the only one of the five menu-screen functions that calls NOTHING
 * at all -- a census of `call` sites in artifacts/disasm.txt
 * 0x416a9c..0x41748a returns zero -- so it is pure in the strongest
 * sense the project has, and its oracle is a plain vector sweep over
 * every scancode.
 *
 * Domain: the CONTENT of the string written to `dest`, plus the length
 * (batch 13's get_version_str convention, reused).
 *
 * How the table below was produced
 * --------------------------------
 * NOT by reading 108 blocks by eye.  The 2543-byte body is a compare
 * chain followed by 108 tiny write blocks, and each block is one of
 * three shapes -- immediate `movb`/`movw`/`movl` stores into (%ebx), a
 * `rep movsb` from a .rdata address, or GCC's alignment-aware
 * `movsb`/`movsw`/`rep movsl` copy idiom.  A throwaway decoder walked
 * the chain, decoded every block into the bytes it writes, and read the
 * .rdata sources out of the image with pefile; this file is its output,
 * reviewed against the disassembly for the handful of blocks that are
 * not literally a strcpy of a literal (there are none -- every one is).
 * The decoder is not shipped: it is scaffolding, and the table it
 * produced IS the evidence.
 *
 * Three findings, all of which survive into the text the player sees:
 *
 *  1. KEY_R's label is the LOWERCASE "r" (0x416f28 stores 0x0072), the
 *     single exception among the twenty-six letters.  A typo in the
 *     original, kept.
 *  2. KEY_BACKSLASH and KEY_BACKSLASH2 both render as "\" -- two
 *     physically different keys the options screen cannot tell apart.
 *  3. KEY_TILDE renders as the literal word "TILDE", in capitals, while
 *     every other punctuation key renders as its own glyph.
 *
 * Why an if/else chain and not a `switch`
 * ---------------------------------------
 * The object code tests the values in SOURCE order, and that order is
 * not sorted: KEY_SEMICOLON (105) is tested between KEY_COLON (68) and
 * KEY_QUOTE (69), where the author evidently grouped the punctuation
 * keys by keyboard position.  GCC expands a `switch` by value (jump
 * table or sorted binary search) and would have destroyed that order;
 * an if/else chain preserves it.  So the original is an if/else chain,
 * and the out-of-order 105 is the proof.  The nineteen scancodes with no
 * branch of their own -- KEY_ABNT_C1 (94), KEY_CONVERT..KEY_BACKQUOTE
 * (97..104) and KEY_COMMAND..KEY_UNKNOWN8 (106..114) -- fall through to
 * the final else, as does 0 and anything above KEY_CAPSLOCK.
 *
 * `dest` is written with strcpy and never bounds-checked; the longest
 * label is "Print Screen" (13 bytes with its NUL), which is what a
 * caller's buffer has to be able to hold.
 */
#include "game_types.h"
#include "game_state.h"
#include "game_funcs.h"
#include "allegro_api.h"

#include <string.h>

void key_to_str(int k, char *dest)
{
    if (k == KEY_A)                   strcpy(dest, "A");
    else if (k == KEY_B)              strcpy(dest, "B");
    else if (k == KEY_C)              strcpy(dest, "C");
    else if (k == KEY_D)              strcpy(dest, "D");
    else if (k == KEY_E)              strcpy(dest, "E");
    else if (k == KEY_F)              strcpy(dest, "F");
    else if (k == KEY_G)              strcpy(dest, "G");
    else if (k == KEY_H)              strcpy(dest, "H");
    else if (k == KEY_I)              strcpy(dest, "I");
    else if (k == KEY_J)              strcpy(dest, "J");
    else if (k == KEY_K)              strcpy(dest, "K");
    else if (k == KEY_L)              strcpy(dest, "L");
    else if (k == KEY_M)              strcpy(dest, "M");
    else if (k == KEY_N)              strcpy(dest, "N");
    else if (k == KEY_O)              strcpy(dest, "O");
    else if (k == KEY_P)              strcpy(dest, "P");
    else if (k == KEY_Q)              strcpy(dest, "Q");
    else if (k == KEY_R)              strcpy(dest, "r");
    else if (k == KEY_S)              strcpy(dest, "S");
    else if (k == KEY_T)              strcpy(dest, "T");
    else if (k == KEY_U)              strcpy(dest, "U");
    else if (k == KEY_V)              strcpy(dest, "V");
    else if (k == KEY_W)              strcpy(dest, "W");
    else if (k == KEY_X)              strcpy(dest, "X");
    else if (k == KEY_Y)              strcpy(dest, "Y");
    else if (k == KEY_Z)              strcpy(dest, "Z");
    else if (k == KEY_0)              strcpy(dest, "0");
    else if (k == KEY_1)              strcpy(dest, "1");
    else if (k == KEY_2)              strcpy(dest, "2");
    else if (k == KEY_3)              strcpy(dest, "3");
    else if (k == KEY_4)              strcpy(dest, "4");
    else if (k == KEY_5)              strcpy(dest, "5");
    else if (k == KEY_6)              strcpy(dest, "6");
    else if (k == KEY_7)              strcpy(dest, "7");
    else if (k == KEY_8)              strcpy(dest, "8");
    else if (k == KEY_9)              strcpy(dest, "9");
    else if (k == KEY_0_PAD)          strcpy(dest, "0 (Pad)");
    else if (k == KEY_1_PAD)          strcpy(dest, "1 (Pad)");
    else if (k == KEY_2_PAD)          strcpy(dest, "2 (Pad)");
    else if (k == KEY_3_PAD)          strcpy(dest, "3 (Pad)");
    else if (k == KEY_4_PAD)          strcpy(dest, "4 (Pad)");
    else if (k == KEY_5_PAD)          strcpy(dest, "5 (Pad)");
    else if (k == KEY_6_PAD)          strcpy(dest, "6 (Pad)");
    else if (k == KEY_7_PAD)          strcpy(dest, "7 (Pad)");
    else if (k == KEY_8_PAD)          strcpy(dest, "8 (Pad)");
    else if (k == KEY_9_PAD)          strcpy(dest, "9 (Pad)");
    else if (k == KEY_F1)             strcpy(dest, "F1");
    else if (k == KEY_F2)             strcpy(dest, "F2");
    else if (k == KEY_F3)             strcpy(dest, "F3");
    else if (k == KEY_F4)             strcpy(dest, "F4");
    else if (k == KEY_F5)             strcpy(dest, "F5");
    else if (k == KEY_F6)             strcpy(dest, "F6");
    else if (k == KEY_F7)             strcpy(dest, "F7");
    else if (k == KEY_F8)             strcpy(dest, "F8");
    else if (k == KEY_F9)             strcpy(dest, "F9");
    else if (k == KEY_F10)            strcpy(dest, "F10");
    else if (k == KEY_F11)            strcpy(dest, "F11");
    else if (k == KEY_F12)            strcpy(dest, "F12");
    else if (k == KEY_ESC)            strcpy(dest, "ESC");
    else if (k == KEY_TILDE)          strcpy(dest, "TILDE");
    else if (k == KEY_MINUS)          strcpy(dest, "-");
    else if (k == KEY_EQUALS)         strcpy(dest, "=");
    else if (k == KEY_BACKSPACE)      strcpy(dest, "Backspace");
    else if (k == KEY_TAB)            strcpy(dest, "Tab");
    else if (k == KEY_OPENBRACE)      strcpy(dest, "{");
    else if (k == KEY_CLOSEBRACE)     strcpy(dest, "}");
    else if (k == KEY_ENTER)          strcpy(dest, "Enter");
    else if (k == KEY_COLON)          strcpy(dest, ":");
    else if (k == KEY_SEMICOLON)      strcpy(dest, ";");
    else if (k == KEY_QUOTE)          strcpy(dest, "'");
    else if (k == KEY_BACKSLASH)      strcpy(dest, "\\");
    else if (k == KEY_BACKSLASH2)     strcpy(dest, "\\");
    else if (k == KEY_COMMA)          strcpy(dest, ",");
    else if (k == KEY_STOP)           strcpy(dest, ".");
    else if (k == KEY_SLASH)          strcpy(dest, "/");
    else if (k == KEY_SPACE)          strcpy(dest, "Space");
    else if (k == KEY_INSERT)         strcpy(dest, "Insert");
    else if (k == KEY_DEL)            strcpy(dest, "Delete");
    else if (k == KEY_HOME)           strcpy(dest, "Home");
    else if (k == KEY_END)            strcpy(dest, "End");
    else if (k == KEY_PGUP)           strcpy(dest, "Pg Up");
    else if (k == KEY_PGDN)           strcpy(dest, "Pg Down");
    else if (k == KEY_LEFT)           strcpy(dest, "Left");
    else if (k == KEY_RIGHT)          strcpy(dest, "Right");
    else if (k == KEY_UP)             strcpy(dest, "Up");
    else if (k == KEY_DOWN)           strcpy(dest, "Down");
    else if (k == KEY_SLASH_PAD)      strcpy(dest, "/ (Pad)");
    else if (k == KEY_ASTERISK)       strcpy(dest, "*");
    else if (k == KEY_MINUS_PAD)      strcpy(dest, "- (Pad)");
    else if (k == KEY_PLUS_PAD)       strcpy(dest, "+ (Pad)");
    else if (k == KEY_DEL_PAD)        strcpy(dest, "Del (Pad)");
    else if (k == KEY_ENTER_PAD)      strcpy(dest, "Enter (Pad)");
    else if (k == KEY_PRTSCR)         strcpy(dest, "Print Screen");
    else if (k == KEY_PAUSE)          strcpy(dest, "Pause");
    else if (k == KEY_YEN)            strcpy(dest, "Yen");
    else if (k == KEY_KANA)           strcpy(dest, "Kana");
    else if (k == KEY_LSHIFT)         strcpy(dest, "L Shift");
    else if (k == KEY_RSHIFT)         strcpy(dest, "R Shift");
    else if (k == KEY_LCONTROL)       strcpy(dest, "L Ctrl");
    else if (k == KEY_RCONTROL)       strcpy(dest, "R Ctrl");
    else if (k == KEY_ALT)            strcpy(dest, "Alt");
    else if (k == KEY_ALTGR)          strcpy(dest, "Alt Gr");
    else if (k == KEY_LWIN)           strcpy(dest, "Left Win");
    else if (k == KEY_RWIN)           strcpy(dest, "Right Win");
    else if (k == KEY_MENU)           strcpy(dest, "Menu");
    else if (k == KEY_SCRLOCK)        strcpy(dest, "Scroll Lock");
    else if (k == KEY_NUMLOCK)        strcpy(dest, "Num Lock");
    else if (k == KEY_CAPSLOCK)       strcpy(dest, "Caps Lock");
    else                              strcpy(dest, "undefined");
}
