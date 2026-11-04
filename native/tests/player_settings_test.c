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
    load_textures();
    CHECK(!s_tex_on);
    for (i = 0; i < NTEXCAT; ++i) CHECK(s_tex_cat[i] == 1);
    s_tex_on = 1; s_tex_cat[1] = 0;
    load_keys();
    for (i = 0; i < NHL_KEYMAP_COUNT; ++i) {
        CHECK(s_key[i][0] == DEFAULTS[i][0]);
        CHECK(s_key[i][1] == DEFAULTS[i][1]);
        s_key[i][0] = 0x41 + i;
        s_key[i][1] = 0x80 + i;
    }
    for (i = 0; i < NVIDEO; ++i) s_vsel[i] = VIDEO[i].count - 1;
    CHECK(save_video()); CHECK(save_keys()); CHECK(save_textures());
    memset(s_vsel, 0, sizeof s_vsel); memset(s_key, 0, sizeof s_key);
    load_video(); load_keys();
    s_tex_on = 0; memset(s_tex_cat, 0, sizeof s_tex_cat); load_textures();
    CHECK(s_tex_on && !s_tex_cat[1]);
    for (i = 0; i < NTEXCAT; ++i) if (i != 1) CHECK(s_tex_cat[i]);
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
    {
        char directory[MAX_PATH], file[MAX_PATH]; FILE *f;
        ini_path("", directory, sizeof directory, 1);
        strcat(directory, "texture_pack"); CHECK(CreateDirectoryA(directory, NULL));
        CHECK(!has_pictures(directory, 0));
        snprintf(file, sizeof file, "%s\\0123456789ABCDEF_sample_original.DDS", directory);
        f = fopen(file, "wb"); CHECK(f); CHECK(fwrite("DDS ", 1, 4, f) == 4); fclose(f);
        CHECK(has_pictures(directory, 0)); load_textures(); CHECK(s_tex_installed);
        INITCOMMONCONTROLSEX ic = { sizeof ic, ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES };
        WNDCLASSA wc = {0}; CHECK(InitCommonControlsEx(&ic));
        wc.lpfnWndProc = wndproc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "HitzTexturesConfigTest";
        CHECK(RegisterClassA(&wc));
        HWND h = CreateWindowExA(0, wc.lpszClassName, "", WS_OVERLAPPED, 0, 0, 620, 640, NULL, NULL, wc.hInstance, NULL);
        CHECK(h); show_page(PAGE_TEXTURES);
        CHECK(TabCtrl_GetItemCount(s_tab) == 3);
        s_vsel[0] = 0; tex_sync(); CHECK(!IsWindowEnabled(s_tmaster));
        CHECK(GetWindowLongPtrA(s_tnote, GWL_STYLE) & WS_VISIBLE);
        s_vsel[0] = 1; tex_sync(); CHECK(IsWindowEnabled(s_tmaster));
        CHECK(IsWindowEnabled(s_tcat[0]));
        s_tex_on = 0; tex_sync(); CHECK(!IsWindowEnabled(s_tcat[0]));
        s_tex_installed = 0; tex_sync(); CHECK(!IsWindowEnabled(s_tmaster));
        CHECK(!GetDlgItem(h, 320)); /* No downloader yet. */
        s_wait_row = 0; s_wait_slot = 0;
        { int before = s_key[0][0]; assign(VK_F8); CHECK(s_key[0][0] == before); CHECK(s_wait_row == 0); }
        DestroyWindow(h);
    }
    puts("PASS: Video/Controls/Textures persistence, DDS detection, scale/pack gating and all 24 runtime bindings");
    return 0;
}
