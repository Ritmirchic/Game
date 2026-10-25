"""Create compact, persistent deliverables without committing engine caches."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib
ROOT=Path(__file__).resolve().parents[2]
out=ROOT/'delivery';out.mkdir(exist_ok=True)

def archive(name, files):
    with ZipFile(out/name,'w',ZIP_DEFLATED,compresslevel=9) as z:
        for source,target in files:z.write(source,target)
    print(name, (out/name).stat().st_size)

files=[]
for p in (ROOT/'space_miner').rglob('*'):
    if p.is_file() and not any(s in p.parts for s in ('.godot','__pycache__')) and not p.name.endswith('.tmp'):
        files.append((p,p.relative_to(ROOT)))
workflow=ROOT/'.github/workflows/space-miner-build.yml'
files.append((workflow,workflow.relative_to(ROOT)))
if (out/'BUILD_INFO.md').exists(): files.append((out/'BUILD_INFO.md','delivery/BUILD_INFO.md'))
archive('SPACE_MINER_SOURCE.zip',files)
for name,folder in [('SPACE_MINER_WEB.zip','web'),('SPACE_MINER_LINUX.zip','linux')]:
    files=[(p,p.relative_to(ROOT/'releases'/folder)) for p in (ROOT/'releases'/folder).rglob('*') if p.is_file()]
    files += [(ROOT/'space_miner/PLAYER_GUIDE.md','PLAYER_GUIDE.md'),(ROOT/'space_miner/TEST_RESULTS.md','TEST_RESULTS.md')]
    files += [(p,Path('licenses')/p.name) for p in (ROOT/'space_miner/licenses').iterdir() if p.is_file()]
    archive(name,files)
with (out/'SHA256SUMS.txt').open('w') as f:
    for p in sorted(out.iterdir()):
        if p.suffix.lower() in ('.zip','.exe'):
            f.write(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n')
