# Lyra Possession Sample

Game feature sample for the purposes of finding out how to get pawn swapping working in Lyra.

## Notes

Due to how this thing has been constructed (and network shenanigans), the standard C++ debugger isn't as useful. Le print-to-console debugging method will probably be the most reliable to pin-point where to use the debugger.

### Current Issues

- Inputs are double-bound
  - Single button press, two input events triggered
  - The character animation plays twice as fast when this happens
- Abilities don't trigger
  - Physical inputs are triggered, the ASC does not trigger the abilities
  - Movement and mouse look almost always works when this occurs
  - Jump *or* crouch won't work
  - All other abilities don't work
- Inputs are improperly bound
  - Has similar symptoms of abilities not triggering, but another round of upossess/repossess sometimes fixes it
- Respawn doesn't work on `LyraCharacterWithAbilities`
- The unpossession chain does not fully clean up after itself

### Edits

Lyra code which is edited is wrapped with `//@EditBegin` and `//@EditEnd`.

### Other

For the purposes of debugging, some things are setup in a weird way (don't copy this structure).

- Input mappings are applied to everyone but the keys do *not* overlap. Conflicting bindings cause neither one to work.
- The player controller has legacy input bindings just for ease of implementation.
- The RPC setup is *not* recommended for production, it's only for testing.
