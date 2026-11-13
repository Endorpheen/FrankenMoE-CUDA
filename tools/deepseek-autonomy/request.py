#!/usr/bin/env python3
"""Untrusted request producer;does not grant approval or execute host commands."""
import argparse,json,os,uuid
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--message',required=True);p.add_argument('--integration',action='store_true');p.add_argument('files',nargs='+');a=p.parse_args();rid=uuid.uuid4().hex;d=Path('/work/.requests');d.mkdir(exist_ok=True);tmp=d/(rid+'.tmp');tmp.write_text(json.dumps({'id':rid,'message':a.message,'files':a.files,'integration':a.integration}));os.replace(tmp,d/(rid+'.json'));print('Requested '+rid+';read /results/'+rid+'.json;this is not acceptance.')
