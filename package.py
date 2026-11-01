"""Package the Windows 3.0 EGC driver for DOS Setup. GPL-2.0-only."""
from pathlib import Path
import argparse, hashlib, json, re, zipfile

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--driver', required=True, type=Path)
    parser.add_argument('--helpers', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    if args.out.exists():
        raise SystemExit('Output ZIP already exists')
    sha = lambda data: hashlib.sha256(data).hexdigest()
    driver = args.driver.read_bytes()
    expected = json.loads((root / 'BUILD-INPUTS.json').read_text())['expected_driver_sha256']
    if sha(driver) != expected:
        raise SystemExit('Display binary differs from the expected build')
    files = {'WIN16/EGCORDER.DRV': driver}
    helper_hashes = json.loads((root / 'HELPER-INPUTS.json').read_text())['expected_outputs']
    for name, digest in helper_hashes.items():
        data = (args.helpers / name).read_bytes()
        if sha(data) != digest:
            raise SystemExit('Helper differs from the expected build: ' + name)
        files['WIN16/' + name] = data
    for name in ['OEMSETUP.INF', 'BACKUP.BAT', 'RESTORE.BAT', 'INSTALL.TXT', 'M28FIX.TXT', 'RIGHTS.TXT', 'EGCPRES.TAG']:
        files['WIN16/' + name] = (root / 'installer' / name).read_bytes()
    for name in ['EGCLOGO.RLE', 'EGCLOGN.RLE']:
        files['WIN16/' + name] = (root / 'assets' / name).read_bytes()
    files['WIN16/LICENSE.TXT'] = (root / 'LICENSE').read_text().replace('\r\n', '\n').replace('\n', '\r\n').encode('utf-8')
    files['README.TXT'] = b'WINDOWS 3.0 EGC DRIVER\r\nRead WIN16\\INSTALL.TXT before installation.\r\nFor M28 Standard mode, also read WIN16\\M28FIX.TXT.\r\nCorresponding source is EGC30SRC.ZIP on the same release page.\r\n'
    manifest = dict(version='0.21-appearance.2',code_license='GPL-2.0-only',copyright='2026 retrogcom',
        repository='https://github.com/retrogcom/windows-3.0-egc',fonts_included=False,
        files={name:dict(bytes=len(data),sha256=sha(data)) for name,data in sorted(files.items())})
    files['FILES.JSN'] = (json.dumps(manifest,indent=2)+'\n').encode('ascii')
    for name in files:
        for component in name.split('/'):
            if not re.fullmatch(r'[A-Z0-9_-]{1,8}(\.[A-Z0-9_-]{1,3})?',component):
                raise SystemExit('Not a DOS 8.3 filename: ' + name)
    args.out.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(args.out,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
        for name,data in sorted(files.items()):
            info=zipfile.ZipInfo(name,(2026,11,1,0,0,0)); info.compress_type=zipfile.ZIP_DEFLATED
            info.external_attr=0o100644<<16; archive.writestr(info,data)
    with zipfile.ZipFile(args.out) as archive:
        assert archive.testzip() is None and set(archive.namelist()) == set(files)
        for name,data in files.items():
            assert archive.read(name)==data,name
    print('PASS:',args.out,len(files),'entries; SHA256',sha(args.out.read_bytes()))

if __name__ == '__main__':
    main()