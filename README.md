# Using C++ to Build a Traditional Roguelike, Second Edition

The code for *Using C++ to Build a Traditional Roguelike: From an Empty Window to a Finished Dungeon Crawler, with SDL 3*, second edition, by John Horton. It's *Amulets of SDL*, the game the book builds, as it stands at the end of every chapter. You'll find the book, and the rest of the *Learning C++ by Building Games* series, at [GameCodeSchool.com](https://gamecodeschool.com).

Looking for the first edition's code? It's still at [Roguelike-tutorial-series](https://github.com/EliteIntegrity/Roguelike-tutorial-series).

## What's here

There's a folder for each chapter, from `Chapter 01` to `Chapter 27`. Each holds a complete Visual Studio project, as it stands at the end of that chapter: the code in `src`, the font, tiles and sounds in `assets`, the game's data in `data` from Chapter 16 on, and the two SDL DLLs it needs to run. SDL 3 and SDL_ttf sit beside the folders, in `SDL3` and `SDL3_ttf`.

| Folder | Chapter |
|---|---|
| `Chapter 01` | 1. A Window of Glyphs |
| `Chapter 02` | 2. Carving the Dungeon |
| `Chapter 03` | 3. The @ Moves |
| `Chapter 04` | 4. Seeing and Remembering |
| `Chapter 05` | 5. Something Stirs |
| `Chapter 06` | 6. Turns and Blows |
| `Chapter 07` | 7. The Game Class |
| `Chapter 08` | 8. Tile Mode |
| `Chapter 09` | 9. The Mouse |
| `Chapter 10` | 10. Dijkstra Maps |
| `Chapter 11` | 11. Monster Minds |
| `Chapter 12` | 12. Items and the Identification Gamble |
| `Chapter 13` | 13. Equipment and Derived Stats |
| `Chapter 14` | 14. Status Effects |
| `Chapter 15` | 15. Deeper Floors |
| `Chapter 16` | 16. Content as Data |
| `Chapter 17` | 17. Save and Load, Done Right |
| `Chapter 18` | 18. Caves |
| `Chapter 19` | 19. Doors, Secrets and Traps |
| `Chapter 20` | 20. Vaults |
| `Chapter 21` | 21. Ranged Combat |
| `Chapter 22` | 22. Character Classes |
| `Chapter 23` | 23. Commands That Take Many Turns |
| `Chapter 24` | 24. Juice |
| `Chapter 25` | 25. Sound |
| `Chapter 26` | 26. Victory |
| `Chapter 27` | 27. A Bot That Plays |

The code in the book matches these files character for character, at every checkpoint. So when something won't build, compare your files with the chapter's folder. You can start the book at any chapter, too, from the folder of the chapter before it.

## On Windows

Open a chapter's `AmuletsOfSDL.slnx` in Visual Studio 2026, choose **Trust and Continue** if Visual Studio asks, and press **F5**. Each project points at the `SDL3` and `SDL3_ttf` folders at the top of the repository, so it builds wherever you put the repository, as long as the folders stay together. It opens with **Show All Files** on, as Chapter 1 sets it up, so Solution Explorer shows `src` and the other folders as they are on disk.

The project you make yourself, in Chapter 1, points at `C:\SDL3` and `C:\SDL3_ttf` instead, where Appendix A and Chapter 1 put SDL.

## On macOS and Linux

`CMakeLists.txt` builds every chapter with CMake. It uses SDL and SDL_ttf if they're installed and new enough, and downloads them the first time if they aren't. Appendix B of the book, For Mac and Linux Users, walks through it with VS Code.

## Licenses

The code is under the license in [LICENSE.md](LICENSE.md). You may use it in your own games and programs, free or commercial, but please don't republish it as teaching material of your own.

The tiles and sounds are from my own games, *Ormeq* and *Ammerow: Lands Beyond*, and they have a license of their own, in [ASSETS-LICENSE.md](ASSETS-LICENSE.md). You may use them in your own games, free or commercial, with credit.

The font, `RobotoMono-Light.ttf`, is Roboto Mono, copyright 2015 The Roboto Mono Project Authors, under the Apache License 2.0. A copy of the license is beside the font in each `assets` folder, as `RobotoMono-LICENSE.txt`.

SDL 3 and SDL_ttf are copyright Sam Lantinga, under the zlib license, which is in their folders. SDL_ttf includes FreeType and HarfBuzz, under their own licenses.
