"""Generate the RecompLauncher manifest for a platform release."""


def manifest(platform):
    linux = platform == 'linux'
    data = '{local}/' + ('nhlhitz2003' if linux else 'NHLHitz2003')

    def setting(identifier, label, key, choices, default):
        return {
            'id': identifier, 'label': label,
            'choices': [{'label': name, 'set': {key: str(value)}} for name, value in choices],
            'defaults': {key: str(default)},
        }

    toggle = [('Off', 0), ('On', 1)]
    settings = [
        setting('scale', 'Render scale', 'display/resolution_scale',
                [(f'{n}x', n) for n in range(1, 6)], 1),
        setting('fullscreen', 'Fullscreen', 'display/fullscreen', toggle, 1),
        setting('vsync', 'VSync', 'display/vsync', toggle, 0),
        setting('frame-cap', 'Frame cap', 'display/frame_rate',
                [(f'{n} FPS', n) for n in (30, 60, 120, 144, 240)] + [('Unlimited', 0)], 60),
        setting('msaa', 'MSAA', 'display/msaa', [('Off', 1)] + [(f'{n}x', n) for n in (2, 4, 8)], 1),
        setting('anisotropy', 'Anisotropic filtering', 'display/anisotropy',
                [('Off', 1)] + [(f'{n}x', n) for n in (2, 4, 8, 16)], 1),
        setting('textures', 'Hi-res textures', 'textures/enabled', toggle, 0),
    ]
    settings[-1]['help'] = 'Requires the texture pack and a render scale of 2x or higher.'
    return {
        '$schema': 'https://recomplauncher.com/schema/recomplauncher.json',
        'format': 1,
        'name': 'NHL Hitz 2003',
        'repo': 'dalves408/nhlhitz2003recomp',
        'game': {'kind': 'iso_or_folder', 'label': 'Your NHL Hitz 2003 Xbox USA disc image or extracted game folder',
                 'check': {'file': 'default.xbe', 'xdvdfs': 'default.xbe'}},
        'start': {'program': ['NHLHitz2003.AppImage' if linux else 'nhl_hitz_2003.exe'],
                  'args': ['--play'] if linux else []},
        'prepare': {'program': ['prepare-game.sh' if linux else 'nhl_import.exe'],
                    'args': ['{game}'] if linux else ['--prepare', '{game}', '{install}/game_files'],
                    'label': 'Verify and import NHL Hitz 2003 game files', 'timeout': 900},
        'config': [{'format': 'ini', 'file': [{'path': data + '/settings.ini'}],
                    'new': {'display/stretch': '0',
                            **{key: value for option in settings for key, value in option['defaults'].items()}}}],
        'settings': settings,
        'saves': [data + '/SaveData'],
        'looks': {'graphics': 'opengl' if linux else 'd3d11', 'lines': 480},
    }
