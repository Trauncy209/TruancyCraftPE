#!/usr/bin/env python3
"""Package one already built Android ABI; official signing is optional."""
import argparse
import copy
import os
from pathlib import Path
import shutil
import subprocess
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--abi', choices=['arm64-v8a','armeabi-v7a'], default='arm64-v8a')
parser.add_argument('--output', type=Path)
parser.add_argument('--sdk', default=os.environ.get('ANDROID_HOME', os.environ.get('ANDROID_SDK_ROOT')))
parser.add_argument('--build-tools', default='36.0.0')
parser.add_argument('--platform', default='android-34')
parser.add_argument('--work', type=Path)
parser.add_argument('--native-libs', type=Path, help='Separate NDK_LIBS_OUT directory for a coherent renderer checkpoint')
parser.add_argument('--sound-bank', type=Path, help='Verified immutable Java sound bank directory')
parser.add_argument('--gles3', action='store_true', help='Require a GLES3 device/context and emit a separate experimental APK')
args = parser.parse_args()
if not args.sdk:
    parser.error('Set ANDROID_HOME or pass --sdk.')
root = Path(__file__).resolve().parents[1]
native_root = root if (root/'src/AppConstants.h').is_file() else root/'original-tcpe'
sdk = Path(args.sdk).resolve()
tools = sdk / 'build-tools' / args.build_tools
android_jar = sdk / 'platforms' / args.platform / 'android.jar'
project = native_root / 'project/android'
native_libs = (args.native_libs or project/'libs').resolve()
work = (args.work or root / 'build/android-apk').resolve()
for required in (android_jar, tools/'aapt', tools/'d8', tools/'zipalign', tools/'apksigner', native_libs/args.abi/'libminecraftpe.so', native_libs/args.abi/'libtcpe_java_network.so'):
    if not required.is_file():
        parser.error(f'Missing required input: {required}; build native libraries first.')
work.mkdir(parents=True, exist_ok=True)
# Remove generated outputs only. Retain the development key for consistent signing.
for folder in ('generated', 'classes', 'dex'):
    path = work/folder
    if path.exists(): shutil.rmtree(path)
    path.mkdir()

def run(*cmd):
    subprocess.run([str(x) for x in cmd], check=True)

images = native_root/'data/images'
# Android loads image assets from its asset root; retain all generated models/fonts.
shutil.copytree(images, project/'assets', dirs_exist_ok=True)
for data_folder in ('app', 'fonts', 'lang', 'sound', 'sounds'):
    source_folder = native_root/'data'/data_folder
    if source_folder.is_dir():
        shutil.copytree(source_folder, project/'assets'/data_folder, dirs_exist_ok=True)
for name in ('java-terrain.png', 'java-items.png', 'java-hud.png', 'java-bossbars.png', 'java-hearts.png'):
    shutil.copy2(images/name, project/'assets'/name)
shutil.copytree(images/'java-font', project/'assets/java-font', dirs_exist_ok=True)
shutil.copytree(images/'java-items', project/'assets/java-items', dirs_exist_ok=True)
sound_bank = (args.sound_bank or native_root/'data/sounds/java').resolve()
sound_assets = project/'assets/sounds/java'
if sound_assets.exists(): shutil.rmtree(sound_assets)
shutil.copytree(sound_bank, sound_assets)
entity_assets = [images/'item/java_arrow.png', images/'item/java-shield.png', images/'item/java-shield-patterned-base.png', *sorted((images/'item/java-shield-patterns').glob('*.png')), images/'mob/java-armor-stand.png', images/'mob/java-wind-charge.png', *sorted((images/'armor').glob('java-*.png')), *sorted((images/'particle').glob('java-gust-*.png')), *sorted((images/'particle').glob('java-firework-*.png'))]
for asset in entity_assets:
    target = project/'assets'/asset.relative_to(images)
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(asset, target)
manifest = project/'AndroidManifest.xml'
if args.gles3:
    import xml.etree.ElementTree as ET
    ET.register_namespace('android','http://schemas.android.com/apk/res/android'); tree=ET.parse(manifest); ET.SubElement(tree.getroot(),'uses-feature',{'{http://schemas.android.com/apk/res/android}glEsVersion':'0x00030000','{http://schemas.android.com/apk/res/android}required':'true'})
    manifest=work/'gles3-manifest/AndroidManifest.xml'; manifest.parent.mkdir(parents=True,exist_ok=True); tree.write(manifest,encoding='utf-8',xml_declaration=True)
