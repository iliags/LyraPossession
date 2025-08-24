# LyraPossession

Game feature sample for the purposes of finding out how to get pawn swapping working in Lyra.

## Current Issues

- Inputs are double-bound
  - Single button press, two input events triggered
  - The character animation plays twice as fast when this happens
- Abilities don't trigger
  - Physical inputs are triggered, the ASC does not trigger the abilities
  - Movement usually always works when this occurs
  - Jump *or* crouch won't work
  - All other abilities don't work
- Inputs are improperly bound
  - Has similar symptoms of abilities not triggering, but another round of upossess/repossess sometimes fixes it
- Respawn doesn't work on `LyraCharacterWithAbilities`

## Notes

### Edits

Lyra code which is edited is wrapped with `//@EditBegin` and `//@EditEnd`.

### Other

For the purposes of debugging, some things are setup in a weird way (don't copy this structure).

- Input mappings are applied to everyone but the keys do *not* overlap. Conflicting bindings cause neither one to work.
- The player controller has legacy input bindings just for ease of implementation.
- The RPC setup is *not* recommended for production, it's only for testing.
