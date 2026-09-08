/* main_state.c -- a cluster of trivial single-global accessors/mutators
 * from F:\projects\icytower\trunk\source\main.c, kept in their own file
 * rather than folded into update_frame.c/line_intersect.c (main.c's two
 * other recovered functions so far) since none of the three share any
 * logic with this group. ok_to_play() (batch 4) and get_version_str()
 * (batch 13) have since been added below; get_demo() is already above.
 *
 * Original source/decl_line (artifacts/dwarf_info.txt), none of the six
 * taking a parameter or touching the FPU:
 *   get_demo              decl_line 565
 *   get_controls           decl_line 570
 *   switchedFromProgram    decl_line 1342
 *   switchedToProgram      decl_line 1348
 *   clickedCloseButton     decl_line 1354
 *   get_version_str        decl_line unresolved (added batch 13, see below)
 */
#include "game_types.h"
#include "game_state.h"

/* get_demo -- accessor for the single global demo-replay pointer.
 * Recovered from artifacts/disasm.txt (0x40696c..0x406976): `return demo;`
 */
Treplay *get_demo(void)
{
    return demo;
}

/* get_controls -- accessor for the single global player-1 control state,
 * the same "return &global" idiom as get_gamepad() (control.c): the
 * address literal in the disassembly (0x5000c8) is just the compiler's
 * computed &ctrl, not a value this source ever writes down.
 * Recovered from artifacts/disasm.txt (0x406978..0x406982).
 */
Tcontrol *get_controls(void)
{
    return &ctrl;
}

/* switchedFromProgram -- Allegro window-focus-lost callback: records that
 * the game no longer has input focus. Recovered from artifacts/disasm.txt
 * (0x406a5c..0x406a6a).
 */
void switchedFromProgram(void)
{
    hasFocus = 0;
}

/* switchedToProgram -- Allegro window-focus-gained callback: the mirror
 * of switchedFromProgram(). Recovered from artifacts/disasm.txt
 * (0x406a6c..0x406a7a).
 */
void switchedToProgram(void)
{
    hasFocus = 1;
}

/* clickedCloseButton -- Allegro window-close-button callback: latches the
 * close request for the main loop to act on. Recovered from
 * artifacts/disasm.txt (0x406a7c..0x406a89).
 */
void clickedCloseButton(void)
{
    closeButtonClicked = 1;
}

/* get_version_str -- returns the game's version string. Recovered from
 * artifacts/disasm.txt (0x406960..0x406969): `mov $0x4d4b20,%eax; ret`,
 * a literal .rdata address with no named global behind it (batch 3's
 * skip note above). Batch 13 closed the gap the earlier note left open
 * ("its bytes were not independently re-extracted") by reading the bytes
 * at that VA directly out of the image with pefile:
 *
 *   python -c "import pefile; pe = pefile.PE('assets/icytower15.exe'); \
 *              print(pe.get_data(0x4d4b20 - pe.OPTIONAL_HEADER.ImageBase, 16))"
 *   -> b"1.5.1\x00Caught `%s'..."  (the NUL-terminated run at 0x4d4b20 is
 *      "1.5.1", immediately followed in .rdata by an unrelated
 *      allegro_message() format string -- confirms the extraction landed
 *      on the right string and did not run past it).
 *
 * The recovered source returns a fresh string literal; its compiled
 * address is necessarily different from 0x4d4b20 (a new literal, not a
 * relocated one), so the comparison domain is the returned BYTES, not the
 * returned pointer VALUE -- verified by
 * carrier/lift/harness/get_version_str_check.py (new, this batch), which
 * runs the ORIGINAL bytes under unicorn, reads the NUL-terminated string
 * at the guest VA in EAX out of the loaded image, and compares it to the
 * bytes returned by a tiny program linking this file.
 */
char *get_version_str(void)
{
    return "1.5.1";
}

/* syncProfileFromOptions -- copies the 4 profile-persisted fields of
 * `options` (the in-memory settings the options menu edits) into
 * `*profile` (the record that gets written to disk), so a settings change
 * survives past this session.
 *
 * DWARF gives this function no address of its own to disassemble by
 * name: it is `DW_TAG_subprogram <0x1c973>`, decl_line 971,
 * `DW_AT_inline = 1` -- an ABSTRACT instance with no DW_AT_low_pc,
 * inlined at its three call sites inside play() (batch 12's
 * play_callsite_census.py entry "syncProfileFromOptions 0 -> 3": all
 * three sites inlined, so the census's by-name count is 0 even though the
 * logic runs 3 times). play.c's own header comment already identified
 * that the OUT-OF-LINE copy of those same fourteen instructions sits at
 * 0x406a14 with its own address, unreferenced from play() but reachable
 * from elsewhere in main.c -- so it is this VA, not the abstract DIE, that
 * this file recovers.
 *
 * Recovered from artifacts/disasm.txt (0x406a14..0x406a4d):
 *
 *   406a17: mov 0x4dd27c,%eax       eax = profile
 *   406a1c: mov 0x4fe54c,%edx       edx = *(msc_volume)
 *   406a22: mov %edx,0x528(%eax)    profile->msc_volume = edx
 *   406a28: mov 0x4fe550,%edx       edx = *(snd_volume)
 *   406a2e: mov %edx,0x52c(%eax)    profile->snd_volume = edx
 *   406a34: mov 0x4fe530,%edx       edx = *(jump_hold)
 *   406a3a: mov %edx,0x4e0(%eax)    profile->jump_hold = edx
 *   406a40: mov 0x4fe528,%edx       edx = *(flash)
 *   406a46: mov %edx,0x4dc(%eax)    profile->flash = edx
 *
 * 0x4fe528 is the named global `options` (Toptions, interop_index.json);
 * 0x4fe530/0x4fe54c/0x4fe550 are options+8/+0x24/+0x28. Confirmed against
 * a compiled offsetof() probe on this project's own game_types.h (not
 * guessed): `offsetof(Toptions,flash)==0`, `jump_hold==8`,
 * `msc_volume==0x24`, `snd_volume==0x28` on the flash/checksum/jump_hold/.../
 * msc_volume/snd_volume field order already in that header, and
 * `offsetof(Tprofile,{flash,jump_hold,msc_volume,snd_volume}) ==
 * {0x4dc,0x4e0,0x528,0x52c}` matching the four stores above exactly, byte
 * for byte, in the disassembly's own store order (msc_volume, snd_volume,
 * jump_hold, flash -- not the fields' declaration order in either
 * struct). `profile` (0x4dd27c, `Tprofile *`) is read, not written; the
 * function does not itself guard against a NULL profile (every call site
 * inside play() runs only once a profile is loaded), so neither does this
 * recovery -- a NULL profile crashes here exactly as it would in the
 * original.
 *
 * No FPU, no return value, no call. Offline-verified (memory domain:
 * profile->{flash,jump_hold,msc_volume,snd_volume}) by
 * carrier/lift/harness/sync_profile_check.py (new, this batch).
 */
void syncProfileFromOptions(void)
{
    profile->msc_volume = options.msc_volume;
    profile->snd_volume = options.snd_volume;
    profile->jump_hold  = options.jump_hold;
    profile->flash      = options.flash;
}
