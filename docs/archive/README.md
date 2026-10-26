# Archivio

Lo storico del progetto fino al 2026-09-13, spostato qui quando i test sono stati ridotti a
quelli fondamentali e la documentazione riscritta sullo stato attuale. Niente di questa
cartella descrive il codice di oggi: per quello ci sono [`../../README.md`](../../README.md),
[`../ARCHITECTURE.md`](../ARCHITECTURE.md) e [`../../TODO.md`](../../TODO.md).

| File | Cosa contiene |
|---|---|
| [`README-storia.md`](README-storia.md) | Il vecchio README: il racconto milestone per milestone (M0-M13), con misure, sabotaggi e limiti dichiarati |
| [`TODO-storia.md`](TODO-storia.md) | Il vecchio TODO: ogni voce OPEN-n / CLOSED-n per intero, con sintomi, ipotesi, misure e chiusure. I commenti in `src/` che citano «TODO.md OPEN-n» rimandano qui |
| [`TODO-diario-2026-10-09.md`](TODO-diario-2026-10-09.md) | Il TODO dal 2026-09-13 al 2026-10-09: ogni funzione costruita con misure, difetti trovati e chiusi. Le voci ancora aperte sono riassunte in `TODO.md`; gli OPEN-n citati in `src/` sono qui o in `TODO-storia.md` |
| [`README-M13-2026-10-09.md`](README-M13-2026-10-09.md) | La riga di M13 del README com'era il 2026-10-09, il racconto lungo dei mondi a tema |
| [`M0_FINDINGS.md`](M0_FINDINGS.md) | Spike di validazione: portali su raylib, near plane obliquo, prestazioni |
| [`M1_FINDINGS.md`](M1_FINDINGS.md) | Core voxel: meshing, luce, mondo 512 |
| [`M2_FINDINGS.md`](M2_FINDINGS.md) | Modding: VM per mod, capability, hot reload |
| [`M3_FINDINGS.md`](M3_FINDINGS.md) | Server e rete: handshake, streaming, persistenza |
| [`M4_FINDINGS.md`](M4_FINDINGS.md) | Portali completi: ricorsione, taglio dei corpi, orientamenti |
| [`M5_FINDINGS.md`](M5_FINDINGS.md) | Due mondi in un processo |
| [`M6_FINDINGS.md`](M6_FINDINGS.md) | Portali fra server: ticket, link lenti, cifratura |
| [`M7_FINDINGS.md`](M7_FINDINGS.md) | Gioco minimo: inventario, craft, casse, HUD, giorno/notte |
| [`M8_FINDINGS.md`](M8_FINDINGS.md) | ECS e API meccaniche, i quattro blocchi classici dei puzzle a portali |
| [`M9_FINDINGS.md`](M9_FINDINGS.md) | Prestazioni e robustezza: profiler, occlusion culling, migrazione |

I link interni a questi file sono stati corretti allo spostamento. Quelli che citano test o
script cancellati il 2026-09-13 (`tests/*.sh`, i `--*-check` del client) puntano a file che
non esistono più: si recuperano dalla storia git, per esempio
`git show 21acb50:tests/portal1.sh`.
