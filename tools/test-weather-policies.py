#!/usr/bin/env python3
"""Run standalone weather rule tests; does not replace rendered/network/device QA."""
from pathlib import Path
import argparse,os,subprocess,tempfile
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default=os.environ.get('CXX','c++'));a=p.parse_args();root=Path(__file__).resolve().parents[1]
tests=['weather_state','weather_precipitation_policy','weather_celestial_policy','weather_impact_policy','weather_sound_voice_budget','weather_lightning','weather_ecology']
with tempfile.TemporaryDirectory(prefix='tcpe-weather-policy-') as tmp:
 for name in tests:
  exe=Path(tmp)/name
  subprocess.run([a.compiler,'-std=c++14','-O2','-UNDEBUG','-I'+str(root/'src'),str(root/'tests'/(name+'.cpp')),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True);print(name+': PASS',flush=True)
print('Weather policy suite PASS; rendered, audio, full server and physical device evidence are separate.')
