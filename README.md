# Lyra Possession Sample

Game feature sample for the purposes of finding out how to get pawn swapping working in Lyra.

## Notes

### Current Issues

- LyraCharacters have not been tested
- LyraCharacterWithAbilities
  - Death and respawning have not been tested
  - Overlapping input sets which are applied per-character has not been tested
    - Overlapping inputs will not work if applied to all characters

### Edits

Lyra code which is edited is wrapped with `//@EditBegin` and `//@EditEnd`.

### Other

For the purposes of debugging, some things are setup in a weird way (don't copy this structure).

- Input mappings are applied to everyone but the keys do *not* overlap. Conflicting bindings cause neither one to work.
- The player controller has legacy input bindings just for ease of implementation.
- The RPC setup is *not* recommended for production, it's only for testing.
- Character BP's are duplicated because of a bug in 5.6 which occasionally cause changes to child BP's also change parent BP's.
