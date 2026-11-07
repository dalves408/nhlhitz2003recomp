/* JOJO item 23: the Linux game reads keymap.ini (written by jojo/config/nhl_config.py) from
 * $XDG_DATA_HOME/nhlhitz2003, and a remapped key drives its Xbox control. */
#include "../overrides/keyboard.c"
#include <sys/stat.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Failed line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(void)
{
    char dir[] = "/tmp/hitz_keymap_XXXXXX", sub[256], path[300];
    InputBindings b;
    XBOX_INPUT_STATE pad;
    FILE *f;
    int ca, cup;
    CHECK(mkdtemp(dir));
    snprintf(sub, sizeof sub, "%s/nhlhitz2003", dir); CHECK(mkdir(sub, 0755) == 0);
    snprintf(path, sizeof path, "%s/keymap.ini", sub);
    CHECK((f = fopen(path, "w")));
    /* As configparser writes it: other section first, a=Space, A's old key Z now on B's alt. */
    fputs("[other]\na=1\n\n[keys]\na=32\na_alt=0\nb=88\nb_alt=90\ndpad_up = 87\n", f);
    fclose(f);
    setenv("XDG_DATA_HOME", dir, 1);
    xbox_InputKeyboardBindings(&b);
    ca = row_control(&xbox_key_map[8]);         /* a */
    cup = row_control(&xbox_key_map[0]);        /* dpad_up */
    CHECK(b.count[ca] == 1 && b.key[ca][0] == 32);
    CHECK(b.key[row_control(&xbox_key_map[9])][1] == 'Z');
    CHECK(b.count[cup] == 1 && b.key[cup][0] == 'W');
    CHECK(b.key[row_control(&xbox_key_map[16])][1] == 0x68);
    xbox_InputKeySet(32, 1); xbox_InputKeyboardState(&pad);
    CHECK(pad.Gamepad.bAnalogButtons[XBOX_BUTTON_A] == 255);
    xbox_InputKeysClear(); xbox_InputKeySet('Z', 1); xbox_InputKeyboardState(&pad);
    CHECK(pad.Gamepad.bAnalogButtons[XBOX_BUTTON_A] == 0);
    CHECK(pad.Gamepad.bAnalogButtons[XBOX_BUTTON_B] == 255);
    unlink(path); rmdir(sub); rmdir(dir);
    puts("PASS: keymap.ini read on Linux and applied to the keyboard pad");
    return 0;
}
