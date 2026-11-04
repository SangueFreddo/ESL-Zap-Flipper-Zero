/*
 * ESL Zap - infrared e-paper label toolkit for Flipper Zero
 * Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
 *
 * User interface strings. Flipper fonts are ASCII only, so Italian text
 * is written without accented letters.
 */
#pragma once

#include <stdint.h>

/* X(id, english, italian) */
#define EZ_STRINGS(X)                                                                   \
    X(S_HOME_IMAGE, "Send image", "Invia immagine")                                     \
    X(S_HOME_TEXT, "Write text", "Scrivi testo")                                        \
    X(S_HOME_TEST, "Test card", "Immagine prova")                                       \
    X(S_HOME_TAGS, "My tags", "I miei tag")                                             \
    X(S_HOME_PAGE, "Show page", "Cambia pagina")                                        \
    X(S_HOME_SETTINGS, "Settings", "Impostazioni")                                      \
    X(S_HOME_HELP, "Help", "Guida")                                                     \
    X(S_NO_TAG, "No tag yet: add one!", "Nessun tag: aggiungilo!")                      \
    X(S_TAGS_TITLE, "My tags", "I miei tag")                                            \
    X(S_TAG_ADD, "+ Add a tag", "+ Aggiungi tag")                                       \
    X(S_TAG_ADD_NFC, "+ Import from NFC", "+ Importa da NFC")                           \
    X(S_NFC_TITLE, "Import from NFC", "Importa da NFC")                                 \
    X(S_NFC_SCAN_TITLE, "Searching NFC tag", "Cerco il tag NFC")                        \
    X(S_NFC_SCAN_HINT1, "Hold the Flipper's back", "Appoggia il retro del")             \
    X(S_NFC_SCAN_HINT2, "on the ESL label", "Flipper sull'etichetta")                  \
    X(S_NFC_FOUND, "ESL tag found!", "Tag ESL trovato!")                                \
    X(S_NFC_INCOMPATIBLE, "Not compatible", "Non compatibile")                          \
    X(S_NFC_READ_FAIL, "Chip found but not\nreadable. Hold the\nFlipper still, retry.", "Chip trovato ma non\nleggibile. Tieni fermo\nil Flipper e riprova.") \
    X(S_NFC_NOT_ESL, "This NFC chip is not\nan ESL label chip.", "Questo chip NFC non e'\nquello di un'etichetta.") \
    X(S_RETRY, "Retry", "Riprova")                                                      \
    X(S_NFC_NO_NDEF, "NFC chip found, but\nwithout label data.", "Chip NFC trovato ma\nsenza dati etichetta.") \
    X(S_NFC_RADIO, "Radio tag (VUSION):\nnot infrared, not\nsupported", "Tag radio (VUSION):\nnon infrarosso, non\nsupportato") \
    X(S_NFC_UNKNOWN, "ID not recognized.\nType the barcode\ninstead.", "ID non riconosciuto.\nScrivi il codice\na barre.") \
    X(S_NFC_BADCHECK, "Read error.\nRead the tag again.", "Errore di lettura.\nRileggi il tag.") \
    X(S_TAG_USE, "Use this tag", "Usa questo tag")                                      \
    X(S_TAG_INFO, "Details", "Dettagli")                                                \
    X(S_TAG_RENAME, "Rename", "Rinomina")                                               \
    X(S_TAG_SIZE, "Panel size", "Dimensioni")                                           \
    X(S_TAG_DELETE, "Delete", "Elimina")                                                \
    X(S_DELETE_Q, "Delete this tag?", "Eliminare il tag?")                              \
    X(S_CANCEL, "Cancel", "Annulla")                                                    \
    X(S_BC_TITLE, "Barcode: digits 3 to 16", "Codice: cifre da 3 a 16")                             \
    X(S_BC_HINT, "Up/Down digit  OK done", "Su/Giu cifra  OK fine")                     \
    X(S_BC_UNKNOWN, "Unknown model", "Modello sconosciuto")                             \
    X(S_BC_SEGMENT, "segment, pages only", "a segmenti, solo pagine")                   \
    X(S_BC_EXISTS, "Tag already saved", "Tag gia' salvato")                             \
    X(S_BC_INVALID, "Type digits 3 to 16", "Scrivi le cifre 3-16")                         \
    X(S_NAME_TITLE, "Name this tag", "Nome del tag")                                    \
    X(S_TAG_DEFAULT, "Tag", "Tag")                                                      \
    X(S_SIZE, "Size", "Dimensioni")                                                     \
    X(S_COLORS, "Colors", "Colori")                                                     \
    X(S_SAVE, "Save", "Salva")                                                          \
    X(S_C_MONO, "B/W", "B/N")                                                           \
    X(S_C_RED, "B/W+Red", "B/N+Rosso")                                                  \
    X(S_C_YELLOW, "B/W+Yellow", "B/N+Giallo")                                           \
    X(S_INFO_BARCODE, "Barcode", "Codice")                                              \
    X(S_INFO_MODEL, "Model", "Modello")                                                 \
    X(S_INFO_PANEL, "Panel", "Schermo")                                                 \
    X(S_INFO_ADDRESS, "Address", "Indirizzo")                                           \
    X(S_INFO_UNKNOWN, "unknown", "sconosciuto")                                         \
    X(S_NEED_TAG, "Add a tag first", "Prima aggiungi un tag")                           \
    X(S_NEED_SIZE, "Set the panel size first", "Imposta prima le dimensioni")           \
    X(S_SEGMENT_ONLY, "This tag only supports pages", "Questo tag supporta solo pagine") \
    X(S_NO_IMAGES, "No images found", "Nessuna immagine")                               \
    X(S_NO_IMAGES_HINT,                                                                 \
      "Copy .bmp files to\napps_data/esl_zap/images\nor use the web installer",         \
      "Copia file .bmp in\napps_data/esl_zap/images\no usa l'installer web")            \
    X(S_SEND, "Send", "Invia")                                                          \
    X(S_OPTIONS, "Options", "Opzioni")                                                  \
    X(S_OPT_FIT, "Fit", "Adatta")                                                       \
    X(S_FIT_CONTAIN, "Whole", "Intera")                                                 \
    X(S_FIT_COVER, "Fill", "Riempi")                                                    \
    X(S_FIT_STRETCH, "Stretch", "Estendi")                                              \
    X(S_OPT_DITHER, "Dithering", "Dithering")                                           \
    X(S_OPT_INVERT, "Invert", "Inverti")                                                \
    X(S_OPT_ACCENT, "Use color", "Usa colore")                                          \
    X(S_OPT_PAGE, "Page", "Pagina")                                                     \
    X(S_OPT_FRAME, "Frame", "Cornice")                                                  \
    X(S_OPT_TEXTCOLOR, "Text color", "Colore testo")                                    \
    X(S_BLACK, "Black", "Nero")                                                         \
    X(S_RED, "Red", "Rosso")                                                            \
    X(S_YELLOW, "Yellow", "Giallo")                                                     \
    X(S_ON, "On", "Si")                                                                 \
    X(S_OFF, "Off", "No")                                                               \
    X(S_TEXT_TITLE, "Your text (_ = space)", "Testo (_ = spazio)")                      \
    X(S_TX_TITLE, "Sending", "Invio")                                                   \
    X(S_PH_PREP, "Preparing image...", "Preparo immagine...")                           \
    X(S_PH_WAKE, "Waking the tag...", "Sveglio il tag...")                              \
    X(S_PH_DATA, "Sending image...", "Invio immagine...")                               \
    X(S_PH_PART, "Sending part", "Invio parte")                                         \
    X(S_PH_FINISH, "Finishing...", "Completamento...")                                  \
    X(S_TX_AIM, "Point Flipper at the tag", "Punta il Flipper al tag")                  \
    X(S_TX_STOP, "Stop", "Stop")                                                        \
    X(S_DONE, "Done!", "Fatto!")                                                        \
    X(S_DONE_SUB, "Tag refreshes in a few sec", "Il tag si aggiorna a breve")           \
    X(S_AGAIN, "Again", "Ripeti")                                                       \
    X(S_HOME, "Home", "Home")                                                           \
    X(S_FAILED, "Failed", "Errore")                                                     \
    X(S_STOPPED, "Stopped", "Interrotto")                                               \
    X(S_ERR_IR, "IR port is busy", "Porta IR occupata")                                 \
    X(S_ERR_BIG, "Image too complex", "Immagine troppo complessa")                      \
    X(S_ERR_MEM, "Not enough memory", "Memoria insufficiente")                          \
    X(S_ERR_IMAGE, "Image error", "Errore immagine")                                    \
    X(S_PAGE_TIME, "Duration", "Durata")                                                \
    X(S_FOREVER, "Forever", "Sempre")                                                   \
    X(S_PAGE_SEND, "Send command", "Invia comando")                                     \
    X(S_SET_LANG, "Language", "Lingua")                                                 \
    X(S_SET_PHY, "IR mode", "Modo IR")                                                  \
    X(S_SET_WAKE, "Wake-up", "Risveglio")                                               \
    X(S_SET_SENDS, "Repeats", "Ripetizioni")                                            \
    X(S_SET_OUTPUT, "IR LED", "LED IR")                                                 \
    X(S_OUT_INT, "Internal", "Interno")                                                 \
    X(S_OUT_EXT, "Ext. A7", "Est. A7")                                                  \
    X(S_SET_FEEDBACK, "LED/vibro", "LED/vibro")                                         \
    X(S_SET_ABOUT, "About ESL Zap", "Info su ESL Zap")                                  \
    X(S_WELCOME, "Welcome!", "Benvenuto!")                                              \
    X(S_WELCOME_TEXT,                                                                   \
      "Send pictures to\ne-paper tags you own.\nUse it only on your\nown tags!",        \
      "Invia immagini alle\ntue etichette e-paper.\nUsalo solo sui tag\ndi tua proprieta'!") \
    X(S_START, "Start", "Inizia")                                                       \
    X(S_HELP_TEXT,                                                                      \
      "HOW IT WORKS\n"                                                                  \
      "1. Add a tag: type digits 3 to 16 of its barcode (skip the first two and the last one), or import it from NFC.\n"           \
      "2. Choose Send image, Write text or Test card.\n"                                \
      "3. Hold the Flipper 5-30 cm from the tag, IR window facing it.\n"                \
      "4. Press Send and wait. The tag redraws a few seconds later.\n\n"                \
      "IMAGES\n"                                                                        \
      "Copy .bmp files to SD:/apps_data/esl_zap/images, or use the web installer "      \
      "that converts any photo for you. Any size works: ESL Zap scales and dithers.\n\n" \
      "TIPS\n"                                                                          \
      "- No reaction? Set Wake-up to 15 s and Repeats to 3x.\n"                         \
      "- PP16 mode is faster but only newer tags support it.\n"                         \
      "- Up/Down on the home screen switch the active tag.\n"                           \
      "- In text, _ is a space and | starts a new line.\n\n"                            \
      "WARNING\n"                                                                       \
      "Never remove the tag battery: most tags lose their memory for good.\n"           \
      "Use ESL Zap only on tags you own.",                                              \
      "COME FUNZIONA\n"                                                                 \
      "1. Aggiungi un tag: scrivi le cifre da 3 a 16 del codice a barre (salta le prime due e l'ultima), oppure importalo da NFC.\n"   \
      "2. Scegli Invia immagine, Scrivi testo o Immagine prova.\n"                      \
      "3. Tieni il Flipper a 5-30 cm dal tag, con la finestra IR verso il tag.\n"       \
      "4. Premi Invia e attendi. Il tag si aggiorna dopo qualche secondo.\n\n"          \
      "IMMAGINI\n"                                                                      \
      "Copia file .bmp in SD:/apps_data/esl_zap/images, oppure usa l'installer web "    \
      "che converte qualsiasi foto. Va bene ogni dimensione: ESL Zap adatta e "         \
      "applica il dithering.\n\n"                                                       \
      "CONSIGLI\n"                                                                      \
      "- Nessuna reazione? Imposta Risveglio a 15 s e Ripetizioni a 3x.\n"              \
      "- Il modo PP16 e' piu' veloce ma solo i tag recenti lo supportano.\n"            \
      "- Su/Giu nella home cambiano il tag attivo.\n"                                   \
      "- Nel testo, _ e' uno spazio e | va a capo.\n\n"                                 \
      "ATTENZIONE\n"                                                                    \
      "Non togliere mai la batteria del tag: quasi sempre perde la memoria per "        \
      "sempre.\nUsa ESL Zap solo sui tuoi tag.")                                        \
    X(S_ABOUT_TEXT,                                                                     \
      "ESL Zap\nby SangueFreddo\ngithub.com/SangueFreddo\n\nIR e-paper tag toolkit.\nProtocol knowledge from "     \
      "furrtek's public ESL research. Inspired by TagTinker.\n\nMIT License",           \
      "ESL Zap\ndi SangueFreddo\ngithub.com/SangueFreddo\n\nToolkit per tag e-paper IR.\nProtocollo dalla ricerca " \
      "pubblica di furrtek sulle ESL. Ispirato a TagTinker.\n\nLicenza MIT")

typedef enum {
#define EZ_STR_ENUM(id, en, it) id,
    EZ_STRINGS(EZ_STR_ENUM)
#undef EZ_STR_ENUM
        S_COUNT,
} EzString;

void ez_i18n_set(uint8_t language);
const char* T(EzString id);
