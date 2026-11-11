# Load characterization files

This folder contains the supplied workbook and its PDF export. The workbook is the primary analysis file because it contains the raw tables and five chart sheets.

## Workbook structure

`Graphs_All_Loads.xlsx` has ten visible sheets:

- Five measurement sheets named `0.5k`, `2k`, `3k`, `4k`, and `10k`.
- Five corresponding `_Graph` sheets, with five embedded charts total.

Each measurement table uses these fields: `Frequency_Hz`, `PulseWidth_us`, `DAC`, and `ADC_Raw`. The five tables span about 1.49 million data rows in total. The graph sheets use Excel dynamic-array formulas (`SORT` and `UNIQUE`) to populate frequency selections, so use a recent Excel version that supports those functions.

The workbook was retained as supplied. No cells, formulas, or chart definitions were changed while preparing this repository.

Three of the five embedded chart caches contain fewer stored point values than the number of rows referenced by their chart series. Recalculate and visually review all five charts in a current Excel version before relying on the displayed results.

## PDF export

`Graphs_All_Loads.pdf` has 52 pages. Visual inspection showed raw tabular rows on the first page and a single `ADC_Raw` versus DAC chart on the last page. The PDF is therefore an original print export, not a polished five-chart report. Use the workbook's five chart sheets to explore the load comparisons.

## Data interpretation

The project report describes resistor-load characterization and raw ADC response. The workbook sheet names identify the load cases, but the supplied workbook does not provide a separate methods sheet documenting the circuit, calibration, sample averaging, or units for every plotted axis. Treat the values as prototype characterization data and confirm their setup before drawing electrical or clinical conclusions.

