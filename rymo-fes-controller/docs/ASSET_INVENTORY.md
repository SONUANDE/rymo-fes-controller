# Asset inventory

## Git-tracked project files

- `firmware/main.c`: source listing extracted and lightly corrected from the supplied Word document.
- `host/rtt_logger.py`: supplied PyQtGraph live RTT plotter.
- `host/requirements.txt`: Python packages required by the plotter.
- `analysis/Graphs_All_Loads.xlsx`: supplied workbook with five load tables and five chart sheets.
- `analysis/Graphs_All_Loads.pdf`: supplied 52-page print export.
- `media/photos/`: 19 resized JPEGs with camera metadata removed.
- `docs/`: project summary, hardware notes, source caveats, and publishing instructions.

## Local-only source archive

The complete bundle retains the original files from the gallery ZIP under `source-materials/`. The whole directory is ignored by Git because the original report and media have not been reviewed for public release and the videos are large.

| Original asset | Count / size | Git status |
|---|---:|---|
| `Fes__code_documentatin(readable).docx` | 1 Word document | Local-only; source copied to `firmware/main.c` |
| `Project_Documentation(Sonu_A).pdf` | 1 PDF report | Local-only |
| JPEG project photos | 20 (one exact duplicate) | Originals local-only; 19 optimized copies tracked |
| `VID20260831181047.mp4` | 84,732,527 bytes | Local-only |
| `VID20260831181242.mp4` | 75,240,699 bytes | Local-only |
| `VID20260831181511.mp4` | 101,176,131 bytes | Local-only |

The largest video is approximately 96.5 MiB. GitHub documents a warning above 50 MiB and a 100 MiB hard limit for regular Git objects. Consider Git LFS or a release asset if approved videos need to be shared.

## Analysis files

The workbook and PDF were supplied separately and are included in `analysis/`. The PDF is an original print export, not a polished report. The workbook contains about 1.49 million measurement rows across five load cases. See [analysis notes](../analysis/README.md).
