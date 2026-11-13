## Summary

Describe what changed and why.

## Verification

Mark the exact level run after your final edit:

- [ ] Host tests: `python -m unittest discover -s tests -v` (`HOST_TESTED`)
- [ ] Guardian tests: `python -m unittest discover -s tools/guardian/tests -v` (`HOST_TESTED`)
- [ ] Markdown links & design tokens: `python scripts/check_markdown_links.py && python scripts/verify_color_tokens.py`
- [ ] ESP32-S3 compile: `python scripts/validate_repo.py --require-target-compile` (`TARGET_COMPILED`)
- [ ] Physical hardware test (if applicable): note board behavior (`HARDWARE_TESTED` or `NOT_TESTED`)

## Hardware Safety Checklist

- [ ] No changes to `FEATURE_BATTERY_MONITOR=0` or `FEATURE_AUDIO=0` in `firmware/BmoGameboy/src/hal/config.h`
- [ ] No strapping pins (`0/3/45/46`) or octal PSRAM pins (`33-37`) assigned
- [ ] `FlashMode=qio`, `PSRAM=opi`, and `firmware/BmoGameboy/partitions.csv` preserved
- [ ] No game ROMs, box art packs, `.wasm` binaries, or `.env`/keys staged
