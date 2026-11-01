"""Build the unchanged display driver with separately supplied historical inputs."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--inputs',required=True,type=Path,help='Licensed BIN/INCLUDE/RES input tree')
    p.add_argument('--dosbox',required=True,type=Path,help='DOSBox-X executable')
    p.add_argument('--out',required=True,type=Path,help='New isolated build directory')
    a=p.parse_args();root=Path(__file__).resolve().parent
    manifest=json.loads((root/'BUILD-INPUTS.json').read_text())
    sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
    for row in manifest['inputs']:
        f=a.inputs/row['path']
        if not f.is_file() or sha(f)!=row['sha256']:raise SystemExit('Missing or different input: '+str(f))
    target=a.out.resolve()
    if target.exists():raise SystemExit('Build output already exists; select a new directory')
    runner=a.dosbox.resolve()
    if not runner.is_file():raise SystemExit('DOSBox-X executable not found')
    target.mkdir(parents=True)
    shutil.copytree(root/'driver/SRC',target/'SRC')
    shutil.copy2(root/'driver/BUILD.BAT',target/'BUILD.BAT')
    for row in manifest['inputs']:
        output=target/row['path'];output.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(a.inputs/row['path'],output)
    for name in ['OUT','LOGS']:(target/name).mkdir()
    conf='[sdl]\noutput=surface\nshowmenu=false\nwaitonerror=false\n[dosbox]\nmachine=vgaonly\nmemsize=4\n[cpu]\ncore=normal\ncputype=286\ncycles=max\n[mixer]\nnosound=true\n[autoexec]\nmount c "'+str(target)+'"\nc:\ncall build.bat\nexit\n'
    cfg=target/'build.conf';cfg.write_text(conf,encoding='ascii')
    with (target/'LOGS/host.txt').open('wb') as log:
        child=subprocess.Popen([str(runner),'-defaultconf','-conf',str(cfg),'-fastlaunch','-nogui'],cwd=target,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0,stdout=log,stderr=log)
        try:code=child.wait(timeout=60)
        except subprocess.TimeoutExpired:child.terminate();child.wait(timeout=5);raise SystemExit('Build timed out; inspect LOGS')
    if code or (target/'LOGS/BUILD.ERR').exists() or not (target/'LOGS/BUILD.OK').exists():raise SystemExit('Build failed; inspect LOGS')
    actual=sha(target/'OUT/EGCBASE.DRV')
    if actual!=manifest['expected_driver_sha256']:raise SystemExit('Build completed but differs from the release binary: '+actual)
    print('PASS byte-identical display driver: '+actual)
if __name__=='__main__':main()
