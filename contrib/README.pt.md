# Add-ons

*[Read in English](README.md) · [Leggi in italiano](README.it.md)*

O que está aqui **não faz parte do jogo**: nenhum mundo carrega isto se um jogador não instalar.
Cada pasta é um mod feito do jeito que alguém de fora do projeto mandaria um, guardado no
repositório para testar o caminho das contribuições -- instalado na pasta de mods do jogador,
carregado ao lado dos mods do jogo, e jogado -- antes que algo passe para `mods/`.

| Add-on | O que é |
|---|---|
| [`lizard`](lizard) | O homem lagarto, uma espécie para o jogador: feito com o Meshy, cubos no esqueleto do robô, uma cauda que balança, três estilos e os acessórios do robô ajustados a ele |
| [`fox`](fox) | O homem raposa, uma espécie para o jogador: feito com o Meshy, faces de cor chapada no esqueleto do robô, uma cauda peluda que balança, dois estilos (tapa-olho, aljava) e os extras do robô feitos sob medida |

## Testar um

Coloque-o na pasta de mods do jogador, `~/.cubes-and-portals/mods` (Windows:
`%APPDATA%/cubes-and-portals/mods`), como um jogador faria:

- com o launcher: solte a pasta do add-on na janela dele, depois "Adicionar a" (ela é copiada); ou
- enquanto você trabalha nele, um link, para ver cada mudança na hora:

  ```sh
  ln -s "$PWD/contrib/lizard" ~/.cubes-and-portals/mods/lizard
  ```

Depois inicie um mundo pelo menu de título, ou `tools/track.sh`: os dois passam os mods do jogador
para o sandbox e os salões de bolso, e um add-on cujo `mod.conf` diz `rooms = all` para todas as
salas do servidor. Uma espécie aparece na página de aparência (Personagem) depois das do jogo. Para
tirá-lo, apague o link ou a cópia ("Remover" na página de mods do launcher), ou desligue-o para um
mundo na página de mods dele.

## O que é um add-on

Um mod como qualquer outro ([docs/MODDING.pt.md](../docs/MODDING.pt.md)): uma pasta com `mod.conf` e
`init.luau`, seus arquivos ao lado, nada fora da pasta. Uma espécie para o jogador é um corpo
`robot:robot_<nome>` e seus braços em primeira pessoa `robot:robot_<nome>_arms`, com `rooms = all`
para que seja desenhada em todo mundo; seus estilos, a peça que ela sempre usa e a sua versão dos
acessórios seguem os mesmos nomes ([docs/MODDING.pt.md](../docs/MODDING.pt.md), "As cores e os
acessórios do jogador").

O que o fez -- um script do Blender, uma fonte do Meshy -- também pode ficar na pasta dele. Uma
fonte grande fica fora do repositório (`.gitignore`): `lizard/meshy/lizard.glb` tem 16,6 MB. A da
raposa é pequena (`fox/meshy/`: a imagem de que foi feita, 1 MB, e o modelo do Meshy, 2,7 MB) e
fica.
