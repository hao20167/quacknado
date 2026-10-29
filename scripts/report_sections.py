"""Replace one numbered experiment without deleting subsequent experiments."""
import re
from pathlib import Path

def update_section(text, marker, path=Path('docs/benchmarks/report.md')):
    old=path.read_text()
    start=old.find(marker)
    if start<0:
        path.write_text(old+text)
        return
    following=re.search(r'\n## Vòng \d+:',old[start+len(marker):])
    end=start+len(marker)+following.start() if following else len(old)
    path.write_text(old[:start]+text+old[end:])