run(tools/'aapt', 'package', '-f', '-m', '-J', work/'generated', '-M', manifest, '-S', project/'res', '-A', project/'assets', '-I', android_jar, '-F', work/'resources.apk')
# The Verizon flavor references an obsolete external licensing SDK and is not
# declared in this manifest. Preserve its source, but omit that unused flavor.
sources = sorted(p for p in (project/'src').rglob('*.java') if p.name != 'Minecraft_Verizon.java')
sources += sorted((work/'generated').rglob('*.java'))
run('javac', '-source', '8', '-target', '8', '-classpath', android_jar, '-d', work/'classes', *sources)
run(tools/'d8', '--min-api', '21', '--lib', android_jar, '--output', work/'dex', *sorted((work/'classes').rglob('*.class')))
# WAV is excluded from compression by aapt's media defaults. Keep exact PCM
# bytes, but deflate the bank; the Java audio bridge streams compressed assets
# into its app-owned cache before SoundPool loads them.
with zipfile.ZipFile(work/'resources.apk') as resources, zipfile.ZipFile(work/'unsigned.apk', 'w') as package:
    for original in resources.infolist():
        data = resources.read(original.filename)
        info = copy.copy(original)
        if info.filename.startswith('assets/sounds/java/'):
            info.compress_type = zipfile.ZIP_DEFLATED
        package.writestr(info, data)
with zipfile.ZipFile(work/'unsigned.apk', 'a', compression=zipfile.ZIP_DEFLATED) as apk:
    apk.write(work/'dex/classes.dex', 'classes.dex')
    for library in sorted((native_libs/args.abi).glob('*.so')):
        apk.write(library, 'lib/'+args.abi+'/'+library.name)
run(tools/'zipalign', '-f', '-P', '16', '4', work/'unsigned.apk', work/'aligned.apk')
release_signing = bool(os.environ.get('TCPE_RELEASE_KEYSTORE'))
name = 'TruancyCraftPE-0.5.23-Android-'+args.abi+('' if release_signing else '-development')+'.apk'
output = (args.output or work/name).resolve()
output.parent.mkdir(parents=True, exist_ok=True)
if release_signing:
    os.environ['ANDROID_BUILD_TOOLS_DIR'] = str(tools)
    run('bash', native_root/'tools/sign-release-apk.sh', work/'aligned.apk', output)
else:
    key = work/'debug.keystore'
    if not key.exists():
        run('keytool', '-genkeypair', '-keystore', key, '-storepass', 'android', '-keypass', 'android', '-alias', 'androiddebugkey', '-dname', 'CN=TCPE Local Development', '-keyalg', 'RSA', '-keysize', '2048', '-validity', '3650')
    run(tools/'apksigner', 'sign', '--ks', key, '--ks-pass', 'pass:android', '--key-pass', 'pass:android', '--out', output, work/'aligned.apk')
run(tools/'apksigner', 'verify', '--verbose', output)
run(tools/'zipalign', '-c', '-P', '16', '4', output)
with zipfile.ZipFile(output) as apk:
    for asset in [*(images/name for name in ('java-terrain.png', 'java-items.png', 'java-hud.png', 'java-bossbars.png')), *entity_assets]:
        if apk.read('assets/'+asset.relative_to(images).as_posix()) != asset.read_bytes():
            raise RuntimeError('Packaged entity/atlas asset differs: '+str(asset))
    for sound in sorted(sound_bank.iterdir()):
        if sound.is_file() and apk.read('assets/sounds/java/'+sound.name) != sound.read_bytes():
            raise RuntimeError('Packaged sound bank differs: '+sound.name)
    for library in sorted((native_libs/args.abi).glob('*.so')):
        if apk.read('lib/'+args.abi+'/'+library.name) != library.read_bytes():
            raise RuntimeError('Packaged native payload differs: '+library.name)
    for folder in ('java-font', 'java-items'):
        for asset in sorted((images/folder).rglob('*')):
            if asset.is_file():
                name = 'assets/'+folder+'/'+asset.relative_to(images/folder).as_posix()
                if apk.read(name) != asset.read_bytes():
                    raise RuntimeError('Packaged image resource differs: '+name)
print('Native payloads, Java font/item resources and complete sound bank verified against source.')
print(f'Built APK: {output}')
print('Device installation, launch and gameplay are not verified by packaging.')
