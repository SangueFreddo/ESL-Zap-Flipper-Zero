# Come pubblicare ESL Zap sul catalogo Flipper

## 1. Screenshot VERI (gia' fatti)
Gia' presenti in `screenshots/` (128x64, dai tuoi screenshot qFlipper):
  01-home.png, 02-tags.png, 03-add.png, 04-barcode.png, 05-settings.png
Il manifest li referenzia gia'. Non serve altro.

## 2. Commit e push dell'app
Nel repo ESL-Zap-Flipper-Zero:
    git add -A
    git commit -m "Icon, catalog metadata and screenshots"
    git push
Poi copia il SHA completo:
    git rev-parse HEAD

## 3. Fork del catalogo e manifest
- Fai il fork di https://github.com/flipperdevices/flipper-application-catalog
- Crea la cartella: applications/Infrared/esl_zap/
- Copiaci dentro catalog/manifest.yml
- In manifest.yml sostituisci REPLACE_WITH_COMMIT_SHA con il SHA del punto 2.

## 4. Apri la Pull Request
- Apri una PR dal tuo fork verso flipperdevices/flipper-application-catalog (branch main).
- Titolo: "New app: ESL Zap (Infrared)".
- La CI prova a buildare l'app con la firmware ufficiale stabile: la tua build "Official (release)" passa gia'.
- Rispondi alle eventuali domande dei revisori.
