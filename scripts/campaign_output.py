"""Protect recorded campaigns; reruns must choose a new output directory."""
import argparse
from pathlib import Path

def campaign_directory(path):
    root=Path(path)
    for name in ('metadata.json','raw.csv','summary.csv'):
        if (root/name).exists():
            raise ValueError(f'Campaign output exists: {root/name}; choose --output with a new path')
    root.mkdir(parents=True,exist_ok=True)
    return root

def prepare_campaign(default):
    parser=argparse.ArgumentParser()
    parser.add_argument('--output',default=default)
    args=parser.parse_args()
    try:return campaign_directory(args.output)
    except ValueError as error:parser.error(str(error))
