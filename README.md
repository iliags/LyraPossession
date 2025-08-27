# Lyra Possession Sample

Game feature sample for the purposes of finding out how to get pawn swapping working in Lyra.

## Notes

### Current Issues

- LyraCharacters have not been tested
- LyraCharacterWithAbilities
  - Overlapping input sets which are applied per-character has not been tested
    - Overlapping inputs will not work if applied to all characters

### Non-Issues

The testing environment does not have quality of life features implemented so some "bugs" will occur. The fixes for these are specific to each game and don't need to be implemented in this project.

- On death, LyraCharacterWithAbilities hangs around for a bit.
  - The character isn't hidden fully and it takes the GC a second or two to cleanup
- The default pawn data spawns instead of the correct pawn data on death.
  - There are a few ways to handle this, but it's mostly specific to each game.
  - This might be fixed later, but for now at least a character spawns with the correct abilities/inputs.

### Edits

Lyra code which is edited is wrapped with `//@EditBegin` and `//@EditEnd`.

### Other

For the purposes of debugging, some things are setup in a weird way (don't copy this structure).

- Input mappings are applied to everyone but the keys do *not* overlap. Conflicting bindings cause neither one to work.
- The player controller has legacy input bindings just for ease of implementation.
- The RPC setup is *not* recommended for production, it's only for testing.
- Character BP's are duplicated because of a bug in 5.6 which occasionally cause changes to child BP's also change parent BP's.
