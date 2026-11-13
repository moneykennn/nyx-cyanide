"""Verify that both Cyanide script runners expose and tear down the bridge."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
quick = (root / 'Cyanide/tweaks/QuickLoader.m').read_text(encoding='utf-8')
repo = (root / 'Cyanide/tweaks/RepoTweaks.m').read_text(encoding='utf-8')
script = (root / 'nyx-repo/esp-experimental.js').read_text(encoding='utf-8')
for name in ('nyx_start', 'nyx_frame', 'nyx_stop'):
    assert f'context[@"{name}"]' in quick, f'QuickLoader lacks {name}'
    assert f'context[@"{name}"]' in repo, f'RepoTweaks lacks {name}'
    assert f'{name}(' in script, f'Script does not call {name}'
assert 'NYXStop(@"nyx.quickloader")' in quick
assert 'NYXStop(tweakID)' in repo
print('PASS: both JS runners expose bridge names and teardown hooks; no iOS runtime tested.')
