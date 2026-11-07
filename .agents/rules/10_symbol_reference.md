# Symbol Reference

Firmware declarations in the current headers are the source of truth. Do not copy a symbol name from an older handoff without checking the current source.

## Look up a symbol

- Run `python -m tools.guardian lookup <symbol>` for the current knowledge graph.
- Open [`docs/generated/symbols.md`](../../docs/generated/symbols.md) for the generated header index.
- Open the linked generated page for the declaration and source header path.
- For task context, run `python -m tools.guardian prompt-pack "<task>"`.

## Regenerate

After adding or changing public header declarations, run:

```powershell
python -m tools.guardian symbols
python -m tools.guardian index
```

The symbol pages are derived output. Edit firmware headers, then regenerate the pages. Keep this rule as a short pointer instead of copying declarations into it.
