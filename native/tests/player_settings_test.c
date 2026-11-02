/* Exercise the actual settings writer and runtime reader with isolated AppData. */
#include "../config/config.c"
#include "../overrides/keyboard.c"
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Failed line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(int argc, char **argv)
{
    InputBindings bindings;
    XBOX_INPUT_STATE pad;
    int i, control;
    const int initial[NVIDEO] = { 1, 1, 0, 0, 60, 1, 1 };
    CHECK(argc == 2);
    CHECK(SetEnvironmentVariableA("LOCALAPPDATA", argv[1]));
    load_video();
    for (i = 0; i < NVIDEO; ++i) CHECK(VIDEO[i].value[s_vsel[i]] == initial[i]);
    load_keys();
    for (i = 0; i < NHL_KEYMAP_COUNT; ++i) {
        CHECK(s_key[i][0] == DEFAULTS[i][0]);
        CHECK(s_key[i][1] == DEFAULTS[i][1]);
        s_key[i][0] = 0x41 + i;
        s_key[i][1] = 0x80 + i;
    }
    for (i = 0; i < NVIDEO; ++i) s_vsel[i] = VIDEO[i].count - 1;
    CHECK(save_video()); CHECK(save_keys());
    memset(s_vsel, 0, sizeof s_vsel); memset(s_key, 0, sizeof s_key);
    load_video(); load_keys();
    for (i = 0; i < NVIDEO; ++i) CHECK(s_vsel[i] == VIDEO[i].count - 1);
    /* An existing user's explicitly uncapped choice survives the new 60 FPS default. */
    CHECK(VIDEO[4].value[s_vsel[4]] == 0);
    xbox_InputKeyboardBindings(&bindings);
    CHECK(xbox_key_map_count == NHL_KEYMAP_COUNT);
    for (i = 0; i < NHL_KEYMAP_COUNT; ++i) {
        CHECK(s_key[i][0] == 0x41 + i && s_key[i][1] == 0x80 + i);
        control = row_control(&xbox_key_map[i]);
        CHECK(control >= 0);
        CHECK(bindings.count[control] == 2);
        CHECK(bindings.key[control][0] == 0x41 + i);
        CHECK(bindings.key[control][1] == 0x80 + i);
    }
    xbox_InputKeySet(0x41 + 8, 1);
    xbox_InputKeyboardState(&pad);
    CHECK(pad.Gamepad.bAnalogButtons[XBOX_BUTTON_A] == 255);
    xbox_InputKeysClear(); xbox_InputKeyboardState(&pad);
    CHECK(pad.Gamepad.bAnalogButtons[XBOX_BUTTON_A] == 0);
    puts("PASS: Video/Controls persistence and all 24 runtime bindings");
    return 0;
}
