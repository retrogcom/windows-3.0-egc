"""Build the EGC companion, startup loaders and DOS utilities. GPL-2.0-only."""
from pathlib import Path
import argparse, hashlib, json, os, shutil, struct, subprocess

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['inputs', 'dosbox', 'nasm', 'out']:
        parser.add_argument('--' + name, required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    target = args.out.resolve()
    if target.exists():
        raise SystemExit('Choose a new output directory')
    manifest = json.loads((root / 'HELPER-INPUTS.json').read_text())
    sha = lambda data: hashlib.sha256(data).hexdigest()
    for item in manifest['tools']:
        source = args.inputs / item['path']
        if not source.is_file() or sha(source.read_bytes()) != item['sha256']:
            raise SystemExit('Missing or different input: ' + item['path'])
    for tool in [args.nasm, args.dosbox]:
        if not tool.is_file():
            raise SystemExit('Tool not found: ' + str(tool))
    target.mkdir(parents=True)
    for directory in ['BIN', 'SRC', 'OUT', 'LOGS']:
        (target / directory).mkdir()
    for item in manifest['tools']:
        shutil.copy2(args.inputs / item['path'], target / item['path'])
    batch = ['@echo off', r'cd \OUT']
    for name in ['EGCTEXT', 'LOGO', 'LOGONO']:
        shutil.copy2(root / 'helpers' / (name + '.ASM'), target / 'SRC' / (name + '.ASM'))
        batch += [fr'C:\BIN\MASM /Mx /Zi /L C:\SRC\{name}.ASM,{name}.OBJ,{name}.LST,NUL; > C:\LOGS\{name}A.TXT',
                  'if errorlevel 1 goto fail',
                  fr'C:\BIN\LINK {name}.OBJ,{name}.EXE,{name}.MAP/MAP; > C:\LOGS\{name}L.TXT',
                  'if errorlevel 2 goto fail']
    batch += [r'echo okay > C:\LOGS\BUILD.OK', 'exit', ':fail',
              r'echo failed > C:\LOGS\BUILD.ERR', 'exit']
    (target / 'BUILD.BAT').write_bytes(('\r\n'.join(batch) + '\r\n').encode('ascii'))
    config = '[sdl]\noutput=surface\nshowmenu=false\nwaitonerror=false\n[dosbox]\nmachine=vgaonly\nmemsize=4\n[cpu]\ncore=normal\ncputype=286\ncycles=max\n[mixer]\nnosound=true\n[autoexec]\nmount c "' + str(target) + '"\nc:\ncall build.bat\nexit\n'
    cfg = target / 'build.conf'
    cfg.write_text(config, encoding='ascii')
    with (target / 'LOGS/host.txt').open('wb') as log:
        child = subprocess.Popen([str(args.dosbox.resolve()), '-defaultconf', '-conf', str(cfg), '-fastlaunch', '-nogui'],
            cwd=target, env=dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy'),
            creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0, stdout=log, stderr=log)
        try:
            code = child.wait(timeout=60)
        except subprocess.TimeoutExpired:
            child.terminate(); child.wait(timeout=5)
            raise SystemExit('Build timed out')
    if code or not (target / 'LOGS/BUILD.OK').exists() or (target / 'LOGS/BUILD.ERR').exists():
        raise SystemExit('Build failed; inspect LOGS')
    for source, dest in [('EGCTEXT', 'EGCTEXT.GR2'), ('LOGO', 'EGCLOGO.LGO'), ('LOGONO', 'EGCLOGN.LGO')]:
        data = (target / 'OUT' / (source + '.EXE')).read_bytes()
        last, pages, relocs, paragraphs = struct.unpack_from('<4H', data, 2)
        if data[:2] != b'MZ' or relocs:
            raise SystemExit('Unexpected helper executable layout: ' + source)
        end = (pages - 1) * 512 + (last or 512)
        (target / 'OUT' / dest).write_bytes(data[paragraphs * 16:end])
    subprocess.run([str(args.nasm.resolve()), '-f', 'bin', str(root / 'helpers/CMPINI.ASM'), '-o', str(target / 'OUT/CMPINI.COM')], check=True)
    subprocess.run([str(args.nasm.resolve()), '-Wall', '-Werror', '-w-reloc-abs-word', '-f', 'bin', 'M28W3.ASM', '-o', str(target / 'OUT/M28W3.COM')], cwd=root / 'compat/M28W3', check=True)
    for name, expected in manifest['expected_outputs'].items():
        actual = sha((target / 'OUT' / name).read_bytes())
        if actual != expected:
            raise SystemExit('Output differs from release: ' + name + ' ' + actual)
    print('PASS: all five helper binaries match the release')

if __name__ == '__main__':
    main()