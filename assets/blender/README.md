# I file Blender

I sorgenti dei modelli del gioco: robot e specie, kart, creature, oggetti in mano, blocchi
disegnati come modelli, armi e oggetti dei mondi. I `.gltf` che il gioco carica stanno nelle
cartelle dei mod e vengono esportati da qui con gli script di `tools/`.

- Quasi tutti si rifanno da zero con lo script `tools/make_*.py` dal nome corrispondente, che
  li salva qui ed esporta i modelli nei mod: per esempio
  `blender --background --python tools/make_chess_models.py`.
- Gli altri (`robot.blend`, `robot18.blend`, `kart.blend`, `mobs.blend`,
  `chest_furnace.blend`, `hopper.blend`, `robot_tools_arms.blend`) sono la base che gli script
  `tools/export_*.py` e `make_*.py` aprono o leggono; il commento in testa a ogni script dice
  quale file vuole.

Aperti con Blender 5.2. I `.blend1` (i backup che Blender lascia salvando) non vanno committati.
