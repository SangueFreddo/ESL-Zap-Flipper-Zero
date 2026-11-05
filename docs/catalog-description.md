# ESL Zap

Turn your Flipper Zero into a remote for **infrared electronic shelf labels (ESL)** that you own, the e-paper price tags used on shop shelves. Point the Flipper at a label and send it a **picture**, some **text** or a built-in **test card**. It drives the tag over infrared, exactly like the store base station, using the public protocol research by furrtek.

## Features

- **Images and text**: any BMP (1/4/8/24/32-bit, any size) is scaled, dithered and sent. Text mode and a test card included.
- **Red / yellow aware**: detects the accent colour of tricolour tags and sends the colour plane automatically.
- **Easy tag add**: type only digits 3 to 16 of the barcode, or import from an NFC chip; the Flipper reads it live.
- **Preview first**: see what the label will show, on the Flipper screen, before sending.
- **Bilingual**: full interface in Italian and English.

## How to use

1. **My tags -> Add a tag**: type digits 3 to 16 of the barcode. The model and screen size appear while you type. Tags with an NFC chip can be added with **Import from NFC**.
2. Choose **Send image** (a BMP from `apps_data/esl_zap/images`), **Write text** or **Test card**.
3. Check the preview and the options (fit, dithering, invert, colour, page).
4. Hold the Flipper 5 to 30 cm from the label, IR window facing it, and press **Send**. The label redraws a few seconds later.

## Notes

Use it only on labels **you own**. Radio tags (SES-imagotag / VUSION at 2.4 GHz) are not supported: the Flipper hardware cannot reach them. Never remove a label battery, most models keep their address and image in volatile memory.

Infrared ESL protocol reverse-engineered and published by furrtek (PrecIR). Inspired by TagTinker; ESL Zap is an independent implementation written from scratch.
